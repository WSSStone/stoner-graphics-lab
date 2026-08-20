# Feature Specification: Native Metal Backend

**Feature Branch**: `027-metal-backend`
**Created**: 2026-08-18
**Status**: Draft
**Input**: User description: "为 roadmap phase 27 metal backend 制定 spec"

## Clarifications

### Session 2026-08-18

- Q: Metal 的 cooked shader payload 应从哪条权威路径生成？ → A: 保留现有 GLSL/SPIR-V 权威链；离线 cook 从已验证 SPIR-V 生成 Metal 代码并编译为目标平台原生 Metal shader library。
- Q: Feature 027 完成时，Metal backend 必须覆盖哪一层 RHI 能力？ → A: 覆盖全部当前公开且语义适用于 Metal 的 Feature 007/008 RHI 契约；不得仅因当前 demo 未使用而标记 unsupported。
- Q: 谁应创建、持有并销毁附着到窗口视图的 CAMetalLayer？ → A: Metal Backend presentation context 独占 CAMetalLayer；Application 独占窗口/视图，Backend 在借用窗口有效期内负责 attach/detach。
- Q: Feature 027 是否必须把 Apple Silicon 与 Intel Metal Mac 都作为正式支持目标？ → A: 是；最低 deployment target 范围内的 Apple Silicon 与 Intel Metal Mac 均为正式目标，并分别需要可获得的 native validation。
- Q: Windows/Linux 上的 Asset Cooker 对 Metal target 应提供到什么程度？ → A: 三平台均生成并验证从 SPIR-V 派生的规范化 Metal 源码与确定性证据；仅 macOS 可编译原生 Metal library 并发布有效 Metal cooked generation。

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Execute Existing RHI Workloads on Metal (Priority: P1)

An engine developer selects the native Metal backend on a supported macOS
machine and runs existing Renderer workloads through the same RHI-facing
contracts used by other backends. Device, resource, descriptor, command,
synchronization, render-pass, and pipeline operations execute on a real Metal
device without Renderer or Application code calling Metal directly.

**Why this priority**: A real RHI realization is the core value of the feature
and the proof that the engine abstraction is portable beyond Vulkan.

**Independent Test**: Run the complete backend conformance suite for every
currently public RHI operation whose semantics apply to Metal, including native
GPU readback for graphics, compute, transfer, and synchronization outcomes.

**Acceptance Scenarios**:

1. **Given** a supported macOS device, **When** native Metal is selected and a
   valid offscreen RHI workload is submitted, **Then** it completes on a real
   Metal device and returns the expected readback result.
2. **Given** valid RHI resources and bindings, **When** graphics and transfer
   commands execute, **Then** resource contents, state transitions, and
   completion visibility match the declared RHI behavior.
3. **Given** an unsupported RHI capability or resource combination, **When** it
   is requested, **Then** the backend rejects it before unsafe execution and
   reports the unsupported capability deterministically.
4. **Given** a stale, foreign, already-destroyed, or incompatible RHI object,
   **When** it is used by a Metal operation, **Then** the operation fails without
   accessing invalid native state or corrupting subsequent work.

---

### User Story 2 - Present and Recover a Visible macOS Window (Priority: P1)

A developer runs the existing desktop demo with the Metal backend and sees
native rendered output in the Application-owned macOS window. Presentation
continues correctly through ordinary resize, minimize, restore, focus, and
close behavior without leaking platform details into Application or Renderer.

**Why this priority**: Visible presentation and lifecycle recovery distinguish
a usable desktop backend from an offscreen-only proof.

**Independent Test**: Run a bounded visible Metal session, exercise repeated
resize/minimize/restore cycles, capture an accepted frame, and verify clean
shutdown with no validation or lifecycle errors.

**Acceptance Scenarios**:

1. **Given** a valid Application window and selected Metal backend, **When** the
   demo starts, **Then** the Metal presentation context creates and owns its
   native layer, attaches it to the borrowed Application-owned window view, and
   does not transfer window or view ownership to Backend.
2. **Given** an available drawable, **When** a frame is rendered and presented,
   **Then** the visible image is complete, correctly oriented, and corresponds
   to the submitted frame.
