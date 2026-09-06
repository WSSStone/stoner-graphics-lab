# Feature Specification: Application Interactive Rendering Lab & ImGui Integration

**Feature Branch**: `030-interactive-rendering-lab`\
**Created**: 2026-09-06\
**Status**: Draft\
**Input**: User description: "为 roadmap 下一个 phase 制定 spec" — specify Phase 030 from Roadmap 3.1.0: promote the existing calibration camera into a reusable, single-window rendering lab with Dear ImGui controls, safe input routing, live output settings, responsive native presentation, and separate preview/acceptance evidence.

## Clarifications

### Session 2026-09-06

- Q: 导入同一 workload 的相机预设时，如果当前窗口尺寸或宽高比与导出时不同，应如何处理？ → A: A — 适配当前窗口。保持当前窗口尺寸，恢复相机位置、朝向与垂直 FOV，按当前 drawable 宽高比重建投影；预设中的导出尺寸仅作上下文，不要求匹配或调整窗口。正式验收仍使用独立冻结的尺寸与相机。

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Navigate a Production Scene Interactively (Priority: P1)

As a rendering developer, I can move through an already loaded production scene, look around, adjust movement speed and field of view, and return to its starting camera without restarting, so I can inspect the renderer from useful viewpoints.

**Why this priority**: Navigation is the minimum useful lab and prepares the application for the effects that follow Phase 030.

**Independent Test**: Open each existing Lantern and Sponza cooked workload in the lab with the UI hidden; exercise movement, look, reset, focus loss and window restoration, and compare camera results with a deterministic input sequence.

**Acceptance Scenarios**:

1. **Given** a valid loaded workload and a focused viewport, **When** the user applies W/S, A/D, Q/E and Shift, **Then** the camera moves forward/backward, sideways and vertically with an observable speed boost, using the existing calibration-camera coordinate convention.
2. **Given** the viewport owns the mouse, **When** the user presses and drags the right mouse button, **Then** the camera rotates with a captured cursor; releasing the button or pressing Escape releases capture without a residual look delta.
3. **Given** a changed camera, **When** the user resets it, **Then** the workload's initial camera is restored and a camera discontinuity is identified for future temporal consumers.
4. **Given** navigation is active, **When** focus is lost or the window is minimized and later restored, **Then** motion stops, the cursor is released, and navigation resumes only after fresh input without a large time-step jump.

---

### User Story 2 - Operate Controls Without Moving the Camera (Priority: P1)

As a rendering developer, I can use panels, type values, scroll and paste text while the scene remains stable, so UI interaction cannot accidentally change the viewpoint I am inspecting.

**Why this priority**: Camera input leakage makes every rendering comparison unreliable.

**Independent Test**: Use a deterministic panel/input fixture with the camera initially frozen; exercise every supported input class and compare camera state before and after UI-owned actions.

**Acceptance Scenarios**:

1. **Given** a text field owns keyboard input, **When** the user types movement-key characters or uses supported copy/paste shortcuts, **Then** the field receives text and the camera does not translate, rotate or change field of view.
2. **Given** a widget owns a mouse drag or scroll, **When** the pointer moves outside its bounds, **Then** ownership lasts through the gesture and neither right-mouse look nor wheel field-of-view control leaks into the camera.
3. **Given** a focused viewport without UI capture, **When** the user navigates and then activates a widget, **Then** camera ownership ends before the widget's action takes effect; already held keys cannot restart navigation until released and pressed again.
4. **Given** the window's display scale changes, **When** the user selects, drags or scrolls controls, **Then** hit testing, text, clipping and viewport positioning remain aligned in logical and drawable coordinates.

---

### User Story 3 - Compare Existing Rendering and Output Settings Live (Priority: P1)

As a rendering developer, I can inspect the loaded scene and change its camera, manual exposure, existing tone map, output profile and debug bypass in one panel, so I can compare existing rendering behavior without editing files or relaunching.

