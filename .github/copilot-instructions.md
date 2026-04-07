# tpp — Template Preprocessor

`tpp` is a C++17 template engine. It parses type definitions (structs, variants, enums in `.tpp.types` files) and template sources (`.tpp`) that use `@…@` delimiters for expressions and control flow (`@if@`, `@for@`, `@switch@`, `@end for@`, etc.), then renders them against JSON input.

## Architecture

| Component | File | Role |
|---|---|---|
| Type parsing | `TypedefParser.cc` | Tokenizes/parses `.tpp.types` definitions |
| Template parsing | `TemplateParser.cc` | Builds the template AST |
| Rendering | `Rendering.cc` | Executes the AST against a `RenderContext` |
| Compiler bridge | `CompilerOutput.cc` | Type/function lookup for the renderer |
| Public API | `include/tpp/` | `Compiler.h`, `Types.h`, `AST.h`, `Diagnostic.h`, etc. |
| Internal headers | `tpp/` | `TypedefParser.h`, `TemplateParser.h`, `Tokenizer.h`, `Parser.h` |
| CLI tool | `Executables/tpp/Main.cc` | Command-line entry point |
| `tpp_library` macro | `cmake/TppHelpers.cmake` | CMake helper consumed by downstream test targets |

All logic lives in the `tpp` namespace.

## Build & Test

→ See `.github/skills/test/SKILL.md` for the full procedure.

**Quick reference:**
- Build: use `Build_CMakeTools` (VS Code CMake Tools). Only fall back to `cd build && make -j4` when that tool is unavailable.
- Test: use `RunCtest_CMakeTools`. Build directory is `build/`; test binary is `build/Test/lib_cpp_test`.
- CMake standard: C++17, min CMake 3.20.

## Code Style

- **Indent:** 4 spaces, no tabs.
- **Naming:** `PascalCase` for classes/structs; `snake_case` for functions and variables.
- **Namespace:** wrap all implementation code in `namespace tpp { … }`.
- **Section dividers:** `// ════════…` decorative separators are conventional in larger files.

## Acceptance Tests

→ See `.github/skills/add-acceptance-test/SKILL.md` for the full procedure.

**Quick reference:**
- Each test is a self-contained directory under `Test/TestCases/<name>/`.
- Required files: `typedefs.tpp.types`, `template.tpp`, `input.json`, and one expected-result file (`expected_output.txt`, `expected_diagnostics.json`, or `expected_errors.json`).
- No test registration needed — the test runner auto-discovers subdirectories.
- `error_` prefix in directory name signals an expected failure test.
- **No trailing newline** in `expected_output.txt` — `Program::run()` strips the final newline.

## Key Pitfalls

- **`@@` is an error:** Two adjacent `@` with nothing between them is a parse error. Adjacent directives like `@v@@end for@` work naturally — the tokenizer is mode-based (text mode / directive mode); the closing `@` of one directive puts it back in text mode, so the next `@` opens a new directive.
- **Block vs inline lines:** Lines that contain *only* structural directives (plus whitespace) are "block lines" — they are consumed without emitting output. Mixed lines (text + directive) are inline.
- **Block indentation:** Leading whitespace of the first non-empty line in a block body is the "zero marker" — all body lines are de-indented by that amount, then re-indented at the insertion column.
- **Recursive types:** `FieldDef`/`VariantDef` carry `bool recursive = false`; `tpp2cpp` generates `std::unique_ptr<T>` for recursive fields.
- **`replace_string_in_file` non-breaking spaces:** The tool can inject U+00A0 in `| ` separator positions. Use `hexdump` to diagnose if tpp reports unexpected key errors.
- **`file(GLOB …)` in CMakeLists.txt:** After adding new source files, re-run `cmake ..` to reconfigure before building.