3. **Given** a resize or drawable-size change, **When** the next renderable frame
   begins, **Then** presentation resources reflect the new drawable extent and
   stale resources are not submitted.
4. **Given** a minimized, zero-sized, occluded, or temporarily drawable-less
   window, **When** the loop advances, **Then** rendering pauses or skips
   presentation without busy failure and resumes after a drawable returns.
5. **Given** normal close or backend shutdown, **When** outstanding work is
   drained, **Then** Backend detaches and releases its native layer before the
   borrowed window/view becomes invalid, all remaining device-owned objects are
   released in a valid order, and the process exits successfully.

---

### User Story 3 - Cook and Consume Asset-Backed Metal Library Payloads (Priority: P1)

A content or engine developer uses the existing authoritative GLSL/SPIR-V
shader chain associated with stable Shader Asset identities. The cooker derives
Metal code from validated SPIR-V, compiles and publishes a target-compatible
native Metal shader library, and the runtime consumes only that cooked library
without runtime source compilation or backend-specific Asset ownership.

**Why this priority**: Native pipelines are not production-usable until the
existing Material/Shader Asset and cooked-generation contracts can supply them
deterministically.

**Independent Test**: Cook representative triangle and deferred shaders for a
Metal target, load them through strict cooked mode, create native pipelines,
and prove that missing, altered, or wrongly tagged payloads fail closed.

**Acceptance Scenarios**:

1. **Given** one Shader Asset with validated authoritative SPIR-V, **When** a
   Metal derivation runs on any supported host, **Then** it produces identical
   normalized Metal source and transformation evidence; **When** the final cook
   runs on macOS, **Then** the published generation additionally records the
   native Metal shader library plus complete compiler, target, and version
   dependency evidence.
2. **Given** a valid strict-cooked generation, **When** the Metal backend creates
   a pipeline, **Then** it consumes the payload selected through existing Asset
   and Renderer contracts without reading repository shader source directly.
3. **Given** a payload with the wrong backend, target profile, stage, entry
   point, interface, or version evidence, **When** pipeline creation is
   attempted, **Then** creation fails before command submission with actionable
   diagnostics.
4. **Given** a valid Asset handle after pipeline creation, **When** Asset-side
   references are released, **Then** Renderer and Backend retain only their own
   correctly scoped RHI/native lifetime; Asset does not own a GPU object.
5. **Given** a Windows or Linux host, **When** final Metal generation publication
   is requested, **Then** the cooker reports that native compilation requires a
   macOS host and does not publish source-only output as a valid Metal payload.

---

### User Story 4 - Run Backend-Neutral Triangle and Deferred Validation (Priority: P2)

An engine developer runs the existing triangle and deferred demonstrations with
Metal selected without changing scene, material, frame-planning, or render-graph
logic. The same demonstrations remain runnable through Vulkan/MoltenVK so that
backend differences can be detected instead of hidden in duplicated demo code.

**Why this priority**: Reusing non-trivial workloads proves architectural
portability and protects the established Vulkan path from regression.

**Independent Test**: Execute the shared triangle and deferred compositions on
Metal, compare normalized GPU readbacks and accepted visible captures against
their backend-independent expectations, then rerun the Vulkan path.

**Acceptance Scenarios**:

1. **Given** a backend-neutral triangle composition, **When** Metal is selected,
   **Then** it renders through the existing RHI path with no Metal branch in
   Renderer or Application frame logic.
2. **Given** the existing deferred composition, **When** Metal is selected,
   **Then** GBuffer creation, world-space normal storage, depth policy, lighting,
   and final output satisfy the same semantic probes used by the Renderer.
3. **Given** equivalent supported workloads on Metal and Vulkan/MoltenVK,
   **When** normalized outputs are compared, **Then** differences remain within
   declared semantic and image tolerances and are attributed to a backend.
4. **Given** a machine where both backends are available, **When** one is chosen
   explicitly, **Then** backend selection is observable and the other backend
   remains available; a failed native initialization is not reported as success
   from a silently substituted backend.

---

### User Story 5 - Diagnose Failures and Preserve Cross-Platform Builds (Priority: P3)

