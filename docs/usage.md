| `<tpp/Types.h>` | Type definitions (`StructDef`, `EnumDef`, `TypeRef`) and lookup helpers |
# tpp Usage & Tooling Guide

This guide covers the tpp compiler toolchain as a whole: how source files become validated IR, how that IR feeds multiple backends, how to integrate the generated code into builds, and how to use the runtime and editor tooling.

For the template language itself, see the [Language Reference](language.md).

---

## Design Philosophy

tpp is built around a **single compiler frontend and multiple independent backends**.

The compiler (`tpp`) reads your type definitions and template sources, validates them, and emits a **JSON document** describing everything it knows: the compiled types, the compiled template functions (as executable AST nodes), and any registered policies. This JSON document is the stable output contract.

Every backend has an **easy job**: take that detailed intermediate representation and transform it into something useful. Because the compiler has already done all the hard work — type checking, field validation, policy registration, AST construction — a backend can focus purely on transformation without reimplementing any language logic.

This is what gives the project its leverage. tpp is not a C++-only template generator with a few side tools attached. It is a compiler pipeline whose output can be consumed by:

- `tpp2cpp` for native C++ types and functions
- `tpp2java` and `tpp2swift` for source generation in other languages
- `render-tpp` for direct rendering in scripts and tests
- `lib_tpp` for embedding compilation and rendering in a host application
- `tpp-lsp` and the VS Code extension for diagnostics, navigation, and preview

The more complete way to think about the project is: **one typed source language, one validating frontend, many consumers**.

### Why this matters for C++

The `tpp2cpp` backend generates C++ types and function wrappers from the intermediate representation. Because the types were already validated and compiled by `tpp`, the generated C++ code:

- is guaranteed to be structurally consistent with the template logic
- produces functions with signatures that match the template parameter lists exactly
- includes `from_json` / `to_json` serialisation automatically
- embeds the original tpp source in every type via `tpp_typedefs()`, enabling round-tripping

This means template functions can be **called as normal C++ functions** with full compile-time type checking — no JSON at the call site, no runtime type dispatch, no surprises. The heavy checking happens once, at tpp compile time, and the result is a C++ function you can trust.

A standalone **C++ library** (`lib_tpp`) is also available for cases where you want to compile and render templates entirely at runtime, without a build-time code generation step.

### Why this matters beyond C++

Because the compiler output is backend-neutral, the project can keep adding consumers without duplicating the language implementation. That matters for maintenance and for confidence: type checking, variant semantics, policy behavior, and block rendering rules are all defined once and reused.

In practice, that gives tpp a very wide range. It can sit in a build pipeline, a test generator, a codegen workflow, an editor integration, or a dynamic runtime environment without splitting into separate dialects or inconsistent engines.

---

## `tpp` — The Compiler

`tpp` is the compiler frontend. It reads a project directory, compiles all type definitions and templates declared in `tpp-config.json`, and emits the result to stdout as JSON.

### Synopsis

```
tpp [folder]
```

If `folder` is omitted, the current working directory is used.

### Options

| Option | Short | Description |
|---|---|---|
| `--help` | `-h` | Print usage information and exit |
| `--source-ranges` | — | Include `sourceRange` fields in the emitted IR for source mapping and IDE features |

### How It Works

1. `tpp` reads `tpp-config.json` from the target folder.
2. It expands any glob patterns in `"types"` and `"templates"` to concrete file paths (sorted alphabetically within each glob).
3. Type files are added to the compiler first, in config order. Then template files.
4. Registered `"replacement-policies"` are loaded.
5. The compiler validates all types, checks template field accesses, and builds the AST.
6. **On success:** the `IR` JSON is written to stdout.
7. **On failure:** diagnostics are written to stderr in GCC format — compatible with VS Code's problem matcher and most CI log parsers.

### Output

Successful compilation emits a single JSON document on stdout:

```
tpp . > my-project.json
```

This JSON is the input consumed by all backends (`tpp2cpp`, `render-tpp`, etc.) and by the C++ library. For a full description of the IR schema, see the [IR Reference](ir.md).

