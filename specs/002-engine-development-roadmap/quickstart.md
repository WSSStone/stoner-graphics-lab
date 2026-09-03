# Quickstart: Engine Development Roadmap

**Feature**: 002-engine-development-roadmap
**Date**: 2026-04-21
**Last Amended**: 2026-09-01

## What This Feature Produces

A single master document, `doc/roadmap.md`, that defines the complete
development plan for the Stoner Graphics Lab engine. Roadmap 2.3.1 contains
runtime Features 003 through 041 across Core, Asset, RHI, Backend, Renderer,
and Application ownership areas. Feature 002 is this roadmap meta-feature and
is not reused as a runtime phase number.

## Prerequisites

- [x] Feature 001 and runtime Features 003-028 are complete
- [x] The `doc/` directory exists at project root
- [x] A draft `doc/roadmap.md` already exists (from earlier work)

## How to Use the Roadmap

### 1. Find the Next Phase to Work On

Open `doc/roadmap.md` and look at the **Phase Overview Table**. Find the first phase with status `⬜ Todo` whose dependencies are all `✅ Done`.

### 2. Start a New Phase

Copy the phase's **Speckit Prompt** and run:

```bash
# Example for the current next phase
/speckit.specify Renderer HDR Post-Processing and Output Transform...
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

**Feature 029 — Renderer: HDR Post-Processing & Output Transform** is the next
planned phase. Features 003 through 028 are complete, and 029 establishes the
shared HDR SceneColor-to-display contract required before anti-aliasing and
later temporal effects.

```
/speckit.specify Implement Renderer HDR Post-Processing and Output Transform on Features 013, 015, 018, 019, 027, and 028: use one Forward/Deferred Render Graph path from RGBA16F linear Rec.709/sRGB-D65 SceneColor through manual exposure and explicit pre/post-tonemap insertion points; provide versioned SDR Khronos PBR Neutral, ACES fitted, and Extended Reinhard tone maps with Khronos as default; provide separate ACES-style HDR viewing transforms for 1000/2000-nit PQ Rec.2020 and scRGB/EDR output-device profiles; integrate applicable HDR swapchains/drawables, native presentation/readback, resize/mode changes, and debug bypass. Windows retains SDR but no HDR validation. macOS Metal alone performs live human PQ/EDR visual authority; automation must not judge HDR appearance. Preserve Feature 028 v2 as historical evidence; use successor exact-dimension Candidates/no alignment for SDR and bounded manual JSON attestations for HDR. Exclude AA, bloom, depth of field, motion blur, automatic exposure, vendor upscalers, and a post-processing editor.
```
