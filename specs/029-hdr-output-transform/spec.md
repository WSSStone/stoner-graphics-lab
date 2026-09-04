# Feature Specification: Renderer HDR Post-Processing & Output Transform

**Feature Branch**: `029-hdr-output-transform`

**Created**: 2026-09-01

**Status**: Draft

**Input**: User description: "Define Roadmap Feature 029 as the backend-neutral
HDR SceneColor-to-display pipeline shared by Forward and Deferred, with explicit
pre-tonemap and post-tonemap insertion points, manual exposure, versioned tone
mapping, explicit sRGB/output transfer, Render Graph integration, Vulkan and
Metal native presentation/readback, resize handling, and diagnostic bypass.
Preserve Feature 028 v2 as historical evidence; require revisioned, exact-
dimension, maintainer-accepted Candidates for changed SDR output and explicit
macOS live-view maintainer attestations for HDR visual output. Do not implement
anti-aliasing, bloom, depth of field, motion blur, automatic exposure, vendor
upscalers, or a post-processing editor."

## Clarifications

### Session 2026-09-01

- Q: Feature 029 首个正式、并用于生成 v3 图像基线的 tone-map 版本，应冻结为哪种算法？ → A: 同时实现 `Sdr.KhronosPbrNeutral.v1`、明确标注为近似拟合且不声称 Academy ACES 一致性的 `Sdr.NarkowiczAcesFit.v1`，以及 `Sdr.ExtendedReinhardRec709.v1`；`Sdr.KhronosPbrNeutral.v1` 为默认值。
- Q: Feature 029 应正式支持哪组显示输出设备？ → A: 实现完整 Unreal 式矩阵：SDR sRGB、Rec.709 和显式 gamma，以及 1000/2000-nit ST-2084/PQ 和 1000/2000-nit scRGB/EDR HDR 输出，包括相应 HDR swapchain。
- Q: 三种已选 tone-map 曲线与四种 HDR output-device profile 应如何组合？ → A: 三种曲线仅用于 SDR；HDR 使用独立、版本化的 ACES-style HDR viewing transform，再进入目标 1000/2000-nit PQ 或 scRGB/EDR ODT。
- Q: Feature 029 应冻结哪一种 scene-referred linear working color space，作为 HDR SceneColor 和所有 output-device transform 的输入？ → A: 使用 linear Rec.709/sRGB primaries、D65、RGBA16F；所有 ODT 显式转换到目标显示色域。
- Q: Feature 029 的完成门槛应如何验证 1000/2000-nit HDR profile，特别是在实体显示器未必能实际达到声明峰值的情况下？ → A: Windows 不承担 HDR 验证；macOS Metal 对 PQ 和 EDR/scRGB HDR 输出进行维护者肉眼验收，不使用自动视觉判定。自动化仅验证非视觉的变换、格式、metadata、提交和 readback 合同。
- Q: HDR metadata 与 engine-owned viewing transform 应如何按平台治理 Apple 的 display adaptation？ → A: `hdr10-static` 保留为 Renderer/profile 的内容意图。Vulkan 可在能力存在时通过 `VK_EXT_hdr_metadata` 应用；Metal 不把它映射为 `CAEDRMetadata`。Metal PQ 使用 `BGR10A2Unorm`、ITU-R 2100 PQ、`wantsExtendedDynamicRangeContent=true` 和 `EDRMetadata=nil`，允许声明 PQ colorspace 后的 Core Animation 色彩管理，但不启用系统 tone mapping。Metal EDR 同样保持 `EDRMetadata=nil`，由 Renderer 按同代 native reference white/headroom 完成 linear EDR packing。该平台分流必须进入 resolved state、诊断和人工验收上下文。

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Produce One Formal Display Output (Priority: P1)

As a renderer integrator, I can submit either a Forward or Deferred RGBA16F,
scene-referred linear Rec.709/sRGB-D65 HDR SceneColor and receive one declared
display-ready output through the same backend-neutral policy, so the engine has
a single authoritative place where scene-referred color becomes presented
color.

**Why this priority**: Every other post-processing, presentation, validation,
and future anti-aliasing capability depends on an unambiguous formal output
path. A path that exists only for one renderer or one graphics backend would
not establish the required engine contract.

**Independent Test**: Render equivalent bounded reference scenes through
Forward and Deferred, inspect their declared frame stages, and verify that each
produces exactly one display output from a valid linear HDR SceneColor while
invalid input fails before any output is published.

**Acceptance Scenarios**:

1. **Given** a valid Forward frame with a finite RGBA16F, linear Rec.709/sRGB-D65
   HDR SceneColor, **When** the output pipeline executes, **Then** it produces
   exactly one display-ready output through the canonical stage order and
   identifies that output as the frame's formal presentation source.
2. **Given** an equivalent Deferred frame, **When** the same output policy
   executes, **Then** it uses the same stage semantics, settings, and output
   contract rather than a Deferred-specific tone-mapping path.
3. **Given** missing, zero-sized, non-finite, or semantically incompatible
   SceneColor input, **When** output preparation is requested, **Then** it fails
   closed with an actionable diagnostic and publishes neither a partial output
   nor a stale prior-frame result.
4. **Given** two consumers that both attempt to claim the formal display output
   for one view and frame, **When** the graph is validated, **Then** the conflict
   is rejected deterministically before native execution.

---

### User Story 2 - Control Exposure and Output Appearance (Priority: P2)

As a graphics developer, I can select a finite manual exposure, an explicit SDR
tone-map or HDR viewing-transform version, and a supported output-device
profile, then know exactly which display transform and encoding are applied, so
image appearance is reproducible across renderer strategies, graphics backends,
displays, and later workload revisions.

**Why this priority**: The formal path is useful only when its color decisions
are explicit, versioned, and stable. Silent backend transfer, implicit exposure,
or an unversioned tone-map change would invalidate image evidence without a
traceable policy change.

**Independent Test**: Apply canonical HDR color vectors and bounded rendered
scenes at the frozen manual-exposure samples to the double-precision CPU oracle
and applicable Vulkan/Metal offscreen shader readbacks, without requiring a
native presentation surface. For every supported SDR/HDR output-device profile,
verify that the selected SDR tone-map or HDR viewing-transform version, display
transform, color gamut, reference/peak luminance, and transfer encoding are
recorded and applied exactly once. Native surface presentation/readback is the
independent responsibility of User Story 4.

