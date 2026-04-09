# tpp Usage & Tooling Guide

This guide covers the tpp compiler toolchain — how to integrate it into your build system, use the CLIs, consume the C++ library, and set up the VS Code extension.

For the template language itself, see the [Language Reference](language.md).

---

## Design Philosophy

tpp is built around a **single compiler frontend and multiple independent backends**.

The compiler (`tpp`) reads your type definitions and template sources, validates them, and emits a **JSON document** describing everything it knows: the compiled types, the compiled template functions (as executable AST nodes), and any registered policies. This JSON document is the stable output contract.

Every backend has an **easy job**: take that detailed compiler output and transform it into something useful. Because the compiler has already done all the hard work — type checking, field validation, policy registration, AST construction — a backend can focus purely on transformation without reimplementing any language logic.

### Why this matters for C++

The `tpp2cpp` backend generates C++ types and function wrappers from the compiler output. Because the types were already validated and compiled by `tpp`, the generated C++ code:

- is guaranteed to be structurally consistent with the template logic
- produces functions with signatures that match the template parameter lists exactly
- includes `from_json` / `to_json` serialisation automatically
- embeds the original tpp source in every type via `tpp_typedefs()`, enabling round-tripping

This means template functions can be **called as normal C++ functions** with full compile-time type checking — no JSON at the call site, no runtime type dispatch, no surprises. The heavy checking happens once, at tpp compile time, and the result is a C++ function you can trust.

A standalone **C++ library** (`lib_tpp`) is also available for cases where you want to compile and render templates entirely at runtime, without a build-time code generation step.

---

## `tpp` — The Compiler

`tpp` is the compiler frontend. It reads a project directory, compiles all type definitions and templates declared in `tpp-config.json`, and emits the result to stdout as JSON.

### Synopsis

```
tpp [options] [folder]
```

If `folder` is omitted, the current working directory is used.

### Options

| Option | Short | Description |
|---|---|---|
| `--help` | `-h` | Print usage information and exit |
| `--verbose` | `-v` | Pretty-print the output and log progress to stdout (human-readable mode) |
| `--log <file>` | | Also write all output to the specified log file (for debugging) |

### How It Works

1. `tpp` reads `tpp-config.json` from the target folder.
2. It expands any glob patterns in `"types"` and `"templates"` to concrete file paths (sorted alphabetically within each glob).
3. Type files are added to the compiler first, in config order. Then template files.
4. Registered `"replacement-policies"` are loaded.
5. The compiler validates all types, checks template field accesses, and builds the AST.
6. **On success:** the `CompilerOutput` JSON is written to stdout.
7. **On failure:** diagnostics are written to stderr in GCC format — compatible with VS Code's problem matcher and most CI log parsers.

### Output

Successful compilation emits a single JSON document on stdout:

```
tpp ./my-project > my-project.json
```

This JSON is the input consumed by all backends (`tpp2cpp`, `render-tpp`, etc.) and by the C++ library.

### Diagnostics

Compile errors go to stderr in GCC format:

```
path/to/template.tpp:12:5: error: field 'nane' not found on type 'Item'
```

Exit code is `0` on success, non-zero on failure.

---

## `render-tpp` — Scripting Backend

`render-tpp` is a minimal rendering backend for scripting and testing. It reads compiler output JSON from stdin, renders a named template with a JSON input, and writes the result to stdout.

### Synopsis

```
render-tpp <template-name> <input-json>
```

| Argument | Description |
|---|---|
| `<template-name>` | The name of the template function to render (e.g. `main`) |
| `<input-json>` | The input data as a JSON string |

Compiler output JSON is read from stdin.

### Example

```bash
tpp ./my-project | render-tpp main '{"name": "World"}'
```

This is useful in shell pipelines, test scripts, and CI workflows where you want to render a template without writing C++ at all.

---

## `tpp2cpp` — C++ Code Generation Backend

`tpp2cpp` reads compiler output JSON and generates C++ source files. It has three modes, each producing one output file.

### Synopsis

```
tpp2cpp [options]
```

Compiler output JSON is read from stdin by default, or from a file with `--input`.

### Options

| Option | Short | Description |
|---|---|---|
| `--help` | `-h` | Print usage information and exit |
| `--verbose` | `-v` | Print verbose output |
| `--log <file>` | | Also write output to a log file |
| `--namespace <name>` | `-ns` | Wrap all generated code in a C++ namespace |
| `--types` | `-t` | Generate a types header (`_types.h`) |
| `--functions` | `-fun` | Generate a functions header (`_functions.h`) |
| `--implementation` | `-impl` | Generate a function implementations file (`_implementation.cc`) |
| `--include <file>` | `-i` | Add an `#include` directive at the top of the generated file (repeatable) |
| `--input <file>` | | Read compiler output from `<file>` instead of stdin |

Exactly one of `--types`, `--functions`, or `--implementation` must be specified.

### Three-Step Code Generation

tpp2cpp produces three separate files that work together:

