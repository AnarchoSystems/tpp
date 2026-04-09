# tpp — A Template Language

tpp is a **typed template language** for structured text generation. You define the shape of your data in a typed schema, write templates that interpolate and iterate over it, and let the compiler verify every field access before any output is produced.

The result isn't just a rendered string — it's a **compile-time-verified pipeline**. tpp can turn your templates into typed C++ functions that your own compiler will check for you.

---

## Core Concepts

### One Frontend, Multiple Backends

tpp separates concerns cleanly:

```
  .tpp type defs  ──┐
                    ├──▶  tpp (compiler)  ──▶  compiler-output.json
  .tpp templates  ──┘                               │
                                                    ├──▶  tpp2cpp   → C++ types + functions
                                                    ├──▶  render-tpp → rendered output (scripting)
                                                    └──▶  lib_tpp   → C++ runtime API
```

The `tpp` compiler frontend emits a **self-contained JSON document** that describes the compiled types, compiled template ASTs, and registered policies. Every backend consumes that document. Because the compiler has already done all the hard work — type checking, field validation, AST construction — each backend has an easy, focused job.

### Typed Templates → Type-Safe C++ Functions

When you declare types and use them in templates, `tpp2cpp` can generate C++ code:

```
// types.tpp
struct Item {
    name  : string;
    count : int;
}
```

```
// template.tpp
template render_item(item: Item)
- @item.name@: @item.count@
END
```

Generates:

```cpp
// generated: project_functions.h
std::string render_item(const Item& item);
```

You call it with a real `Item` value. Pass the wrong type and your C++ compiler rejects it — at compile time, not at runtime.

### Confidence Through Typing

The tpp compiler catches — *before any code runs* — every instance of:

- accessing a field that doesn't exist on a type
- reading an `optional<T>` field without first checking it's present
- forgetting to handle a variant case (with `checkExhaustive`)
- violating a declared content policy

See [What's the Point of Typing?](docs/language.md#whats-the-point-of-typing) for the full story.

---

## Quick Start

This assumes you have built this cmake project.

**1. Define your types** (`types.tpp`):

```
struct Item
{
    name  : string;
    count : int;
    note  : optional<string>;
}
```

**2. Write your template** (`template.tpp`):

```
template main(items: list<Item>)
@for item in items | sep="\n"@- @item.name@ (@item.count@)@if item.note@ — @item.note@@end if@@end for@
END
```

**3. Configure the project** (`tpp-config.json`):

```json
{
    "types": ["types.tpp"],
    "templates": ["template.tpp"]
}
```

**4. Compile and render**:

```bash
tpp . | render-tpp main '{"items": [{"name": "Apples", "count": 4}, {"name": "Figs", "count": 1, "note": "fresh"}]}'
```

Output:
```
- Apples (4)
- Figs (1) — fresh
```

**5. Or generate C++**:

```bash
tpp . > project.json
tpp2cpp -t  --input project.json > project_types.h
tpp2cpp -fun --input project.json -i project_types.h > project_functions.h
tpp2cpp -impl --input project.json -i project_functions.h > project_implementation.cc
```

---

## Documentation

| Document | Contents |
|---|---|
| [Language Reference](docs/language.md) | Full language syntax: types, templates, expressions, loops, conditionals, switch, render via, policies, alignment, comments, escaping, tpp-config |
| [Usage & Tooling](docs/usage.md) | CLI reference (`tpp`, `render-tpp`, `tpp2cpp`), C++ library API, CMake `tpp_add()` integration, VS Code extension |

---

## Building

Requires C++17 and CMake ≥ 3.20.

```bash
cmake -B build
cmake --build build
```

Targets built:
- `tpp` — the compiler CLI
- `tpp2cpp` — the C++ code generation backend
- `render-tpp` — the scripting rendering backend
- `tpp-lsp` — the language server (for the VS Code extension)
- `lib_tpp` — the C++ library

Run the test suite:

```bash
ctest --test-dir build --output-on-failure
```

---

## VS Code Extension

The `tpp-language-support` extension provides syntax highlighting, error diagnostics as you type, and a live preview panel.

**To set it up:**
1. Build `tpp-lsp` (see above)
2. Build the extension (`cd vscode-extension && npm install && npm run compile`)
3. Set `tpp.lspServerPath` in your VS Code settings to the path of the built `tpp-lsp` binary

See [Usage & Tooling → VS Code Extension](docs/usage.md#vs-code-extension) for full installation and configuration instructions.
