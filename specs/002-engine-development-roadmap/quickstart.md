# Quickstart: Engine Development Roadmap

**Feature**: 002-engine-development-roadmap
**Date**: 2026-04-21
**Last Amended**: 2026-09-06

## What This Feature Produces

A single master document, `doc/roadmap.md`, that defines the complete
development plan for the Stoner Graphics Lab engine. Roadmap 3.1.0 contains
runtime Features 003 through 053 across Core, Asset, RHI, Backend, Renderer,
and Application ownership areas. Feature 002 is this roadmap meta-feature and
is not reused as a runtime phase number. Features 030-042 form the complete
Vulkan/Metal renderer track; 043-050 are geometry/advanced lighting and 051-053
are independent optional backends. Historical numbers resolve through
[migration-3.1.md](migration-3.1.md).

## Prerequisites

- [x] Feature 001 and runtime Features 003-028 are complete; 029 is complete by explicit maintainer exception
- [x] The `doc/` directory exists at project root
- [x] A draft `doc/roadmap.md` already exists (from earlier work)

## How to Use the Roadmap

### 1. Find the Next Phase to Work On

Open `doc/roadmap.md` and look at the **Phase Overview Table**. Use the recommended complete-renderer track; choose a `⬜ Todo` phase whose dependencies are all `✅ Done`. Numbering does not force unrelated backend work before SSGI.

### 2. Start a New Phase

Copy the phase's **Speckit Prompt** and run:

```bash
# Example for the current next phase
/speckit.specify Application Interactive Rendering Lab & ImGui Integration...
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

**Feature 030 — Application: Interactive Rendering Lab & ImGui Integration** is next.
The existing camera preview already has WASD/QE/Shift/right-mouse/FOV/reset/export,
but it is a calibration-only path, not a GUI or a formal HDR interactive lab.
Reuse it; add UI input capture, live controls, native presentation without
mandatory synchronous CPU readback, and display-linear SDR/PQ/EDR UI composition.

Then follow 031 TAA -> 032-040 rendering effects -> 041 full profiling -> 042
integrated acceptance. Future effects reuse the interactive shell.
Feature 029's [maintainer exception closeout](../029-hdr-output-transform/closeout.md)
does not waive Feature 030's evidence gates. Keep formal scene captures UI-free
by default; previews and preset exports do not accept baselines.

```text
/speckit.specify Implement Application Interactive Rendering Lab & ImGui Integration on Features 004, 008, 013, 015, 016, 017, 018, 019, 027, 028, 029. Promote the existing calibration-only camera preview into a reusable interactive rendering lab before TAA and subsequent effects. Application owns camera controls, input routing, UI state and commands; Renderer owns backend-neutral UI draw packets and Render Graph execution through RHI. Integrate a pinned Dear ImGui revision behind private adapters, without exposing ImGui types in public engine contracts or calling Vulkan/Metal directly from Application/Renderer. Use an engine input adapter rather than installing competing GLFW callbacks. Deliver Reusable WASD/QE/Shift free camera, right-mouse look, cursor capture/release, focus-loss recovery, speed/FOV controls, reset and bounded camera/settings presets; distinguish camera cut/reset/FOV changes for the later temporal consumer; Dear ImGui controls for the loaded scene, camera, manual exposure, existing tone maps, SDR/PQ/EDR output modes and debug bypass; expose capability failures explicitly and recover safely from unsupported mode changes; later effects extend this shell; Keyboard, mouse, scroll and UTF-8 text events, clipboard basics and explicit UI capture arbitration; typing or dragging widgets must not also move the camera; start with one native window and a HiDPI-correct viewport; Immutable UI draw snapshots, font/texture upload lifecycle, indexed triangles, clip/scissor rectangles and alpha blending through Renderer/RHI on Vulkan and Metal; no third-party ownership leakage or parallel backend-specific demo renderer; Interactive native presentation must not require synchronous CPU readback every frame; bound frames in flight, resource retirement and idle/minimized behavior; explicit captures may use a separate bounded readback path; Display-linear UI composition after scene post-processing and before the sole Feature 029 output transfer/native packing; define UI reference-white brightness, input color decoding/gamut conversion and linear alpha blending for SDR/PQ/EDR; UI bypasses scene exposure, TAA, DOF, motion blur and bloom; Formal scene captures default to UI disabled with frozen camera/settings; interactive previews and preset exports are not Accepted evidence. Bounded UI-specific smoke evidence is separate; no automatic baseline update or HDR appearance scoring. Milestones: M0: reusable camera/input arbitration and one-window native presentation without a mandatory per-frame CPU readback; M1: pinned ImGui/private adapters, font/texture lifecycle, HiDPI and Vulkan/Metal UI rendering; M2: live scene/output controls, SDR/PQ/EDR UI composition and resize/focus/minimize/mode-switch recovery; M3: bounded automated input/lifecycle/off-parity checks and maintainer hands-on navigation/control review; macOS HDR appearance remains human-only. Exclude Full editor/world authoring, native OS widget toolkit, material/node editors, docking or multi-window viewports, general IME/accessibility framework, full GPU profiling and new rendering algorithms; no VT foundation or texture-streaming implementation. Preserve completed 003-029 identities and evidence. Changed formal SDR scene output requires a workload revision bump, exact-dimension Candidate and explicit maintainer acceptance; prohibit alignment/cropping/scaling/resampling. Keep bounded PNG/JSON evidence. Windows claims SDR validation only; macOS PQ/EDR appearance requires live maintainer review under Feature 029 Apple metadata governance.
```
