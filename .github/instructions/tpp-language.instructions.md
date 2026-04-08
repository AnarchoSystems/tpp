---
description: "Use when writing or editing tpp template files (.tpp), type definition files (.tpp.types), or acceptance test cases. Covers the full tpp template language: directives, type definitions, expressions, for loops, if/else, switch/case, render_via, function calls, and escaping rules."
applyTo: "**/*.tpp,**/*.tpp.types"
---

# tpp Language Reference

All directives use `@…@` delimiters. Anything outside delimiters is literal text.

---

## Type Definitions (`.tpp.types`)

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
    Point
};
```

**Primitive types:** `string`, `int`, `bool`  
**Aggregate types:** `list<T>`, `optional<T>`  
**Variant (enum):** tags may carry a payload type `Tag(T)` or be bare `Tag`  
**Optional fields** use `optional<T>` — present or absent in the JSON input  
**Recursive types** are allowed; the compiler detects cycles automatically

---

## Template Functions (`.tpp`)

```
template name(param1: Type1, param2: Type2)
... body ...
END
```

- Entry point in tests is always `template main(…)`. But the runtime exposes all templates. The relevant headers for the API are [`Compiler.h`](../../include/tpp/Compiler.h), [`CompilerOutput.h`](../../include/tpp/CompilerOutput.h), and [`FunctionSymbol.h`](../../include/tpp/FunctionSymbol.h).
- A single `.tpp` file may contain multiple `template` declarations.
- Multiple `.tpp` files and multiple `.tpp.types` files are allowed per test — all are processed.

---

## Expression Interpolation

```
@expr@
```

Renders the value of a variable or field path. Field access uses `.`:

```
@user.name@
@point.x@
@step.argument@
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
| `sep="…"` | `sep=", "` | String inserted *between* items |
| `precededBy="…"` | `precededBy=">"` | String inserted *before* the first item if it exists |
| `followedBy="…"` | `followedBy="!"` | String inserted *after* the last item if it exists |
| `enumerator=name` | `enumerator=idx` | 0-based integer index variable |

**Compact inline form** (no body whitespace):

```
(@for v in args.values | sep=", "@@v@@@end for@)
```

> `@v@@@end for@` — the `@@` is treated as adjacent delimiters; the parser skips the empty pair and continues with `@end for@`.

**Nested loops:**

```
@for row in rows@
{@for cell in row | sep=", "@@cell@@@end for@},
@end for@
```

---

## Conditional

```
@if condition@
  true branch
@else@
  false branch
@end if@
```

- `condition` is a field path. True if: a `bool` field is true, or an `optional<T>` field is present.
- Negate with `not`: `@if not p.nick@`
- `@else@` is optional.
- **Optional guard rule:** a field accessed inside the true branch of `@if field@` is considered guarded (present). Accessing an optional field outside any guard is a compile error. Trying to guard an optional on a branch where it is known not to exist is also an error.

---

## Switch / Case

```
@switch variant_expr | checkExhaustive@
@case TagName@
  body (no payload binding)
@end case@
@case TagName(binding)@
  @binding@ is the payload
@end case@
@end switch@
```

- `checkExhaustive` (flag, no value) — compiler error if any variant tag has no `@case@`.
- Unmatched tags silently emit nothing (not an error unless `checkExhaustive` is set).
- `(binding)` extracts the payload of a variant tag.

---

## Direct Function Call

```
@functionName(arg)@
```

Invokes another template function inline and inserts its output at that position.

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

Dispatches to `functionName` for each item in `collection`, concatenating the results. The item is passed as an implicit argument. Options are the same as for `@for@` loops (e.g. `sep="…"`, `enumerator=idx`).

render_via can also be used for switch. In that case, it does not take options.

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

---

## Block vs Inline Lines

A line is a **block line** if it contains *only* structural directives plus optional whitespace — it is consumed without emitting any output or newline.

A line is an **inline line** if it mixes literal text or expressions with directives — the whole line is emitted.

**Block indentation:** the leading whitespace of the first non-empty line inside a block body is the "zero marker". All body lines are de-indented by that amount, then re-indented at the column of the enclosing directive.

---

## Escaping

- `@@` (two adjacent `@` with nothing between them) is an **error** — use `\@` to produce a literal `@` in output.
- Adjacent directives like `@v@@end for@` work naturally: the `@` that closes `@v@` puts the tokenizer back in text mode, and the very next `@` opens the following directive. No special syntax needed.