**Why this priority**: This provides the reusable control surface required by subsequent roadmap effects.

**Independent Test**: Load one existing workload, exercise every existing supported output/profile choice, submit invalid or unsupported choices, and inspect the effective settings and continued presentation.

**Acceptance Scenarios**:

1. **Given** an interactive scene, **When** the user changes valid exposure, camera or applicable tone-map settings, **Then** the next eligible rendered frame uses one consistent settings snapshot and the panel displays the effective value.
2. **Given** SDR output, **When** the user selects any of Feature 029's three existing tone maps, **Then** that version is identified and used; selecting HDR instead exposes its existing viewing profile without implying that SDR tone maps also control HDR.
3. **Given** a supported output transition, **When** it is requested, **Then** the lab either completes the transition and identifies the new effective profile or reports failure and retains a valid presentation state, with no mixed old/new output state.
4. **Given** an unsupported profile, invalid number or failed transition, **When** it is selected or imported, **Then** the lab reports the reason and retains the last valid setting; if that output is no longer available, it visibly resolves to supported SDR or pauses with a diagnostic when no valid output is available.
5. **Given** a scene is loaded, **When** its panel is opened, **Then** the workload/package identity, camera and effective rendering/output state are available; the panel does not imply that it can edit the scene's assets or world.

---

### User Story 4 - Read the UI Consistently Across Display Modes (Priority: P2)

As a maintainer, I can read controls over bright and dark scenes in SDR and supported HDR modes, so exposure or later scene effects do not make the controls bloom, smear or change brightness.

**Why this priority**: The lab must remain usable while inspecting the very display settings that affect scene appearance.

**Independent Test**: Present bounded text, colored-patch, translucent-panel and textured-widget fixtures over dark/bright backgrounds; vary scene exposure and output profiles, verify numeric color contracts, and perform live HDR review on the supported macOS display.

**Acceptance Scenarios**:

1. **Given** a fixed UI reference-white setting, **When** scene exposure or the scene tone map changes, **Then** the UI's own color and luminance contribution remains unchanged; a translucent panel may still reveal the changed scene beneath it.
2. **Given** SDR or supported PQ/EDR output, **When** opaque and translucent UI fixtures are rendered, **Then** UI colors are decoded, converted and blended in the declared display-linear domain before the single final output transfer, with no double encoding or scene-exposure multiplier.
3. **Given** the UI is hidden, **When** an otherwise identical frozen scene is captured, **Then** its image and semantic probes match the established UI-free output under the existing exact-dimension comparison policy.
4. **Given** the active display's scale or HDR capabilities change, **When** output state is refreshed, **Then** UI scale and brightness use the same effective display state as the presented frame and the panel reports any fallback.

---

### User Story 5 - Restore a Useful Lab Setup (Priority: P2)

As a rendering developer, I can explicitly export and restore a bounded camera/settings preset, so an investigation is repeatable without turning a preview into an accepted image reference.

**Why this priority**: Repeatable setup makes the lab useful for later effects while preserving existing evidence authority.

**Independent Test**: Export a valid preset, change the session and restore it at both matching and different drawable extents/aspect ratios; verify pose and vertical field of view are preserved without resizing the window. Attempt truncated, oversized, wrong-workload, unknown-version and non-finite presets and compare effective state before and after rejection.

**Acceptance Scenarios**:

1. **Given** a valid camera and output configuration, **When** the user explicitly exports a preset and restores it for the same workload at the same drawable extent, **Then** the camera and valid requested settings round-trip with their identities and numeric precision intact.
2. **Given** malformed, incompatible or over-budget preset data, **When** import is attempted, **Then** it is rejected with a reason and no partial camera/settings change.
3. **Given** a valid preset requests a profile unavailable on this device, **When** it is imported, **Then** the whole import is rejected with an explicit capability reason and the current valid camera/settings remain unchanged; the file is never rewritten. If the current effective output independently loses capability, the normal SDR-fallback-or-pause policy applies without applying any preset fields.
4. **Given** a preview preset or screenshot exists, **When** formal validation is invoked, **Then** it uses the independently frozen workload camera/settings and does not consume the live session or treat the export as Accepted evidence.
5. **Given** a valid same-workload preset exported at a different drawable extent or aspect ratio, **When** it is restored, **Then** the current window dimensions remain unchanged, camera position/orientation and vertical field of view are restored, and projection is rebuilt for the current drawable aspect ratio; horizontal framing may change. Exported dimensions remain source context, and the restore identifies a camera discontinuity.