**Acceptance Scenarios**:

1. **Given** a valid scene and manual exposure of zero stops, **When** it is
   processed repeatedly with one SDR tone-map or HDR viewing-transform version,
   **Then** normalized stage diagnostics and output pixels remain deterministic.
2. **Given** the same finite SceneColor at exposure values one stop apart,
   **When** the pre-tonemap signal is inspected, **Then** the exposed linear
   values have the expected factor-of-two relationship and the final output is
   monotonic for non-negative input.
3. **Given** a selected SDR tone-map version, **When** applicable Vulkan and
   Metal offscreen shader paths process the same canonical inputs, **Then** both
   identify the same version and produce decoded display values within the
   declared conformance tolerance without requiring native presentation state.
4. **Given** an SDR output request that selects the default tone-map setting,
   **When** settings are resolved, **Then** it records Khronos PBR Neutral v1 as
   the explicit effective version; `Sdr.NarkowiczAcesFit.v1` and
   `Sdr.ExtendedReinhardRec709.v1` are available only through explicit selection.
5. **Given** an HDR output request, **When** settings are resolved, **Then** it
   selects the versioned ACES-style HDR viewing transform and the requested
   peak-specific ODT without running any SDR tone-map curve first.
6. **Given** an SDR sRGB output profile, **When** the frame is transformed,
   **Then** Renderer explicitly performs linear-to-sRGB encoding into an UNorm
   FinalOutput and presentation performs no second color conversion.
7. **Given** a supported HDR output profile and compatible display path,
   **When** the frame is transformed and presented, **Then** the selected
   1000/2000-nit PQ or scRGB/EDR display transform, gamut, encoding, swapchain,
   and metadata contract agree with the same-frame readback evidence.
8. **Given** an output surface with explicit transfer responsibility,
   **When** the frame is presented and read back, **Then** the transfer occurs
   exactly once and the report distinguishes encoded storage from decoded
   comparison values.
9. **Given** an unknown tone-map/viewing/output-device version, non-finite exposure,
   unsupported HDR display capability, or ambiguous output-transfer ownership,
   **When** output preparation is requested, **Then** the request fails or is
   reported Unsupported before graph execution rather than choosing a fallback.

---

### User Story 3 - Compose Ordered Post-Processing Stages (Priority: P3)

As a renderer feature author, I can attach a bounded operation to a declared
pre-tonemap or post-tonemap insertion point and observe its deterministic order,
so later temporal reconstruction can operate on HDR SceneColor and later
display-space processing can operate after tone mapping without creating a
second post-processing framework.

**Why this priority**: Explicit insertion semantics prevent phase-order errors
and duplicate frameworks. They are also the contract needed by Feature 030,
where TAA belongs before tone mapping and FXAA belongs after tone mapping.

**Independent Test**: Register named no-op and diagnostic color operations at
both insertion points, permute declaration order, and verify that valid
operations receive only the declared color domain, execute in stable order, and
cannot claim or bypass a stage they do not own.

**Acceptance Scenarios**:

1. **Given** a pre-tonemap operation, **When** it executes, **Then** it receives
   finite linear HDR SceneColor after manual exposure and before tone mapping,
   and its result becomes the input to the next declared stage.
2. **Given** a post-tonemap operation, **When** it executes, **Then** it receives
   display-referred linear color after tone mapping and before the final output
   transfer.
3. **Given** multiple operations at one insertion point, **When** their declared
   order is valid, **Then** equivalent registrations produce the same ordering
   and graph dependencies on every supported platform.
4. **Given** duplicate order keys, a dependency cycle, an undeclared color
   domain, or an operation that tries to own the final transfer, **When** the
   stage list is validated, **Then** the frame fails before native work begins.
5. **Given** a diagnostic bypass selection, **When** the frame executes,
   **Then** the selected stage is exposed without silently changing the
   canonical formal-output policy, and the bypass mode is clearly marked as
   non-authoritative evidence.

---

### User Story 4 - Present, Read Back, and Resize Safely (Priority: P4)

As a demo or validation operator, I can present and read back the same completed
SDR or HDR output frame on Vulkan and Metal across drawable-size or output-mode
changes, so visible results and machine-verifiable results have one provenance
and never reuse stale extent-dependent or output-device state.

**Why this priority**: A correct mathematical transform is insufficient if
presentation applies another transfer, readback observes a different frame, or
resize reuses incompatible resources.

**Independent Test**: Execute a fixed frame on required Vulkan and Metal native
paths for supported SDR profiles and on macOS Metal for PQ and EDR/scRGB HDR
profiles. Mechanically verify non-visual transform/readback contracts, prepare
a same-frame macOS HDR live-review request whose strongest state is
`ready-for-live-review`, and cycle through nonzero, zero, restored, and changed
output-device states while inspecting terminal ownership and frame-token
evidence. The maintainer's actual live visual decision belongs exclusively to
User Story 5 and is not required to complete this story's non-visual increment.

**Acceptance Scenarios**:

1. **Given** a supported Vulkan or Metal device and an SDR or HDR-compatible
   drawable, **When** a frame completes, **Then** native presentation and output
   readback identify the same submission frame, dimensions, output-device
   profile, transform settings, gamut, luminance scale, and transfer
   interpretation.
2. **Given** a change from one nonzero drawable extent to another, **When** the
   next frame executes, **Then** every extent-dependent output is recreated or
   rebound for the exact new dimensions and no old-size image is presented or
   read back.
3. **Given** a minimized or zero-drawable application, **When** output is
   requested, **Then** presentation pauses without manufacturing a zero-sized
   formal image; after restoration, the first successful frame uses only the
   restored extent.
4. **Given** a native allocation, recording, submission, completion,
   presentation, or readback failure, **When** cleanup completes, **Then** no
   partial output is accepted and all frame-owned resources return to their
   declared terminal baseline.
