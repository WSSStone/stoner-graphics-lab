# Research: Interactive Rendering Lab

**Date**: 2026-09-06\
**Feature**: [spec.md](spec.md)\
**Status**: Research decisions resolved; this is design evidence, not implementation validation.

## R01 — Reuse camera math without inheriting calibration authority

**Decision**: Move reusable navigation math into Application's `FFreeCameraController`; retain Demo's frozen camera registry and calibration-candidate exporter as compatibility adapters. Interactive initialization, reset and preset restore rebuild projection at the current drawable aspect. Formal runs continue consuming frozen matrices.

**Evidence**: `Demo/StonerDemo/Private/FProductionCameraPreview.cpp` implements +X-forward/+Y-right/+Z-up movement, normalized diagonals, speed 1.5 units/s, Shift x4, 0.003 radians/logical pixel, wheel step -0.035 radians and 20–90 degree FOV. `FProductionCameraPreset.cpp` uses positive-X StandardZ and near/far 0.1/100. `Source/Application/Public/Application/FCameraComponent.h` defaults far to 1000, so simply substituting its defaults changes production output. Existing preview `Initialize` supplies aspect before `Reset` replaces it from the frozen projection.

**Rationale**: Preserve proven math while implementing the explicit clarification: pose and vertical FOV survive window changes; original export dimensions are context. Use existing 1e-4 matrix checks and 5e-4 inverse checks where applicable.

**Alternatives considered**: Keep a Demo-private lab controller (not reusable); replay exported projection (wrong aspect); resize the window (rejected by maintainer); adopt generic camera defaults (changes historical output).

## R02 — Input has one platform owner and explicit gesture ownership

**Decision**: Extend existing Application events/window services for Unicode text, Super keys, pointer enter, scale, clipboard and cursor mode. Preserve cross-event sequence ordering. Feed private UI adapters first; commit camera updates only after current-frame UI ownership/activation resolves. A press-owner ledger and release-to-rearm quarantine prevent held-key and drag leakage.

**Evidence**: `Source/Application/Private/FGlfwWindowDriver.cpp` currently owns callbacks and closes on Escape in the native key callback. There is no character callback, clipboard service, captured cursor service or content-scale event. `FInputState::FocusLost` leaves pointer baselines; later queued down events can re-arm input while unfocused. Existing input/window streams share sequence generation but are consumed separately.

**Rationale**: Remove unconditional Escape close from the private driver; each application mode owns exit policy. Clear pointer baseline/deltas and quarantine downs on focus/capture transitions. Calibration mode may explicitly retain Escape-to-exit. Modifier keys never substitute for committed UTF-8 text.

**Alternatives considered**: Official ImGui GLFW callbacks (competing event ownership); previous-frame capture alone (first-click leakage); hover-only gating (fails drag/text ownership); OS key names as text (breaks Unicode and layouts).

## R03 — Pin Dear ImGui and isolate every platform escape path

**Decision**: Use non-docking Dear ImGui `v1.92.5`, commit `6d910d5487d11ca567b61c7824b0c78c569d62f0`. This is a deliberate stable baseline, not a latest-version claim. Preserve upstream files and notices; compile only `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp` into an Application-private build dependency. Do not compile official graphics/window backends or the demo source for the shipped lab.