A developer receives bounded, stable diagnostics for Metal capability,
lifecycle, shader, resource, submission, and presentation failures. Developers
on Windows and Linux can still build and test the project without Apple SDKs or
Metal runtime availability.

**Why this priority**: A platform backend must fail transparently on its host
and remain isolated everywhere else to be maintainable in a multi-API engine.

**Independent Test**: Inject failures at each native lifecycle boundary, repeat
normalized diagnostic runs, and build/test the complete non-Metal project on
Windows and Linux with no Metal SDK present.

**Acceptance Scenarios**:

1. **Given** an injected failure during device, resource, pipeline, command,
   synchronization, or presentation setup, **When** initialization unwinds,
   **Then** no partially initialized object becomes usable and all acquired
   ownership is released exactly once.
2. **Given** a command or presentation failure after startup, **When** it is
   reported, **Then** diagnostics identify the operation, stable object/context
   identity, result category, and recovery or terminal state without exposing
   unstable native pointer values.
3. **Given** equivalent deterministic failure inputs, **When** validation is
   repeated, **Then** normalized diagnostics and terminal states are identical.
4. **Given** Windows or Linux without Apple frameworks, **When** the repository
   is built and tested, **Then** Metal implementation units are excluded while
   public backend selection and unsupported-result behavior remain compilable.

### Edge Cases

- No compatible Metal device is present, or the selected device lacks a required
  format, sample count, binding limit, synchronization behavior, or presentation
  capability.
- Device discovery reports multiple devices or changes ordering between runs.
- Resource size, row alignment, texture subresource range, attachment format,
  storage visibility, or binding index exceeds a reported capability.
- A resource is destroyed while referenced by recorded or in-flight commands,
  or backend shutdown begins while work remains outstanding.
- A synchronization wait times out, completion arrives after cancellation, or
  the native device reports loss or removal.
- Shader cooking receives malformed or unsupported SPIR-V, or pipeline creation
  receives an invalid native Metal library, absent entry point, mismatched
  stages, incompatible attachment formats, or stale Shader Asset evidence.
- A cooked generation contains a valid Shader Asset but omits its Metal payload
  or references content outside the generation.
- A non-macOS host successfully derives normalized Metal source but is asked to
  publish it as a final native Metal payload without Apple compiler evidence.
- The window is resized repeatedly while frames are in flight, changes display
  scale, becomes zero-sized, or cannot provide a drawable temporarily.
- The Application window begins closing while a Metal layer remains attached or
  presentation completion is still pending.
- Presentation attachment is attempted with a headless, foreign, stale, or
  already-destroyed platform-window handle.
- Coordinate handedness, winding, viewport origin, clip-space depth, sRGB
  conversion, or world-space normal interpretation differs between backends.
- Readback memory has backend-specific row padding or completion visibility.
- Metal is unavailable on Windows/Linux, while shared RHI and Renderer tests
  still instantiate deterministic/mock or Vulkan paths.

## Architecture & Design Constraints *(mandatory)*

- **Dependency Direction**: Metal MUST be implemented as `Backend -> RHI +
  Core`. Renderer and Application MUST NOT call Metal directly, and Metal MUST
  NOT become a dependency of Asset.
- **Asset Boundary**: Asset owns CPU payload identity, metadata, dependencies,
  import/cook/load contracts, and immutable shader bytes. Renderer owns RHI
  realization; Backend owns native Metal objects. No GPU handle or Metal header
  may enter public Asset contracts.
- **Platform Boundary**: Apple window/layer, device, framework, and runtime
  integration MUST remain within platform or Backend implementation boundaries.
  Public RHI and Application headers MUST remain consumable without Apple SDKs.
  Application owns the platform window/view; Metal Backend owns its attached
  native layer and may only borrow the valid platform-window handle.
- **Existing Contracts First**: The feature MUST realize every currently public
  Feature 007/008 RHI lifecycle, result, capability, resource, descriptor,
  command, queue, synchronization, render-pass, framebuffer, shader, and
  graphics/compute pipeline semantic that applies to Metal. Necessary RHI
  corrections MUST be backend-neutral and validated against existing
  implementations.
