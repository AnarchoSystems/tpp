# tpp — Template Preprocessor

`tpp` is a C++17 template engine. It parses type definitions (structs, variants, enums) and template sources (`.tpp`) that use `@…@` delimiters for expressions and control flow (`@if@`, `@for@`, `@switch@`, `@comment@`, `@end for@`, etc.), then renders them against JSON input.

## Architecture

### Backend Model

tpp uses a **single compiler frontend / multiple backends** design. The `tpp` CLI compiles everything to a self-contained JSON document (`IR`). Backends consume that JSON and have an easy, focused job:

| Backend | Binary | Job |
|---|---|---|
| C++ code gen | `tpp2cpp` | Emits typed C++ struct/function headers from the intermediate representation |
| Script rendering | `render-tpp` | Renders a named template with a JSON input — for scripting/testing |
| C++ runtime | `lib_tpp` | Full compiler + renderer as a linkable library |
| Language server | `tpp-lsp` | Powers VS Code diagnostics and live preview |

Because the compiler has already done all type-checking, each backend doesn't need to reimplement language logic. Generated C++ functions are compile-time safe — passing the wrong argument type is a C++ compile error.

### Implementation Components

| Component | File | Role |
|---|---|---|
| Type parsing | `TypedefParser.cc` | Tokenizes/parses type definitions |
| Template parsing | `TemplateParser.cc` | Builds the template AST |
| Rendering | `Rendering.cc` | Executes the AST against a `RenderContext`; handles alignment and policy scopes |
| Compiler bridge | `IR.cc` | Type/function lookup for the renderer |
| Public API | `Libraries/lib_tpp/include/tpp/` | `Compiler.h`, `IR.h`, `Types.h`, `AST.h`, `Policy.h`, `Diagnostic.h`, etc. |
| Internal headers | `tpp/` | `TypedefParser.h`, `TemplateParser.h`, `Tokenizer.h`, `Parser.h` |
| Compiler CLI | `Executables/tpp/Main.cc` | Command-line entry point; reads tpp-config.json, emits JSON to stdout |
| tpp2cpp CLI | `Executables/backends/tpp2cpp/Main.cc` | Generates C++ types/functions/impl from intermediate representation JSON |
| render-tpp CLI | `Executables/backends/render-tpp/Main.cc` | Renders a named template with JSON input from stdin |
| `tpp_add` macro | `cmake/TppHelpers.cmake` | CMake helper: automates three-step tpp2cpp code generation |

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
- Required files: a `tpp-config.json`, at least one template file, an `input.json`, and one expected-result file (`expected_output.txt`, `expected_diagnostics.json`, or `expected_errors.json`).
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
- **Policy scope propagation:** A policy declared on a `@for@`, `@render … via@`, or `@switch@` scope propagates through *all* function calls made from within that scope. A callee does not need to re-declare the policy. Use `policy="none"` on a specific interpolation to opt out.
- **Alignment spec length:** The `align="spec"` string length must equal the number of `@&@` separators in the loop body plus one. Too-short spec is a compile error.
