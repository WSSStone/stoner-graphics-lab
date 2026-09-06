# Implementation Plan: Application Interactive Rendering Lab & ImGui Integration

**Branch**: `030-interactive-rendering-lab` | **Date**: 2026-09-06 | **Spec**: [spec.md](spec.md)\
**Input**: Feature specification from `specs/030-interactive-rendering-lab/spec.md`\
**Status**: Design complete; implementation and hardware acceptance have not run.

## Summary

Promote the proven calibration navigation into an Application-owned single-window lab for existing strict-cooked Lantern/Sponza workloads. Private Dear ImGui adapters consume engine events and produce immutable Renderer draw/texture packets; Renderer composes display-linear UI immediately before Feature 029's sole output transfer. Live settings, capability failures, bounded presets and lifecycle recovery share a coherent session state. Imported presets preserve pose and vertical FOV and rebuild projection for the current drawable, as explicitly clarified.

The largest implementation risk is native presentation: the current preview performs per-frame readback/reupload, the output executor waits after submission, and Vulkan's generic command executor itself waits indefinitely and tears down per-call resources. M0 therefore includes backend-private persistent deferred submission and a direct acquired-target Renderer path, not just a UI loop around existing synchronous code. Existing formal APIs and evidence authority remain separate.

Detailed decisions and rejected alternatives are in [research.md](research.md). Public values/state relationships are in [data-model.md](data-model.md); the five contracts freeze behavior and budgets before implementation.

## Technical Context

**Language/Version**: C++20 with traditional public/private headers and sources; private Objective-C++20 remains in Metal; C for existing yyjson; Python 3 standard-library validation scripts\
**Primary Dependencies**: Existing Core, Application, Asset, Renderer, RHI, Vulkan and Metal; SCons 4.10.1; GLFW desktop adapter; optional VK_EXT_swapchain_maintenance1 presentation fences with bounded acquire-history fallback; private Dear ImGui v1.92.5 at 6d910d5487d11ca567b61c7824b0c78c569d62f0; existing yyjson 0.12.0; existing offline GLSL/SPIR-V, SPIRV-Cross and metallib pipeline\
**Storage**: Process-local session/input/UI/frame state; explicit bounded local JSON preset exports; immutable strict-cooked scene/UI-shader generations; bounded PNG/JSON evidence; no database, autosave, ImGui ini store or runtime source fallback\
**Testing**: Focused StonerTest suites, existing camera/output/Asset regressions, strict Windows/macOS/Linux builds, Linux ASan/UBSan/TSan, native Vulkan/Metal UI tests, shader derivation and evidence consumer checks; current physical SDR and hands-on macOS HDR review\
**Target Platform**: Windows discrete Vulkan, macOS Metal on the existing macOS 12+/MSL2.4 baseline, Linux Vulkan including hosted Lavapipe; optional future backends remain capability-correct and outside required native acceptance\
**Project Type**: Layered native rendering engine plus StonerDemo desktop lab and validation tools\
**Performance Goals**: Two bounded frames in flight; zero image readback copies/maps/waits and no per-frame queue/device idle with captures disabled; next-eligible-frame ordinary settings updates; four representative 1,000-frame endurance runs and selected 20-cycle stress, with all-profile functional coverage; no hardware-independent FPS promise\
**Constraints**: One window/workload, private UI types, sole output transform, RGB-only UI blend into alpha-one display-linear target, finite resource/deadline budgets, unchanged formal authority, 64 KiB presets and 1 MiB evidence records\
**Scale/Scope**: Existing Lantern/Sponza Deferred production workflows, bounded Forward UI/output fixture, SDR variants plus supported four PQ/EDR profiles, 256 texture slots, 4096 draw commands and two capture requests; no editor/docking/VT/new rendering effects/full profiler

Architecture choices and exact limits/tolerances are documented in the contracts. Query native capabilities at runtime before selecting the presentation path; extension support is not a verified fact from planning. Missing optional maintenance1 support selects the bounded fallback and does not block backend implementation. Collect target-device inventory when accessible; actual native execution remains required for platform acceptance. Design changes must update the relevant contract before implementation/evidence collection.

## Constitution Check

Pre-research and post-design review both pass at the design level. These checks do not claim that future builds/tests have passed. The constitution's actual dependency directions supersede the template's abbreviated layering wording.