---

### User Story 6 - Keep the Lab Stable During a Long Session (Priority: P2)

As a rendering developer, I can keep navigating, resizing, hiding panels and changing output modes during a bounded session, so inspection remains responsive without accumulating resources or requiring a screenshot readback for every frame.

**Why this priority**: The lab becomes the shared shell for later rendering features; lifecycle failures would otherwise affect each new feature.

**Independent Test**: Run the bounded session defined in SC-005, with readback instrumentation and resource high-water counters, then compare baseline and teardown state.

**Acceptance Scenarios**:

1. **Given** an interactive session with capture disabled, **When** frames are presented, **Then** there are no mandatory synchronous image readbacks and the number of outstanding frames and retained UI resources stays within declared limits.
2. **Given** a minimized or zero-drawable window, **When** the event loop continues, **Then** drawing and UI uploads pause, events remain serviceable, and restore or close completes without stale resources or a busy render loop.
3. **Given** queued frames still reference UI fonts or textures, **When** UI resources are replaced, scaled or released, **Then** the queued frames remain valid and resources are retired after their last use.
4. **Given** a bad UI texture reference, over-budget draw data or failed allocation, **When** UI submission is prepared, **Then** the error is bounded and diagnosed, partial UI output is not published, and the lab either continues scene-only or stops cleanly if presentation itself is unavailable.

---

### User Story 7 - Validate the Lab Without Weakening Image Authority (Priority: P2)

As a maintainer, I can separately review interaction, UI rendering and unchanged formal scene output, so a successful UI smoke test does not silently accept a new scene reference or HDR appearance.

**Why this priority**: Features 028 and 029 already establish strict evidence ownership that this interactive surface must preserve.

**Independent Test**: Execute deterministic input/lifecycle tests, native UI fixtures and UI-disabled scene parity checks; inspect bounded reports and perform the documented hands-on navigation/control review.

**Acceptance Scenarios**:

1. **Given** formal scene validation starts with a live lab session nearby, **When** the formal run executes, **Then** UI defaults to disabled, camera/settings remain frozen, and UI/input events cannot mutate the run.
2. **Given** UI-specific screenshots and reports are produced, **When** they are inspected, **Then** they are labeled as UI smoke evidence with software, device, workload, settings and dimensions, separately from Accepted scene references.
3. **Given** changed formal SDR pixels, **When** acceptance is requested, **Then** the workload revision, exact-dimension Candidate and explicit maintainer decision are required; alignment, cropping, scaling and resampling cannot make the comparison pass.
4. **Given** automated HDR contract tests pass, **When** closeout is assessed, **Then** HDR appearance remains pending until current live macOS maintainer review is recorded; neither a numeric check nor Feature 029's one-time closeout exception substitutes for that review.

### Edge Cases