5. **Given** a macOS Metal PQ or EDR/scRGB run whose non-visual gates pass,
   **When** HDR visual acceptance is requested, **Then** automation emits only a
   same-frame `ready-for-live-review` request and leaves the result
   `manual-review-required`; only User Story 5 may complete the separate
   maintainer-authored live-view decision.
6. **Given** Windows Vulkan executes Feature 029, **When** validation is
   aggregated, **Then** Windows continues its required SDR coverage but reports
   no HDR visual or physical authority and is not required to validate HDR
   swapchain output.

---

### User Story 5 - Accept a New Formal Image Revision (Priority: P5)

As a maintainer, I can distinguish the completed Feature 028 single-sample,
no-general-post-processing images from the new Feature 029 output and explicitly
review exact-size Candidates before they become authority, so historical
correctness evidence is preserved and visual changes cannot be hidden by image
normalization.

**Why this priority**: Feature 029 intentionally changes formal output. Its
closeout must prove that the change is reviewed rather than silently rewriting
or reinterpreting already accepted v2 references.

**Independent Test**: Run each affected production workload under its successor
revision. For SDR, verify a missing accepted reference fails closed with bounded
Candidate evidence, exercise forbidden dimension/alignment mutations, and
confirm only an explicit maintainer action can admit a Candidate as Accepted.
For HDR, verify no automated image comparison is authoritative and only a fresh
macOS live-view maintainer attestation can accept visual output.

**Acceptance Scenarios**:

1. **Given** an affected Feature 028 v2 workload, **When** Feature 029 becomes
   its formal output path, **Then** the workload advances to a new revision,
   initially v3, while every v2 image and its `sampleCount=1`/no-general-post-
   processing meaning remain unchanged historical evidence.
2. **Given** no Accepted SDR reference for the new workload/backend/device
   class, **When** an authority run completes valid semantic and native gates,
   **Then** it fails closed as Candidate and emits only the bounded review
   evidence.
3. **Given** an SDR Candidate whose dimensions differ from its reference or
   whose comparison would require alignment, cropping, scaling, warping, or
   resampling, **When** comparison begins, **Then** it is rejected before
   perceptual scoring.
4. **Given** a valid exact-dimension SDR Candidate, **When** ordinary automation
   completes, **Then** it remains non-authoritative until a maintainer explicitly
   reviews and accepts its exact workload/backend/device-class record.
5. **Given** Feature 028's one-time Windows evidence carry-forward, **When**
   Feature 029 authority is assembled, **Then** that exception is not reused for
   changed SDR output, which is validated afresh on the required physical
   Windows Vulkan and M4 Metal authorities. HDR visual authority is instead a
   fresh same-revision, maintainer-observed macOS Metal PQ and EDR/scRGB run.
6. **Given** complete automated HDR transform/readback evidence but no macOS
   live-view decision, **When** aggregation runs, **Then** HDR visual acceptance
   remains incomplete and no image Candidate/reference comparison may promote
   it.

### Edge Cases

- SceneColor contains finite negative components, values far above display
  white, subnormal values, or non-finite values in only one channel.
- Exposure is at the supported minimum or maximum, uses negative zero, or would
  overflow the exposed intermediate without a bounded policy.
- An SDR tone-map or HDR viewing-transform version is missing, unknown,
  deprecated, or changed while a frame is already in flight.
- Output encoding metadata disagrees with the native surface's declared
  transfer behavior, creating a risk of zero or double sRGB/PQ conversion or an
  unintended scRGB scale.
- An HDR profile requests 1000 or 2000 nits but the OS, display, swapchain
  format, color space, EDR headroom, or metadata path cannot prove that profile.
- PQ output uses the wrong Rec.2020 primaries or mastering metadata, or scRGB
  output uses an inconsistent reference-white-to-linear scale.
- The Forward transparent handoff occurs after Deferred composition but before
  the formal pre-tonemap insertion point.
- An insertion declares duplicate ordering, cycles, reads its own future
  output, changes extent or sample count, or supplies an incompatible color
  domain.
- A debug view selects raw HDR data for an LDR presentation target; the system
  must require an explicit diagnostic visualization or HDR-preserving readback
  instead of silently clamping it into formal output.
- The drawable changes size between graph preparation and native submission,
  repeatedly alternates sizes, becomes zero, or is restored at a new display
  density.
- Presentation succeeds but readback fails, readback succeeds for an older
  frame, or the backend reports an output whose dimensions differ from the
  completed submission.
- Forward and Deferred use different valid HDR formats or alpha semantics for
  otherwise equivalent scenes.
- A producer labels non-Rec.709 primaries, a non-D65 white point, encoded RGB,
  or a non-RGBA16F texture as the canonical SceneColor handoff.
- An exact-size SDR Candidate has a one-pixel translation or a color-transfer
  mismatch that could be hidden by alignment or resampling.
- A required native backend or physical authority is unavailable; the result
  must remain Unsupported or incomplete with a replacement lane, never a
  simulated pass.
- A macOS HDR run passes transform/readback checks but no maintainer actually
  views the live PQ or EDR output, or an automated image metric attempts to
  substitute for the missing visual decision.
- A Windows report claims HDR visual/physical validation even though Windows is
  deliberately outside the Feature 029 HDR authority contract.

## Architecture & Design Constraints *(mandatory)*

- **Renderer Ownership**: Renderer owns the backend-neutral HDR stage semantics,
  manual exposure, SDR tone-map and HDR viewing-transform versions, output-
  transfer decision, insertion-point ordering, output-device profile, and
  formal-output identity. Native backends execute those decisions but MUST NOT
  define competing output policies.
- **RHI Abstraction**: The feature MUST NOT bypass the RHI layer to call Vulkan
  or Metal directly from Renderer. Native presentation, synchronization, and
  readback ownership remain isolated behind their backend boundaries.
- **Render Graph Authority**: HDR SceneColor, intermediate values, formal output,
  optional diagnostic readback, and all pass dependencies/lifetimes/transitions
  MUST be declared through the existing Render Graph contract. Hidden side
  passes or backend-private post-processing graphs are prohibited.
- **Forward/Deferred Unification**: Forward and Deferred remain selectable
  strategies but converge on one compatible HDR SceneColor contract and one
  formal output path. Existing Deferred composition and Forward transparency
  ordering MUST remain semantically correct.
