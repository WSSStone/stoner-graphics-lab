# Quickstart: Engine Development Roadmap

**Feature**: 002-engine-development-roadmap
**Date**: 2026-04-21

## What This Feature Produces

A single master document — `doc/roadmap.md` — that defines the complete development plan for the Stoner Graphics Lab engine. It contains 23 phases organized across 5 architectural layers, each with enough detail to initiate a `/speckit.specify` cycle.

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
# Example for Phase 002
/speckit.specify Core foundation types and memory management: fixed-width integer types...
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

## Recommended First Phase

**Phase 002 — Core: Types & Memory** is the recommended starting point. It has no dependencies beyond the completed SCons skeleton and unlocks the most downstream phases.

```
/speckit.specify Core foundation types and memory management: fixed-width integer types (FPlatformTypes), engine string type (FString), hashed name type (FName), smart pointer wrappers (TSharedPtr, TUniquePtr), memory utilities (FMemory with aligned allocation), and container aliases (TArray, TMap). All types follow UE5 naming conventions. Must be cross-platform (Win/Mac/Linux) and include unit tests.
```
