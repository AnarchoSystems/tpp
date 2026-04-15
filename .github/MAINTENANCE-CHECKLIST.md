# Maintenance Checklist

Use this file when a change touches multiple repo surfaces that must stay in sync.

## Backend Changes

When adding a backend or changing backend capabilities:

- Update the backend model table in `.github/copilot-instructions.md`
- Update the backend overview in `README.md`
- Update backend usage in `docs/usage.md`
- Update CI if the new backend is built or tested there
- Add or update acceptance coverage in `Test/TestCases/`
- Verify the full build and full CTest suite

## CLI Flag Changes

When adding, removing, or changing CLI flags for `tpp` or any backend:

- Update the relevant `Main.cc`
- Update `.github/copilot-instructions.md` if the architectural contract changed
- Update `docs/usage.md` command synopsis and option tables
- Update `README.md` if the flag changes the public workflow
- Verify the affected command paths and then run the full test suite

## Language Feature Changes

When adding or changing template-language behavior:

- Update `.github/instructions/tpp-language.instructions.md`
- Update `docs/language.md` if the change affects user-facing explanation or examples
- Add or update acceptance tests in `Test/TestCases/`
- Add IR snapshots or LSP specs when the feature affects those surfaces
- Verify with the full build and full CTest suite

## IR Or Schema Changes

When changing `IR.h`, lowering, or codegen-facing schema:

- Update the relevant source files in `Libraries/lib_tpp/` and `Executables/backends/`
- Update `docs/ir.md` if the public IR meaning changed
- Rebuild all backends
- Run the full CTest suite
- Update IR snapshots if the schema change is intentional

## Test Workflow Changes

When adding a new expected-result artifact or changing test discovery:

- Update `.github/skills/add-acceptance-test/SKILL.md` if the workflow changed
- Update `.github/skills/add-acceptance-test/references/expected-formats.md`
- Add comments in `Test/CMakeLists.txt` if discovery behavior is not obvious
- Verify discovery and the full CTest suite