#!/bin/sh
set -eu

REPO_ROOT=$(git rev-parse --show-toplevel)
BUILD_DIR="$REPO_ROOT/build"

require_build_dir() {
    if [ ! -d "$BUILD_DIR" ]; then
        echo "Build directory '$BUILD_DIR' does not exist. Configure the project first." >&2
        exit 1
    fi
}

staged_touches() {
    git diff --cached --name-only --diff-filter=ACMR -- "$@" | grep -q .
}

check_generated_header_clean() {
    header="$1"
    hint="$2"
    if ! git diff --quiet -- "$header"; then
        echo "$header is out of date. $hint" >&2
        git --no-pager diff -- "$header" >&2 || true
        exit 1
    fi
}

require_build_dir

need_ir_check=0
need_codegen_check=0

if staged_touches \
    "$REPO_ROOT/Libraries/lib_tpp/ir.tpp.types" \
    "$REPO_ROOT/Libraries/lib_tpp/tpp-config.json"
then
    need_ir_check=1
fi

if staged_touches \
    "$REPO_ROOT/Executables/backends/codegen/codegen.tpp.types" \
    "$REPO_ROOT/Executables/backends/codegen/tpp-config.json"
then
    need_codegen_check=1
fi

if [ "$need_ir_check" -eq 1 ] || [ "$need_codegen_check" -eq 1 ]; then
    echo "Checking generated headers before build/test..."
    targets=""
    if [ "$need_ir_check" -eq 1 ]; then
        targets="$targets update-ir-types"
    fi
    if [ "$need_codegen_check" -eq 1 ]; then
        targets="$targets update-codegen-types"
    fi
    (cd "$BUILD_DIR" && cmake --build . --target $targets -j8)

    if [ "$need_ir_check" -eq 1 ]; then
        check_generated_header_clean \
            "$REPO_ROOT/Libraries/lib_tpp/include/tpp/IR.h" \
            "Run 'cmake --build build --target update-ir-types' and stage the result."
    fi

    if [ "$need_codegen_check" -eq 1 ]; then
        check_generated_header_clean \
            "$REPO_ROOT/Executables/backends/codegen/CodegenTypes.h" \
            "Run 'cmake --build build --target update-codegen-types' and stage the result."
    fi
fi

echo "Building project..."
(cd "$BUILD_DIR" && cmake --build . -j8)

echo "Running tests..."
(cd "$BUILD_DIR" && ctest -j8 --output-on-failure)