- **Responsibility Separation**: Device discovery, capability mapping, resource
  ownership, binding translation, command encoding, synchronization, pipeline
  creation, presentation, diagnostics, and validation composition MUST remain
  separable responsibilities; no backend god-class or giant orchestration
  function is permitted.
- **Backend Coexistence**: Native Metal and Vulkan/MoltenVK MUST remain explicit,
  independently diagnosable choices. Shared Demo/Renderer behavior MUST not be
  forked into backend-specific scene or rendering algorithms.
- **Advanced Graphics Readiness**: Capability and pipeline contracts MUST not
  preclude later meshlets, ray tracing, global illumination, or residency work,
  but this feature MUST NOT implement those capabilities.
- **Naming Conventions**: New engine-facing C++ contracts MUST follow PascalCase
  and Unreal Engine-style naming conventions.
- **Cross-Platform Compatibility**: macOS MUST execute native Metal validation;
  both Apple Silicon and Intel Metal Macs within the supported deployment range
  are first-class targets. Windows and Linux MUST compile and run all applicable
  shared, deterministic, non-Metal, and cross-platform shader-derivation
  validation without Apple SDK assumptions; only macOS may finalize and publish
  a native Metal shader library.
- **Automated Validation**: Cross-platform CI MUST cover shared contract and
  build isolation. Real visible rendering evidence is required on supported
  macOS hardware; hosted environments lacking display or suitable hardware MUST
  document and run the strongest available native offscreen gate plus a manual
  visible acceptance gate.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST expose native Metal as an explicit backend choice
  on supported macOS systems without changing Renderer-facing RHI contracts.
- **FR-002**: Selecting Metal MUST create and own a real native device and MUST
  report the selected adapter identity and capability summary deterministically.
- **FR-003**: Device selection MUST be deterministic for an unchanged set of
  available devices and explicit user selection MUST take precedence over the
  default policy.
- **FR-004**: Initialization MUST reject absent or insufficient Metal capability
  before publishing a usable RHI device.
- **FR-005**: The Metal backend MUST implement every currently public Feature
  007/008 RHI operation whose semantics apply to Metal, including device,
  capability, swapchain/presentation, resource, descriptor/binding, shader,
  graphics and compute pipeline, render-pass/framebuffer, command,
  queue/submission, and synchronization contracts. The pre-implementation
  operation inventory and classification rules in
  `contracts/rhi-operation-matrix.md` are the frozen baseline; any later public
  RHI addition MUST be classified before Feature 027 closeout.
- **FR-006**: An RHI operation MUST NOT remain unsupported solely because the
  triangle or deferred validation workload does not currently exercise it; an
  unsupported result is valid only when the Metal platform or selected device
  cannot provide the declared backend-neutral semantic.
- **FR-007**: Every Metal-backed RHI object MUST have explicit lifecycle state,
  device ownership, compatibility validation, and deterministic invalidation.
- **FR-008**: Backend operations MUST reject null, stale, foreign-device,
  incompatible, or destroyed objects before native use.
- **FR-009**: Buffer creation, upload, GPU use, and readback MUST preserve the
  requested byte range and required visibility semantics, including native
  alignment and coherency constraints.
- **FR-010**: Texture creation, upload, attachment use, sampling, and readback
  MUST preserve format, extent, mip/subresource, color-space, usage, and row
  layout semantics for every format required by the target workloads.
- **FR-011**: Descriptor and resource-binding realization MUST validate stage
  visibility, binding identity, resource type, array/count limits, dynamic
  ranges where supported, and pipeline-layout compatibility. Asset-owned cooked
  shader evidence MUST carry the canonical binding map generated by Tools;
  Renderer MUST copy that map into backend-neutral immutable RHI metadata, and
  Metal MUST validate and consume that metadata rather than recomputing slots.
- **FR-012**: Graphics pipeline creation MUST validate shader stages and
  interfaces, vertex input, raster/depth/blend state, attachment formats, sample
  count, and binding layout before producing a usable pipeline.
- **FR-013**: Compute pipeline creation MUST validate shader stage, entry point,
  declared interface, binding layout, and reported dispatch limits before
  producing a usable pipeline.
- **FR-014**: Command recording MUST enforce valid begin/end, render scope,
  resource binding, draw, copy, barrier/dependency, and completion states.
