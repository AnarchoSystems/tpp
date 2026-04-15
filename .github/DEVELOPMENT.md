# Development Guide

This guide is the shortest path to a working local development setup. It links to the authoritative workflow assets instead of repeating them.

## Prerequisites

- CMake 3.20 or newer
- A C++17-capable compiler
- Java 21 if you want to build the Java acceptance-test path
- Swift 5.10 if you want to build the Swift acceptance-test path
- VS Code with CMake Tools is the preferred workflow

## First Build

1. Configure the project.
2. Build the project.
3. Run the full CTest suite.

For the exact tool-driven workflow, use `.github/skills/test/SKILL.md`.

## First Contribution

1. Read `.github/copilot-instructions.md` for architecture and repo rules.
2. Use `.github/AGENT-DISPATCH.md` if you are not sure which asset is authoritative for your task.
3. If the change affects language behavior, add or update an acceptance test with `.github/skills/add-acceptance-test/SKILL.md`.
4. Run the full build and test workflow from `.github/skills/test/SKILL.md`.

## Editing Templates And Types

- Syntax and rules: `.github/instructions/tpp-language.instructions.md`
- User-facing examples and concepts: `docs/language.md`

## Refactors And Structural Changes

Before changing backends, CLI flags, IR shape, or language features, follow `.github/MAINTENANCE-CHECKLIST.md`.