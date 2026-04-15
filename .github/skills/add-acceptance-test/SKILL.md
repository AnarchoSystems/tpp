---
name: add-acceptance-test
description: 'Add a new acceptance test case to the tpp project. Use when: adding a test, creating a test case, writing a new test for a template feature or error condition, verifying a new language feature with a test.'
argument-hint: 'Describe the feature or error case to test'
---

# Add Acceptance Test

Each test case is a self-contained directory under `Test/TestCases/<name>/` with at least three files.
The test runner (`Acceptance.cc`) discovers all subdirectories automatically — no registration needed.

## Naming Convention

| Scenario | Pattern | Example |
|---|---|---|
| Feature / happy path | `snake_case` | `for_separator` |
| Expected compiler or parse error | `error_<description>` | `error_missing_end_for` |
| Expected runtime error | `error_<description>` | `error_wrong_arg_count_over` |

## Required Files

| File | Purpose |
|---|---|
| `tpp-config.json` | Declares types, templates, and preview input data used by the test harness. |
| `template.tpp` | Template source. Must contain a `template main(...)` function ending with `END`. |
| `expected_output.txt` **or** `expected_errors.json` **or** `expected_diagnostics.json` | The expected result — see [formats reference](./references/expected-formats.md). |

**Multiple source files are allowed.** List type files under `"types"` and template files under `"templates"` in `tpp-config.json`. Files are processed in the order they appear in the config (types before templates). This lets you split types or templates across several files.

## Procedure

### Step 1 — Choose the test name
Follow the naming convention above. The directory name becomes the test name in CTest output.

### Step 2 — Write `tpp-config.json`
Declare which `.tpp` files contain type definitions and which are templates, and add a preview entry whose `input` object is used by the acceptance tests. Use `"types": []` when no custom types are needed.

```json
{
    "types": ["typedefs.tpp"],
    "templates": ["template.tpp"],
    "previews": [
        {
            "template": "main",
            "input": {"p": {"x": 3, "y": 7}},
            "name": "default"
        }
    ]
}
```

If the test needs no types:
```json
{
    "types": [],
    "templates": ["template.tpp"],
    "previews": [
        {
            "template": "main",
            "input": {},
            "name": "default"
        }
    ]
}
```

Type files are plain `.tpp` files — the config, not the file extension, determines their role.

### Step 3 — Write the type definitions file (if needed)
If you listed a types file in `tpp-config.json`, create it and define any structs or variants.

```
struct Point
{
    x : int;
    y : int;
}
```

### Step 4 — Write `template.tpp`
The entry point is always `template main(...)`. Use `@expr@` for interpolation and structural
directives (`@for@`, `@if@`, `@switch@`, `@end for@`, etc.) for control flow.

```
template main(p: Point)
(@p.x@, @p.y@)
END
```

### Step 5 — Add preview input to `tpp-config.json`
Put the runtime input for the test in `previews[0].input`. Use `{}` when `main` takes no parameters.

```json
{
    "previews": [
        {
            "template": "main",
            "input": {"p": {"x": 3, "y": 7}},
            "name": "default"
        }
    ]
}
```

The acceptance harness reads the first preview entry, so keep the test input in `previews[0]`.

### Step 6 — Write the expected result file
Choose **exactly one** of the three formats:

- **Success** → `expected_output.txt`: the rendered string, **no trailing newline**
- **Compiler/parse diagnostic** → `expected_diagnostics.json`: LSP diagnostic array
- **Runtime or get-function error** → `expected_errors.json`: `{"getFunctionError": "...", "renderError": ""}`

See [expected formats reference](./references/expected-formats.md) for schemas and examples.

### Step 7 — Build and run the new test
Use the `test` skill to build and verify:
1. `Build_CMakeTools` — compile
2. `RunCtest_CMakeTools` — confirm only the new test (and all others) pass

If the test fails, compare actual vs expected and fix the expected output file or the template.

## Key Rules
- `expected_output.txt` must **not** end with a newline — the compiler strips the trailing `\n`
- For diagnostics, `uri` is exactly `<test-name>/<filename>` — the relative path of the file that emitted the error (e.g. `error_undefined_type/typedefs.tpp` or `my_test/template.tpp`)
- `severity` in diagnostics must be the string `"error"` (lowercase)
- Line and character numbers in diagnostic ranges are **0-based**