- **Insertion Compatibility**: Pre-tonemap and post-tonemap insertion points
  MUST be stable extension contracts. They MUST allow Feature 030 to place TAA
  before tone mapping and FXAA after tone mapping without Feature 029 owning
  jitter, motion vectors, history, reprojection, or any other temporal state.
- **Output-Device Model**: Feature 029 MUST separate scene-referred HDR color
  processing from versioned display/output-device transforms. The initial
  matrix includes SDR sRGB, SDR Rec.709, SDR explicit gamma, 1000/2000-nit
  ST-2084/PQ with Rec.2020, and 1000/2000-nit scRGB/EDR. HDR-capable native
  swapchains and metadata are part of the required applicable backend path.
  SDR uses the three versioned SDR tone-map curves; HDR instead uses one
  independently versioned ACES-style HDR viewing transform followed by the
  selected peak- and encoding-specific ODT.
- **Working Color Space**: The canonical SceneColor working space is RGBA16F,
  scene-referred linear Rec.709/sRGB primaries with D65 white. Output-device
  transforms own every explicit conversion from this space to SDR Rec.709,
  HDR Rec.2020/PQ, or scRGB/EDR display semantics. Feature 029 does not add a
  configurable or second working color space.
- **Design Patterns**: Orthogonal preparation, stage selection, graph
  declaration, execution, presentation, and diagnostics responsibilities MUST
  remain independently testable and MUST NOT collapse into a God-class.
  Strategy/Composite-style composition SHOULD be used where responsibilities
  vary or form an ordered pipeline.
- **Advanced Graphics**: The SceneColor and output contracts MUST remain
  compatible with later Meshlet, ray-tracing, and global-illumination producers.
  They MUST NOT require those producers to create another tone-map or temporal
  framework.
- **Asset and Shader Boundaries**: Asset code retains immutable CPU content and
  no GPU ownership. Shader inputs continue to use the repository's established
  deterministic, target-aware offline authority; this feature MUST NOT add
  runtime shader compilation.
- **Naming Conventions**: Code design MUST follow the project's PascalCase,
  UnrealEngine5-style naming conventions.
- **Cross-Platform Compatibility**: Deterministic behavior MUST build and run on
  Windows, macOS, and Linux. Platform-specific code MUST remain behind native
  backend implementation boundaries or guarded integration code.
- **Automated Cross-Platform Validation**: Windows, macOS, and Linux validation
  MUST cover deterministic preparation, graph, failure, resize, and output-
  transfer behavior. Existing real-device/native lanes established by Features
  019, 027, and 028 MUST cover their applicable Vulkan, Metal, presentation, and
  readback responsibilities without treating unavailable hardware as success.
  Automated HDR checks are non-visual only and MUST NOT decide perceptual HDR
  correctness.
- **Evidence Provenance**: Formal presented pixels, output readback, semantic
  probes, SDR Candidate comparison, and configuration metadata MUST refer to
  one completed frame and one frozen workload/software revision. Normalized
  reports MUST redact host-specific and sensitive data. Required macOS HDR
  visual decisions additionally retain an explicit maintainer attestation tied
  to the exact live run and output-device profile; no captured image substitutes
  for that decision.
- **Scope Boundary**: This feature MUST NOT implement anti-aliasing, temporal
  reconstruction, bloom, depth of field, motion blur, automatic exposure,
  vendor upscalers, or a post-processing editor. Deferred remains
  `sampleCount=1`; MSAA policy belongs to later work. SDR, HDR10/PQ, scRGB/EDR,
  and their applicable native swapchains are explicitly in scope.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST transform a declared finite RGBA16F,
  scene-referred linear Rec.709/sRGB-primary, D65 HDR SceneColor into exactly one
  formal display output for each successfully completed view and frame.
- **FR-002**: Forward and Deferred MUST feed the same backend-neutral output
  policy and stage semantics; neither strategy nor native backend may maintain
  a separate authoritative tone-map or output-transfer policy.
- **FR-003**: Every output request MUST declare its SceneColor identity, extent,
  sample count, RGBA16F format, linear Rec.709/sRGB-D65 working space, alpha
  interpretation, selected view, manual exposure, SDR tone-map or HDR viewing-
  transform version, output-device profile/version, target color gamut,
  reference and peak luminance where applicable, desired output transfer, and
  whether presentation, readback, or both are required.
- **FR-004**: Missing resources, zero or overflowing dimensions, unsupported
  sample count or format, incompatible color-domain metadata, non-invertible
  view state needed by an existing renderer handoff, and non-finite required
  settings MUST fail before graph execution with no partial formal output.
- **FR-005**: Each successful output request MUST publish exactly one formal
  output identity, and duplicate writers or ambiguous presentation sources for
  the same view/frame MUST fail deterministically.
- **FR-006**: The canonical stage sequence MUST be SceneColor handoff, manual
  exposure, pre-tonemap insertion, versioned tone/viewing transform,
  post-tonemap insertion, versioned output-device gamut/transfer encoding, then
  presentation and/or readback.
- **FR-007**: The pre-tonemap insertion point MUST expose finite, scene-referred
  linear Rec.709/sRGB-D65 RGBA16F HDR color after manual exposure and before
  tone mapping; its result MUST remain in that declared domain for the next
  stage.
- **FR-008**: The post-tonemap insertion point MUST expose display-referred
  linear color after tone/viewing transformation and before the final output-
  device gamut/transfer encoding. Its declared reference-white and peak-
  luminance meaning MUST remain valid for the next stage.
- **FR-009**: Multiple operations at an insertion point MUST have stable unique
  identities, explicit deterministic order and dependencies, and bounded
  resource declarations; duplicate order, cycles, missing dependencies, and
  undeclared read/write hazards MUST fail before native execution.
- **FR-010**: An insertion operation MUST NOT silently change output extent,
  sample count, color domain, SDR tone-map or HDR viewing-transform version,
  transfer ownership, formal output-device profile, luminance scale, or output
  identity.
- **FR-011**: Manual exposure MUST be explicit, finite, deterministic, expressed
  in stops relative to neutral exposure, and applied exactly once before the
  pre-tonemap insertion point; automatic or scene-derived exposure is not
  permitted.
