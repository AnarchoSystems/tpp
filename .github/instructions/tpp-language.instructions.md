---
description: "Use when writing or editing tpp template files (.tpp), type definition files (.tpp.types), or acceptance test cases. Covers the full tpp template language: directives, type definitions, expressions, for loops (including alignment and policies), if/else, switch/case, render_via, direct function calls, comments, policies, escaping, and tpp-config.json."
applyTo: "**/*.tpp,**/*.tpp.types"
---

# tpp Language Reference

All directives use `@…@` delimiters. Anything outside delimiters is literal text.

For a full user-facing language reference with detailed examples, see [docs/language.md](../../docs/language.md).

---

## Type Definitions

```
struct Point
{
    x : int;
    y : int;
    label : optional<string>;
}

enum Shape
{
    Circle(float),
    Square(float),
    Triangle
};
```

**Primitive types:** `string`, `int`, `bool`  
**Aggregate types:** `list<T>`, `optional<T>`  
**Variant (enum):** tags may carry a payload `Tag(T)` or be bare `Tag`  
**Optional fields** use `optional<T>` — present or absent in the JSON input  
**Recursive types** are allowed; the compiler detects cycles automatically  
**Doc comments:** `///` single-line or `/** */` block, attached to the next declaration

---

## Template Functions

```
template name(param1: Type1, param2: Type2)
... body ...
END
```

- Entry point in tests is always `template main(…)`. The runtime exposes all templates.
- The relevant headers: [`Compiler.h`](../../Libraries/lib_tpp/include/tpp/Compiler.h), [`IR.h`](../../Libraries/lib_tpp/include/tpp/IR.h), [`FunctionSymbol.h`](../../Libraries/lib_tpp/include/tpp/FunctionSymbol.h).
- A single `.tpp` file may contain multiple `template` declarations.
- Multiple source files per test are supported — list them in `tpp-config.json`.

---

## Comments

### Line / Block / Doc Comments (both file types)

```
// line comment
/* block comment */
/// doc comment — attached to the next declaration
```

### Template Body Comment Blocks

```
@comment@
This entire block is suppressed from output.
@end comment@
```

`@comment@` and `@end comment@` are block lines — they emit no whitespace or newline of their own.

---

## Expression Interpolation

```
@expr@
@user.name@
@step.argument@
```