- [x] **Spec-Driven Development**: Current 030 spec and one accepted clarification precede design; this invocation produces no implementation/tasks or completion claim.
- [x] **Decoupled Architecture**: Asset -> Core; RHI -> Core; Backend -> RHI + Core; Renderer -> Asset + RHI + Core; Application -> Renderer + Asset + Core. Demo remains the composition root. Public UI data is engine-owned; native platform/graphics calls stay private. yyjson is shared only as a build dependency, not an Asset-private API.
- [x] **Design Pattern Discipline**: Camera, input arbitration, UI adaptation, presets, settings transition, Renderer composition and backend submission are separate responsibilities. Output policies remain strategies; registered control sections compose within one UI shell.
- [x] **Multi-API Support**: Backend-neutral draw packets, write masks and deferred-submit capability; real Vulkan/Metal implementations. Unsupported native paths fail explicitly; deterministic tests are not substituted for native results.
- [x] **Advanced Graphics Readiness**: Camera change reasons and a terminal UI stage support 031+ histories/effects without implementing temporal, meshlet or GI work prematurely.
- [x] **Naming Conventions**: PascalCase and UE-style F/E/I prefixes; no C++ Modules or third-party/native public types.
- [x] **Cross-Platform Compatibility**: Core no-replace filesystem primitive and private Application window service have Windows/macOS/Linux implementations; shader payloads use existing offline target derivation.
- [x] **Automated Cross-Platform Validation**: Six strict builds, separate sanitizers, native fixtures, producer/consumer and physical review gates are explicitly planned. Required unavailable coverage remains open with fallback command and follow-up ownership.

Post-design review: no dependency reversal, second GUI, runtime compilation, historical-evidence rewrite or implicit human-approval bypass is introduced. The Vulkan lab prefers maintenance1 presentation fences and otherwise uses acquire-history retirement with bounded generations and explicitly qualified terminal idle cleanup; the maintainer authorized these limited compatibility differences. Legacy/formal paths retain their existing capability baseline and unavailable 030 hardware is not accepted. New RHI and Core work is necessary to satisfy the requested preview/lifetime/export behavior and uses existing layer responsibilities. No constitution amendment is needed.

## Project Structure

### Documentation (this feature)

```text
specs/030-interactive-rendering-lab/
├── spec.md
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── checklists/requirements.md
└── contracts/
    ├── camera-input.md
    ├── ui-rendering.md
    ├── lab-runtime.md
    ├── lab-preset.md
    └── validation-evidence.md
```

[tasks.md](tasks.md) contains the generated dependency-ordered implementation work.

### Source Code (repository root)

The following is the planned placement; named new files do not yet exist.

```text
Source/Application/
  Public/Application/FFreeCameraController.h, FFreeCameraState.h
  Public/Application/FInteractiveLabSession.h, FLabSettingsSnapshot.h
  Public/Application/FInputOwnershipSnapshot.h, FWindowDisplayState.h
  Private/FFreeCameraController.cpp, FLabInputRouter.cpp
  Private/FInteractiveLabSession.cpp, FLabSettingsController.cpp
  Private/FImGuiLabAdapter.cpp, FImGuiInputAdapter.cpp, FImGuiDrawAdapter.cpp
  Private/FLabPresetCodec.cpp, FLabPresetStore.cpp, ImGuiUserConfig.h
  existing FWindow/FInputEvent/FInputState/FInputManager/private drivers
Source/Renderer/
  Public/Renderer/FUIDrawSnapshot.h, FUITextureRequest.h, FUICompositionSettings.h
  Private/FUIDrawValidator.cpp, FUITextureRegistry.cpp, FUICompositionExecutor.cpp
  existing FOutputTransformGraphBuilder/FOutputTransformExecutor
Source/RHI/
  existing pipeline state, command queue, capabilities and presentation bindings
Source/Backend/Vulkan/Private/
  existing FVulkanQueue/FVulkanNativeContext and new FDeferredNativeSubmission records
Source/Backend/Metal/Private/
  existing command/pipeline/presentation completed-handler ownership
Source/Core/
  existing public FPlatformFileSystem and private platform no-replace publication
Demo/StonerDemo/Private/
  FInteractiveLabRun.cpp, FLabProductionFrameContext.cpp
  existing configuration, backend factory, production bindings and formal adapters
ThirdParty/imgui/                 # untouched pinned core + notices/font/provenance
ThirdParty/yyjson/                # existing version, compiled once for private consumers
Content/Shaders/UI/               # new GLSL/SPIR-V/descriptors and derived target payloads
Tests/                           # focused deterministic/native suites and golden fixtures
Config/Validation/InteractiveLab/ # future limits, scripted sessions and evidence schemas
.github/scripts/                 # future lab runner/verifier/aggregate
.github/workflows/feature-030-interactive-lab.yml
Validation/030/                   # future bounded evidence only
```

