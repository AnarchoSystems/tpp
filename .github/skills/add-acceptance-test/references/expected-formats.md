# Expected Result Formats

Structured case expectations live in `test-case.json`. Readable success outputs may live in `expected_output.txt`. IR snapshots still use `expected_ir.json`.

## expected_output.txt (preferred success case)

Contents:
- the exact rendered output

Example for `template main() Hello, world! END`:
```text
Hello, world!
```

Notes:
- the file contents must match the rendered string exactly
- this is the preferred format for human-readable golden outputs

---

## test-case.json (success case, backward-compatible)

Schema:
```json
{
    "expected_output": "<exact rendered output>"
}
```

Example for `template main() Hello, world! END`:
```json
{
    "expected_output": "Hello, world!"
}
```

Notes:
- `expected_output` must match the rendered string exactly
- represent embedded newlines as `\n` in the JSON string

---

## test-case.json (runtime / get-function error)

Used when `tpp::get_function(...)` or `tpp::render_function(...)` returns false.

Schema:
```json
{
    "expect_success": false,
    "expected_errors": {
        "getFunctionError": "<message or empty string>",
        "renderError": "<message or empty string>"
    }
}
```

Example — function not found:
```json
{
    "expect_success": false,
    "expected_errors": {
        "getFunctionError": "function 'main' not found",
        "renderError": ""
    }
}
```

Example — wrong argument count:
```json
{
    "expect_success": false,
    "expected_errors": {
        "getFunctionError": "",
        "renderError": "wrong number of arguments"
    }
}
```

---

## test-case.json (compile / parse error)

Used when the in-process compilation pipeline emits diagnostics while compiling a test case.

Schema — `expected_diagnostics` is an array of LSP `PublishDiagnosticsParams`-style objects:
```json
{
    "expect_success": false,
    "expected_diagnostics": [
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
}
```

Notes:
- Only include the file(s) that actually have diagnostics (omit files with empty diagnostic arrays)
- `uri` is exactly `<test-name>/<file>`, e.g. `error_undefined_type/typedefs.tpp.types` — this is the relative path as reported by the acceptance test harness
- Line/character are **0-based**
- `severity` is always the lowercase string `"error"`

Example — undefined type in `typedefs.tpp.types` line 2, characters 11–22:
```json
{
    "expect_success": false,
    "expected_diagnostics": [
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
}
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

## test-case.json (LSP expectations)

Used by the LSP integration tests to verify semantic tokens, folding ranges, go-to-definition behavior, and hover content.

Top-level sections:

- `lsp.semantic_tokens`
- `lsp.folding_ranges`
- `lsp.go_to_definition`
- `lsp.hover`

All sections are optional. Include only the behaviors the test case is meant to verify.

### semantic_tokens

Schema:
```json
{
    "lsp": {
        "semantic_tokens": {
            "file": "template.tpp",
            "expected_tokens": [
                {"line": 0, "character": 0, "length": 8, "type": "keyword"}
            ]
        }
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
    "lsp": {
        "folding_ranges": {
            "file": "template.tpp",
            "expected": [
                {"startLine": 0, "endLine": 7}
            ]
        }
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
    "lsp": {
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
}
```

Notes:
- `name` is only a test-label for readable failure output
- `file` is relative to the test-case directory
- Positions are 0-based
- To assert that no definition result should exist, omit `expected_file`

### hover

Schema:
```json
{
    "lsp": {
        "hover": [
            {
                "name": "field_hover",
                "file": "template.tpp",
                "line": 3,
                "character": 12,
                "expected_contains": ["string", "field docs"]
            }
        ]
    }
}
```

Notes:
- `name` is only a test-label for readable failure output
- `file` is relative to the test-case directory
- Positions are 0-based
- `expected_contains` lists substrings that must appear somewhere in the returned hover markdown