The IR carries `versionMajor`, `versionMinor`, and `versionPatch` fields that record which compiler produced it. These are informational — the IR schema itself is not yet independently versioned. As the project matures, a formal compatibility policy will be introduced.

### Diagnostics

Compile errors go to stderr in GCC format:

```
path/to/template.tpp:12:5: error: field 'nane' not found on type 'Item'
```

Exit code is `0` on success, non-zero on failure.

---

## `render-tpp` — Scripting Backend

`render-tpp` is the direct execution path for the compiled IR. It is useful in scripts, tests, shell pipelines, and debugging because it exercises the same compiled template semantics without requiring generated host-language code.

### Synopsis

```
render-tpp <template-name> <input-json>
```

| Argument | Description |
|---|---|
| `<template-name>` | The name of the template function to render (e.g. `main`) |
| `<input-json>` | The input data as a JSON string |

The intermediate representation JSON is read from stdin.

### Example

```bash
tpp . | render-tpp main '{"name": "World"}'
```

This is useful in shell pipelines, test scripts, and CI workflows where you want the compiled template behavior immediately, without generating or compiling host-language bindings first.

---

## `tpp2cpp` — C++ Code Generation Backend

`tpp2cpp` reads intermediate representation JSON and generates C++ source files. This is one of the strongest parts of the toolchain: templates stop being an external asset and become ordinary typed C++ APIs that can live inside a normal build.

### Synopsis

```
tpp2cpp <command> [options]
```

The intermediate representation JSON is read from stdin by default, or from a file with `--input`.

### Commands

| Command | Description |
|---|---|
| `types` | Generate a types header (`_types.h`) |
| `functions` | Generate a functions header (`_functions.h`) |
| `impl` | Generate a function implementations file (`_implementation.cc`) |
| `runtime` | Generate a standalone runtime helpers header |

### Options

| Option | Description |
|---|---|
| `-h`, `--help` | Print usage information and exit |
| `-ns <name>` | Wrap all generated code in a C++ namespace |
| `-i <file>` | Add an `#include` directive at the top of the generated file (repeatable) |
| `--input <file>` | Read intermediate representation from `<file>` instead of stdin |
| `--extern-runtime` | Suppress inlining runtime helpers |

### Three-Step Code Generation

tpp2cpp produces three separate files that work together:

#### Step 1: Types Header (`types`)

```bash
tpp . | tpp2cpp types -ns myns > project_types.h
```

Generates a header containing all C++ struct and enum definitions, with:
- `from_json` / `to_json` serialisation
- `tpp_typedefs()` static method on every type, returning the raw tpp source
- Correct declaration ordering (enums before structs, 4-pass structure for circular deps)
- `std::unique_ptr<T>` for recursive fields

#### Step 2: Functions Header (`functions`)

```bash
tpp . | tpp2cpp functions -ns myns -i project_types.h > project_functions.h
```

Generates a header declaring one C++ function per template:

```cpp
// generated example
namespace myns {
    std::string render_item(const Item& item);
    std::string main(const Document& doc);
}
```

#### Step 3: Implementation (`impl`)

```bash
tpp . | tpp2cpp impl -ns myns -i project_functions.h > project_implementation.cc
```

Generates the `.cc` file with native C++ implementations for the compiled template functions. The generated code renders directly from typed C++ values, including native handling for loops, conditionals, switch dispatch, recursive types, and policy application.

The result is not a thin wrapper around a dynamic engine. It is generated C++ that reflects the validated template structure and can be compiled, tested, profiled, and linked like the rest of your code.

### Using Generated Code

```cpp
#include "project_functions.h"

// Call a template function as a normal C++ function:
std::string output = myns::render_item(myItem);
```

Every generated type also has a static `tpp_typedefs()` method returning the type's tpp source code:

```cpp
// Retrieve the raw tpp source that produced this type:
std::string src = myns::Item::tpp_typedefs();
```

---

## `tpp2java` — Java Code Generation Backend

`tpp2java` reads intermediate representation JSON and emits Java source. It demonstrates that the compiler frontend is genuinely backend-neutral: the same validated templates can be lowered into Java rendering code without reinterpreting the language from scratch.