#### Step 1: Types Header (`-t`)

```bash
tpp ./project | tpp2cpp -t -ns myns > project_types.h
```

Generates a header containing all C++ struct and enum definitions, with:
- `from_json` / `to_json` serialisation
- `tpp_typedefs()` static method on every type, returning the raw tpp source
- Correct declaration ordering (enums before structs, 4-pass structure for circular deps)
- `std::unique_ptr<T>` for recursive fields

#### Step 2: Functions Header (`-fun`)

```bash
tpp ./project | tpp2cpp -fun -ns myns -i project_types.h > project_functions.h
```

Generates a header declaring one C++ function per template:

```cpp
// generated
namespace myns {
    std::string render_item(const Item& item);
    std::string main(const Document& doc);
}
```

#### Step 3: Implementation (`-impl`)

```bash
tpp ./project | tpp2cpp -impl -ns myns -i project_functions.h > project_implementation.cc
```

Generates the `.cc` file with the function bodies. At the moment, this just serializes the input parameters and hands them to a `CompilerOutput` rendered into the source. A data structure that makes it possible to render the template with native c++ code is planned for later (this should make it possible to render other languages as well).

### Using Generated Code

```cpp
#include "project_functions.h"

// Call a template function as a normal C++ function:
std::string output = myns::render_item(myItem);
```

Every generated type also has a static `tpp_typedefs()` method:

```cpp
// Retrieve the raw tpp source that produced this type:
std::string src = myns::Item::tpp_typedefs();
```

---

## C++ Library

`lib_tpp` provides the full compiler and renderer as a linkable library. Use it when you want to compile and render templates at runtime — for example, to support user-defined templates, to load template updates without restarting, or to use tpp as a rendering engine inside a C++ application.

### Key Headers

| Header | Purpose |
|---|---|
| `<tpp/Compiler.h>` | Compile type definitions, templates, and policies |
| `<tpp/CompilerOutput.h>` | Query and render compiled templates |
| `<tpp/FunctionSymbol.h>` | Render a single named template function |
| `<tpp/Types.h>` | Type definitions (`StructDef`, `EnumDef`, `TypeRegistry`) |
| `<tpp/Policy.h>` | Policy data model and registry |
| `<tpp/RenderMapping.h>` | Source-to-output range tracking |
| `<tpp/Diagnostic.h>` | Diagnostic and Position types (LSP-compatible) |
| `<tpp/ArgType.h>` | C++ type mapping helpers for `get_function<Args…>()` |

### Compiling Templates

```cpp
#include <tpp/Compiler.h>

tpp::Compiler compiler;
std::vector<tpp::Diagnostic> diags;

compiler.add_types(typeSource, diags);
compiler.add_templates(templateSource, diags);

// Load a policy from a parsed JSON object:
std::string policyError;
compiler.add_policy(policyJson, policyError);

tpp::CompilerOutput output;
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
tpp::FunctionSymbol fn;
std::string error;
if (!output.get_function("main", fn, error)) {
    // function not found
}

nlohmann::json input = {{"name", "World"}};
std::string result;
if (!fn.render(input, result, error)) {
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
std::string output = compilerOutput.renderTracked("main", input, mappings);
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
2. Runs `tpp2cpp -t` on the JSON → `<NAME>_types.h`
3. Runs `tpp2cpp -fun` → `<NAME>_functions.h`
4. Runs `tpp2cpp -impl` → `<NAME>_implementation.cc`

The implementation `.cc` is added as a private source to `<target>`. The generated headers are accessible via the target's include directories — no extra `target_include_directories()` call needed.

All four custom commands re-run automatically whenever any file in `SOURCE_DIR` changes.

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

The `tpp-language-support` extension provides a language-aware editing experience for `.tpp` and `tpp-config.json` files.

### Features

- **Syntax highlighting** for `.tpp` template files and `.tpp` type definition files
- **LSP-powered diagnostics** — type errors, missing fields, undefined types, policy violations — shown as red underlines as you type
- **Live preview panel** (`tpp: Open Render Preview`) — renders a template with sample input defined in `tpp-config.json` and updates the view in real time as you edit
- **JSON schema validation** for `tpp-config.json` — autocompletion and error highlighting in the config file

### Installation

The extension is not yet published to the VS Code Marketplace. Install it manually:

1. **Build the tpp-lsp binary** (the language server backend):

   ```bash
   cmake -B build && cmake --build build --target tpp-lsp
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
   npx vsce package    # produces tpp-language-support-*.vsix
   code --install-extension tpp-language-support-*.vsix
   ```

   Or, for development, open the `vscode-extension` folder in VS Code and press **F5** to launch an Extension Development Host.

### Configuration

In your VS Code `settings.json`, point the extension at the language server binary:

```json
{
  "tpp.lspServerPath": "/path/to/build/bin/tpp-lsp"
}
```

The path may be absolute or relative to the workspace root. The setting is **required** — the extension will not activate without it.

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
