#!/usr/bin/env bash
set -euo pipefail

# Enforce braces around control-flow bodies.
# Supported checks:
# - switch case/default labels must be followed by a block body
# - if / else / else if / for / while / do bodies must start with '{'

scope="${TPP_CASE_BRACE_SCOPE:-all}"

collect_all_files() {
    git ls-files '*.[ch]' '*.cc' '*.cpp' '*.cxx' '*.hh' '*.hpp' '*.hxx'
}

collect_changed_files() {
    if [[ -n "${GITHUB_BASE_REF:-}" ]]; then
        git fetch --no-tags --depth=1 origin "$GITHUB_BASE_REF" >/dev/null 2>&1 || true
        git diff --name-only --diff-filter=ACMR "origin/$GITHUB_BASE_REF"...HEAD -- '*.[ch]' '*.cc' '*.cpp' '*.cxx' '*.hh' '*.hpp' '*.hxx'
        return
    fi

    if git rev-parse --verify HEAD >/dev/null 2>&1; then
        git diff --name-only --diff-filter=ACMR HEAD -- '*.[ch]' '*.cc' '*.cpp' '*.cxx' '*.hh' '*.hpp' '*.hxx'
        return
    fi

    collect_all_files
}

if [[ "$scope" == "changed" ]]; then
    mapfile -t files < <(collect_changed_files)
else
    mapfile -t files < <(collect_all_files)
fi

if [[ ${#files[@]} -eq 0 ]]; then
    echo "control-flow brace enforcement skipped (no matching files)"
    exit 0
fi

violations=0

for file in "${files[@]}"; do
    awk -v file="$file" '
BEGIN {
    waiting_for_brace = 0;
    pending_line = 0;
    pending_kind = "";
}
{
    if (waiting_for_brace) {
        if (pending_kind == "case/default" &&
            $0 ~ /^[[:space:]]*(case[[:space:]].*|default)[[:space:]]*:[[:space:]]*$/) {
            pending_line = NR;
            next;
        }
        if ($0 ~ /^[[:space:]]*$/) {
            next;
        }
        if ($0 ~ /^[[:space:]]*\/\//) {
            next;
        }
        if ($0 ~ /^[[:space:]]*\/\*/) {
            next;
        }
        if ($0 ~ /^[[:space:]]*\{/) {
            waiting_for_brace = 0;
            pending_kind = "";
            next;
        }

        printf "%s:%d: %s body must start with '\''{'\''\n", file, pending_line, pending_kind;
        waiting_for_brace = 0;
        pending_kind = "";
        failed = 1;
    }

    if ($0 ~ /^[[:space:]]*else[[:space:]]+if[[:space:]]*\(.*\)[[:space:]]*$/) {
        waiting_for_brace = 1;
        pending_line = NR;
        pending_kind = "else if";
        next;
    }

    if ($0 ~ /^[[:space:]]*else[[:space:]]*$/) {
        waiting_for_brace = 1;
        pending_line = NR;
        pending_kind = "else";
        next;
    }

    if ($0 ~ /^[[:space:]]*if[[:space:]]*\(.*\)[[:space:]]*$/) {
        waiting_for_brace = 1;
        pending_line = NR;
        pending_kind = "if";
        next;
    }

    if ($0 ~ /^[[:space:]]*for[[:space:]]*\(.*\)[[:space:]]*$/) {
        waiting_for_brace = 1;
        pending_line = NR;
        pending_kind = "for";
        next;
    }

    if ($0 ~ /^[[:space:]]*while[[:space:]]*\(.*\)[[:space:]]*$/) {
        waiting_for_brace = 1;
        pending_line = NR;
        pending_kind = "while";
        next;
    }

    if ($0 ~ /^[[:space:]]*do[[:space:]]*$/) {
        waiting_for_brace = 1;
        pending_line = NR;
        pending_kind = "do";
        next;
    }

    if ($0 ~ /^[[:space:]]*(case[[:space:]].*|default)[[:space:]]*:[[:space:]]*$/) {
        waiting_for_brace = 1;
        pending_line = NR;
        pending_kind = "case/default";
    }
}
END {
    if (failed) {
        exit 2;
    }
}
' "$file" || violations=1

done

if [[ $violations -ne 0 ]]; then
    echo "control-flow brace enforcement failed"
    exit 1
fi

echo "control-flow brace enforcement passed"
