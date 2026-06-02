# Testing Workflow

The test tree mixes three kinds of verification:

- in-process acceptance and LSP tests in `tpp_acceptance_test`
- generated C++ acceptance tests
- generated Java and Swift verification paths

## Acceptance And LSP Discovery

The acceptance and LSP suites now discover test cases at runtime by scanning `Test/TestCases/`.

That means adding a normal acceptance case no longer requires regenerating compile-time case lists just to make `AcceptanceTest` or `LspTest` see it.

Current runtime-discovered suites include:

- acceptance compile and render checks
- CLI compile/render comparisons
- LSP preview, diagnostics, tokens, folding, definition, and hover checks

## Case Layout

Each case directory is self-contained and always includes:

- `tpp-config.json`
- one or more `.tpp` or `.tpp` sources
- `test-case.json`

Success cases may also include `expected_output.txt` for the rendered golden output.

`test-case.json` carries structured expectations such as diagnostics, render errors, and LSP assertions. Success outputs can live either in `expected_output.txt` or in `test-case.json` via `expected_output`. Acceptance, CLI, preview, diagnostics, token, definition, and hover tests consume that normalized view.

## Generated Language Tests

Generated C++, Java, and Swift verification still rely on CMake-managed generation steps because those paths build generated sources as explicit targets.

Those generated-language paths understand success outputs from either `expected_output.txt` or `test-case.json`. They intentionally use configure-time discovery for the set of generated-language cases, because CMake must know generated sources and compile edges while constructing the build graph. Per-case regeneration follows exact `tpp --print-inputs` depfiles plus expectation-file dependencies instead of raw directory globs.

## Practical Workflow

For day-to-day compiler and LSP work:

1. add or edit a case under `Test/TestCases/`
2. update `expected_output.txt` for readable success output, and use `test-case.json` for diagnostics, render errors, LSP assertions, or a JSON-form success expectation when needed
3. rebuild `tpp_acceptance_test`
4. run focused GoogleTest filters for the affected suite

For codegen target changes:

1. add or edit the case under `Test/TestCases/`
2. rebuild the affected backend or generated test target; source, config, and policy edits are tracked directly from `tpp-config.json`, and case-set changes still flow through configure-time discovery

This split is intentional: acceptance/LSP suites are runtime-discovered, while generated-language suites remain configure/build-time discovered so C++/Java/Swift compilation stays explicit and deterministic under CMake.