**Evidence**: The [upstream release](https://github.com/ocornut/imgui/releases/tag/v1.92.5) resolves to the [full commit](https://github.com/ocornut/imgui/commit/6d910d5487d11ca567b61c7824b0c78c569d62f0). The [pinned configuration](https://github.com/ocornut/imgui/blob/6d910d5487d11ca567b61c7824b0c78c569d62f0/imconfig.h) defines private configuration controls.

**Rationale**: A single private `IMGUI_USER_CONFIG` enables `IMGUI_USE_WCHAR32` and disables default Win32, shell and file functions; set `IniFilename` and `LogFilename` null. Otherwise even upstream core can bypass platform ownership. Keep assertions active in strict Release and run the upstream version/layout check at initialization.

**Alternatives considered**: Docking/multi-viewport branch (excluded scope); floating branch (non-reproducible); older static-atlas integration (misses the selected version's texture lifecycle); native ImGui backend renderer (violates RHI ownership).

## R04 — Modern texture requests become owned engine values

**Decision**: Implement the selected version's `RendererHasTextures` create/update/destroy lifecycle through copied engine texture requests. Application-private code owns ImGui pointers and request acknowledgements; Renderer returns preparation/retirement results through opaque engine identities. Copy-on-write generations keep queued draws stable. No third-party pointer, draw callback or native handle crosses a public boundary.

**Evidence**: The [pinned backend guide](https://github.com/ocornut/imgui/blob/6d910d5487d11ca567b61c7824b0c78c569d62f0/docs/BACKENDS.md) describes texture updates and rendering obligations. The [pinned header](https://github.com/ocornut/imgui/blob/6d910d5487d11ca567b61c7824b0c78c569d62f0/imgui.h) defines texture requests, events and platform clipboard callbacks.

**Rationale**: Acknowledge create/update only after successful Renderer preparation; enqueue GPU readiness dependencies before use. Retire destroy only after all upload/render fences complete and snapshot leases release; borrowed output images/present synchronization have separate presentation retirement. Reject arbitrary callbacks; translate the upstream reset-state sentinel into a typed reset operation. Bounded retry never exposes partial uploads.

**Alternatives considered**: Mutate an in-flight atlas (hazards); store native handles in texture IDs (ownership leakage); acknowledge before resource preparation (dangling IDs); unbounded retries (persistent growth).

## R05 — Font and text support is explicit and bounded

**Decision**: Ship English labels and the pinned tree's scalable `Cousine-Regular.ttf` with its OFL notice; use 16 logical-pixel default text. Store/copy/paste valid Unicode through 32-bit code points. Font coverage is the bundled font's actual glyph set, with visible replacement glyph and a bounded missing-glyph diagnostic. No full CJK shaping, emoji or IME claim.

**Evidence**: The [upstream font guide](https://github.com/ocornut/imgui/blob/6d910d5487d11ca567b61c7824b0c78c569d62f0/docs/FONTS.md) and [font directory](https://github.com/ocornut/imgui/tree/6d910d5487d11ca567b61c7824b0c78c569d62f0/misc/fonts) document bundled fonts and licensing. Core retains its [MIT notice](https://github.com/ocornut/imgui/blob/6d910d5487d11ca567b61c7824b0c78c569d62f0/LICENSE.txt).

**Rationale**: This meets text/clipboard and scale requirements without silently promising universal glyph rendering. Verify ASCII, supported accented glyphs and an unsupported supplementary code point independently of byte round-trip correctness. Source-file digests are computed during vendoring from the verified commit, never invented in planning.

**Alternatives considered**: Platform-installed fonts (non-reproducible); arbitrary font loading (unneeded surface); bitmap default at every scale (poor scaling); adding a general font/localization system (out of scope).

## R06 — Insert UI into existing output stages, not a second tone mapper

**Decision**: Add a typed UI composition operation after all post-tonemap scene operations, before the sole output-device transfer. Copy post-tonemap color into a display-linear composition target, then load/blend UI into it. Hidden UI declares no UI target/pass and follows the existing output graph. UI-off startup accepts the existing scene/output-only closure; UI-on startup or later enable preflights UI roots in the same immutable generation.

**Evidence**: `Content/Shaders/PostProcess/OutputTransform.frag` already separates exposure (stage 0), tone/viewing (stage 1) and output-device transfer (stage 2). HDR viewing already produces output-gamut nits: Rec.2020 for PQ and Rec.709 for EDR; SDR tone mapping produces normalized Rec.709. `FOutputTransformGraphBuilder.cpp` and `FOutputTransformExecutor.cpp` own the insertion/execution boundary. `Config/Validation/OutputTransform/Profiles.json` freezes profile versions and tolerance policies.

**Rationale**: Decode UI sRGB color once; convert Rec.709 to Rec.2020 only for PQ; scale UI by its reference-white multiplier in the existing domain. EDR UI reference white resolves from the same native display generation as final packing. Do not add another scene gamut conversion or overwrite Feature 029 profile/version semantics. Scene exposure and future temporal/effect stages never process UI.

**Alternatives considered**: Blend encoded colors (wrong alpha/color math); draw UI before scene exposure (unstable brightness); tone-map UI through scene settings (wrong SDR/HDR semantics); a second presentation transfer (double encoding).

## R07 — Minimal RHI color-write extension avoids legacy alpha changes

**Decision**: Add typed `ERHIColorWriteMask` to the blend/pipeline contract, default RGBA. UI pipelines write RGB only, use source-alpha/one-minus-source-alpha RGB blending, and preserve a destination alpha initialized to one. Validate mask bits and include the mask in Vulkan/Metal pipeline identity.

**Evidence**: Existing `FRHIBlendState` describes RGB factors/op only. Vulkan sets alpha factors to ONE/ZERO, while Metal reuses RGB factors for alpha. Neither exposes a color-write mask, although both native pipelines can apply one. Indexed draws, offsets and scissors already have RHI vocabulary.

**Rationale**: A mask addition is enough for this opaque final-output pipeline and leaves legacy native alpha defaults intact. Test RGBA default parity, RGB-only writes and unsupported mask bits. General independent alpha blending is unnecessary here.

**Alternatives considered**: Add all independent-alpha states now (larger API change); accept native alpha mismatch (wrong conformance); post-fix alpha with extra readback/pass (unnecessary).

## R08 — Build a persistent two-slot production renderer path

**Decision**: Split production preparation, recording, completion and optional capture. Two persistent slots own scene, UI and output resources plus completion fences. Import acquired swapchain image into Renderer graph and render/copy final GPU output directly to it through RHI. Capture is an explicit separate operation. Poll readiness and service events while slots are busy; no per-frame queue/device idle.

**Evidence**: `FProductionCameraPreviewRun.cpp` waits and reads output, then `PresentProductionImage` uploads CPU pixels for presentation. `FProductionContentDeferredExecution.cpp`/`FProductionSubmissionHarness.cpp` bind readbacks even when intermediate readbacks are disabled; `BuildCycleBindings(false)` still keeps final output and validation expects six readbacks. Existing triangle submission and RHI fence/swapchain paths provide reusable vocabulary, not proof that generic production submission is asynchronous. `FOutputTransformExecutor` unconditionally waits after submit. Vulkan `FVulkanQueue::Submit` enters `FVulkanNativeContext::ExecuteRecordedCommands`, which owns per-call native resources, waits indefinitely, maps readbacks and cleans up; its recorded texture-copy path is unsupported. Metal has completed-handler presentation ownership, but the Demo formal facade still waits before presentation.

**Rationale**: Merely hiding readback results does not remove synchronization. Introduce an explicit `None` versus `Formal` capture policy and separate binding validity paths. Add backend-private persistent Vulkan submission records and real completion polling, plus generation-safe acquired-target registration for direct preview presentation. Preserve the synchronous formal facade on top of those records; merely deleting harness waits is insufficient. Preview recording/submission must distinguish queued, presentation-queued and completed results from formal success. Preserve the formal authority path and same-frame token checks. Share loaded assets/material realizations; use frame-safe mutable uniforms/attachments. UI failure falls back to the already prepared scene-only output only before an incompatible submission occurs.

**Alternatives considered**: Keep per-frame CPU image bridge (violates FR-021); add threads around synchronous readback (still serializes); rebuild resources every frame (churn); globally weaken formal binding validation (breaks authority).

A final design review separates rendering fences from presentation completion: the [Khronos semaphore-reuse guidance](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html) explains why a submit fence does not establish presentation retirement and why present semaphores are image-indexed. The initial plan made `VK_EXT_swapchain_maintenance1` a hard prerequisite. The maintainer superseded that choice on 2026-09-06: query support, prefer presentation fences, and use a bounded compatibility path when absent. Follow the [Khronos recreation example](https://docs.vulkan.org/samples/latest/samples/api/swapchain_recreation/README.html) for image-indexed reacquisition and deferred old-swapchain retirement. The local policy caps total generations at two and estimated swapchain color storage at 512 MiB, coalesces requests, and fails rather than accumulating generations when progress stops. Terminal idle cleanup has the Vulkan-guide-documented assurance gap and is explicitly labeled IdleAssumed, with bounded process shutdown and no ordinary-frame idle/readback. These are accepted compatibility differences, not proof-equivalent retirement. contracts/lab-runtime.md defines cancellation, creation failure and report behavior. This removes the optional-extension implementation gate without waiving actual platform execution or changing legacy formal authority.

## R09 — Presets use a dedicated bounded schema, existing JSON implementation

**Decision**: Create `stoner.interactive-lab-preset` v1, separate from `stoner.production-camera-candidate`. Application-private codec uses existing pinned yyjson 0.12.0 via a shared build-only third-party library compiled once; Asset and Application have source-scoped private header access, no public JSON types or cross-layer private includes. Preserve Asset parser behavior and test its existing suites after linkage changes.

**Evidence**: `Source/Asset/SConscript` currently compiles `ThirdParty/yyjson/yyjson.c` as a private C source. Core provides bounded regular-file reads, atomic replacement, durable writes and SHA identity is available through Asset's public digest contract. No public generic Core JSON parser currently exists.

**Rationale**: A 64 KiB schema-limited codec can reject duplicates/unknown fields, preserve float precision and bind workload identity using the existing yyjson parser and default compact writer. Fixed schema construction order plus public SHA-256 provides normalized digests without a custom number formatter or key-sorting framework. Add Core `PublishFileNoReplace` for race-safe default export; use existing atomic replacement only for an explicit overwrite action. All paths go through Core. No automatic preference store.

**Alternatives considered**: Reach into Asset-private codec headers (boundary violation); compile duplicate global yyjson implementations (link hazards); custom permissive parser (maintenance); calibration candidate import (wrong authority); check-then-write overwrite prevention (race).

## R10 — Bounds and overload behavior are versioned contracts

**Decision**: Freeze the exact limits in [lab-runtime.md](contracts/lab-runtime.md) and schema limits in [lab-preset.md](contracts/lab-preset.md). Latest valid pending output request wins; never build an unbounded transition queue. A minimized pending preset lasts until restore/cancel/session close; it occupies one bounded slot, with no rendering. Native waits have finite deadlines while events are serviced.

**Rationale**: The lab needs predictable input/resource behavior, not a universal FPS guarantee. Budgets are explicit pre-implementation choices, not measured performance claims. Native allocation refusal is reported before exceeding them. Stage feature effects/profiling later without changing authority or input routing.

**Alternatives considered**: Unbounded event/retirement history; deadlines dependent on observed test success; silently widening budgets; blocking event service while waiting for GPU completion.

## R11 — Native coverage and human authority remain distinct

**Decision**: Extend existing CI patterns with six strict builds, two sanitizer jobs, native Linux Vulkan under a virtual display, macOS Metal, shader derivation validation, bounded artifact consumer and machine aggregate. Required physical closeout lanes are Windows discrete Vulkan SDR and macOS Metal SDR/PQ/EDR. Four preselected combinations get endurance; all 12 combinations retain native functional checks, both production workloads retain SDR parity and hands-on review, and representative pairs get lifecycle/mode stress. One thin lab validation entrypoint composes existing helpers rather than duplicating policy engines. HDR appearance requires a current four-profile maintainer record; machine results cannot author it.

**Evidence**: `.github/workflows/feature-029-hdr-output.yml` already separates strict, sanitizer, Lavapipe, Metal-nonvisual and artifact-consumer jobs. Features 028/029 contracts define exact dimensions, frozen software guards and bounded PNG/JSON evidence; their one-time exceptions expressly do not carry forward.

**Rationale**: A UI smoke image is not scene acceptance. Compare UI-off frames against existing current-policy references, including semantic probes; changed formal SDR requires a workload revision and explicit Candidate acceptance. Reference provenance may be historical, but the new run and review must identify current software. Keep validation commands planned until implemented.

**Alternatives considered**: Treat hosted success as physical interaction/HDR pass; reuse waived 029 human evidence; compare rescaled screenshots; create new Accepted files automatically.

## Research closure

Resolved unknowns: dependency pin/configuration, font/text baseline, input ordering, camera defaults, preset schema/extent adaptation, JSON linkage, atomic export, texture acknowledgements, color domain/alpha, no-readback frame ownership, fixed budgets and validation matrix. Target hardware execution and capability inventory remain to be collected; optional-extension absence selects fallback and does not gate implementation; documented design choices are not proof of device support. Source paths above identify existing code; new symbols and files in the contracts are proposed, not claims of existing implementation.