- **FR-015**: Submission and synchronization MUST provide observable ordering
  and completion equivalent to the RHI contract for resource reuse, frame
  overlap, readback, and shutdown.
- **FR-016**: Resources referenced by recorded or in-flight work MUST remain
  valid until that work is complete, and destruction MUST NOT race native use.
- **FR-017**: The backend MUST report native capability limits through the
  existing backend-neutral RHI capability vocabulary and MUST explicitly report
  unsupported semantics rather than approximating them silently.
- **FR-018**: Application MUST retain exclusive ownership of the macOS window
  and its view. The Metal presentation context MUST create and exclusively own
  its CAMetalLayer, borrow the valid platform-window handle, attach the layer for
  presentation, and detach/release it before the window or view becomes invalid.
- **FR-019**: Presentation MUST track logical and drawable size, display scale,
  attachment formats, and in-flight frame state.
- **FR-020**: Resize, minimize, restore, temporary drawable absence, and close
  MUST produce deterministic pause, recreation, recovery, or shutdown behavior
  without submitting stale presentation resources.
- **FR-021**: Each presented frame MUST use a drawable acquired for that frame,
  submit complete rendering before presentation, and release frame-scoped
  ownership after completion.
- **FR-022**: The feature MUST provide bounded visible Metal validation on real
  supported macOS hardware, including repeated resize/minimize/restore and clean
  close behavior.
- **FR-023**: The existing repository-owned GLSL/SPIR-V chain MUST remain the
  authoritative shader input for Feature 027; Metal payloads are derived cooked
  data and MUST preserve the same stable Shader Asset identity.
- **FR-024**: For a declared macOS Metal target, the cooker MUST transform
  validated SPIR-V into canonical normalized Metal-compatible source and
  deterministic derivation evidence identically on Windows, macOS, and Linux.
- **FR-025**: On macOS, the cooker MUST compile the normalized Metal-compatible
  source offline into a target-qualified native Metal shader library and include
  the native compiler evidence before publishing a valid Metal generation.
- **FR-026**: Windows and Linux MUST NOT publish normalized Metal source as a
  valid final Metal payload; a final Metal cook request on those hosts MUST fail
  explicitly unless a future separately specified trusted macOS compilation
  facility is configured.
- **FR-027**: The cooked payload evidence MUST include the authoritative
  GLSL/SPIR-V versions, transformation and compiler versions, settings, target
  profile, entry points, interfaces, canonical binding-map entries and digest,
  and resulting native-library digest.
- **FR-028**: Cooking MUST fail when authoritative input or transformation
  evidence is absent, ambiguous, stale, unsupported, or incompatible; strict
  cooked runtime use MUST select the native Metal library through existing Asset
  Manager and Renderer contracts and MUST NOT compile shader source implicitly.
- **FR-029**: Pipeline creation MUST validate native Metal library target,
  stage, entry point, declared interface, and Asset version before native use.
- **FR-030**: Shader payload selection, Asset loading, Renderer RHI realization,
  and Backend native ownership MUST remain distinct lifetime responsibilities.
- **FR-031**: The existing backend-neutral triangle composition MUST execute
  through Metal without Metal-specific scene, material, or frame-planning code.
- **FR-032**: The existing backend-neutral deferred composition MUST execute
  through Metal with semantically equivalent GBuffer, world-space normal, depth,
  lighting, and final-output probes.
- **FR-033**: Metal validation MUST use real GPU-produced readback or presentation
  evidence for native gates; deterministic or semantic-oracle output MUST NOT be
  presented as native execution evidence.
- **FR-034**: The system MUST preserve Vulkan/MoltenVK as an available explicit
  backend path and MUST rerun applicable regression validation when shared RHI,
  Renderer, Application, Asset, Demo, or build contracts change.
- **FR-035**: A failed explicit Metal request MUST remain observable as a Metal
  failure and MUST NOT be reported as successful Metal execution after silent
  fallback to another backend.
- **FR-036**: Comparable Metal and Vulkan outputs MUST use backend-neutral
  semantic expectations and declared tolerances for orientation, color, depth,
  normal, and image differences.
