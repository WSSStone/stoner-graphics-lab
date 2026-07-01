---
description: "Create or update the delivered feature implementation document under doc/"
---

# Generate Feature Implementation Documentation

This command is intended to run as a mandatory `after_implement` hook after `/speckit.implement` completes.

## Behavior

Create or update a delivered feature document in `doc/` using `doc/SYSTEM_DESIGN.MD` as the structural and learning-oriented reference.

The output is not just a delivery memo. It must be an engineering learning summary that helps the reader understand the implemented feature, the design tradeoffs, the relevant C++ techniques, and the rendering/engine/graphics-API concepts behind the code.

## Execution

1. Resolve the current feature directory the same way `/speckit.implement` does:
   - Prefer the `FEATURE_DIR` value from the completed implement flow when available.
   - Otherwise run `.specify/scripts/bash/check-prerequisites.sh --json --require-tasks --include-tasks` from the repository root and parse `FEATURE_DIR`.

2. Validate completion before writing documentation:
   - Read `FEATURE_DIR/tasks.md`.
   - Continue only when all implementation tasks are checked as complete.
   - If tasks are incomplete, report that documentation is skipped because the feature is not fully implemented.

3. Read the documentation references:
   - `doc/SYSTEM_DESIGN.MD` completely.
   - Existing delivered documents in `doc/`, especially adjacent numbered feature docs, to match visual/layout conventions.
   - Feature artifacts from `FEATURE_DIR`: `spec.md`, `plan.md`, `research.md`, `data-model.md`, `contracts/`, `quickstart.md`, and `tasks.md` when present.

4. Inspect the actual implementation:
   - Review changed source, test, and build files relevant to the feature.
   - Use the validation commands and outcomes from the completed implement flow.

5. Write the delivered document:
   - Target path: `doc/[###-feature-slug].html`.
   - Example: `specs/008-rhi-resource-pipeline/` maps to `doc/008-rhi-resource-pipeline.html`.
   - Keep Chinese narrative content.
   - Follow the full structure from `doc/SYSTEM_DESIGN.MD`, including the learning-oriented sections.
   - Use a light theme for the generated HTML.
   - Match the existing `doc/*.html` document structure and layout conventions, but do not copy a dark color palette.
   - Prefer readable light-theme colors: near-white page background, dark body text, subtle gray borders, restrained accent colors, and accessible contrast for code blocks, tables, badges, and navigation.

6. The document must include:
   - Basic metadata, update date, status, feature branch, and spec directory.
   - One to two paragraph summary of the delivered feature.
   - Goals, boundaries, architecture position, usage path, key lifecycle/state rules, module relationships, ADRs, task flow, acceptance evidence, best practices, C++ language points, and software engineering points.
   - Failure modes and debugging entry points.
   - A test matrix that explains what risk each test category covers.
   - Known limitations and future evolution.
   - A rendering/engine/graphics API learning notes section with core concept explanations, cross-API mapping when relevant, a source-reading route, and self-test questions.
   - For graphics-facing features, explicitly connect project concepts to Vulkan/DX12/Metal/OpenGL or explain why no direct mapping exists.

7. Avoid shallow template filling:
   - Do not merely list module names or task names.
   - Explain why important decisions were made, what alternatives were rejected, what bugs the design prevents, and how the implementation relates to real engine or graphics API practice.
   - Cite actual source files, tests, diagnostics, or debug dumps where they help the reader learn.

8. Report the generated or updated document path so a later hook, such as `speckit.git.commit`, can include it.