- **FR-012**: For finite input within the supported numeric range, a one-stop
  exposure increase MUST multiply the exposed linear signal by two before tone
  mapping. Supported exposure bounds and out-of-range rejection MUST be explicit
  and identical across renderer strategies and backends.
- **FR-013**: SDR tone mapping MUST support the stable version identities
  `Sdr.KhronosPbrNeutral.v1`, `Sdr.NarkowiczAcesFit.v1`, and
  `Sdr.ExtendedReinhardRec709.v1`. The Narkowicz strategy is explicitly a sampled
  fit and MUST NOT be reported as Academy ACES conformance. The SDR
  default setting MUST resolve to Khronos PBR Neutral v1, and every resolved
  request, diagnostic, formal workload, and evidence record MUST store that
  effective version explicitly. Unknown version identities MUST fail closed; no
  version or default may change meaning without a new identifier and affected
  workload revision.
- **FR-014**: Each supported SDR tone-map version MUST define canonical results
  for black, diffuse mid-range color, display white, highly emissive values,
  finite negative components, and near-limit finite input; results MUST be
  finite and monotonic for non-negative input. HDR output MUST NOT execute an
  SDR tone-map curve first; it MUST select a separate versioned ACES-style HDR
  viewing transform whose scene-to-display mapping is parameterized only by the
  declared 1000/2000-nit target, then apply that profile's PQ or scRGB/EDR ODT.
- **FR-015**: Non-finite SceneColor or intermediate components MUST fail the
  affected output rather than be hidden by tone mapping. Finite negative and
  over-range values MUST follow one documented deterministic policy before
  final encoding.
- **FR-016**: The initial output-device matrix MUST include versioned profiles
  for SDR sRGB, SDR Rec.709, SDR explicit gamma, 1000-nit and 2000-nit ST-2084/
  PQ Rec.2020, and 1000-nit and 2000-nit scRGB/EDR. Each profile MUST define its
  color primaries/gamut, conversion from the canonical Rec.709/sRGB-D65 working
  space, reference white, peak luminance, numeric range, transfer encoding,
  storage/swapchain format class, metadata, and decoded comparison domain.
- **FR-017**: Output-transfer responsibility MUST be explicit for every formal
  surface and readback and applied exactly once. SDR sRGB MUST use a Renderer-
  explicit linear-to-sRGB transform into UNorm FinalOutput with no presentation
  conversion; every other SDR/HDR profile MUST likewise identify exactly which
  output-device stage owns gamut conversion, luminance mapping, and encoding.
- **FR-018**: Formal comparison MUST decode stored/presented values according to
  the declared output-device profile before semantic comparison and MUST report
  storage encoding, color gamut, reference/peak luminance, and comparison color
  domain. Formal output alpha meaning and value policy MUST be declared,
  deterministic, and consistent across Forward, Deferred, Vulkan, and Metal.
- **FR-019**: The same valid source values and settings MUST produce stable
  normalized stage records and output values across repeated execution, with
  native Vulkan/Metal differences for the same output-device profile bounded by
  its declared decoded-color and luminance conformance tolerances.
- **FR-020**: All SceneColor, insertion intermediates, tone-mapped color, formal
  output, presentation, and requested readback accesses MUST be represented in
  the Render Graph with explicit dependencies, lifetimes, transitions, and
  culling protection for externally observed results.
- **FR-021**: Empty optional insertion lists MUST remain valid and MUST not alter
  the canonical exposure, tone-map, transfer, presentation, or readback stages.
- **FR-022**: Diagnostic bypass MUST explicitly select a named stage and record
  its source color domain, reference-white/peak meaning, visualization/encoding
  policy, and non-authoritative status. It MUST NOT mutate the canonical formal-
  output configuration or masquerade as accepted image evidence.
- **FR-023**: Raw HDR diagnostic inspection MUST use HDR-preserving readback or
  an explicit bounded visualization transform; silently clamping raw HDR into
  an LDR presentation target is prohibited.
- **FR-024**: Feature 029 insertion contracts MUST support a later replacement
  of pre-tonemap SceneColor by temporal reconstruction and a later post-tonemap
  display-space filter, while Feature 029 itself stores no jitter, motion-vector,
  history, reprojection, or anti-aliasing state.
- **FR-025**: A nonzero drawable resize MUST invalidate every incompatible
  extent- or output-device-dependent resource and bind the next successful frame
  to the exact new extent and active profile without presenting or reading stale
  content.
- **FR-026**: A zero drawable or minimized application MUST pause presentation
  and formal image capture without treating a zero-sized image as success; the
  first restored output MUST use only the restored extent and current frame.
- **FR-027**: Repeated resize, minimize, restore, failure, and teardown sequences
  MUST release frame/output ownership exactly once, reject stale handles, and
  return every tracked owner to its declared terminal baseline.
- **FR-028**: Vulkan and Metal native paths MUST create and use the swapchain,
  drawable color space, format, EDR/HDR state, and metadata required by every
  supported applicable output-device profile, then present and read back the
  policy selected by Renderer without a backend-private correction, shader tone
  map, hidden transfer, image flip, alignment, or resizing step. Vulkan MAY
  apply HDR10 static metadata through `VK_EXT_hdr_metadata` when supported.
  Metal PQ MUST use `BGR10A2Unorm`, ITU-R 2100 PQ, EDR opt-in, and
  `EDRMetadata=nil`; it MAY allow Core Animation color management for the
  declared PQ colorspace but MUST NOT request `CAEDRMetadata` system tone
  mapping. Metal EDR MUST also set `EDRMetadata=nil` and MUST NOT request
  system tone mapping.
- **FR-029**: Presented pixels and formal output readback MUST share one real
  completed submission frame token, exact dimensions, workload revision,
  exposure, SDR tone-map or HDR viewing-transform version, insertion order,
  output-device version, gamut, reference/peak luminance, transfer policy,
  backend, and device-class evidence.
- **FR-030**: Native allocation, graph binding, recording, submission,
  synchronization, presentation, or readback failure MUST stop dependent work,
  publish no partial or stale formal output, and retain the first actionable
  normalized diagnostic.