- **FR-037**: Native failure injection MUST cover device initialization,
  allocation, shader/pipeline creation, command recording/submission,
  synchronization, drawable acquisition/presentation, and shutdown boundaries.
- **FR-038**: Partial initialization and injected failures MUST unwind all
  acquired native ownership exactly once and MUST not publish a usable partial
  object.
- **FR-039**: Diagnostics MUST identify backend, operation, stable object or
  frame context, result category, capability or lifecycle reason, and recovery
  state without unstable native addresses.
- **FR-040**: Normalized diagnostics for equivalent deterministic inputs MUST be
  stable across repeated runs.
- **FR-041**: Metal implementation units and Apple framework linkage MUST be
  excluded on unsupported platforms while shared public headers and backend
  selection remain compilable.
- **FR-042**: Validation MUST cover native Metal execution on both Apple Silicon
  and Intel Metal Macs within the supported deployment range. When automation
  for either architecture is temporarily unavailable, the gap, manual command,
  evidence owner, and follow-up gate MUST be documented, and Feature 027 MUST
  remain incomplete until the required automated hardware lane passes.
- **FR-043**: Automated validation MUST include macOS native offscreen Metal,
  Windows/macOS/Linux shared Debug and strict Release coverage, and applicable
  sanitizer validation for shared ownership and lifecycle code. A required
  GPU-capable macOS hardware lane for each supported CPU architecture MUST pass
  native offscreen gates before closeout; an `unavailable` probe from a standard
  hosted build job is diagnostic evidence and MUST NOT satisfy this requirement.
- **FR-044**: Validation evidence MUST distinguish deterministic/mock,
  native-offscreen, visible-manual, and cross-backend comparison tiers and MUST
  record backend/device/capability and shader-payload version evidence.
- **FR-045**: The feature MUST NOT add iOS application lifecycle, Metal mesh
  shaders, Metal ray tracing, new Asset ownership of GPU state, or a new
  backend-specific rendering algorithm.

### Key Entities

- **Metal Backend Device**: The native realization of one RHI device, including
  stable adapter identity, capability summary, lifecycle state, and ownership of
  backend-created objects.
- **Metal Resource Record**: Device-owned native state for an RHI buffer,
  texture, sampler, binding object, framebuffer attachment, or staging/readback
  allocation, with compatibility and in-flight lifetime evidence.
- **Metal Command Context**: Recording and submission state that translates RHI
  command intent into ordered native work and tracks completion dependencies.
- **Metal Pipeline Record**: A validated native graphics or compute pipeline
  associated with attachment, vertex, binding, state, and Shader Asset evidence.
- **Metal Presentation Context**: The non-owning bridge from an
  Application-owned macOS window to an exclusively Backend-owned attached
  native layer, drawable acquisition, size/lifecycle state, frame ownership,
  and presentation completion.
- **Metal Shader Payload**: An immutable target-qualified native Metal shader
  library derived offline from authoritative SPIR-V, with entry point/interface
  and complete derivation evidence retained by the existing Shader Asset and
  cooked generation, without GPU ownership.
- **Metal Validation Record**: Normalized evidence identifying validation tier,
  backend/device capabilities, workload, shader versions, results, tolerances,
  failure state, and captured artifact references.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: On supported macOS hardware, 100% of currently public Feature
  007/008 RHI operations whose semantics apply to Metal pass backend conformance
  validation; native operations produce expected GPU evidence, and genuine
  hardware limitations match reported capabilities and deterministic results.
- **SC-002**: A visible Metal demo runs for at least 3,000 frames and completes
  at least 20 resize/minimize/restore cycles with correct output, no unrecovered
  presentation failure, and exit code 0.
- **SC-003**: Triangle and deferred Metal probes pass their backend-neutral
  semantic checks, and normalized Metal/Vulkan comparisons remain within every
  declared tolerance for all accepted reference scenes.
- **SC-004**: Twenty repeated shader derivations on each supported host produce
  identical normalized Metal source and derivation-evidence digests. Twenty
  repeated final cooks under one exact tuple of CPU architecture, deployment
  target, Xcode build, SDK, Metal compiler, profile, and inputs produce identical
  metallib byte digests, DDC keys, evidence digests, pipeline outcomes, and
  normalized reports. Results from different tuples compare derivation identity
  only and are never required to have byte-identical native libraries or DDC
  keys.
