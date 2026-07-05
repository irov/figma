# AI Agent Instructions

`PROMPT.md` is the source of truth for the Figma SDK product prompt and must stay current enough to recreate the project.

Before making changes:

1. Read `PROMPT.md`.
2. Decide whether the task changes behavior, architecture, public API, `.fig` import, binding schema, viewer UX, build system, tests, roadmap, or project constraints.

When changes affect project requirements:

1. Update `PROMPT.md` in the same change.
2. Keep the update factual and durable, not a timestamp/changelog entry.
3. Preserve the current decisions: native SDK in C++17, CMake build, direct local `.fig` input, backend-free render-list runtime, sidecar `.ux.json` binding, `ActionRouter`, `DataContext`, and Mengine C++ codestyle.

When changes do not affect requirements:

1. Do not churn `PROMPT.md`.
2. Mention in the final response that no prompt update was needed.

For implementation work:

1. Prefer narrow changes aligned with `PROMPT.md`.
2. Keep `figma_sdk` independent from AppKit, Electron, DOM, CSS, and rendering backends.
3. Route SDK-owned dynamic memory through the runtime allocator.
4. Surface unsupported Figma/prototype behavior through diagnostics instead of silently dropping it.

