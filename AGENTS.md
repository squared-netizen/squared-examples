# Squared Examples Agent Instructions

## Repository role

This repository contains independent, complete applications built with Squared
and Squared Project Generator. Each directory beneath `apps/` is a generated,
independently buildable application with pinned immutable module versions.

The repository was clean and aligned with `origin/main` at the last recorded
inspection. Recheck; never assume that remains true.

## Current documentation milestone

Document the existing `apps/gui-controls-gallery` application and update the
repository roadmap. Do not create Text Editor, Console, Canvas, or File Dialog
applications yet. Those component examples are gated until the full Squared
widget library is complete.

Record these future constraints:

- Text Editor and Console compose primitive Squared widgets and must not
  introduce another widget hierarchy.
- Canvas demonstrates the stable retained drawing and input contracts.
- File Dialog uses primitive widgets first and integrates with HoloDisk only
  after the UI contract is stable.
- `gdx-holo` is the preferred showcase skin.

## Required first steps

1. Inspect `git status --short --branch` and compare with `origin/main`.
2. Read the root README, workflows, the gallery README, project metadata,
   `.squared-pg` module records, application-owned C++ headers and sources,
   and existing documentation.
3. Identify generated, copied, and third-party files before editing.
4. Present a short plan before changing files.

## Boundaries

- Framework implementation belongs in the `squared` repository.
- Generator or template behavior belongs in `squared-pg`.
- Do not hand-edit copied `modules/squared-*` as the authoritative source.
- Do not document or modify vendored Lua, yyjson, SDL Java sources, generated
  wrappers, Gradle wrapper files, or third-party licenses as project APIs.
- Application-owned code under
  `app/src/main/cpp/application/` is in scope.

## Documentation architecture

Maintain two parallel, cross-linked trees:

- `docs/programmer/` — how to build, run, configure, and learn from examples.
- `docs/developer/` — application implementation, ownership, build
  composition, workflows, and maintenance.

Each tree must have a `README.md` index. Each gallery page links to its
counterpart.

### Programmer documentation

Document:

- prerequisites and offline Termux build commands;
- project verification and APK output;
- controls demonstrated and how users interact with them;
- public application-owned C++ APIs, parameters, return values, ownership,
  errors, and minimal examples;
- pinned module versions and what they mean for an example consumer;
- the absence of HoloDisk where applicable.

Keep engine internals and copied-module implementation detail out of this tree.

### Developer documentation

Document:

- generated-project structure and which files are application-owned;
- application lifecycle and event flow;
- painter/backend boundary;
- asset ownership and skin loading;
- build and GitHub Actions workflows;
- data structures, complexity, invariants, and limitations;
- design patterns used and why.

## Doxygen and Graphviz

Add Doxygen comments only to application-owned public C++ APIs. Do not edit
copied framework headers or third-party code merely to add comments.

Configure the app's existing Doxygen workflow to surface undocumented
application API and broken references. Preserve the existing generated-project
documentation toolchain.

Add editable Graphviz sources for:

- application/module package dependencies derived from
  `.squared-pg/modules.lua` and `modules.cmake`;
- application lifecycle and input flow;
- skin asset loading and rendering flow, if implemented by the gallery.

Keep generated SVG beneath `build/docs/` unless the repository already
commits rendered documentation.

## Verification

Use the generator's existing commands:

```sh
cd apps/gui-controls-gallery
squared-pg project verify .
squared-pg docs
squared-pg project build .
```

Also validate all `.dot@@ files with `dot -Tsvg` and run
`git diff --check` from the repository root. If Android dependencies are
unavailable, report that precisely and still run non-Android documentation
validation.

## Git safety

- Update this existing repository only; never create another repository.
- Do not create new component application directories during this milestone.
- Do not stage, commit, push, tag, publish, or open a pull request unless the
  user explicitly asks after reviewing the diff.
- Preserve all existing work and report changed files and validation results.