- **FR-031**: Windows, macOS, and Linux MUST run deterministic validation for
  preparation, stage ordering, graph declarations, color math, output transfer,
  SDR/HDR output-device selection, diagnostic bypass, resize/mode change,
  failure, and cleanup. Linux MUST additionally retain the existing real Vulkan
  offscreen/readback coverage where applicable. These platform suites may
  validate HDR math and data contracts but MUST NOT claim automated HDR visual
  acceptance.
- **FR-032**: Applicable real-device Metal conformance established by Feature
  027 MUST prove the Feature 029 SDR path plus PQ and scRGB/EDR HDR output on
  macOS with real command completion, GPU readback, a live HDR-capable drawable,
  and explicit maintainer visual review. Windows Vulkan remains required for
  its existing SDR authority but has no Feature 029 HDR visual, physical, or
  swapchain-validation gate. Simulation, a substituted backend, a silent SDR
  downgrade, or an automated visual score cannot satisfy macOS HDR authority.
- **FR-033**: Any Feature 028 formal production workload whose presented output
  changes under Feature 029 MUST advance from v2 to a successor revision,
  initially v3, and MUST include the output-device profile/version in its formal
  identity. Camera, scene, light, transforms, and `sampleCount=1` MUST remain
  frozen unless separately revisioned and reviewed.
- **FR-034**: Feature 028 v2 Accepted references and reports MUST remain
  unchanged historical correctness evidence for the single-sample,
  no-general-post-processing renderer and MUST NOT be regenerated,
  reinterpreted, or relabeled as Feature 029 output.
- **FR-035**: A new formal SDR workload/backend/device-class tuple without an
  exact Accepted reference MUST fail closed after semantic/native checks and
  produce a Candidate; ordinary execution MUST NOT create, promote, replace, or
  accept a reference. HDR visual output has no automated image Candidate/
  reference comparison path and follows FR-045 instead.
- **FR-036**: Every formal SDR Candidate and reference comparison MUST use
  identical frozen inputs and exact declared dimensions. Validation MUST reject
  mismatched dimensions before comparison and MUST NOT translate, align, crop,
  scale, warp, resample, or search for a best fit.
- **FR-037**: SDR calibration MUST retain the existing one-pixel translation,
  blank, stale-frame, origin, missing-geometry, material, lighting/normal, and
  color-transfer mutations, and every proposed SDR Accepted reference MUST
  reject all applicable mutations.
- **FR-038**: Each new formal SDR reference MUST be derived from fresh captures
  at the frozen Feature 029 revision, retain per-reference calibration evidence,
  and require explicit maintainer review and acceptance before it can become
  authority. HDR visual authority retains a fresh FR-045 attestation instead of
  an automatically compared image reference.
- **FR-039**: Feature 028's one-time Windows closeout carry-forward MUST NOT be
  generalized to Feature 029. Changed SDR production output MUST pass fresh
  physical image authority on the required maintainer M4 Metal and x86_64
  Windows Vulkan devices at the same frozen Feature 029 revision; neither
  substitutes for the other. For Windows T102, physical authority requires a
  physical discrete Vulkan GPU; an active Console or RDP session is permitted
  for exact application GPU readback with successful same-frame native window
  presentation and recorded session/adapter evidence. RDP evidence does not
  claim physical-monitor scanout or equivalence to Console presentation, and
  RDP-client screenshots/video cannot replace GPU readback. Changed HDR production output uses only the fresh
  macOS Metal human-visual authority defined by FR-045 and MUST NOT claim
  Windows HDR validation.
- **FR-040**: Formal production authority MUST preserve Feature 028's exact
  512-by-512 SDR Candidate/reference and exact-drawable contract unless a future
  workload revision explicitly replaces that contract. The 1024-by-1024
  interactive preview remains non-authoritative. Each accepted SDR output-device
  profile MUST retain its own exact device-class evidence and MUST NOT compare
  against a reference from another profile. HDR live review uses the declared
  drawable extent but does not create an automated comparison reference.
- **FR-041**: Image evidence MUST remain lossless PNG plus canonical JSON.
  Canonical report JSON MUST be at most 1 MiB and reference at most 64 artifacts;
  each artifact MUST be at most 64 MiB and the aggregate MUST be at most 256 MiB.
  Unbounded frame sequences, desktop captures, and undocumented local evidence
  are prohibited.
- **FR-042**: Normalized diagnostics and evidence MUST identify stage, first
  failure, renderer strategy, workload revision, view/frame token, extent,
  sample count, exposure, SDR tone-map or HDR viewing-transform version,
  insertion identities/order, transfer policy, output-device version, gamut,
  reference/peak luminance, output and swapchain format, HDR/EDR metadata,
  bypass status, backend/device class, and native completion/readback/
  presentation disposition without host paths, credentials, pointer values, or
  unrestricted environment data.
- **FR-043**: Validation MUST exercise Forward and Deferred with empty and
  populated insertion lists, canonical color vectors, production SceneColor,
  working-space/format mismatches, non-finite and boundary values, transfer
  mismatches, graph hazards, native failure injection, resize/minimize/restore,
  stale-frame rejection, exact SDR Candidate/reference policy, and missing/
  malformed/manual macOS HDR attestation behavior.
- **FR-044**: Feature 029 MUST NOT implement anti-aliasing, temporal
  reconstruction, bloom, depth of field, motion blur, automatic exposure,
  vendor upscalers, a post-processing editor, a general MSAA path, or any change
  to Deferred's default `sampleCount=1` policy.
- **FR-045**: Required macOS HDR visual acceptance MUST exercise every
  1000/2000-nit PQ and scRGB/EDR output-device profile on the live Metal
  presentation path after all non-visual gates pass. A maintainer MUST inspect
  each profile and explicitly record pass/fail plus revision, device/display,
  profile, settings, and observed defects in bounded canonical JSON. Automation
  MAY verify prerequisites and evidence completeness but MUST NOT calculate an
  HDR perceptual score, compare an HDR Candidate/reference image, infer the
  visual decision, promote HDR image evidence, or replace the human viewing step
  with an SDR screenshot or capture. Every profile MUST have a current
  non-superseded `pass` decision for Feature 029 closeout; a `fail` decision is
  retained as immutable evidence, blocks closeout, and can be replaced only by
  appending a linked superseding attestation after a corrected run.