**Structure Decision**: Follow existing per-layer Public/Private source separation and SCons source-scoped private includes. Adapt `Source/Application/SConscript`, `Source/Renderer/SConscript`, `Demo/StonerDemo/SConscript`, `Tests/SConscript` and `site_scons/LayerBuilder.py` as needed for private ImGui and shared yyjson build objects. Upstream code is compiled once with its own warning/config scope; public include paths do not expose it. Existing engine strict warnings remain enforced. No standalone GUI renderer or new runtime module dependency is introduced.

## Phase 0 — Research Closure

Three bounded research reviews covered camera/input/presets, Renderer/RHI/native execution and verified upstream ImGui integration. Decisions R01–R11 resolve all technical unknowns. The selected upstream pin/font/configuration, native synchronous bottlenecks, missing color-write mask, missing text/capture services and atomic export gap are recorded with code/source evidence in research.md.

No new API version is guessed from memory. The pinned release is a deliberate baseline; exact vendored file digests are an implementation-time reproducibility check against that full commit, not an unresolved dependency choice.

## Phase 1 — Design and Contracts

| Contract | Decisions frozen | Requirements |
| --- | --- | --- |
| [camera-input.md](contracts/camera-input.md) | Ordered events, UI-first arbitration, key quarantine, cursor/focus, camera bounds, scale/text | FR-002–FR-010 |
| [ui-rendering.md](contracts/ui-rendering.md) | Copied indexed packets, texture generations/acknowledgements, UI color/white/alpha, shader closure | FR-015–FR-018 |
| [lab-runtime.md](contracts/lab-runtime.md) | Session/transition state, deferred-submit seam, two slots, direct target, exact limits and CLI | FR-001, FR-011–FR-015, FR-021–FR-026 |
| [lab-preset.md](contracts/lab-preset.md) | v1 bounded schema/yyjson-writer digest, identity, current-aspect restore, atomic protected export | FR-019–FR-020 |
| [validation-evidence.md](contracts/validation-evidence.md) | Suites, native/physical matrix, UI-off parity, current human review, bounded authority | FR-025–FR-029; SC-001–SC-008 |

The data model links session/settings/display revisions to immutable packet and frame ownership. Public signatures may be refined without changing these behaviors; any new resource, authority or dependency behavior must update the affected contract before tasks implement it.

## Phase 2 — Implementation Sequencing for Task Generation

### M0 — Navigation and true deferred native presentation

1. Preserve existing formal tests and capture counters; establish separate execution-purpose validity before changing submission.
2. Add ordered focus/text/scale events, private cursor/clipboard services and reusable camera math; remove unconditional native Escape close and retain explicit legacy application policy.
3. Implement RHI deferred-submit capability/contract, separate render/presentation completion and borrowed acquired output binding. Query and enable optional maintenance1 presentation fences when supported; otherwise select acquire-history fallback. Implement the two-generation/512 MiB presentation budget, coalesced transitions, qualified terminal idle cleanup and 10 s shutdown watchdog from contracts/lab-runtime.md. Missing extension inventory is not an implementation gate; unavailable required physical execution remains open. Build Vulkan persistent native records/fence polling; adapt Metal ownership and keep synchronous formal callers supported.
4. Split Renderer output record/submit/retire, production frame binding/readback selection and Demo lab startup. The US1 UI-off MVP uses only existing scene/output closure; UI roots are not an M0 prerequisite. Submit two frame-owned workloads directly to acquired output targets without readback/idle bridge.
5. Prove delayed completion, bounded slot reuse, direct presentation, mode-specific resize/close retirement and zero native readback counters on both backends. Exercise both Vulkan selection branches through bounded fixtures without multiplying the workload/profile endurance matrix. M0 is not done if only an Application wait was removed.

### M1 — Private UI, textures and indexed rendering

1. Vendor verified ImGui/font sources/notices and private config; wire a single library into Application/test consumers, no official native backends.
2. Implement input adapter and current-frame capture arbitration; compose fixed panels through independent sections.
3. Add immutable packets and copy-on-write texture generations with modern create/update/destroy acknowledgements and budgets.
4. Add RHI ColorWriteMask with legacy RGBA default and native pipeline fingerprint coverage; implement indexed/scissor UI rendering and strict-cooked UI shader roots. UI-on startup preflights those roots; later enable uses the current generation and remains UI-off with a diagnostic if its UI closure is absent.
5. Pass draw/texture/scale/alpha fixtures, unsupported-callback/invalid-packet cases and in-flight replacement stress.

### M2 — Live settings, color composition and presets

