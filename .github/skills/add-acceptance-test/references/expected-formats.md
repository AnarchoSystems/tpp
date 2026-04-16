# Expected Result File Formats

## expected_output.txt (success case)

Plain text — the exact rendered output of the template. **No trailing newline.**

Example for `template main() Hello, world! END`:
```
Hello, world!
```

---

## expected_errors.json (runtime / get-function error)

Used when `tpp::get_function(...)` or `tpp::render_function(...)` returns false.

Schema:
```json
{
    "getFunctionError": "<message or empty string>",
    "renderError": "<message or empty string>"
}
```

Example — function not found:
```json
{
    "getFunctionError": "function 'main' not found",
    "renderError": ""
}
```

Example — wrong argument count:
```json
{
    "getFunctionError": "",
    "renderError": "wrong number of arguments"
}
```

---

## expected_diagnostics.json (compile / parse error)

Used when the in-process compilation pipeline emits diagnostics while compiling a test case.

Schema — array of LSP `PublishDiagnosticsParams`-style objects:
```json
[
    {
        "uri": "<test-name>/typedefs.tpp.types",
        "diagnostics": [
            {
                "range": {
                    "start": {"line": <0-based>, "character": <0-based>},
                    "end":   {"line": <0-based>, "character": <0-based>}
                },
                "message": "<human-readable error>",
                "severity": "error"
            }
        ]
    }
]
```

Notes:
- Only include the file(s) that actually have diagnostics (omit files with empty diagnostic arrays)
- `uri` is exactly `<test-name>/<file>`, e.g. `error_undefined_type/typedefs.tpp.types` — this is the relative path as reported by the acceptance test harness
- Line/character are **0-based**
- `severity` is always the lowercase string `"error"`

Example — undefined type in `typedefs.tpp.types` line 2, characters 11–22:
```json
[
    {
        "uri": "error_undefined_type/typedefs.tpp.types",
        "diagnostics": [
            {
                "range": {
                    "start": {"line": 2, "character": 11},
                    "end":   {"line": 2, "character": 22}
                },
                "message": "undefined type 'UnknownType'",
                "severity": "error"
            }
        ]
    }
]
```

---

## expected_ir.json (IR snapshot)

Used by the `IRStability` test to detect intentional or accidental schema changes in the compiler output.

Schema:
- A full JSON serialization of `tpp::IR`
- Version fields are present in the snapshot, but schema comparison ignores `versionMajor`, `versionMinor`, and `versionPatch`

When to use it:
- Add or update `expected_ir.json` when a success-case test should participate in IR schema stability checks
- Update snapshots when an IR change is intentional and verified

How to update:
- Run the repo target that refreshes snapshots: `cmake --build build --target update_ir_snapshots`

Notes:
- Not every success case needs an IR snapshot, but structural language features should generally have one
- The snapshot should be committed exactly as generated so schema diffs stay mechanical

---

## lsp-test.json (LSP expectations)

Used by the LSP integration tests to verify semantic tokens, folding ranges, and go-to-definition behavior.

Top-level sections:

- `semantic_tokens`
- `folding_ranges`
- `go_to_definition`

All sections are optional. Include only the behaviors the test case is meant to verify.

### semantic_tokens

Schema:
```json
{
    "semantic_tokens": {
        "file": "template.tpp",
        "expected_tokens": [
            {"line": 0, "character": 0, "length": 8, "type": "keyword"}
        ]
    }
}
```

Notes:
- `file` is relative to the test-case directory
- Positions are 0-based
- Tokens must match exactly at the expected positions

### folding_ranges

Schema:
```json
{
    "folding_ranges": {
        "file": "template.tpp",
        "expected": [
            {"startLine": 0, "endLine": 7}
        ]
    }
}
```

Notes:
- `startLine` and `endLine` are 0-based
- The expected array is matched exactly

### go_to_definition

Schema:
```json
{
    "go_to_definition": [
        {
            "name": "TypeDef_ref",
            "file": "template.tpp",
            "line": 0,
            "character": 22,
            "expected_file": "typedefs.tpp",
            "expected_line": 7,
            "expected_character": 7
        }
    ]
}
```

Notes:
- `name` is only a test-label for readable failure output
- `file` is relative to the test-case directory
- Positions are 0-based
- To assert that no definition result should exist, omit `expected_file`