- **FR-046**: Windows MUST perform no Feature 029 HDR visual, physical, or
  native-swapchain acceptance and MUST NOT emit a report claiming such
  authority. This exception applies only to HDR output; existing Windows SDR
  builds, deterministic coverage, native presentation/readback, Candidate
  generation, and physical image authority remain required.

### Key Entities *(include if feature involves data)*

- **HDR SceneColor Handoff**: The finite RGBA16F, scene-referred linear
  Rec.709/sRGB-primary, D65 color result for one view/frame, with identity,
  dimensions, sample count, alpha meaning, renderer strategy, and completion
  provenance.
- **Output Transform Request**: One immutable selection of input SceneColor,
  manual exposure, SDR tone-map or HDR viewing-transform version, insertion
  operations, output transfer, destination requirements, diagnostic mode, and
  formal workload identity.
- **Insertion Operation**: A stable named pre-tonemap or post-tonemap operation
  with declared order, dependencies, input/output color domain, bounded resource
  needs, and failure disposition.
- **SDR Tone-Map Version**: A stable semantic identity for one mapping from
  exposed linear HDR color to SDR display-referred linear color, including
  canonical vector expectations and compatibility status. Feature 029 initially
  admits `Sdr.KhronosPbrNeutral.v1` as the default plus explicitly selected
  `Sdr.NarkowiczAcesFit.v1` and `Sdr.ExtendedReinhardRec709.v1`.
- **HDR Viewing-Transform Version**: A stable semantic identity for the
  ACES-style scene-to-display mapping used only by HDR profiles, including
  canonical vector expectations, target peak, reference white, and its required
  relationship to the selected PQ or scRGB/EDR ODT.
- **Output-Device Profile**: A stable versioned SDR or HDR display target that
  defines gamut, reference white, peak luminance, numeric range, encoding,
  swapchain/drawable capability, metadata, and decoded comparison semantics.
  Feature 029 admits the seven SDR/PQ/scRGB profiles declared by FR-016.
- **Formal Output Frame**: The single display-ready result for a view/frame,
  related to one completed submission token, exact extent, storage format,
  output-device profile, output transfer, presentation result, and optional
  same-frame readback.
- **Diagnostic Bypass Record**: A non-authoritative selection of one named
  intermediate stage plus the explicit visualization or readback interpretation
  used to inspect it.
- **Output Evidence Bundle**: Bounded normalized metadata, semantic results,
  exact-size SDR lossless images, SDR calibration mutations and Candidate/
  reference state, maintainer HDR visual attestations, and digests tied to one
  frozen workload/software revision and device class. HDR screenshots may be
  supporting diagnostics only and have no visual acceptance authority.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% of valid Forward and Deferred reference frames declare the
  same seven-stage canonical sequence and exactly one formal output; 100% of
  missing, ambiguous, or invalid inputs fail before native execution and publish
  no output.
- **SC-002**: For at least 32 canonical HDR vectors covering black, neutral,
  display white, saturated primaries, finite negative values, emissive values,
  and numeric boundaries, the finite exposure sample set
  `{-16,-8,-1,0,+1,+8,+15,+16}` is exercised across all three SDR strategies and the
  1000/2000-nit HDR viewing configurations for the canonical cases assigned by
  the checked-in vector manifest. The explicit one-stop pairs `(-1,0)`, `(0,+1)`,
  and `(+15,+16)` have the expected factor-of-two pre-tonemap relationship;
  non-negative outputs are monotonic, all expected results are finite, and zero
  HDR cases run an SDR curve before the HDR transform.
- **SC-003**: At least 16 canonical output-device vectors per each of the seven
  required profiles prove gamut/luminance mapping and encoding are applied
  exactly once. Decoded Vulkan and Metal values for all asserted SDR channels
  are within `2/255` of the common reference. For HDR, each decoded linear RGB
  nits component MUST satisfy
  `E=max(0.02 nit,0.0025*max(1 nit,abs(expected)),M*Qnative)`, where `Qnative` is
  the maximum adjacent native-code decoded step bracketing the expected value
  and `M=1.5` for packed-10 PQ or `M=2.0` for FP16 scRGB/Metal EDR. XYZ tolerance
  is the absolute matrix propagation of the three RGB `E` values plus `1e-6`;
  CPU double-reference comparisons use
  `max(1e-10,1e-10*abs(expected))`. No comparison value may be non-finite.
- **SC-004**: Twenty repeated preparations and executions of each deterministic
  reference case produce byte-identical normalized stage order, settings,
  diagnostics, and same-backend readback on a stable device/run configuration.
- **SC-005**: 100% of valid empty, pre-tonemap-only, post-tonemap-only, and
  combined insertion cases preserve the declared domains and stable order;
  100% of duplicate-order, cycle, hazard, domain, extent, sample-count, and
  transfer-ownership violations fail before submission.
- **SC-006**: Every declared diagnostic bypass mode selects the requested stage,
  reports its color interpretation, and is rejected as formal authority; no
  bypass test changes the subsequent canonical output configuration.
- **SC-007**: A resize matrix containing at least three nonzero extents, zero
  extent, minimize/restore, display-density change, and SDR/HDR output-device
  change completes 100 consecutive transitions on each applicable required
  native path, with HDR mode transitions required on macOS Metal only, exact
  output dimensions/profile, zero stale-frame acceptance, zero stale-handle
  reuse, and terminal owner counts at their declared baselines.
- **SC-008**: On required native Vulkan and Metal paths, 100% of successful
  SDR presentation/readback pairs, plus required macOS Metal PQ and EDR/scRGB
  pairs, carry the same completed frame token, extent, output-device settings,
  swapchain capability/metadata, and decoded output. All injected allocation-
  through-readback failures preserve the first diagnostic and leave zero
  partial accepted frames.
