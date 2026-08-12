---
description: Enforce explicit braces for all C++ control-flow bodies in repository source files.
applyTo: "**/*.{cc,cpp,cxx,h,hh,hpp,hxx}"
---

# C++ Braces Rule (Required)

Always use braces for control-flow statements, even for single statements.

Required forms:
- `if (...) { ... }`
- `else { ... }`
- `for (...) { ... }`
- `while (...) { ... }`
- `do { ... } while (...);`

Switch guidance:
- Use braces around each `case`/`default` body block.
- Example:
  ```cpp
  switch (kind)
  {
  case Kind::A:
  {
      handleA();
      break;
  }
  default:
  {
      break;
  }
  }
  ```

Do not introduce indentation-only one-liners for control flow.
When editing existing code, normalize touched control-flow statements to this style.
