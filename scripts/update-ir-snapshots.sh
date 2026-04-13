#!/bin/bash
# update-ir-snapshots.sh — Regenerate IR snapshots and bump version if needed.
# Usage: ./scripts/update-ir-snapshots.sh <build-dir>
set -euo pipefail

BUILD_DIR="$(cd "${1:?Usage: $0 <build-dir>}"; pwd)"
REPO_ROOT="$(cd "$(dirname "$0")/.."; pwd)"
VERSION_JSON="$REPO_ROOT/version.json"
TPP_EXE="$BUILD_DIR/Executables/tpp/tpp"
TEST_EXE="$BUILD_DIR/Test/tpp_acceptance_test"
TEST_DIR="$REPO_ROOT/Test"

has_missing_snapshots=0
missing_snapshot_count=0
for dir in "$REPO_ROOT"/Test/TestCases/*/; do
    test_name=$(basename "$dir")
    case "$test_name" in error_*) continue ;; esac
    if [ -f "$dir/expected_output.txt" ] && [ ! -f "$dir/expected_ir.json" ]; then
        has_missing_snapshots=1
        missing_snapshot_count=$((missing_snapshot_count + 1))
    fi
done

echo "=== Step 1: Build test binary ==="
cmake --build "$BUILD_DIR" --target tpp_acceptance_test -j8

echo "=== Step 2: Run IRStability.SchemaCheck ==="
SCHEMA_OUTPUT=$(cd "$TEST_DIR" && "$TEST_EXE" --gtest_filter="IRStability.SchemaCheck" 2>&1 || true)
echo "$SCHEMA_OUTPUT"

BUMP=""
if echo "$SCHEMA_OUTPUT" | grep -q "IR_SCHEMA_RESULT:MAJOR"; then
    BUMP="major"
elif echo "$SCHEMA_OUTPUT" | grep -q "IR_SCHEMA_RESULT:MINOR"; then
    BUMP="minor"
fi

if [ -z "$BUMP" ] && [ "$has_missing_snapshots" -eq 0 ]; then
    # Check if test passed or if there were simply no snapshots
    if echo "$SCHEMA_OUTPUT" | grep -q "No IR snapshots found"; then
        echo "=== No snapshots yet — generating initial set ==="
    elif echo "$SCHEMA_OUTPUT" | grep -q "IR schema check passed"; then
        echo "=== No schema changes detected — nothing to do ==="
        exit 0
    else
        echo "=== Unknown test result — generating snapshots anyway ==="
    fi
else
    echo "=== Step 3: Bumping $BUMP version ==="
    cmake -DVERSION_JSON="$VERSION_JSON" -DBUMP="$BUMP" \
          -P "$REPO_ROOT/cmake/BumpVersion.cmake"

    echo "=== Step 4: Regenerate Version.h ==="
    cmake -DVERSION_JSON="$VERSION_JSON" \
          -DOUTPUT="$BUILD_DIR/Libraries/lib_tpp/tpp/Version.h" \
          -P "$REPO_ROOT/cmake/GenerateVersionHeader.cmake"

    echo "=== Step 5: Rebuild tpp with new version ==="
    cmake --build "$BUILD_DIR" --target tpp -j8
fi

if [ "$has_missing_snapshots" -eq 1 ]; then
    echo "=== Missing snapshots detected: $missing_snapshot_count ==="
fi

echo "=== Step 6: Regenerate IR snapshots ==="
count=0
for dir in "$REPO_ROOT"/Test/TestCases/*/; do
    test_name=$(basename "$dir")
    # Skip error test cases (they don't produce valid IR)
    case "$test_name" in error_*) continue ;; esac

    if [ -f "$dir/expected_output.txt" ]; then
        ir=$("$TPP_EXE" "$dir" 2>/dev/null) || continue
        echo "$ir" > "$dir/expected_ir.json"
        count=$((count + 1))
    fi
done
echo "   Generated $count snapshot(s)"

echo "=== Step 7: Rebuild test binary ==="
cmake --build "$BUILD_DIR" --target tpp_acceptance_test -j8

echo "=== Step 8: Verify IRStability.SchemaCheck ==="
(cd "$TEST_DIR" && "$TEST_EXE" --gtest_filter="IRStability.SchemaCheck")

echo "=== Done ==="
if [ -n "$BUMP" ]; then
    echo "Version bumped ($BUMP). Remember to commit version.json and snapshots."
else
    echo "Initial snapshots generated. Remember to commit them."
fi
