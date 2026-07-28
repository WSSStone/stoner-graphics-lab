# Quickstart: Engine Development Roadmap

**Feature**: 002-engine-development-roadmap
**Date**: 2026-04-21
**Last Amended**: 2026-07-28

## What This Feature Produces

A single master document, `doc/roadmap.md`, that defines the complete
development plan for the Stoner Graphics Lab engine. Roadmap 2.1 contains
runtime Features 003 through 038 across Core, Asset, RHI, Backend, Renderer,
and Application ownership areas. Feature 002 is this roadmap meta-feature and
is not reused as a runtime phase number.

## Prerequisites

- [x] Phase 001 (SCons Project Skeleton) is complete
- [x] The `doc/` directory exists at project root
- [x] A draft `doc/roadmap.md` already exists (from earlier work)

## How to Use the Roadmap

### 1. Find the Next Phase to Work On

Open `doc/roadmap.md` and look at the **Phase Overview Table**. Find the first phase with status `⬜ Todo` whose dependencies are all `✅ Done`.

### 2. Start a New Phase

Copy the phase's **Speckit Prompt** and run:

```bash
# Example for the current next phase
/speckit.specify Asset core, identity, and registry foundation...
```

### 3. Follow the Speckit Workflow

```
/speckit.specify  →  Create feature spec
/speckit.clarify  →  Resolve ambiguities
/speckit.plan     →  Generate implementation plan
/speckit.tasks    →  Break into tasks
/speckit.implement → Execute tasks
```

### 4. Update the Roadmap

After completing a phase, update its status in `doc/roadmap.md`:
- Change `⬜ Todo` to `✅ Done` in the Phase Overview Table
- Update the Dependency Graph styling if desired

## Key Files

| File | Purpose |
|------|---------|
| `doc/roadmap.md` | The master roadmap document |
| `specs/002-engine-development-roadmap/spec.md` | Feature specification |
| `specs/002-engine-development-roadmap/plan.md` | This implementation plan |
| `specs/002-engine-development-roadmap/research.md` | Technology decisions |
| `specs/002-engine-development-roadmap/data-model.md` | Entity model |

## Recommended Next Phase

**Feature 020 — Asset: Core, Identity & Registry Foundation** is the next
planned phase. Features 003 through 019 are complete, and 020 establishes the
identity and extension contracts needed by texture, mesh, material, cooker, and
runtime-management phases.

```
/speckit.specify Add the Asset layer foundation for Stoner Graphics Lab: typed canonical logical-path FAssetId values with optional subresources, separate FAssetVersion hashes, metadata and dependency records, an in-memory FAssetRegistry, resolver/importer/loader/cooker extension contracts, typed soft references, deterministic format dispatch, diagnostics, lifecycle rules, and Windows/macOS/Linux headless tests. Asset depends only on Core and must not create RHI or graphics API objects.
```
