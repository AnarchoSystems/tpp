# tpp Intermediate Representation

The tpp compiler emits a single JSON document — the **intermediate representation** (IR) — that encodes everything a backend needs: compiled types, template functions as instruction trees, and policy definitions. This document describes the IR schema and the semantics of each node.

For how to produce and consume the IR, see the [Usage Guide](usage.md). For the template language that compiles into this IR, see the [Language Reference](language.md).

---

## Overview

The IR is a self-contained JSON object with these top-level fields:

| Field | Type | Description |
|---|---|---|
| `versionMajor` | int | Compiler major version |
| `versionMinor` | int | Compiler minor version |
| `versionPatch` | int | Compiler patch version |
| `structs` | array of [StructDef](#structdef) | All struct types declared in the project |
| `enums` | array of [EnumDef](#enumdef) | All enum (variant) types declared in the project |
| `functions` | array of [FunctionDef](#functiondef) | All compiled template functions |
| `policies` | array of [PolicyDef](#policydef) | All registered replacement policies |

The version fields record which compiler produced the IR. These are informational — backends should not gate behaviour on them.

---

## Type System

Types flow through the IR in two forms: **schema definitions** (the struct/enum declarations that backends use for code generation) and **inline type annotations** on expressions and parameters.

### TypeKind

Every type reference in the IR is a `TypeKind` — a tagged union that mirrors the tpp type system:

| Tag | Payload | Description |
|---|---|---|
| `Str` | — | String type |
| `Int` | — | Integer type |
| `Bool` | — | Boolean type |
| `Named` | string | A user-defined struct or enum, by name |
| `List` | TypeKind | Ordered collection of elements |
| `Optional` | TypeKind | A value that may be absent |

In JSON, a `TypeKind` is encoded as a single-key object where the key is the tag:

```json
{"Str": {}}
{"Named": "Person"}
{"List": {"Named": "Item"}}
{"Optional": {"Int": {}}}
```

---

## Schema Types

Schema definitions describe the data shapes that templates operate on. Backends use these to generate native types (C++ structs, Java classes, Swift structs, etc.).

### StructDef

A named record type with ordered fields.

| Field | Type | Description |
|---|---|---|
| `name` | string | Type name |
| `fields` | array of [FieldDef](#fielddef) | Ordered list of fields |
| `doc` | string | Doc comment text (empty string if none) |
| `sourceRange` | [SourceRange](#sourcerange) or null | Source location (present when `--source-ranges` is set) |
| `rawTypedefs` | string | The original tpp type definition source text, including doc comments |

#### FieldDef

| Field | Type | Description |
|---|---|---|
| `name` | string | Field name |
| `type` | [TypeKind](#typekind) | Field type |
| `recursive` | bool | Whether this field creates a recursive type cycle |
| `doc` | string | Doc comment text |
| `sourceRange` | [SourceRange](#sourcerange) or null | Source location |

The `recursive` flag tells backends that this field needs indirection in the generated code (e.g. `std::unique_ptr<T>` in C++, `Box<T>` in Rust).

### EnumDef

A tagged union (sum type) with named variants.

| Field | Type | Description |
|---|---|---|
| `name` | string | Type name |
| `variants` | array of [VariantDef](#variantdef) | Ordered list of variants |
| `doc` | string | Doc comment text |
| `sourceRange` | [SourceRange](#sourcerange) or null | Source location |
| `rawTypedefs` | string | The original tpp type definition source text, including doc comments |

#### VariantDef

| Field | Type | Description |
|---|---|---|
| `tag` | string | Variant tag name |
| `payload` | [TypeKind](#typekind) or null | Payload type (null for tag-only variants) |
| `recursive` | bool | Whether this variant creates a recursive type cycle |
| `doc` | string | Doc comment text |
| `sourceRange` | [SourceRange](#sourcerange) or null | Source location |

---

## Template Functions

### FunctionDef

Each template compiles into a `FunctionDef` — a named function with typed parameters, a body of instructions, and an optional default policy.

| Field | Type | Description |
|---|---|---|
| `name` | string | Function name |
| `params` | array of [ParamDef](#paramdef) | Typed parameter list |
| `body` | array of [Instruction](#instructions) | The compiled instruction tree |
| `policy` | string | Default policy tag (empty string if none) |
| `doc` | string | Doc comment text |
| `sourceRange` | [SourceRange](#sourcerange) or null | Source location |

#### ParamDef

| Field | Type | Description |
|---|---|---|
| `name` | string | Parameter name |
| `type` | [TypeKind](#typekind) | Parameter type |

---

## Instructions

The `body` of a `FunctionDef` (and the bodies of `For`, `If`, `Switch`, etc.) is an array of instructions. Each instruction is a tagged union:

| Tag | Payload | Description |
|---|---|---|
| `Emit` | [EmitInstr](#emitinstr) | Emit literal text |
| `EmitExpr` | [EmitExprInstr](#emitexprinstr) | Emit an interpolated expression |
| `AlignCell` | — | Column alignment separator (`@&@`) |
| `For` | [ForInstr](#forinstr) | Loop over a collection |
| `If` | [IfinStr](#ifinstr) | Conditional |
| `Switch` | [SwitchInstr](#switchinstr) | Pattern match on a variant |
| `Call` | [CallInstr](#callinstr) | Direct function call |
| `RenderVia` | [RenderViaInstr](#renderviainstr) | Call a function for each element in a collection |

### ExprInfo

All expression references in instructions carry resolved type information:

| Field | Type | Description |
|---|---|---|
| `path` | string | Dot-separated field access path (e.g. `"item.name"`) |
| `type` | [TypeKind](#typekind) | Resolved type of the expression |

### EmitInstr

Emit literal text into the output stream.

| Field | Type | Description |
|---|---|---|
| `text` | string | Literal text to emit |

### EmitExprInstr

Emit the string value of an expression, optionally applying a policy.

| Field | Type | Description |
|---|---|---|
| `expr` | [ExprInfo](#exprinfo) | The expression to interpolate |
| `policy` | string | Policy tag to apply (empty string for no policy) |

### ForInstr

Loop over each element of a collection expression.

| Field | Type | Description |
|---|---|---|
| `varName` | string | Loop variable name |
| `enumeratorName` | string | Enumerator variable for the loop (e.g. `"_enum"`) |
| `collection` | [ExprInfo](#exprinfo) | The collection to iterate |
| `body` | array of Instruction | Loop body |
| `sep` | string | Separator text emitted between iterations (empty for none) |
| `followedBy` | string | Text emitted after the last iteration if the collection is non-empty |
| `precededBy` | string | Text emitted before the first iteration if the collection is non-empty |
| `isBlock` | bool | Whether the for loop was on a block line (affects whitespace handling) |
| `insertCol` | int | Column position where block content is inserted (for indentation) |
| `policy` | string | Policy tag for this scope |
| `hasAlign` | bool | Whether alignment is active in this loop |
| `alignSpec` | string | Alignment specification string (e.g. `"llr"` for left/left/right) |

### IfInstr

Conditional emission.

| Field | Type | Description |
|---|---|---|
| `condExpr` | [ExprInfo](#exprinfo) | The condition expression (must be `Bool` or `Optional`) |
| `negated` | bool | Whether the condition is negated (`@if not …@`) |
| `thenBody` | array of Instruction | Body when condition is true |
| `elseBody` | array of Instruction | Body when condition is false (empty if no else branch) |
| `isBlock` | bool | Whether the if was on a block line |
| `insertCol` | int | Column position for block content insertion |

### SwitchInstr

Pattern match on a variant (enum) expression.

| Field | Type | Description |
|---|---|---|
| `expr` | [ExprInfo](#exprinfo) | The expression to match |
| `cases` | array of [CaseInstr](#caseinstr) | Case branches |
| `isBlock` | bool | Whether the switch was on a block line |
| `insertCol` | int | Column position for block content insertion |
| `policy` | string | Policy tag for this scope |

#### CaseInstr

| Field | Type | Description |
|---|---|---|
| `tag` | string | Variant tag to match |
| `bindingName` | string | Name bound to the payload (empty if no binding) |
| `payloadType` | [TypeKind](#typekind) or null | Type of the payload (null for tag-only variants) |
| `body` | array of Instruction | Case body |

### CallInstr

Direct call to another template function.

| Field | Type | Description |
|---|---|---|
| `functionName` | string | Name of the function to call |
| `arguments` | array of [ExprInfo](#exprinfo) | Arguments (must match the callee's parameter types) |

### RenderViaInstr

Call a function once for each element in a collection — a higher-order loop.

| Field | Type | Description |
|---|---|---|
| `collection` | [ExprInfo](#exprinfo) | The collection to iterate |
| `functionName` | string | Function to call per element |
| `sep` | string | Separator between calls |
| `followedBy` | string | Text after the last call |
| `precededBy` | string | Text before the first call |
| `isBlock` | bool | Whether the render-via was on a block line |
| `insertCol` | int | Column position for block content insertion |
| `policy` | string | Policy tag for this scope |

---

## Policies

Policies define text transformation rules applied to interpolated expressions. They are registered externally (via JSON files referenced in `tpp-config.json`) and attached to scopes or individual interpolations.

### PolicyDef

| Field | Type | Description |
|---|---|---|
| `tag` | string | Policy identifier used in templates |
| `length` | [PolicyLength](#policylength) or null | Length constraints |
| `rejectIf` | [PolicyRejectIf](#policyrejectif) or null | Rejection pattern |
| `require` | array of [PolicyRequire](#policyrequire) | Required pattern transformations |
| `replacements` | array of [PolicyReplacement](#policyreplacement) | Find-and-replace pairs |
| `outputFilter` | array of [PolicyOutputFilter](#policyoutputfilter) | Output validation filters |

#### PolicyLength

| Field | Type | Description |
|---|---|---|
| `min` | int or null | Minimum allowed length |
| `max` | int or null | Maximum allowed length |

#### PolicyRejectIf

| Field | Type | Description |
|---|---|---|
| `regex` | string | If the value matches this regex, reject it |
| `message` | string | Error message on rejection |

#### PolicyRequire

| Field | Type | Description |
|---|---|---|
| `regex` | string | Required pattern |
| `replace` | string | Replacement template (empty for validation-only) |
| `compiledReplace` | string | Pre-compiled replacement string (internal use) |

#### PolicyReplacement

| Field | Type | Description |
|---|---|---|
| `find` | string | Text to find |
| `replace` | string | Replacement text |

#### PolicyOutputFilter

| Field | Type | Description |
|---|---|---|
| `regex` | string | Regex that the final output must match |

---

## Source Locations

When the compiler is invoked with source range tracking (used by the language server), each definition includes its source location.

### SourceRange

| Field | Type | Description |
|---|---|---|
| `start` | [SourcePosition](#sourceposition) | Start of the range |
| `end` | [SourcePosition](#sourceposition) | End of the range |

### SourcePosition

| Field | Type | Description |
|---|---|---|
| `line` | int | Zero-based line number |
| `character` | int | Zero-based character offset |

---

## Whitespace Semantics

Several instructions carry `isBlock` and `insertCol` fields. These encode whitespace decisions made by the compiler:

- **`isBlock`**: The directive appeared on a line containing only structural directives (plus whitespace). Block lines are consumed without emitting their surrounding whitespace — the directive's body replaces the entire line.
- **`insertCol`**: When `isBlock` is true, this records the column at which the directive started. Body content is re-indented to this column: the first non-empty line's leading whitespace becomes the "zero marker", and all body lines are de-indented by that amount, then re-indented to `insertCol`.

Inline directives (`isBlock = false`) emit their content in-place without any indentation adjustment.

---

## Producing and Consuming the IR

The `tpp` compiler produces the IR:

```
tpp ./my-project > ir.json
```

Backends consume it:

```
tpp2cpp types < ir.json       # Generate C++ types
tpp2cpp functions < ir.json   # Generate C++ function implementations
render-tpp main < ir.json     # Render the 'main' template with JSON input
```

The C++ library (`lib_tpp`) can also produce and consume the IR programmatically — see the [Usage Guide](usage.md) for details.