- Simultaneous movement keys, diagonal movement, key repeat, missed key-up on focus loss, and UI activation while navigation keys remain held.
- Right-mouse capture followed by widget activation, Escape, focus loss, minimization or closure; capture is always released and stale deltas are discarded.
- Empty text, multi-byte UTF-8, unsupported glyphs, invalid text bytes and unavailable clipboard access; report limitations without corrupting text or camera state. Full IME composition is outside this phase.
- Invalid field of view, non-finite exposure/speed, extreme frame delta, singular camera matrices and incompatible presets; effective state remains finite and valid.
- Non-integer display scale, moving between displays, zero drawable extent and resize while frames or UI resources remain in use. Preset import at a different drawable extent adapts projection without resizing the window; at zero drawable extent, import is deferred with a visible pending status until a valid aspect ratio is available, without partially applying the preset.
- A burst of profile changes, HDR availability loss, mode-change failure or closure during a transition; only a coherent effective output may be presented.
- Empty UI frames, clipped or off-screen geometry, invalid indices/texture identities and exceeded font/texture/draw limits; no unchecked resource use or partial submission.
- Explicit screenshot requests while minimized, during transition or after the capture queue is full; report deferred/rejected requests with a bounded policy rather than blocking navigation indefinitely.
- UI disabled in a formal run, accidental interactive preset selection, and a failed attempt to write an Accepted path from a preview operation.

## Architecture & Design Constraints *(mandatory)*

These constraints are inherited from the project constitution and the Phase 030 roadmap. They are binding integration boundaries; detailed designs, exact third-party revisions and resource budgets belong in the implementation plan.

