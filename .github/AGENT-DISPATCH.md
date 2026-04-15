# Agent Dispatcher

Use this file when you know the task category but do not yet know which repo asset is authoritative.

## Start Here

- Repo architecture and maintenance obligations: see `.github/copilot-instructions.md`
- Template or typedef syntax: see `.github/instructions/tpp-language.instructions.md`
- Build or test workflow: see `.github/skills/test/SKILL.md`
- Add or update an acceptance test: see `.github/skills/add-acceptance-test/SKILL.md`
- Runtime, backend, and CLI usage: see `docs/usage.md`
- User-facing language explanation and examples: see `docs/language.md`

## Common Tasks

### I need to edit a `.tpp` or `.tpp.types` file

1. Read `.github/copilot-instructions.md` for repo rules and pitfalls.
2. Use `.github/instructions/tpp-language.instructions.md` as the authoritative syntax reference.
3. Use `docs/language.md` only for user-facing explanation, examples, and concepts.

### I need to build, run tests, or verify a refactor

1. Use `.github/skills/test/SKILL.md`.
2. Prefer VS Code CMake Tools over raw terminal commands.

### I need to add a new acceptance test

1. Use `.github/skills/add-acceptance-test/SKILL.md`.
2. Use `.github/skills/add-acceptance-test/references/expected-formats.md` for result-file schemas.
3. Check `Test/TestCases/` for naming and layout examples.

### I need to change backend behavior or CLI flags

1. Read `.github/copilot-instructions.md` for backend architecture.
2. Follow `.github/MAINTENANCE-CHECKLIST.md` before editing.
3. Keep `README.md`, `docs/usage.md`, and `.github/copilot-instructions.md` in sync.

### I need to change compiler IR, lowering, or rendering semantics

1. Start with `.github/copilot-instructions.md` for component ownership.
2. Follow `.github/MAINTENANCE-CHECKLIST.md` for IR/schema and language-feature changes.
3. Verify against the full CTest suite.

### I am new to the repo and need the shortest path to productive work

1. Read `.github/DEVELOPMENT.md`.
2. Build and test using `.github/skills/test/SKILL.md`.
3. If your first change is a feature, add or update a test with `.github/skills/add-acceptance-test/SKILL.md`.

## Source Of Truth Boundaries

- `.github/instructions/*.instructions.md`: agent-facing authoritative reference for specific file/task domains
- `.github/skills/*/SKILL.md`: step-by-step workflows
- `.github/copilot-instructions.md`: repo-wide architecture, conventions, pitfalls, and maintenance notes
- `docs/*.md`: user-facing product and language documentation
- `README.md`: high-level project entrypoint

If two files overlap, prefer the more specific file for execution and keep the broader file as an index or summary.