- **SC-005**: Every required native failure-injection point reaches the expected
  terminal state, publishes no partial object, and returns all tracked native
  ownership and in-flight work counts to zero after shutdown.
- **SC-006**: A 10,000-iteration bounded resource/pipeline/command lifecycle
  stress run completes with no invalid access, ownership imbalance, or sanitizer
  finding. Iterations 1-1,000 are warm-up; RSS is sampled every 100 iterations
  from 1,100 through 10,000. The median of the final ten samples MUST exceed the
  median of the first ten post-warm-up samples by no more than the greater of
  16 MiB or 5% of the first median. The report MUST retain every sample and both
  medians so the threshold is reproducible.
- **SC-007**: Native conformance and lifecycle validation passes on at least one
  Apple Silicon Mac and one Intel Metal Mac within the supported deployment
  range; evidence records architecture, OS, device, and capability data.
- **SC-008**: Windows, macOS, and Linux automated Debug and strict Release gates
  all pass; Windows/Linux require no Apple SDK or framework, and applicable
  shared sanitizer gates report zero findings.
- **SC-009**: 100% of accepted macOS native evidence identifies real Metal
  execution, selected device/capabilities, Shader Asset payload version, and
  validation tier; no deterministic or semantic-oracle result is counted as a
  native pass.
- **SC-010**: Existing Vulkan/MoltenVK triangle, deferred, Asset, and native
  regression gates affected by shared changes continue to pass with no
  backend-specific Renderer/Application fork.

## Assumptions

- Feature 027 targets macOS desktop on both Apple Silicon and Intel Metal Macs
  supported by the selected minimum deployment target; iOS, iPadOS, and their
  application lifecycle are outside this phase.
- The backend is selected explicitly for validation. Vulkan/MoltenVK remains a
  supported alternative, but silent fallback does not satisfy a Metal gate.
- Repository-owned GLSL/SPIR-V remains authoritative. Offline cooking derives
  Metal-compatible code from validated SPIR-V and compiles a target-qualified
  native Metal shader library; production runtime paths consume that library
  and do not depend on runtime source compilation.
- Windows, macOS, and Linux may derive and validate canonical normalized Metal
  source. Final native-library compilation and valid Metal generation
  publication require a macOS host with the selected Apple toolchain; remote
  compilation infrastructure is outside Feature 027.
- Existing RHI contracts are the authority. Backend-neutral corrections are
  allowed when Metal exposes a missing semantic, provided all implementations,
  callers, tests, and documentation are updated together.
- Existing Application window ownership and event/lifecycle contracts remain
  authoritative. Backend receives only a borrowed platform-window bridge,
  creates and owns the attached CAMetalLayer, and must detach/release that layer
  before the borrowed window/view becomes invalid.
- Feature 027 covers every currently public Feature 007/008 RHI operation whose
  semantics apply to Metal. Triangle and deferred are integration workloads,
  not the boundary of backend completeness; only genuine platform/device
  limitations may produce an unsupported result.
- Standard hosted macOS CI proves compilation, cooking, and device-probe
  behavior but may lack usable Metal hardware or reliable visible desktop
  interaction. Required native offscreen evidence therefore comes from explicit
  GPU-capable macOS hardware lanes for arm64 and x86_64; visible evidence remains
  a manual hardware acceptance gate when automation cannot drive the desktop
  lifecycle honestly.
- Feature 028 will validate artist-authored production content across Vulkan and
  Metal; Feature 027 uses bounded repository-owned scenes/assets to establish
  backend correctness first.

## Out of Scope

- iOS/iPadOS application lifecycle, touch input, mobile presentation policy, or
  App Store packaging.
- Metal mesh shaders, ray tracing, global illumination, meshlets, streaming,
  virtual geometry, or residency policy.
- New source model/image formats, production-content acceptance, editor shader
  compilation, shader graph authoring, or runtime arbitrary source compilation.
- Moving GPU object ownership, native Metal state, or RHI residency into Asset.
- Replacing Vulkan/MoltenVK or introducing backend-specific Renderer scene and
  material algorithms.
