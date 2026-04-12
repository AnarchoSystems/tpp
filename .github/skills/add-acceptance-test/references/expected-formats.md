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

Used when `compiler.add_types(...)` or `compiler.add_templates(...)` emits diagnostics.

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