- **AC-001 — RHI Abstraction**: Application owns camera controls, input routing, UI state and commands. Renderer owns backend-neutral UI draw snapshots and Render Graph execution through RHI. Application and Renderer MUST NOT directly call Vulkan/Metal or add a parallel backend-specific demo renderer. Existing Asset/Core dependency boundaries remain intact.
- **AC-002 — Private UI Integration**: Integrate a pinned Dear ImGui revision behind private adapters. Public engine contracts MUST NOT expose ImGui or native graphics types. UI input consumes the engine's input/event stream and MUST NOT install competing GLFW callbacks. Pin selection and dependency provenance are planning work.
- **AC-003 — Draw Ownership**: Immutable UI snapshots MUST support indexed triangles, texture identities, clip/scissor rectangles and alpha blending on Vulkan and Metal. Font/texture uploads, reuse and retirement MUST respect frame ownership; invalid or oversized snapshots fail before native submission.
- **AC-004 — Design Patterns**: Avoid God-classes; separate orthogonal camera, input, UI, output-policy and rendering responsibilities using the constitution's Strategy/Composite discipline where applicable.
- **AC-005 — Advanced Graphics**: Later effects extend this lab's controls and consume its camera-discontinuity information. Phase 030 MUST preserve compatibility with future ray tracing, meshlets and GI without implementing them or requiring a temporal history framework before Feature 031.
- **AC-006 — Naming Conventions**: Code designed from this specification MUST follow PascalCase and UnrealEngine5-style naming conventions.
- **AC-007 — Cross-Platform Compatibility**: Support Windows, macOS and Linux with platform details behind existing boundaries. Validate applicable Vulkan and Metal paths and retain deterministic headless tests; unsupported capability reports MUST be truthful and never substitute mock output for native execution.
- **AC-008 — Automated Cross-Platform Validation**: Include Windows/macOS/Linux build and headless validation plus applicable native UI/input/lifecycle coverage. Any temporary gap MUST name the uncovered behavior, manual command, evidence and follow-up task before implementation is considered complete. Physical interaction and HDR review remain distinct from hosted automation.
- **AC-009 — Color Ownership**: UI composition is display-linear, in the active output gamut, after all scene effects and before Feature 029's sole output transfer/native packing. Coordinate gamut conversion with that output transform so it occurs once. Decode standard UI colors from sRGB/Rec.709-D65; texture inputs declare their color domain; alpha is linear coverage. UI white defaults to one active output-profile reference white, represented as an explicit UI brightness setting independent of scene exposure. Blend linear colors before transfer. UI bypasses scene exposure, TAA, DOF, motion blur and bloom.
- **AC-010 — Existing Output Governance**: Preserve Feature 029's scene-linear working domain, separate SDR tone maps/HDR viewing profiles and Apple metadata rules. Metal PQ retains its declared PQ format/colorspace and EDR opt-in; PQ and EDR retain `EDRMetadata=nil`, with no `CAEDRMetadata` system tone mapping. EDR uses Renderer-owned native-reference-white packing from the same display-state generation. Vulkan HDR metadata remains capability-dependent. Windows evidence claims SDR only; macOS HDR appearance requires live human authority.
- **AC-011 — Bounded Presentation**: Interactive native presentation MUST NOT require per-frame synchronous CPU image readback. The plan MUST declare limits and overflow behavior for frames in flight, UI draws, font/texture bytes, retirement, presets and explicit capture requests before implementation; diagnostics MUST expose observed counts and high-water marks without requiring the full Feature 041 profiler. Optional presentation extensions MUST be detected at runtime and preferred when available; their absence MUST select a documented bounded compatibility path rather than reject otherwise supported lab startup. Compatibility losses MUST be confined to declared transition delays, bounded retained presentation resources and qualified terminal cleanup.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The lab MUST provide an explicit interactive mode for one native window and an existing strict-cooked production workload, identifying its package, workload revision, backend and active output. Invalid content MUST fail through the existing loading rules with no runtime source fallback.
- **FR-002**: Users MUST be able to use W/S forward/backward, A/D strafe, Q/E vertical movement, Shift acceleration and right-mouse look with the existing calibration-camera basis. Diagonal movement MUST be normalized so it does not unintentionally exceed the selected speed.
- **FR-003**: Users MUST be able to adjust finite movement speed and vertical field of view, use viewport-owned wheel field-of-view control, and reset to the workload's initial camera. Bounds and units MUST be visible and invalid values rejected without partial change.
- **FR-004**: Camera movement MUST use elapsed time, with a bounded step after pauses; equivalent scripted input duration at different frame cadences MUST produce equivalent camera state within declared numeric tolerances.
- **FR-005**: Cursor capture MUST belong only to an active viewport look gesture. Release, Escape, focus loss, minimize and close MUST release capture and discard stale input; Escape cancels capture or the active UI interaction without forcing application exit.
- **FR-006**: Camera state MUST distinguish continuous movement from cut/preset restore, reset, field-of-view change and viewport-extent change, allowing Feature 031 to invalidate histories without adding those histories in Phase 030.
- **FR-007**: The lab MUST route keyboard, mouse buttons/motion, scroll and UTF-8 text events and support basic text copy/paste through the engine input boundary, with explicit unavailable/unsupported outcomes.
- **FR-008**: UI ownership MUST be resolved before camera updates for the same input interval. UI-owned keyboard, text, mouse or scroll input MUST cause no camera change; gesture ownership persists until termination and held keys require a fresh press after ownership changes.
- **FR-009**: The lab MUST allow the UI to be shown and hidden and present visible navigation/capture instructions. Hiding UI or losing focus MUST clear its active gesture ownership without leaving stuck camera input.
- **FR-010**: UI drawing and hit testing MUST account for logical size, drawable size and display scale, including non-integer scale and display changes; clipping MUST remain inside the active drawable viewport.
- **FR-011**: A loaded-scene panel MUST show scene/workload identity, camera pose, speed, field of view, requested/effective output, manual exposure, applicable existing tone-map/viewing-profile identity, debug bypass, capability failures and bounded execution counters. Unavailable measurements MUST be labeled unavailable.
- **FR-012**: Users MUST be able to change camera settings, manual exposure, all three existing SDR tone maps, existing SDR output variants, both 1000/2000-nit PQ and EDR profiles where supported, and existing debug bypass without restarting. The UI MUST distinguish controls applicable to SDR from those applicable to HDR.
- **FR-013**: Each frame MUST consume a coherent camera/settings/output state. Valid ordinary edits take effect on the next eligible frame; changes requiring output recreation MAY span a bounded transition while their pending status is shown. Intermediate incompatible settings MUST NOT be presented.
- **FR-014**: Unsupported, invalid or failed changes MUST produce an actionable diagnostic and retain the previous valid state. Loss of that state's capability MUST visibly fall back to supported SDR, or pause with a diagnostic when no valid output remains; failure MUST NOT be reported as the requested mode succeeding.
- **FR-015**: The lab MUST accept later feature-owned controls/debug views within the same input, state and UI composition rules. No new rendering effect or second GUI/input framework is required for this phase.
- **FR-016**: Text, colored geometry, clipped indexed draws and textured widgets MUST render with valid font/texture lifetimes under AC-003 and AC-009. Unknown/stale texture references and invalid draw bounds MUST be rejected with a diagnostic.
- **FR-017**: UI brightness MUST be explicit, finite, positive, bounded and expressed relative to the active profile's reference white. Opaque UI appearance MUST be independent of scene exposure/tone mapping; alpha blending MUST preserve the scene contribution without applying scene effects to the UI.
- **FR-018**: UI-disabled output MUST preserve existing scene semantics, dimensions and probes for identical frozen inputs. A detected change MUST follow FR-027 rather than be hidden by comparison adaptation.
- **FR-019**: Users MUST be able to explicitly export/import a versioned, size-bounded local camera/settings preset containing workload identity, exported drawable extent as source context, camera pose and projection parameters including explicit vertical field of view, requested output/profile versions, exposure, complete debug selection (mode, stage, source domain and visualization range) and UI brightness, with round-trip numeric precision and a digest. Export MUST NOT overwrite an existing file without an explicit overwrite action.
- **FR-020**: Preset import MUST validate the entire record, workload compatibility, finite values and supported version before committing state. Rejection leaves effective state unchanged; an unsupported requested profile rejects the whole import and is reported without rewriting source intent. Independent loss of the current effective output follows FR-014 without applying rejected preset fields. Presets MUST NOT contain GPU resources or alter cooked content. A valid same-workload preset MUST adapt to the current non-zero drawable extent without resizing the window or requiring an extent match: restore position, orientation and vertical field of view, preserve the preset's other valid projection parameters, and rebuild projection using the current drawable aspect ratio. Exported dimensions or derived projection matrices MUST NOT override that aspect ratio. At zero drawable extent, defer the entire import with visible pending status until a valid extent is available; commit no partial state. This rule applies only to interactive presets, never to frozen formal captures.
- **FR-021**: Interactive rendering with captures disabled MUST perform zero required synchronous scene/UI image readbacks. Explicit captures MUST use a separate bounded request path, recording whether UI is included and reporting failure or deferral when no stable drawable/output is available.
- **FR-022**: Frames in flight, draw data, font/texture storage, pending captures and resource retirement MUST obey documented budgets with deterministic overflow outcomes. Completed resources MUST retire without affecting still-pending frames. Abandoned work MUST retain live owners until valid retirement or the explicitly declared terminal compatibility cleanup; timeouts MUST NOT authorize unsafe ordinary reuse.
- **FR-023**: Resize, display-scale change, UI toggle, minimize/restore, focus loss/recovery and supported output-mode changes MUST either recover valid presentation or report a terminal failure and execute bounded shutdown with truthful cleanup assurance and residual-owner reporting. Zero drawable extent MUST suspend rendering/uploads while retaining event handling.
- **FR-024**: Bounded diagnostics MUST identify effective settings, capability resolution, camera discontinuities, UI capture owner, active/peak resource counts, readback counts and lifecycle failures. Diagnostics MUST report the selected presentation-retirement mode, fallback reason and shutdown assurance. They MUST NOT grow without limit or claim full GPU profiling.
- **FR-025**: Formal scene captures MUST default to UI disabled and frozen workload settings, reject live session/preset overrides and keep their existing bounded readback/probe path available. Any explicitly UI-inclusive capture MUST be labeled separately as non-authoritative UI smoke evidence.
- **FR-026**: Interactive previews, preset exports and UI smoke evidence MUST NOT create, modify or promote Accepted scene baselines. UI evidence MUST identify the tested software revision, workload, device/backend, profile, camera/settings and exact dimensions in bounded PNG/JSON records.
- **FR-027**: Changed formal SDR output MUST require a workload revision bump, an exact-dimension Candidate, calibration/semantic checks under the inherited policy and explicit maintainer acceptance. Automatic alignment, cropping, scaling, resampling and baseline updates are prohibited.
- **FR-028**: Current macOS PQ/EDR appearance MUST receive live maintainer review with bounded context for all four 1000/2000-nit profiles. Automation MAY validate color math, metadata and lifecycle but MUST NOT score HDR appearance, fabricate attestation or reuse Features 028/029's one-time carry-forward exceptions.
- **FR-029**: Closeout MUST include deterministic input/camera/preset tests, UI draw/color/lifecycle checks, UI-disabled parity, supported native paths and maintainer hands-on navigation/control review. Both extension-enabled and forced-disabled capability branches MUST have focused coverage; an actually executed compatible fallback may satisfy a native gate with its limitations recorded. Evidence MUST distinguish current machine results, human decisions, unavailable coverage and historical references; formal authority retains the inherited frozen-software before/after guards.