### Synopsis

```bash
tpp2java <command> [options]
```

The intermediate representation JSON is read from stdin by default, or from a file with `--input`.

### Commands

| Command | Description |
|---|---|
| `source` | Generate Java types and rendering functions |
| `runtime` | Generate only the standalone runtime helpers class |

### Options

| Option | Description |
|---|---|
| `-h`, `--help` | Print usage information and exit |
| `--input <file>` | Read intermediate representation from `<file>` instead of stdin |
| `-ns <name>` | Use `<name>` as the generated Java class name |
| `--extern-runtime` | Suppress inlining runtime helpers in generated source |

### Example

```bash
tpp my_project | tpp2java source -ns Functions > Functions.java
```

This backend is used by the Java acceptance-test pipeline in `Test/MakeJavaTest/` and by `cmake/TppJavaHelpers.cmake`.

---

## `tpp2swift` — Swift Code Generation Backend

`tpp2swift` reads intermediate representation JSON and emits Swift source. Like the Java backend, it extends the same typed source language into another target environment while preserving one compiler frontend and one semantic model.

### Synopsis

```bash
tpp2swift <command> [options]
```

The intermediate representation JSON is read from stdin by default, or from a file with `--input`.

### Commands

| Command | Description |
|---|---|
| `source` | Generate Swift types and rendering functions |
| `runtime` | Generate only the standalone runtime helpers file |

### Options

| Option | Description |
|---|---|
| `-h`, `--help` | Print usage information and exit |
| `--input <file>` | Read intermediate representation from `<file>` instead of stdin |
| `-ns <name>` | Wrap generated code in an enum namespace |
| `--extern-runtime` | Suppress inlining runtime helpers in generated source |

### Example

```bash
tpp . | tpp2swift source -ns Functions > Functions.swift
```

This backend is used by the Swift acceptance-test pipeline in `Test/MakeSwiftTest/` and by `cmake/TppSwiftHelpers.cmake`.

---

## C++ Library

`lib_tpp` provides the full compiler and renderer as a linkable library. Use it when you want to compile and render templates at runtime — for example, to support user-defined templates, to load template updates without restarting, to power editor tooling, or to embed tpp as an application subsystem rather than an offline codegen step.

### Key Headers

| Header | Purpose |
|---|---|
| `<tpp/Compiler.h>` | Compile type definitions, templates, and policies |
| `<tpp/IR.h>` | Compiled template IR (types, functions, policies) |
| `<tpp/Rendering.h>` | Render template functions from the IR |
| `<tpp/Types.h>` | Type definitions (`StructDef`, `EnumDef`, `TypeRef`) |
| `<tpp/SemanticModel.h>` | Retained semantic model plus lookup/query helpers used by compiler and tooling |
| `<tpp/Policy.h>` | Policy data model and registry |
| `<tpp/RenderMapping.h>` | Source-to-output range tracking |
| `<tpp/Tooling.h>` | Public source-analysis helpers used by IDE and tooling integrations |
| `<tpp/Diagnostic.h>` | Diagnostic and Position types (LSP-compatible) |
| `<tpp/ArgType.h>` | C++ type mapping helpers for `get_function<Args…>()` |

### Compiling Templates

```cpp
#include <tpp/Compiler.h>

tpp::Compiler compiler;
std::vector<tpp::Diagnostic> diags;

compiler.add_types(typeSource, diags);
compiler.add_templates(templateSource, diags);

// Load a policy from raw file text:
compiler.add_policy_text(policyText, diags);

tpp::IR output;
bool ok = compiler.compile(output);
```

#### Compile From a Generated Type

If you have a type generated by `tpp2cpp`, you can register it directly:

```cpp
compiler.add_type<myns::Item>(diags);
```

`add_type<T>()` calls `T::tpp_typedefs()` automatically.

### Rendering — Dynamic API

