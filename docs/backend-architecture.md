# Backend Architecture

Separate backend executables are intentional.

tpp does not collapse C++, Java, Swift, and runtime rendering into one monolithic backend binary. Each backend keeps its own executable, command surface, and output conventions.

## Backend Layers

The backend side is split into one shared codegen layer plus per-backend entry points.

### Local CLI ownership

Backend CLI behavior is intentionally local to each executable.

That includes:

- subcommands
- flags such as `-ns`, `-i`, and `--input`
- usage text
- argument validation
- stdin vs file loading behavior
- host-level error reporting and exit policy

This is deliberate. Backend entry points look similar, but they do not share one stable command contract. Keeping parsing local avoids a central abstraction that every new backend would need to modify.

### `tpp_codegen`

`Executables/backends/codegen/CodegenHelpers.h` owns shared code generation helpers:

- IR-to-template-context conversion
- doc comment formatting helpers
- shared render-function context building
- public IR name qualification and string literal helpers

This is the reusable backend model layer used by `tpp2cpp`, `tpp2java`, and `tpp2swift`.

## Backend Executables

### `tpp2cpp`

The C++ backend remains the most specialized backend.

It still uses its own embedded backend templates and native generation flow because it produces multiple command shapes (`types`, `functions`, `impl`) and has C++-specific include and namespace concerns.

### `tpp2java` and `tpp2swift`

These are thin wrappers over shared backend logic:

- parse their own command lines locally
- load IR locally from stdin or `--input <file>`
- build language-specific source and function contexts
- dispatch to generated rendering functions

### `render-tpp`

`render-tpp` is the direct runtime backend.

It follows the same user-facing IR input contract as the codegen backends:

- stdin by default
- optional `--input <file>` override

But that behavior is implemented in the executable itself rather than through a shared backend host layer.

## Backend Contract

All backend executables consume public IR only.

They are not supposed to depend on compiler frontend internals, staged pipeline artifacts, or semantic-model implementation details. That rule keeps the backend boundary clear and makes IR the real interoperability contract.