### Key Entities *(include if feature involves data)*

- **Lab Session**: One loaded workload, native window, effective camera/settings, requested/pending output changes and an explicitly non-authoritative preview identity.
- **Camera State and Change Reason**: Pose, projection, movement settings and viewport context, with a distinction between continuous motion and changes that invalidate future temporal history.
- **Input Ownership**: The current keyboard/mouse/scroll/text consumer, captured gesture and focus state; determines whether camera input is permitted.
- **Rendering Settings Snapshot**: One frame's valid camera, manual exposure, tone/viewing profile, output capability generation, debug choice and UI reference-white brightness.
- **UI Draw Snapshot**: Immutable geometry, texture references and clip regions for one viewport/frame, with finite declared limits and resource lifetime ownership.
- **Lab Preset**: Versioned bounded local data for repeating a camera/settings setup; names its workload, camera position/orientation, explicit vertical field of view and other valid projection parameters. Exported drawable dimensions are source context; effective projection is derived from the current drawable aspect ratio on import. It preserves requested output intent without authority to resize the window or change Accepted references.
- **Lab Validation Record**: Bounded interaction, lifecycle, parity or UI smoke evidence with software/device/settings identity, separate from formal SDR acceptance and live HDR review.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: For both existing production workloads, a maintainer can complete navigation, right-mouse look, speed/FOV adjustment, reset, exposure change and a supported output-mode change in one session without restart, with every action receiving a visible result or explicit unsupported reason.
- **SC-002**: All scripted keyboard, text, mouse-drag, scroll, copy/paste and focus-transfer cases produce zero unintended camera-state changes while the UI owns input; equivalent navigation sequences at 30, 60 and 120 updates per second agree within the documented camera tolerance.
- **SC-003**: Every valid ordinary settings edit appears by the next eligible rendered frame; every unsupported/invalid transition retains a coherent valid state or explicitly pauses. No scenario silently reports a requested profile as effective after failure.
- **SC-004**: UI text, clipping, hit targets and translucent fixtures pass checks at 100%, 150% and 200% scale. Fixed opaque UI contributions remain unchanged across at least -3, 0 and +3 stops of scene exposure within the declared color tolerance; each supported HDR profile receives a recorded live readability/brightness decision.
- **SC-005**: Each required native workload/profile combination completes capture-disabled functional checks; the four fixed representative combinations in the validation contract complete at least 1,000 measured presented frames after 120 warmup frames, and remaining combinations complete at least 120 presented frames. Representative workload/backend pairs perform at least 20 cycles per applicable lifecycle class and supported mode sequence; remaining workload/backend pairs receive at least one integration cycle as specified in that contract. There are zero required synchronous image readbacks, crashes, stuck-input cases or resource-budget violations; after draining work, transient resources return to the mode-specific documented quiescent state. A fallback may retain bounded presentation owners until reacquisition or qualified terminal cleanup; successful compatibility cleanup MUST NOT be labeled proven presentation retirement, and forced termination remains failure.
- **SC-006**: Preset serialization round-trips all declared fields. Same-extent restoration reproduces the valid camera/settings; restoration at different extents and aspect ratios preserves camera position, orientation and vertical field of view within the declared numeric tolerance, uses the current drawable aspect ratio for projection, and changes zero window dimensions. All malformed, oversized, wrong-workload, unknown-version and non-finite test records are rejected without partial state mutation. Preview/preset operations change zero Accepted files.
- **SC-007**: UI-disabled formal captures pass the inherited exact-dimension scene/probe policy for both workloads on each required SDR lane, or the phase remains unaccepted until a revisioned Candidate is explicitly accepted. UI smoke output never substitutes for this result.
- **SC-008**: Windows, macOS and Linux automated coverage has explicit pass/fail results for applicable tests; every required hands-on interaction and macOS HDR review has a current decision or a clearly open gap. No historical exception or unavailable lane is labeled as a current pass.