With a policy modifier (see [Policies](#policies)):

```
@data.value | policy="escape-html"@
```

---

## For Loop

```
@for item in collection | options@
  body
@end for@
```

**Options** (all optional, space-separated after `|`):

| Option | Example | Meaning |
|--------|---------|---------|
| `sep="…"` | `sep=", "` | Inserted *between* items |
| `precededBy="…"` | `precededBy="["` | Prepended before the first item (if list non-empty) |
| `followedBy="…"` | `followedBy="]"` | Appended after the last item (if list non-empty) |
| `enumerator=name` | `enumerator=idx` | 0-based integer loop counter |
| `align` | `align` | Column-pad output (all left-aligned) |
| `align="spec"` | `align="rl"` | Column-pad with per-column spec (`l`eft, `r`ight, `c`enter) |
| `policy="name"` | `policy="escape-html"` | Apply policy to all interpolations in loop body |

### Alignment

Use `@&@` inside the loop body to mark column boundaries. The compiler collects all rows, computes max column widths, and pads accordingly:

```
@for row in t.rows | align="rll" sep="\n"@@row.name@ @&@@row.type@ @&@@row.value@@end for@
```

The `align="rll"` spec means: right-align column 1, left-align columns 2 and 3. Spec length must equal the number of `@&@` separators plus one.

### Compact Inline Form

```
(@for v in args.values | sep=", "@@v@@end for@)
```

Adjacent closing/opening delimiters (`@v@@end for@`) work naturally — the closing `@` of `@v@` returns to text mode, and the next `@` opens `@end for@`.

---

## Conditional

```
@if condition@
  true branch
@else@
  false branch
@end if@
```

- `condition` resolves to a `bool` field (true when the value is `true`) or an `optional<T>` field (true when present).
- Negate with `not`: `@if not p.nick@`
- `@else@` is optional.
- **Optional guard rule:** accessing an optional field inside the true branch of `@if field@` is guarded (valid). Accessing an optional field outside any guard is a **compile error**. Accessing the field in the `@else@` branch (where it is known absent) is also an error.

---

## Switch / Case

```
@switch variant_expr | options@
@case TagName@
  body (no payload)
@end case@
@case TagName(binding)@
  @binding@ holds the payload
@end case@
@end switch@
```

**Options:**

| Option | Meaning |
|--------|---------|
| `checkExhaustive` | Compile error if any variant tag lacks a `@case@` |
| `policy="name"` | Apply policy to all interpolations in all case bodies |

---

## Direct Function Call

```
@functionName(arg)@
```

Invokes another template function inline. Used for composition and recursion:

```
template render_node(node: IntListNode)
@node.value@@if node.next@, @render_node(node.next)@@end if@
END
```

---

## Render Via

```
@render collection via functionName | options@
```

Calls `functionName` once per element. Supports the same options as `@for@` (`sep`, `precededBy`, `followedBy`, `enumerator`, `policy`). When `functionName` has multiple overloads for different types, the correct one is selected at runtime (visitor pattern):

```
template render_elem(i: int)
@i@ is an integer
END

template render_elem(s: string)
"@s@" is a string
END

template main(items: list<Item>)
@render items via render_elem | sep="\n"@
END
```

Also works for a single variant value (no options):

```
@render variant_expr via functionName@
```

---

## Policies

A policy is a named, reusable set of validation and transformation rules applied to string values at render time. Policies are declared in JSON files and registered in `tpp-config.json`.

### Policy File Format

```json
{
  "tag": "escape-html",
  "length":      { "min": 1, "max": 500 },
  "reject-if":   { "regex": "<script", "message": "script tags not allowed" },
  "require":     [{ "regex": "^[\\w\\s]+$" }],
  "replacements":[{ "find": "<", "replace": "&lt;" }, { "find": ">", "replace": "&gt;" }],
  "output-filter":[{ "regex": "^[^<>]+$" }]
}
```

A `require` step with a `replace` string transforms the value via regex substitution; use `@subexpr_N@` for capture groups:

```json
{ "regex": "^(\\d+)$", "replace": "number: @subexpr_1@" }
```

### Applying Policies

**Per variable:**
```
@data.value | policy="escape-html"@
```

**Per template parameter** (applies to that parameter's value throughout the body):
```
template render_safe(v: string | policy="escape-html")
<td>@v@</td>
END
```

**Per for loop** (applies to all interpolations in the body, including through function calls):
```
@for item in data.items | sep=", " policy="escape-html"@@item@@end for@
```

**Per render_via:**
```
@render data.values via render_item | sep="\n" policy="escape-html"@
```

**Per switch:**
```
@switch item | policy="escape-html"@
@case A(v)@A: @v@@end case@
@end switch@
```

**Override / suppress inherited policy:**
```
@item | policy="none"@
```

### Scope Propagation

Policies propagate through function calls. If a for loop declares `policy="escape-html"`, that policy is active when any function called from within the loop body evaluates an interpolation — even if the callee doesn't declare the policy itself.

---

## Block vs Inline Lines

A **block line** contains only structural directives and optional whitespace — it is consumed without emitting any output or newline. A **inline line** mixes literal text or expressions with directives and is emitted in full.

**Block indentation:** the leading whitespace of the first non-empty line in a block body is the "zero marker". All body lines are de-indented by that amount, then re-indented at the insertion column of the enclosing directive.

---

## Escaping

- Write `\@` to produce a literal `@` in output.
- Adjacent directives (`@v@@end for@`) work naturally — the tokenizer returns to text mode after each closing `@`, so the next `@` opens the following directive.
- Two `@` signs with *nothing between them* in text mode is a **parse error**: "empty directive". This is not the same as adjacent directives.

---

## tpp-config.json

Every tpp project has a `tpp-config.json` file. All paths are relative to the config file.

```json
{
    "types": ["typedefs.tpp", "types/*.tpp"],
    "templates": ["template.tpp"],
    "replacement-policies": ["escape-html.policy.json"],
    "previews": [
        { "name": "Default", "template": "main", "input": { "x": 1 } }
    ]
}
```

| Key | Description |
|---|---|
| `types` | Glob patterns for type-definition files. Expanded and sorted alphabetically per glob. |
| `templates` | Glob patterns for template source files. Processed after types. |
| `replacement-policies` | Paths to policy JSON files to load before compilation. |
| `previews` | Preview configurations for the VS Code live preview panel. Each entry has `template` (required), `name` (optional), and `input` (inline JSON or path to a `.json` file). |
