---
name: test
description: 'Configure, build, and run tests for this C++ CMake project using VS Code CMake Tools. Use when: building the project, compiling changes, running tests, running ctest, checking for build errors, running a clean build, or verifying a feature works end-to-end.'
---

# Configure, Build & Test

## When to Use
- User asks to build, compile, or make the project
- User asks to run tests, run ctest, or verify correctness
- User wants a clean build (wipe and re-configure)
- After implementing a feature or fix, to confirm it works

## Tools
Prefer VS Code CMake Tools over the terminal for all steps:
- **Build**: `Build_CMakeTools` — configures automatically if needed, then compiles
- **Run tests**: `RunCtest_CMakeTools` — runs all CTest tests with output on failure
- **List targets**: `ListBuildTargets_CMakeTools` — useful when targeting a specific executable
- **List tests**: `ListTests_CMakeTools` — inspect registered test names before running
- **Clean**: `run_vscode_command` with command `cmake.cleanAll` — wipes build artifacts without touching source

Only fall back to the terminal (`cd build && make -j4` / `ctest --output-on-failure`) when a CMake Tools tool call fails or is unavailable.

## Procedure

### Standard Build + Test
1. Call `Build_CMakeTools` (no arguments needed; it configures then compiles).
2. If the build succeeds, call `RunCtest_CMakeTools` to run all acceptance tests.
3. Report a summary: number passed / total, and any failing test names.

### Clean Build
1. Call `run_vscode_command` with `cmake.cleanAll` to delete build artifacts.
2. Then follow the **Standard Build + Test** procedure above.

### Build Errors
1. Read the compiler output carefully — it includes file path, line, and error message.
2. Fix the source file(s) identified in the errors.
3. Re-run `Build_CMakeTools` to confirm the fix compiles.
4. Then run `RunCtest_CMakeTools`.

### Test Failures
1. Note the failing test name(s) printed by CTest.
2. Locate the corresponding test case directory under `Test/TestCases/<name>/`.
3. Compare actual vs expected output (files in that directory) to understand the failure.
4. Fix the source, then rebuild and retest.

## Project Context
- Build directory: `build/`
- Test executable: `build/Test/lib_cpp_test`
- Acceptance tests are data-driven: each subdirectory of `Test/TestCases/` is one test case
- CMake standard: C++17