## Assumptions

- **Roadmap authority**: [Roadmap 3.1.0 Phase 030](../../doc/roadmap.md#phase-030--application-interactive-rendering-lab--imgui-integration) and the [phase index](../002-engine-development-roadmap/phase-index.json) identify this as the next Application phase. Its direct dependencies are **004, 008, 013, 015, 016, 017, 018, 019, 027, 028 and 029**. Completed 003-029 identities and evidence remain historical authority under the recorded [migration](../002-engine-development-roadmap/migration-3.1.md).
- **Users and content**: The initial users are engine developers and maintainers. Lantern and Sponza use their existing strict-cooked roots and camera presets. A session opens one workload selected at startup; scene inspection means showing and controlling the loaded scene's rendering state. In-session scene switching, asset editing and authoring are not required.
- **Settings defaults**: Initial camera/output defaults and valid exposure/profile ranges come from the selected workload and Feature 029. UI white defaults to the active profile's reference white. UI is enabled in interactive mode and disabled in formal mode. Preset persistence is explicit export/import; restore adapts projection to the current drawable aspect ratio while retaining pose and vertical field of view. Exported dimensions are context, not a window-size request or a compatibility gate. Automatic preferences, autosave and a preset database are not required.
- **Compatibility scope**: The shared UI/output contract applies to existing Forward and Deferred paths; production-content acceptance uses the existing Deferred workloads and a bounded existing fixture covers Forward integration. Native SDR evidence covers physical Windows Vulkan and macOS Metal; supported hosted/Linux native checks complement those lanes without claiming physical-display authority. Windows HDR validation is outside scope; macOS Metal PQ/EDR human review is required.
- **Planning defaults**: Numeric tolerances, camera control bounds, resource budgets, transition/capture deadlines and test commands will be frozen in the plan/contracts before implementation and before collecting evidence. SC-005 retains 1,000-frame/20-cycle stress on a fixed risk-selected subset, with short functional coverage of every required workload/profile combination; this phase makes no universal frame-rate or full profiling claim.
- **Delivery slices**: M0 delivers reusable camera/input arbitration and presentation without mandatory per-frame readback; M1 adds private pinned UI integration, draw/font/texture lifecycle and scale handling; M2 adds live settings and SDR/PQ/EDR composition/recovery; M3 closes automated parity/lifecycle checks and current hands-on review. These milestones refine scope rather than assert completed work.
- **Explicit exclusions**: No full editor/world authoring, material/node editor, native widget toolkit, docking, multiple windows/viewports, general IME/accessibility framework, full GPU profiler, automatic exposure, new rendering algorithms, temporal histories, Virtual Texturing foundation or texture streaming. Later 031-040 features reuse this lab; full profiling belongs to 041.
- **Evidence continuity**: Features 028/029's implementation and ordinary evidence rules are reused. Their one-time closeout exceptions are not future permissions. HDR mathematical correctness is separate from physical appearance; live review cannot be replaced by automatic image scoring.
