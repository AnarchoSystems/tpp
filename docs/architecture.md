# Architecture

tpp is a single validating compiler frontend with multiple consumers.

The repo is organized around one rule: language semantics are implemented once, then reused through stable output contracts.

## Pipeline

1. `tpp` reads `tpp-config.json`, type definitions, templates, and policy files.
2. The compiler validates names, types, optionals, variants, policies, and template semantics.
3. The compiler emits public IR (`tpp::IR`).
4. Consumers use that IR instead of reimplementing compiler logic.

Current consumers:

- `tpp2cpp` generates typed C++ declarations and implementations.
- `tpp2java` and `tpp2swift` generate source for other languages.
- `render-tpp` renders IR directly for scripts, tests, and debugging.
- `lib_tpp` exposes compile and runtime APIs for embedding.
- `tpp-lsp` uses compiler-produced metadata for diagnostics, tokens, hover, preview, and navigation.

## Public Vs Internal

Public library surfaces live under `Libraries/lib_tpp/include/tpp/`.

Current public entry points:

- `Compiler.h` exposes the high-level `tpp::compile(...)` path from project sources to public IR.
- `IR.h` defines the backend-neutral public model consumed by backends and runtime rendering.
- `Runtime.h` exposes dynamic rendering against compiled IR.
- `Tooling.h` exposes tokenization and source-analysis helpers that do not require compiler AST ownership.

Internal compiler surfaces live under `Libraries/lib_tpp/tpp/` and `Libraries/lib_tpp/src/`.

Current internal-only examples:

- project-config resolution in `ProjectConfigResolver.h`
- staged compiler pipeline types in `CompilerPipeline.h`
- compiler model headers in `AST.h`, `Types.h`, and `SemanticModel.h`
- tooling-only compile and folding entry points in `ToolingCompile.h` and `ToolingFolding.h`
- AST-bearing tooling parsing in `ToolingInternal.h`
- semantic-model-to-public-IR assembly in `PublicIRConverter.h` and `PublicIRConverter.cc`
- IR execution helpers in `IRInterpreter.h` and `IRInterpreter.cc`

The architectural direction is to keep shrinking public exposure of compiler internals until downstream consumers depend only on public IR, runtime APIs, and stable tooling/query results.

## Compiler Ownership

`Compiler.cc` owns frontend orchestration:

- lex input sources
- parse typedefs, templates, and policies
- build semantic model and diagnostics
- hand semantic output to the IR conversion layer

`PublicIRConverter.cc` owns semantic-model-to-public-IR assembly.

`Lowering.cc` is now a private helper compilation unit for function-body lowering used by that conversion layer, not a separately included interface.

`ProjectConfigResolver.cc` owns `tpp-config.json` loading and glob expansion shared by the compiler CLI and test harness.

`IRInterpreter.cc` owns instruction-level runtime execution used by the public rendering API.

That split keeps frontend orchestration out of the translation rules for public IR shape while avoiding an extra header boundary around lowering.

## LSP Boundary

The language server is intentionally downstream of the compiler.

`WorkspaceProject` recompiles on demand, stores public IR, and exposes query helpers used by the LSP implementation. The raw semantic model object is no longer exposed through a public accessor; exact schema source locations stay behind internal workspace helpers, while the visible LSP query surface is public-IR-backed.

Compiler-model parsing artifacts still exist inside some LSP implementation files for on-demand template analysis, but they are no longer part of the workspace-level query contract.