```cpp
#include <tpp/Rendering.h>

const tpp::FunctionDef *fn = nullptr;
std::string error;
if (!tpp::get_function(output, "main", fn, error)) {
    // function not found
}

nlohmann::json input = {{"name", "World"}};
std::string result;
if (!tpp::render_function(output, *fn, input, result, error)) {
    // render error (e.g. policy violation)
}
```

### Rendering — Type-Safe API

For typed dispatch, use the templated `get_function<Args…>()` overload:

```cpp
// Returns std::function<std::string(const Item&)>
auto render = output.get_function<Item>("render_item");
std::string out = render(myItem); // throws std::runtime_error on error
```

`ArgType<T>` maps C++ types to parameter types: scalars (`bool`, `long`, `double`, enums) pass by value; everything else passes as `const T&`.

### Tracked Rendering

`renderTracked()` renders a named function and collects source-to-output range mappings for every structural node (for loops, if nodes, switch nodes). This is used by the LSP extension to implement the live preview panel:

```cpp
std::vector<tpp::RenderMapping> mappings;
std::string output = tpp::renderTracked(ir, "main", input, mappings);
```

---

## CMake Integration

The `cmake/TppHelpers.cmake` module provides `tpp_add()` — a convenience function that automates the three-step `tpp2cpp` code generation and wires it into your existing CMake target.

### Including the Module

```cmake
include(cmake/TppHelpers.cmake)
```

Or, if tpp is a subdirectory in your project:

```cmake
add_subdirectory(tpp)
include(tpp/cmake/TppHelpers.cmake)
```

### Project Configuration Options

When embedding tpp as a subproject, the following cache variables control how much of the project is configured and whether third-party dependencies are fetched automatically:

| Option | Default | Description |
|---|---|---|
| `TPP_APPLY_PROJECT_COMPILE_OPTIONS` | `ON` | Apply tpp's default compile options globally during configure |
| `TPP_WARNINGS_AS_ERRORS` | top-level only | Treat warnings as errors in Debug builds when compile options are enabled |
| `TPP_ENABLE_PEDANTIC` | `ON` | Include `-pedantic` in the project compile options |
| `TPP_ENABLE_UNSIGNED_CHAR` | `ON` | Include `-funsigned-char` in the project compile options |
| `TPP_ENABLE_PIPE` | `ON` | Include `-pipe` in the project compile options |
| `TPP_CONFIGURE_TESTS` | top-level only | Configure the `Test/` directory and register CTest entries |
| `TPP_FETCH_NLOHMANN_JSON` | `ON` | Fetch `nlohmann_json` with `FetchContent` |
| `TPP_NLOHMANN_JSON_TARGET` | `nlohmann_json` | Target name to link when JSON support is provided externally |
| `TPP_FETCH_GTEST` | top-level only | Fetch googletest with `FetchContent` |
| `TPP_GTEST_MAIN_TARGET` | `gtest_main` | Target name to link when gtest main is provided externally |

Example for an external consumer that provides its own dependencies and does not need tests:

```bash
cmake -S . -B build \
  -DTPP_CONFIGURE_TESTS=OFF \
  -DTPP_FETCH_NLOHMANN_JSON=OFF \
  -DTPP_NLOHMANN_JSON_TARGET=nlohmann_json \
  -DTPP_FETCH_GTEST=OFF
```

### `tpp_add()` Signature

```cmake
tpp_add(<target>
    SOURCE_DIR <dir>
    NAME       <name>
    [NAMESPACE <namespace>]
    [EXTRA_INCLUDES <file>...]
)
```

| Parameter | Required | Description |
|---|---|---|
| `<target>` | Yes | Existing CMake target to attach generated sources to |
| `SOURCE_DIR` | Yes | Directory containing `tpp-config.json` and the `.tpp` files |
| `NAME` | Yes | Prefix for generated file names (e.g. `mytemplate` → `mytemplate_types.h`, …) |
| `NAMESPACE` | No | C++ namespace to wrap generated code in |
| `EXTRA_INCLUDES` | No | Additional files to `#include` in the generated functions and implementation files |

### Example

```cmake
add_library(mylib mylib.cc)

tpp_add(mylib
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/templates"
    NAME       mytemplate
    NAMESPACE  myns
)

target_link_libraries(mylib PRIVATE lib_tpp)
```

