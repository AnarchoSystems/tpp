---
name: add-acceptance-test
description: 'Add a new acceptance test case to the tpp project. Use when: adding a test, creating a test case, writing a new test for a template feature or error condition, verifying a new language feature with a test.'
argument-hint: 'Describe the feature or error case to test'
---

# Add Acceptance Test

Each test case is a self-contained directory under `Test/TestCases/<name>/` with four files.
The test runner (`Acceptance.cc`) discovers all subdirectories automatically — no registration needed.

## Naming Convention

| Scenario | Pattern | Example |
|---|---|---|
| Feature / happy path | `snake_case` | `for_separator` |
| Expected compiler or parse error | `error_<description>` | `error_missing_end_for` |
| Expected runtime error | `error_<description>` | `error_wrong_arg_count_over` |

## Required Files (all four must exist)

| File | Purpose |
|---|---|
| `typedefs.tpp` | Type definitions (structs, variants). May be empty. |
| `template.tpp` | Template source. Must start with `template main(...)` and end with `END`. |
| `input.json` | JSON object with runtime input data. Use `{}` when `main` takes no parameters. |
| `expected_output.txt` **or** `expected_errors.json` **or** `expected_diagnostics.json` | The expected result — see [formats reference](./references/expected-formats.md). |

## Procedure

### Step 1 — Choose the test name
Follow the naming convention above. The directory name becomes the test name in CTest output.

### Step 2 — Write `typedefs.tpp`
Define any structs or variants needed. Leave the file empty if none are needed.

```
struct Point
{
    x : int;
    y : int;
}
```

### Step 3 — Write `template.tpp`
The entry point is always `template main(...)`. Use `@expr@` for interpolation and structural
directives (`@for@`, `@if@`, `@switch@`, `@end for@`, etc.) for control flow.

```
template main(p: Point)
(@p.x@, @p.y@)
END
```

### Step 4 — Write `input.json`
Provide a JSON object matching the parameters of `main`. Use `{}` for no parameters.

```json
{"p": {"x": 3, "y": 7}}
```

### Step 5 — Write the expected result file
Choose **exactly one** of the three formats:

- **Success** → `expected_output.txt`: the rendered string, **no trailing newline**
- **Compiler/parse diagnostic** → `expected_diagnostics.json`: LSP diagnostic array
- **Runtime or get-function error** → `expected_errors.json`: `{"getFunctionError": "...", "renderError": ""}`

See [expected formats reference](./references/expected-formats.md) for schemas and examples.

### Step 6 — Build and run the new test
Use the `test` skill to build and verify:
1. `Build_CMakeTools` — compile
2. `RunCtest_CMakeTools` — confirm only the new test (and all others) pass

If the test fails, compare actual vs expected and fix the expected output file or the template.

## Key Rules
- `expected_output.txt` must **not** end with a newline — the compiler strips the trailing `\n`
- For diagnostics, `uri` is exactly `<test-name>/typedefs.tpp` or `<test-name>/template.tpp` — a relative path where `<test-name>` is the directory name (e.g. `error_undefined_type/typedefs.tpp`)
- `severity` in diagnostics must be the string `"error"` (lowercase)
- Line and character numbers in diagnostic ranges are **0-based**