- **SC-009**: Windows, macOS, and Linux complete all deterministic suites and
  strict supported builds; required Linux Vulkan and Feature 027 Metal native
  probes complete with real GPU readback rather than simulation or substitution,
  and every claimed macOS HDR path proves a compatible native HDR/EDR drawable
  rather than silently falling back to SDR. Zero Windows reports claim HDR
  visual, physical, or swapchain-validation authority.
- **SC-010**: `production-content-lantern-v2` and
  `production-content-sponza-v2` remain byte-for-byte unchanged as historical
  evidence, while 100% of affected Feature 029 formal outputs use successor
  workload revisions. SDR profiles use output-device-specific exact 512-by-512
  Candidate/reference records; HDR profiles use output-device-specific macOS
  live-view attestations with no automated image reference.
- **SC-011**: 100% of SDR mismatched-dimension, one-pixel translation,
  alignment, crop, scale, warp, resampling, stale, origin, blank, semantic, and
  transfer mutations are rejected; no SDR comparison path exposes a switch that
  can enable those transformations, and no HDR visual comparison path exists.
- **SC-012**: Every new Accepted SDR production reference records explicit
  maintainer acceptance and fresh same-revision M4 Metal plus Windows Vulkan
  physical authority. Every accepted HDR visual decision records fresh same-
  revision macOS Metal PQ or EDR/scRGB human review only. Zero Feature 029
  records rely on Feature 028's one-time Windows carry-forward.
- **SC-013**: Every emitted evidence bundle validates against the 1 MiB JSON,
  64-artifact, 64 MiB per-artifact, and 256 MiB aggregate limits, contains only
  bounded PNG/JSON evidence, and exposes no desktop capture or sensitive host
  data. PNG authority is SDR-only; HDR visual authority is the bounded manual
  JSON attestation.
- **SC-014**: All existing Feature 013, 015, 018, 019, 027, and 028 regression
  suites remain passing, and inspection finds no AA, temporal history, bloom,
  automatic-exposure, vendor-upscaler, or post-process-editor behavior introduced
  by Feature 029.
- **SC-015**: All four macOS Metal HDR profiles complete their non-visual gates
  and have one explicit same-revision maintainer `pass` attestation after live
  viewing. A recorded `fail` remains valid immutable evidence but blocks Feature
  029 closeout until a corrected run receives a new non-superseded `pass`
  attestation; the correction appends evidence and never overwrites the failed
  record. Automated evidence validation accepts zero HDR visual decisions that
  lack that attestation and contains no HDR perceptual-score threshold or
  automatic Candidate-promotion path.

- **SC-016**: With empty insertion lists, every successful formal output uses at
  most three mandatory fullscreen output passes plus one optional exact GPU
  readback copy, presentation-only frames initiate zero CPU readbacks, and the
  planner admits at most 16 pre-tonemap plus 16 post-tonemap operations with only
  bounded per-stage resources. Graph inspection proves a constant number of
  frame-local full-image visits, hence `O(width*height)` work; elapsed time is
  recorded only as a non-qualifying observation.

## Assumptions

- Features 013, 015, 018, 019, 027, and 028 remain completed prerequisites and
  their public behavior is available without rewriting their historical
  specifications or evidence.
- The initial output-device matrix deliberately follows Unreal's device-profile
  shape: SDR sRGB, Rec.709, and explicit gamma plus 1000/2000-nit PQ and scRGB/
  EDR profiles. The architecture uses a scene-referred color stage followed by
  a versioned viewing/tone transform and a device-specific output transform; it
  does not describe the HDR branch as sRGB encoding with a larger numeric range.
- The existing Renderer RGBA16F SceneColor is interpreted as scene-referred
  linear Rec.709/sRGB primaries with D65 white. Feature 029 does not migrate
  texture, material, lighting, or working-space inputs to ACEScg or Rec.2020;
  wider output gamuts are explicit output-device conversions from this source.
- Manual exposure uses a neutral value of zero stops and powers-of-two stop
  semantics. Exact supported bounds are selected during planning.
  `Sdr.KhronosPbrNeutral.v1`, `Sdr.NarkowiczAcesFit.v1`, and
  `Sdr.ExtendedReinhardRec709.v1` are all required and frozen as SDR-only
  policies behind distinct version identities and canonical test vectors before
  implementation evidence is accepted; `Sdr.KhronosPbrNeutral.v1` is the SDR
  default. HDR uses a separately versioned ACES-style viewing
  transform and never feeds an SDR tone-map result into an HDR ODT.
- Feature 029 preserves the production acceptance scene, camera, lights,
  transforms, exact 512-by-512 authority extent, device-class selection,
  semantic-first comparison ordering, and `sampleCount=1`; only the formal
  output policy advances the affected workloads from v2 to v3.
- Existing Feature 028 reference states, calibration workflow, perceptual
  policies, authority preflight, and bounded evidence limits are reused for SDR.
  The feature adds new SDR Candidates and HDR manual-attestation records rather
  than altering historical v2 artifacts; Feature 028 perceptual comparison does
  not decide HDR visual acceptance.
- macOS Vulkan remains outside required authority. Required formal physical
  SDR image authority continues to be maintainer-local M4 Metal and x86_64
  Windows Vulkan; Feature 027's additional Metal architecture/native probes
  remain conformance evidence where available. HDR visual authority is macOS
  Metal only and covers PQ plus EDR/scRGB by live maintainer inspection; Windows
  performs no HDR authority validation.
- Automated transform vectors, GPU readback, swapchain/metadata checks, and
  report validation are correctness prerequisites rather than proxies for HDR
  appearance. The maintainer's live macOS display decision is the sole HDR
  visual acceptance signal, and no photometric claim about the panel's emitted
  1000/2000-nit peak is made without separate measurement outside this feature.
- This clarification intentionally supersedes Roadmap 2.3's earlier statement
  that Feature 029 excludes HDR10/EDR. The roadmap and its mirrored roadmap
  specification MUST be aligned with the expanded output-device scope before
  `/speckit-plan` begins.
- Feature 030 owns anti-aliasing and temporal reconstruction. It will consume
  the pre-tonemap and post-tonemap insertion contracts established here without
  changing their color-domain meaning or creating a duplicate post-processing
  pipeline.
- Asset licensing decisions and compliance remain outside automated output
  acceptance.
