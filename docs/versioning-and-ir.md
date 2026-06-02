# IR And Versioning Workflow

## IR Contract

`tpp::IR` is the compiler output contract consumed by:

- language backends
- runtime rendering
- acceptance tests and IR stability tests
- embedding APIs that want a backend-neutral compiled representation

The repo treats IR shape as an intentional contract, not an incidental serialization detail.

## Current Ownership

The compiler frontend validates source input and produces semantic state.

The semantic-to-public-IR conversion layer then assembles public IR from that validated model. The goal is to keep this translation logic in one owner so public IR changes are deliberate and reviewable.

## Versioning Policy

tpp is still pre-1.0, so intentional contract breaks ship as minor bumps within major version `0`.

The current overhaul target is `0.14.0` once the following are stable:

- public compile/tooling boundary changes
- backend host contract changes
- acceptance test contract changes
- any intentional IR model changes

## Working Rules

- if IR shape changes intentionally, update snapshots and documentation in the same change set
- if CLI contracts or backend input behavior change, update `README.md` and `docs/usage.md` together
- if public/internal ownership changes, update the architecture docs as the code lands rather than deferring the explanation

## Transitional State

This repo is still mid-refactor.

Some workflow pieces still reflect the older bootstrap story and test artifact layout. The architecture direction is to finish with explicitly owned public IR code and a single coherent contract story, then publish that intentional break as `0.14.0`.