### What `tpp_add()` Does

1. Runs `tpp <SOURCE_DIR>` → `<NAME>-tpp.json`
2. Runs `tpp2cpp types` on the JSON → `<NAME>_types.h`
3. Runs `tpp2cpp functions` → `<NAME>_functions.h`
4. Runs `tpp2cpp impl` → `<NAME>_implementation.cc`

The implementation `.cc` is added as a private source to `<target>`. The generated headers are accessible via the target's include directories — no extra `target_include_directories()` call needed.

All four custom commands re-run automatically whenever any file in `SOURCE_DIR` changes.

### `tpp_java_add()`

The `cmake/TppJavaHelpers.cmake` module provides `tpp_java_add()` for generating and compiling Java acceptance tests.

```cmake
tpp_java_add(<target>
  TEST_DIR <dir>
  NAME     <name>
)
```

It runs `tpp`, then `tpp2java source`, then `make-java-test`, and finally `javac` to compile the generated Java sources.

### `tpp_swift_add()`

The `cmake/TppSwiftHelpers.cmake` module provides `tpp_swift_add()` for generating and compiling Swift acceptance tests.

```cmake
tpp_swift_add(<target>
  TEST_DIR <dir>
  NAME     <name>
)
```

It runs `tpp`, then `tpp2swift source`, then `make-swift-test`, and finally `swiftc` to compile the generated Swift sources.

### Using Generated Code

```cpp
// In your C++ source:
#include "mytemplate_functions.h"

void example(const myns::Item& item) {
    std::string output = myns::render_item(item);
}
```

---

## VS Code Extension

The `tpp-language-support` extension provides a language-aware editing experience for `.tpp`, `.tpp.types`, and `tpp-config.json` files.

### Features

- **Syntax highlighting** for `.tpp` template files and `.tpp.types` type definition files
- **LSP-powered diagnostics** — type errors, missing fields, undefined types, policy violations — shown as red underlines as you type
- **Live preview panel** (`tpp: Open Render Preview`) — renders a template with sample input defined in `tpp-config.json` and updates the view in real time as you edit
- **JSON schema validation** for `tpp-config.json` — autocompletion and error highlighting in the config file

### Installation

The extension is not yet published to the VS Code Marketplace. Install it manually:

1. **Build the tpp-lsp binary** (the language server backend):

   ```bash
  cmake -S . -B build
  cmake --build build --target tpp-lsp
   ```

2. **Build the extension** (requires Node.js):

   ```bash
   cd vscode-extension
   npm install
   npm run compile
   ```

3. **Install the `.vsix`** (optional, for persistent install):

  ```bash
  cd vscode-extension
  npm run package     # produces tpp-language-support-*.vsix
  code --install-extension tpp-language-support-*.vsix
  ```

   Or, for development, open the `vscode-extension` folder in VS Code and press **F5** to launch an Extension Development Host.

### Configuration

By default, the extension looks for the language server at `build/bin/tpp-lsp` relative to the workspace root.

The repository generates `.envrc` during CMake configure, so if you use direnv you can approve it once with `direnv allow` after the first configure.

If you need a different location, set it explicitly in your VS Code `settings.json`:

```json
{
  "tpp.lspServerPath": "build/bin/tpp-lsp"
}
```

The path may be absolute or relative to the workspace root.

### Trace Logging

To debug LSP communication, enable trace logging:

```json
{
  "tpp.trace.server": "verbose"
}
```

Output appears in **Output** → **tpp Language Server**.

### Live Preview

The live preview panel renders a named template with sample input and updates automatically as you edit. Configure it in `tpp-config.json`:

```json
{
  "previews": [
    {
      "name": "Default",
      "template": "main",
      "input": { "name": "World" }
    },
    {
      "name": "From file",
      "template": "main",
      "input": "samples/hello.json"
    }
  ]
}
```

Open a template file and run the **tpp: Open Render Preview** command from the Command Palette (`⌘⇧P` / `Ctrl+Shift+P`). The rendered output updates live as you type.