1. Integrate terminal display-linear UI graph stage using existing output units/gamut; verify same-generation EDR white and sole output transfer.
2. Separate GPU-only live diagnostic visualization from explicitly requested numeric readback, preserving formal bypass requirements. Register Renderer-produced diagnostic targets privately in the shared UI texture registry with exact generation leases, producer-before-sample graph dependencies and shared GPU budgets; no CPU request or readback bridge is involved. Implement coherent live camera/exposure/output controls, latest-valid pending requests, explicit fallback and display-generation invalidation.
3. Build bounded preset codec, shared yyjson linkage, Core atomic no-replace publication and Application preset transaction using the pinned yyjson writer, with no custom float formatter; apply accepted current-window adaptation. Reject unsupported preset profiles atomically; independent loss of the active output alone invokes fallback/pause. Serialize the complete debug mode/stage/domain/range, using existing mode IDs and never serializing a capture action.
4. Add explicit bounded capture/report actions and UIUnavailable recovery. Complete minimize/focus/scale/output-switch/pending-preset behavior.
5. Pass numeric color/white/alpha tests, preset round-trip/malformed/race cases and UI-disabled parity regression. US3 settings/debug tests directly drive typed session APIs; US6 later adds JSON script parsing/CLI replay of those cases, so the earlier checkpoint has no later-parser prerequisite. A changed formal output opens a revisioned Candidate requirement rather than a tolerance change.

### M3 — Coverage, evidence and current maintainer review

1. Add hosted build/sanitizer/native/shader coverage and independent consumer jobs through one thin lab validation entrypoint; reuse existing 028/029 helpers and collect each required result once. Keep unavailable coverage explicit.
2. Run the fixed four endurance cases, remaining eight profile-smoke cases and representative lifecycle/mode stress; retain both-workload UI-off formal evidence and current hands-on/four-profile HDR review at frozen software.
3. Perform current navigation/control review and macOS four-profile HDR appearance review; record human decisions separately from machine preflight.
4. Close only when current required gates pass and any changed SDR Candidate is explicitly accepted. Documentation/evidence commits do not relabel the software tested.

Sequencing dependencies: M1 packet definitions may be developed alongside M0 native work, but native UI acceptance requires M0; M2 requires both. M3 evidence requires the final committed implementation. Shader cooking and Core/JSON build changes get their regression tests in their owning work packages. Task generation must split Vulkan execution, Renderer orchestration, Application input/UI, presets and evidence work rather than hide them in one oversized task.

## Validation Strategy and Risks

Run focused suites and relevant old regressions after their owning changes. Full cross-platform/hardware work follows the matrix once final behavior is frozen; no repetitive unrelated tests are prescribed. New test names/commands in quickstart are explicitly planned.

| Risk | Prevention / observable failure |
| --- | --- |
| Generic Vulkan submission still blocks | Native delayed-fence test and zero per-frame idle/readback instrumentation; SubmitDeferred must return before completion |
| GPU generations freed after only scene completion | Separate render fence and image-indexed presentation leases/presentation fences, two occupied slots and resize/texture stress |
| First widget click or focus queue leaks into camera | UI-first staged update, press ledger, focus generation and release-to-rearm tests |
| Wrong EDR white or double gamut/transfer | Domain fixtures and same-generation profile white; unchanged Feature 029 transform versions |
| Alpha differs between Vulkan/Metal | RGB-only mask on alpha-one target; old RGBA default parity |
| Preset restores old aspect or overwrites files | Pose/FOV-derived projection, atomic no-replace, malformed/digest/path/race fixtures |
| New UI shaders bypass strict-cooked closure | Explicit lab root set and producer/consumer target derivation; no runtime compilation |
| Optional hosted availability hides required hardware gap | Typed unavailable result, explicit manual lane and final aggregate requiring current physical/human records |

The implementation is XL, with M0 the primary schedule driver. No numerical duration estimate is asserted before task decomposition. Feature 041 profiling remains later; these resource/synchronization counters are the minimum required to verify bounded interaction.

## Delivery scope and reuse

All roadmap features and correctness boundaries remain required. Endurance concentrates on four preselected paths while functional checks still cover all 12 physical workload/profile combinations. The 030-only validation code is one thin entrypoint and one test module; exact-SHA, artifact, image and applicable numeric checks reuse existing helpers as specified in contracts/validation-evidence.md. Presets keep versioning, digest, bounded parsing, exact float32 recovery and atomic publication using the existing JSON writer. No new serialization or acceptance framework is part of this phase.

## Complexity Tracking

No constitution violations require an exception. Core atomic no-replace export, build-only yyjson sharing, RHI color-write masks and deferred submission are minimal cross-layer extensions justified by explicit feature requirements. They do not introduce an editor, generalized post-processing system, new graphics backend or second asset authority.
