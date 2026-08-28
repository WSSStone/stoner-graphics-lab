# Feature Specification: Production Content Integration & Acceptance

**Feature Branch**: `028-production-content-acceptance`
**Created**: 2026-08-21
**Status**: Draft
**Input**: User description: "为 roadmap Phase 028 制定 Production Content Integration & Acceptance specification"

## Clarifications

### Session 2026-08-21

- Q: Phase 028 的生产资产可见验收必须覆盖哪些 Renderer 路径？ → A: Deferred 执行完整生产验收；Forward 使用同一 Asset root 执行有界 visible/readback smoke。
- Q: Phase 028 可以接纳哪些许可证下的真实生产资产？ → A: 不实施任何资产许可证校验；许可证选择与合规由项目维护者在系统外负责。
- Q: Phase 028 应如何判定真实生产资产的 Vulkan/Metal 渲染图像是否合格？ → A: Semantic/readback probes 必须通过，并按 backend 与 device class 使用版本化容差进行感知图像比较；截图作为验收证据。
- Q: 生产资产完整 load、realize、render、release 循环应采用什么强度和内存通过标准？ → A: Regular gate 执行 20 次并以第 1-2 次为 warm-up；medium/hardware gate 执行 1,000 次并以第 1-20 次为 warm-up；warm-up 计入总次数，内部资源计数归零，从 warm-up 后样本到终止样本的 RSS 净增长不超过 16 MiB。
- Q: Phase 028 完成后，regular、medium 和真实硬件验收分别应在什么时候成为必需 gate？ → A: Regular 对相关 PR/推送自动执行；medium 每周定时并在 feature/release closeout 强制通过；真实硬件在 Feature 028 closeout，以及参考图或渲染路径变化时强制执行。

### Session 2026-08-24

- Q: Sponza 的初始正面构图不适合作为最终图像基线时，Phase 028 应如何选择并冻结视角？ → A: 增加 strict-cooked、native、calibration-only 自由相机预览；维护者选择中庭纵深构图后保存 row-major View 与 Projection 矩阵，正式 gate 仅按 workload revision 消费代码内冻结预设，绝不消费交互状态或调用方覆盖值。
- Q: 修正 native winding 后 Lantern 显示正确正面但现有无抗锯齿画面仍有明显锯齿，Phase 028 是否应同时加入 AA？ → A: 不扩展本 phase；将正确 winding、相机侧灯光和 `sampleCount=1`/无通用后处理输出冻结为 `production-content-lantern-v2` 并重新验收。后处理与抗锯齿在 Feature 028 收尾后另行改造 roadmap，届时必须升级受影响 workload revision 并重新校准。

### Session 2026-08-27

- Q: GitHub-hosted runner 是否可以继续作为精确 RSS 与耗时的权威复现环境？ → A: 不可以。Hosted runner 继续严格裁决构建、确定性、strict runtime、完整生命周期工作量、readback/capture 计数、owner 归零和 stale-handle 拒绝；RSS、allocator/task-VM 与 wall-clock 数据保留为有界 observation，不能单独把 hosted 结果判为失败。固定、受控、独占的物理 runner 才能拥有校准后的 RSS 硬门禁。
- Q: Hosted 环境不稳定是否意味着放宽图像标准？ → A: 不意味着。正式 baseline、semantic attachment 和 FLIP 仍只在精确 device class 的固定物理硬件上作为硬门禁；不允许自动平移、缩放、裁剪、重采样或最佳对齐。容易受边缘覆盖影响的单像素 semantic probe 必须改为有界区域统计。
- Q: Hosted medium 的耗时如何处理？ → A: 耗时仅是防止失控作业的 operational timeout，不是性能合格线。Hosted Sponza 保留完整 1,000/20 生命周期工作量；在最终 Intel runner 于 3,499 秒仍被旧 native watchdog 截断后，完整 package/native 上限调整为 5,400/4,800 秒并使用 120 分钟 workflow 外层界限。真实性能预算不由 hosted runner 裁决，物理权威的 3,600 秒预算不变。

### Session 2026-08-28

- Q: 项目没有 Windows/Vulkan 或已注册 self-hosted runner，只有维护者本机 M4 Metal 时，Feature 028 的物理验收范围如何收敛？ → A: Feature 028 唯一必需物理权威是维护者本机 native arm64 Metal。它必须由显式 `--local-metal-authority` 启动，并通过 clean committed HEAD、精确 `Mac-Metal-Arm64.json`、非 Rosetta、默认 allocator、进程级独占锁、固定 1,000/20 采样协议、窗口 presentation/readback 和 registry-derived device class 前置检查。Windows Vulkan 与 macOS Vulkan 物理证据延期到未来硬件实验室能力，不阻塞 Feature 028；不存在的 self-hosted runner workflow 不得继续由 push 自动排队。

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Admit Representative Production Content (Priority: P1)

A content or engine developer can select artist-authored static-model packages
whose source revision and byte integrity are known, then admit them into a
stable acceptance corpus. The corpus exercises realistic geometry, materials,
textures, and dependency layouts rather than only repository-authored contract
fixtures. License choice and compliance remain an out-of-band maintainer
responsibility and are not an acceptance-system decision.

**Why this priority**: The feature exists to close the gap between synthetic
fixtures and real production content. No later pipeline result is meaningful
until the maintainer-selected input corpus is reproducible and representative.

**Independent Test**: Validate the corpus inventory from a clean checkout and
prove that every accepted source package has complete source provenance,
immutable content hashes, expected dependency coverage, and no undeclared file.

**Acceptance Scenarios**:

1. **Given** a candidate artist-authored package, **When** its corpus record is
   validated, **Then** the record identifies a stable source location and
   revision, every distributed file, and the expected digest of each file.
2. **Given** the complete bounded corpus, **When** coverage is inspected,
   **Then** it contains at least two distinct static-model packages and jointly
   exercises multiple primitives, multiple materials, external and embedded
   dependencies, and 1K and 2K color, normal, and material-data textures.
3. **Given** absent or changed license metadata in an out-of-band maintainer
   note that is not a corpus manifest or package input, **When** corpus
   validation runs, **Then** it does not open, parse, classify, approve, hash,
   or reject the package on that basis; maintainers remain responsible for the
   decision to include it.
4. **Given** a missing, added, altered, or incorrectly attributed source file,
   **When**
   validation runs, **Then** validation fails with the stable package and file
   identity involved.

---

### User Story 2 - Reproduce Source-to-Cooked Runtime Content (Priority: P1)

A developer can take an accepted source model through the existing import,
texture cooking, derived-data, publication, and runtime-loading contracts. The
same stable Asset identities and normalized semantic content are observed in
development source mode and strict cooked mode, while strict cooked loading
remains independent of authoring files.

**Why this priority**: This is the end-to-end proof that Features 020-026 form
one production-capable delivery path rather than a collection of isolated
contracts.

**Independent Test**: Starting from an empty derived-data and publication area,
import and cook the bounded package, publish it, temporarily make its source
unavailable, load all required roots and dependencies in strict cooked mode,
and compare normalized typed payloads against development-mode results.

**Acceptance Scenarios**:

1. **Given** a clean environment and one accepted package, **When** its declared
   model root is imported and cooked, **Then** all model, mesh, material,
   shader, image, and texture dependencies required by that root are present in
   one valid published generation.
2. **Given** equivalent source content and target inputs, **When** clean cooking
   is repeated on supported hosts, **Then** normalized identities, dependency
   records, derived decisions, payload evidence, manifest content, and
   generation identity agree.
3. **Given** one valid published generation and unavailable source files,
   **When** the model is requested in strict cooked mode, **Then** the complete
   typed dependency graph loads successfully without source resolution,
   importing, or implicit fallback.
4. **Given** development and strict-cooked loads of the same accepted root,
   **When** their typed payloads are compared, **Then** identity, type,
   hierarchy, material bindings, geometry semantics, texture semantics, and
   target-appropriate normalized content are equivalent.
5. **Given** a second unchanged cook, **When** derived-data behavior is
   inspected, **Then** every eligible payload is reused and the published
   normalized result matches a clean cook.
6. **Given** a corrupt, missing, substituted, wrong-target, or unlisted payload,
   **When** strict cooked loading is attempted, **Then** it fails closed and
   does not obtain equivalent content from source files.

---

### User Story 3 - Render One Asset-Backed Composition Across Backends (Priority: P1)

An engine developer can run one backend-neutral demo composition built from an
accepted production Asset root and see the same intended scene through the
existing Vulkan and Metal backends. Asset loading, Renderer realization, and
RHI/native ownership remain transactional, so partial failure never leaves a
half-usable scene or leaked resource set.

**Why this priority**: Visible rendering is the user-observable acceptance gate
for the full content path and exposes integration defects that typed payload
comparison alone cannot reveal.

**Independent Test**: Load one accepted package only from a published cooked
generation, realize its complete renderable dependency graph, run the full
deferred composition plus a bounded forward smoke for the same Asset root
through Vulkan and Metal, capture normalized GPU readback plus visible evidence,
and release all CPU and GPU ownership.

**Acceptance Scenarios**:

1. **Given** a valid strict-cooked production model, **When** demo composition
   completes, **Then** all required geometry, material, and texture Assets are
   realized through Renderer and RHI contracts before the scene becomes
   renderable.
2. **Given** the same Asset root, camera, transforms, lights, material inputs,
   and frame state, **When** Vulkan and Metal render the full deferred acceptance
   workload on supported hardware, **Then** each backend produces a complete,
   correctly oriented, non-placeholder image satisfying the shared semantic and
   versioned backend/device-class image acceptance contract.
3. **Given** failure during any load, dependency, texture, buffer, pipeline, or
   scene-realization step, **When** composition aborts, **Then** no partial scene
   is published and all acquired Asset, RHI, and native ownership is released
   exactly once.
4. **Given** repeated create, render, destroy, and recreate cycles, **When** the
   final cycle ends, **Then** resource counts return to the declared baseline
   and a stale handle from an earlier cycle cannot affect the new composition.
5. **Given** backend-specific row layout, color conversion, presentation, or
   floating-point variation, **When** evidence is normalized, **Then** the
   comparison applies declared semantic tolerances without changing source
   content or backend-neutral scene logic.
6. **Given** the same accepted Asset root, **When** the bounded forward smoke is
   run on each required backend, **Then** it produces valid native readback and
   visible output without requiring the complete deferred acceptance matrix to
   be duplicated for Forward.

---

### User Story 4 - Run Tiered Production Acceptance (Priority: P2)

A maintainer can run a bounded production-content gate during ordinary change
validation and a larger medium-corpus gate on demand or on a schedule. The
regular gate remains practical for contributors, while the larger gate can
detect scale, lifetime, or content-composition defects that small fixtures miss.

**Why this priority**: Real content is valuable only if it becomes repeatable
regression coverage without making every ordinary validation run prohibitively
large or dependent on unavailable graphics hardware.

**Independent Test**: Execute the declared regular and medium profiles from
their documented entry points, verify that each selects the exact recorded
corpus and workload, and compare their normalized reports across repeated runs.

**Acceptance Scenarios**:

1. **Given** an ordinary Windows, macOS, or Linux change-validation run, **When**
   the regular profile executes, **Then** it validates the bounded checked-in
   package through import, cooking, strict loading, and all platform-applicable
   headless or native gates within its declared resource budget.
2. **Given** a scheduled or manually requested medium profile, **When** its
   pinned corpus is available, **Then** it exercises every accepted package,
   longer lifecycle repetition, cold and warm processing, and records timing
   and memory evidence separately from deterministic correctness evidence;
   hosted timing/RSS observations do not override completed functional,
   ownership, stale-handle, capture, or readback results.
3. **Given** the maintainer's native arm64 macOS Metal device, **When** the
   hardware profile runs with explicit local authority, **Then** it captures
   visible Metal evidence and native readback for both accepted workloads;
   Windows/Linux/macOS hosted lanes remain build, deterministic, functional,
   and bounded native coverage rather than substitute physical authority.
4. **Given** a host without an applicable native backend or physical graphics
   capability, **When** validation runs, **Then** the unavailable gate is
   reported as unsupported with its required replacement evidence and cannot
   be silently counted as a native pass.
5. **Given** a failure in one validation tier, **When** results are reported,
   **Then** the failing tier, package, pipeline stage, backend, and evidence
   location are identifiable without rerunning every other tier.
6. **Given** an Asset/rendering-related pull request or push, **When** automated
   validation is selected, **Then** the regular profile is required; **Given**
   the weekly schedule or a feature/release closeout, **Then** the medium profile
   is required; **Given** Feature 028 closeout or a reference-image/render-path
   change, **Then** the maintainer-local Metal hardware profile is required.
7. **Given** a GitHub-hosted medium lane that completes the exact declared work
   with all ownership and stale-handle rules satisfied, **When** allocator-
   retained pages or wall-clock variance exceed a previously observed value,
   **Then** the report preserves the measurements and environment class without
   treating either observation as an authoritative correctness failure.
8. **Given** the maintainer's fixed local Metal device, **When** its preflight proves the
   registered device, exclusive process/device/display ownership, default production allocator,
   frozen software image, and declared workload, **Then** calibrated RSS and
   image policies are authoritative and any exceeded hard limit fails closed.

---

### User Story 5 - Inspect and Preserve Acceptance Evidence (Priority: P3)

A maintainer can inspect one bounded, privacy-safe evidence set that explains
which source content, target, generation, backend, device class, workload, and
acceptance rules produced a result. Deterministic evidence can be compared
across hosts while timing, memory, and screenshots remain clearly identified as
environment-dependent observations.

**Why this priority**: Traceable evidence turns a one-time demo into a durable
quality gate and makes failures actionable as the roadmap advances into
Meshlets, streaming, ray tracing, and global illumination.

**Independent Test**: Regenerate reports for success, malformed content,
strict-cooked rejection, realization rollback, and backend-unavailable cases;
validate their schema, boundedness, stable fields, redaction, and links to
corpus and generation evidence.

**Acceptance Scenarios**:

1. **Given** a completed acceptance run, **When** its normalized report is
   inspected, **Then** it identifies corpus revision, source and generation
   evidence, target profile, requested root, dependency coverage, execution
   mode, backend result, comparison outcome, and stable diagnostics.
2. **Given** equivalent deterministic inputs, **When** the same gate is repeated,
   **Then** normalized correctness records are identical apart from explicitly
   excluded environment observations.
3. **Given** captured timing, memory, device, or image evidence, **When** the
   report is published, **Then** those fields are separated from deterministic
   identities and contain no absolute host paths, credentials, user names,
   native pointer values, or unrelated screen content.
4. **Given** any timing, RSS, allocator, or image measurement, **When** it is
   serialized, **Then** the report identifies whether it is a hard requirement,
   an operational timeout, or an observation and identifies the environment
   class allowed to make that classification.
5. **Given** a failed acceptance rule, **When** a developer reads the result,
   **Then** the report identifies the first stable failure, affected Asset or
   backend subject, expected condition, observed category, and next relevant
   reproduction command.

### Edge Cases

- An out-of-band maintainer note has missing, ambiguous, restrictive, or changed
  license metadata; validators never read the note, and the matter remains a
  maintainer compliance concern outside automated corpus acceptance.
- A recorded source URL disappears, redirects, changes bytes without a revision
  change, or becomes temporarily unavailable during a medium-corpus run.
- A package contains URI escaping, non-ASCII names, mixed path separators,
  duplicate normalized names, case-only collisions, data URIs, or references
  outside its admitted root.
- A model is valid but contains empty nodes, unused dependencies, multiple
  scenes, non-uniform or negative transforms, shared meshes, many primitives,
  transparent materials, or missing optional vertex attributes.
- Textures use RGB or RGBA layouts, 1K or 2K dimensions, color, normal, or data
  semantics, embedded or external storage, and compressed or uncompressed
  target payloads.
- One dependency is valid in development mode but missing, stale, wrong-target,
  wrongly typed, or corrupt in the published generation.
- A clean cook succeeds but an unchanged warm cook produces a different
  generation, omits an eligible cache hit, or retains host-specific metadata.
- Source bytes change after import planning or while a cook is active.
- Runtime loading is cancelled or shutdown begins while a large dependency
  graph is loading or Renderer realization is in progress.
- CPU payload loading succeeds but one GPU allocation, upload, descriptor,
  pipeline, command submission, readback, or presentation operation fails.
- Vulkan and Metal differ in coordinate conventions, texture origin, tangent
  basis, color transfer, depth policy, winding, row padding, or floating-point
  precision.
- A hosted runner builds successfully but lacks the native graphics capability
  required for visible or GPU-readback acceptance.
- A screenshot is blank, stale, occluded, partially presented, from the wrong
  backend, or captures unrelated desktop content.
- A semantic probe passes but perceptual comparison against the applicable
  backend/device-class reference fails, or no accepted reference exists for the
  observed class.
- Timing or memory observations are noisy, improve or regress independently of
  deterministic output, or are compared across incomparable host classes.
- A hosted macOS run returns every tracked owner to zero while process RSS grows
  because allocator pages become reusable rather than physically resident.
- A hosted job completes all correctness work outside a prior wall-clock target
  but inside its operational timeout, or times out without producing terminal
  lifecycle evidence.
- A physical runner is shared, updated, thermally constrained, or otherwise
  fails its authority preflight; it must not emit an authoritative RSS/image
  pass or failure until the environment is restored.
- Repository or automation storage limits are exceeded by the medium corpus or
  generated evidence.

## Architecture & Design Constraints *(mandatory)*

- **Asset Boundary**: Asset continues to depend only on Core and owns CPU-side
  identities, metadata, dependencies, immutable payloads, and load contracts.
  Asset MUST NOT own RHI or native graphics objects and MUST NOT depend on
  Renderer, Application, Backend, or offline Tools.
- **Runtime/Tool Separation**: Runtime loading MUST consume published contracts
  without depending on cooker implementation code. Development source import
  and offline cooking may share public Asset contracts but MUST remain distinct
  from strict-cooked runtime execution.
- **Composition Ownership**: Cross-layer orchestration belongs in
  Application/Demo and Validation adapters. Renderer owns transactional RHI
  realization; Backend owns API-specific execution and presentation. Production
  integration MUST NOT create a cross-layer Asset manager or demo god-class.
- **Backend Neutrality**: The same scene, camera, transforms, lights, material
  inputs, and acceptance semantics MUST drive Vulkan and Metal. Renderer and
  Application MUST NOT call a graphics API directly or fork content behavior by
  backend merely to satisfy evidence.
- **Authority and Derivation**: Admitted source content remains authoritative.
  Cooked textures, published generations, GPU resources, screenshots, and later
  Meshlet/LOD data are derived evidence and MUST NOT become competing authoring
  sources.
- **Transactional Lifetime**: Asset dependency loading, Renderer realization,
  and scene publication MUST expose either one complete usable composition or
  failure. Partial success MUST unwind acquired CPU, RHI, native, and
  presentation ownership in dependency-valid order.
- **Determinism**: Host paths, clocks, locale, filesystem enumeration, process
  identity, native addresses, thread completion order, and graphics-device
  naming MUST NOT affect stable Asset identities, normalized cooked results, or
  deterministic correctness evidence.
- **Advanced Graphics Readiness**: Corpus topology, material coverage, bounds,
  and stable evidence MUST remain suitable as input to later Meshlet, streaming,
  ray-tracing, and global-illumination phases without adding those capabilities
  here.
- **Naming Conventions**: Any public code introduced by the feature MUST follow
  PascalCase and Unreal Engine-style project naming conventions. Core naming is
  not globally rewritten by this integration phase.
- **Cross-Platform Compatibility**: Source, validation, and build behavior MUST
  remain supported on Windows, macOS, and Linux. Platform-specific graphics
  capability and presentation handling MUST remain behind existing boundaries.
- **Automated Cross-Platform Validation**: Regular automated validation MUST
  cover Windows, macOS, and Linux. Native visible physical authority is required
  only on the maintainer's local arm64 macOS Metal device for Feature 028;
  Windows Vulkan and macOS Vulkan physical qualification are explicitly
  deferred until corresponding controlled devices exist.
- **Evidence Privacy and Provenance**: Checked-in or published evidence MUST be
  bounded, source-attributable, and free of credentials, absolute user paths,
  unrelated desktop content, and unstable native identifiers. The feature MUST
  NOT evaluate or enforce asset license policy.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The feature MUST establish an acceptance corpus containing at
  least two artist-authored glTF 2.0 or GLB static-model packages from
  independently identified source works or revisions.
- **FR-002**: Every admitted package MUST record a stable source location,
  source work and package name, revision or release identity, author or
  publisher where known, acquisition date, distributed file inventory, and
  SHA-256 digest per file. License or attribution metadata MAY be documented in
  a maintainer-owned note outside the corpus manifest, package inventory, and
  validation inputs, but MUST NOT participate in automated acceptance or corpus
  identity.
- **FR-003**: The system MUST NOT parse, classify, approve, reject, or otherwise
  enforce asset license terms. Asset selection and legal compliance MUST remain
  an out-of-band project-maintainer responsibility.
- **FR-004**: The corpus MUST jointly exercise multiple mesh primitives,
  multiple material slots, a local node hierarchy, indexed triangle geometry,
  external dependencies, embedded dependencies, and shared dependencies.
- **FR-005**: The corpus MUST jointly include 1K and 2K texture inputs and cover
  color, tangent-space normal, and non-color material-data semantics, with both
  RGB and RGBA source representations represented where valid.
- **FR-006**: The corpus inventory MUST declare which package and subresource
  covers each required geometry, hierarchy, material, texture, dependency, and
  container characteristic.
- **FR-007**: The bounded regular-validation package and every file it requires
  MUST be available from a clean checkout without an unpinned network fetch.
- **FR-008**: Medium-corpus content not stored directly in the repository MUST
  be acquired only through pinned source locations and accepted hashes, cached
  outside authoritative source state, and rejected before use when unavailable
  or mismatched.
- **FR-009**: Corpus validation MUST reject missing, extra, altered,
  path-escaping, ambiguously normalized, or undeclared files before import or
  cooking begins.
- **FR-010**: The admitted corpus MUST remain immutable within a feature
  revision; replacing or upgrading a package MUST be an explicit reviewed
  provenance change rather than an implicit remote update.
- **FR-011**: Every accepted package MUST import through the existing Asset
  resolver and glTF/GLB importer contracts without package-specific parser
  branches or manual conversion of its authoritative model.
- **FR-012**: Import MUST publish stable typed identities for the model, meshes,
  materials, images, textures, and shaders required by the accepted root, with
  complete required-dependency closure and deterministic ordering.
- **FR-013**: Import results MUST satisfy all existing geometry, hierarchy,
  coordinate, tangent, bounds, material, color-space, texture-semantic, limit,
  and diagnostic contracts established by Features 021, 023, and 024.
- **FR-014**: Texture cooking MUST preserve color, normal, and material-data
  semantics, generate the required mip coverage, and select a target-compatible
  compressed or declared fallback payload without treating normal or data
  textures as color textures.
- **FR-015**: Cooking MUST produce a complete, self-contained generation for
  each accepted validation root and target; strict-cooked loading MUST NOT
  require repository source content or access content outside that generation.
- **FR-016**: Clean repeated cooks with equivalent source, processing, and
  target inputs MUST produce matching normalized manifests, payload evidence,
  dependency order, and generation identity on every supported host that can
  produce that target.
- **FR-017**: An unchanged warm cook MUST reuse every eligible derived payload;
  its normalized generation MUST match a clean cook of the same final inputs.
- **FR-018**: Strict cooked mode MUST validate and bind one immutable published
  generation before accepting the production model request and MUST invoke no
  source resolver, importer, authoring decoder, or source fallback afterward.
- **FR-019**: Development and strict-cooked loading of an accepted root MUST
  preserve the same Asset identities, requested types, dependency roles,
  hierarchy, material associations, geometry semantics, texture semantics, and
  target-appropriate normalized semantic content.
- **FR-020**: Physical byte equality MUST NOT be required where approved
  compression or serialization changes representation, but every payload family
  MUST define and exercise a normalized semantic-equivalence comparison.
- **FR-021**: Missing, corrupt, substituted, unexpected, wrong-target,
  unsupported, or type-incompatible cooked content MUST fail closed with zero
  source fallback and no partially published root handle.
- **FR-022**: Source mutation during import or cooking MUST invalidate the
  affected operation and preserve the prior valid published generation.
- **FR-023**: The feature MUST provide one backend-neutral production demo
  composition selected by stable Asset root identity rather than direct source
  paths or hard-coded mesh, material, or texture payload bytes.
- **FR-024**: Demo composition MUST request the accepted root through the
  runtime Asset Manager in strict cooked mode and MUST wait for its complete
  required dependency closure before Renderer realization begins.
- **FR-025**: Renderer realization MUST transactionally create the geometry,
  material, texture, descriptor, and pipeline resources required by the loaded
  model; failure MUST publish no partial renderable and MUST release every
  acquired resource exactly once.
- **FR-026**: Deferred MUST be the complete production-content acceptance path.
  Forward MUST run a bounded native readback and visible smoke using the same
  accepted Asset root. Within each path, the same backend-neutral camera,
  transforms, lights, frame state, and material inputs MUST be used for Vulkan
  and Metal, except for explicit target payload selection and capability-
  justified fallback.
- **FR-027**: Vulkan and Metal native acceptance MUST prove execution on the
  requested backend and MUST NOT count a deterministic simulation, software
  fallback, silently substituted backend, or semantic oracle as native success.
- **FR-028**: Native acceptance MUST include GPU-produced readback with stable
  semantic probes and a visible capture that is nonblank, correctly oriented,
  fully presented, attributable to the requested backend, and free of unrelated
  screen content.
- **FR-029**: Image acceptance MUST first require all stable semantic and native
  readback probes to pass, then normalize row layout, channel encoding, image
  origin, and declared color transfer before perceptual comparison against an
  accepted reference for the reported backend and device class.
- **FR-030**: Reference images and perceptual tolerances MUST be versioned by
  workload, backend, and device class. Accepted tolerances MUST distinguish
  expected driver and numeric variation from missing geometry, wrong material
  assignment, incorrect texture semantics, coordinate errors, stale frames, and
  placeholder output. Device class MUST be derived by exact match against a
  versioned registry and canonical capability signature rather than accepted as
  an arbitrary caller string. A missing or ambiguous class/reference MUST NOT
  silently pass.
- **FR-031**: Repeated composition creation, rendering, destruction, and
  recreation MUST return Asset Manager, Renderer, RHI, native, and presentation
  ownership to declared terminal baselines without stale-handle aliasing. The
  regular profile MUST complete 20 full cycles and use cycles 1-2 as warm-up;
  the medium/hardware profile MUST complete 1,000 full cycles and use cycles
  1-20 as warm-up. Warm-up cycles count toward the required total. Every lane
  MUST treat exact cycle/capture/readback counts, terminal owner baselines, and
  stale-handle rejection as hard correctness gates. The maintainer-local Metal lane that
  passes authority preflight MUST additionally keep net RSS growth from the
  sample immediately after warm-up through the terminal sample at or below
  16 MiB. A GitHub-hosted lane MUST collect the same RSS endpoints plus bounded
  task-VM/allocator diagnostics as observations, but those values MUST NOT
  decide its result or substitute for live ownership checks. Lifecycle image validation
  MUST reuse bounded CPU readback storage, read host-visible Metal buffers
  directly after the storage-mode-required synchronization, and allocate a
  second native staging buffer only for device-local storage. This optimization
  MUST NOT reduce GPU copies, capture counts, nonblank checks, or authoritative
  post-lifecycle attachment extraction.
- **FR-032**: The regular validation profile MUST exercise one bounded accepted
  package through provenance, import, clean and warm cooking, standalone
  generation validation, strict-cooked runtime loading, semantic equivalence,
  Renderer realization, 20 complete lifecycle cycles, and all platform-
  applicable execution gates.
- **FR-033**: The medium validation profile MUST exercise every accepted
  package through clean and unchanged warm cooking, 100% reuse of eligible
  payloads, strict-cooked loading with source unavailable, complete normalized
  semantic equivalence, 1,000 complete lifecycle cycles, and aggregate
  dependency, timing, peak-memory, RSS-growth, and diagnostic evidence. Hosted
  aggregation MUST require the exact work, lifecycle, ownership, stale, capture,
  and readback contracts while preserving timing and memory as observations.
- **FR-034**: Regular validation MUST run automatically for Windows, macOS, and
  Linux for every pull request or push that affects Asset delivery, Renderer,
  RHI, native backends, production composition, acceptance policy, or their
  validation inputs. Workload duplication across matrix jobs SHOULD be avoided
  when one producer artifact can be verified by multiple consumers without
  weakening platform-specific coverage.
- **FR-035**: Feature 028 physical acceptance MUST include visible native Metal
  evidence on the maintainer's arm64 macOS device. Windows Vulkan and macOS
  Vulkan physical qualification are deferred requirements for a future
  hardware-lab phase and MUST NOT be represented as completed Feature 028
  evidence. Linux MUST retain build, deterministic execution, and applicable
  bounded headless/software-native validation.
- **FR-036**: A gate unavailable because of host, backend, device, display, or
  tool capability MUST report Unsupported with the missing prerequisite and
  required replacement lane; it MUST NOT be reported as Passed or silently
  omitted from aggregate acceptance.
- **FR-037**: Hardware-only, medium-corpus, scheduled, or manual gates MUST have
  documented reproducible entry points, required environment classes, evidence
  outputs, and ownership. The medium profile MUST run weekly and pass at feature
  and release closeout. The maintainer-local Metal hardware profile MUST pass at
  Feature 028 closeout and whenever an accepted reference image or production
  render path changes.
- **FR-038**: Deterministic correctness evidence MUST be separated from timing,
  memory, device-description, and visible-image observations so environment
  variation cannot alter Asset, cook, manifest, or generation identities. Each
  environment-sensitive measurement MUST declare `required`, `operational`, or
  `observed` disposition; observations MUST NOT be promoted to hard authority by
  aggregation or caller input.
- **FR-039**: Every acceptance report MUST identify the corpus revision,
  package/root identity, source digest set, target profile, generation identity,
  operating mode, dependency coverage, workload revision, result category, and
  evidence digest. A failure before publication MUST identify generation state
  with the stable `not-created` token rather than a fabricated digest; Passed
  reports MUST contain a real generation digest. A native-render report MUST
  additionally identify an
  explicit Vulkan or Metal backend, exact registered device class, and
  either a measured perceptual result when image authority is required, a
  structured `not-required` reason when the profile owns no image authority, or
  a structured `not-run` reason when execution failed before a required
  comparison. A Failed or Unsupported report MUST contain one structured
  first failure; a Passed report MUST contain no failure and, when native, a
  measured passing perceptual result only when its profile requires image
  authority. Schema validation MUST enforce these conditions.
- **FR-040**: Reports and diagnostics MUST use stable bounded ordering, stable
  first-failure selection, actionable failure categories, and normalized Asset,
  dependency, stage, backend, and target subjects. A canonical report MUST NOT
  exceed 1 MiB, reference more than 64 artifacts, reference any single artifact
  larger than 64 MiB, or reference more than 256 MiB in aggregate.
- **FR-041**: Checked-in and published evidence MUST redact absolute user paths,
  credentials, environment secrets, native pointers, process identifiers, and
  unrelated desktop content.
- **FR-042**: The feature MUST provide deterministic negative coverage for
  corpus provenance failure, malformed or unsupported source content, source
  mutation, cook/publication failure, strict-cooked corruption and no-fallback,
  dependency failure, transactional realization rollback, unavailable native
  capability, image/readback rejection, and lifecycle cleanup.
- **FR-043**: Architecture validation MUST report zero new Asset dependencies on
  Tools, RHI, Renderer, Application, Backend, or graphics APIs; cross-layer
  production orchestration MUST remain in Application/Demo or Validation code.
- **FR-044**: The feature MUST NOT add a new source format, skeletal animation,
  editor workflow, hot reload, package/archive delivery, streaming/residency,
  Meshlet or LOD derivation, virtual geometry, ray tracing, or visual-quality
  redesign.
- **FR-045**: Final acceptance MUST update the corpus inventory, validation
  instructions, evidence index, delivered system-design documentation, roadmap
  status, and project memory without rewriting historical specifications to
  conceal implementation gaps.
- **FR-046**: A calibration-only production camera preview MUST use the same
  strict-cooked root, transactional Renderer realization, Deferred execution,
  requested native backend, and application-window presentation path as image
  acceptance while remaining unavailable to automated validation modes.
- **FR-047**: Every formally accepted production workload MUST select exactly
  one backend-neutral camera preset by exact workload revision. The preset MUST
  contain finite, invertible row-major View and Projection matrices; View MUST
  be affine and orthonormal without scale or shear, and Projection MUST match
  the engine's positive-X-forward StandardZ perspective convention. Missing,
  invalid, or ambiguous presets MUST fail closed before rendering.
- **FR-048**: Calibration preview MUST provide right-drag look, W/S forward and
  backward, A/D strafe, Q/E vertical movement, Shift acceleration, wheel FOV,
  reset, snapshot, and exit controls. Snapshot MUST emit a bounded canonical
  candidate record with round-trip float precision, workload, render extent,
  backend, View, Projection, and digest; it MUST NOT automatically modify or
  accept an authoritative preset or image baseline.
- **FR-049**: A frozen camera, light, transform, or render-policy change MUST
  advance the workload revision and invalidate prior image/probe authority for
  that package. New references MUST repeat semantic-probe definition,
  20-capture calibration, mutation rejection, explicit maintainer acceptance,
  and required hardware validation.
- **FR-050**: Feature 028 production image authority MUST retain the existing
  single-sample (`sampleCount=1`) render policy without adding anti-aliasing or
  general post-processing. This visible output MAY be accepted only through the
  normal versioned image process; later anti-aliasing or post-processing MUST
  be treated as a render-policy change under FR-049 rather than replacing an
  accepted reference in place.
- **FR-051**: Validation MUST derive an execution-environment class from a
  repository-owned contract rather than an unrestricted class string. GitHub-
  hosted, maintainer-local Metal, and local diagnostic execution MUST remain
  distinct; only the narrowly scoped explicit local Metal authority path may
  claim physical RSS/image authority.
- **FR-052**: A maintainer-local Metal run MAY claim RSS/image authority only after
  proving native arm64 macOS, the exact registered device class and target,
  exclusive session ownership, a clean committed and frozen
  workload/software revision, default production allocator behavior, and the
  declared warm-up/sample protocol. Failed preflight MUST be Unsupported with a
  replacement lane, not an observational pass.
- **FR-053**: Hosted medium execution MUST preserve 1,000 cycles, 20 included
  warm-up cycles, 2,000 Deferred/Forward captures, seven post-lifecycle
  authoritative readbacks, zero terminal owners, and stale-handle rejection.
  Its RSS, task-VM, allocator, peak-memory, and wall-clock measurements MUST be
  reported but MUST NOT independently fail a completed correctness run.
- **FR-054**: Hosted medium package execution MUST use a 5,400-second complete
  operational timeout and an independent 4,800-second native timeout inside a
  120-minute workflow job. Timeout remains a failure to complete required work,
  but elapsed time below the cap is not a performance qualification. This
  hosted-only envelope MUST NOT alter the maintainer-local Metal 3,600-second lane.
- **FR-055**: Semantic image probes that classify material, orientation, normal,
  depth, lighting, or background regions MUST use bounded region statistics and
  minimum valid-sample coverage rather than one exact pixel. Region definitions
  remain versioned by workload and mutations MUST prove that wrong semantics are
  rejected.
- **FR-056**: Perceptual image comparison MUST require identical frozen camera,
  canonical 512-by-512 acceptance extent, normalization, and pixel coordinates.
  The 1024-by-1024 interactive preview is not an acceptance input. Validation
  MUST NOT translate, scale, crop, warp, resample, or search for a best
  alignment; an intentional one-pixel whole-image translation MUST be rejected
  in calibration mutation coverage, and any non-512 formal candidate or
  reference MUST fail before semantic or FLIP comparison.

### Key Entities

- **Production Content Package**: One admitted artist-authored glTF/GLB work and
  its complete distributed dependency set, identified by source revision,
  source attribution where known, inventory, and immutable file digests.
- **Corpus Manifest**: Canonical inventory of accepted packages, files,
  provenance, expected Asset roots, coverage characteristics, tier membership,
  and integrity evidence; license notes are outside the manifest and all
  validation inputs.
- **Coverage Claim**: A testable mapping from one required production
  characteristic, such as multi-material geometry or a normal texture, to the
  exact package and subresource that demonstrates it.
- **Production Asset Root**: Stable typed model identity requested in
  development or strict-cooked mode and used as the root of a complete required
  dependency graph.
- **Acceptance Generation**: One validated immutable target generation produced
  from an accepted corpus revision and consumed without source fallback.
- **Asset-Backed Composition**: Backend-neutral scene, camera, lighting, and
  frame inputs assembled from a loaded production Asset root and published only
  after transactional Renderer realization succeeds.
- **Validation Profile**: Named regular, medium, or hardware workload defining
  exact corpus membership, target, repetition, environment prerequisites,
  resource bounds, required gates, and evidence outputs.
- **Environment Authority Policy**: Versioned mapping from trusted execution
  environment class and measurement kind to `required`, `operational`, or
  `observed` disposition, including preflight and replacement-lane rules.
- **Acceptance Evidence Set**: Bounded collection of normalized reports,
  readbacks, visible captures, timing and memory observations, diagnostics, and
  content hashes that explains one validation result.
- **Image Acceptance Baseline**: Reviewed reference image and perceptual
  tolerance policy identified by workload revision, backend, and device class;
  it supplements but never replaces mandatory semantic/readback probes.
- **Production Camera Preset**: Exact workload-owned View and Projection
  matrices used by both Deferred and Forward on every backend; calibration
  interaction can propose but never authoritatively override this record.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The accepted corpus contains at least two independently identified
  artist-authored static-model packages, and 100% of distributed files have
  source provenance and matching SHA-256 records; automated acceptance performs
  zero license-policy checks.
- **SC-002**: Corpus coverage inspection confirms at least one external-
  dependency package, one embedded-dependency package, multiple primitives,
  multiple materials, a node hierarchy, shared dependencies, and 1K/2K color,
  normal, and material-data textures with no uncovered required characteristic.
- **SC-003**: For each required target profile, twenty repeated imports and
  clean cooks of the bounded package produce identical normalized Asset
  identities, dependencies, manifests, payload evidence, and generation
  identity.
- **SC-004**: An unchanged warm cook of every accepted validation root reports
  100% reuse of eligible derived payloads and produces a normalized generation
  equivalent to a clean cook.
- **SC-005**: Development and strict-cooked loads of every accepted root achieve
  100% agreement for identity, type, dependency roles, hierarchy, material
  associations, geometry semantics, texture semantics, and each payload
  family's declared normalized semantic comparison.
- **SC-006**: Strict-cooked acceptance succeeds with all authoritative source
  files unavailable and invokes zero source resolvers, importers, authoring
  decoders, or source fallbacks.
- **SC-007**: A negative corpus containing at least 30 targeted provenance,
  path, source, payload, manifest, dependency, target, realization, readback,
  and lifecycle faults rejects 100% of invalid cases with the expected stable
  first-failure category and no partial published composition.
- **SC-008**: The bounded production composition completes full deferred native
  GPU readback and accepted visible rendering plus the required forward native
  readback/visible smoke on the maintainer's native arm64 macOS Metal device; all
  required semantic probes pass, and every deferred image satisfies the
  versioned perceptual policy for its backend and exact registry-derived device
  class.
- **SC-009**: Each regular gate completes 20 repeated load, realize, render,
  release, and recreate cycles with cycles 1-2 as warm-up; each medium/hardware
  gate completes 1,000 cycles with cycles 1-20 as warm-up. Warm-up cycles count
  toward the total. No cycle produces stale-handle aliasing or double release,
  all tracked ownership counts return to baseline. Maintainer-local Metal
  lanes additionally keep RSS growth from the sample immediately after warm-up
  through the terminal sample at most 16 MiB; hosted lanes produce complete
  endpoint and diagnostic observations without using RSS as their result.
- **SC-010**: The regular profile completes its bounded platform-applicable
  source-to-cooked-to-runtime gate within 10 minutes per hosted job, while the
  hosted medium profile is bounded by a 5,400-second package timeout and an
  independent 4,800-second native timeout inside a 120-minute job, and the serialized
  visible local Metal hardware profile completes within 60 minutes;
  these are operational bounds rather than hosted performance qualifications,
  and timing observations do not affect deterministic result identities.
- **SC-011**: Windows, macOS, and Linux automated Debug and strict Release
  validation plus all applicable sanitizer, deterministic, and native gates
  pass on the final revision; the required local Metal hardware gate has accepted,
  digest-recorded evidence from that same revision. The final evidence includes
  a passing regular run, medium closeout run, and all required hardware profiles
  under the cadence defined by FR-034 and FR-037.
- **SC-012**: Equivalent deterministic runs produce byte-identical normalized
  reports in 20 repetitions, while every checked-in or published report and
  image passes automated privacy, provenance, schema, 1 MiB report, 64-artifact,
  64 MiB per-artifact, and 256 MiB aggregate-artifact checks.
- **SC-013**: Architecture validation reports zero Asset-to-Tools/RHI/Renderer/
  Application/Backend/graphics-API dependency violations and zero direct
  graphics-API calls from Renderer or Application.
- **SC-014**: A clean checkout following the documented workflow can validate
  corpus integrity, reproduce the bounded target generation, run strict-cooked
  acceptance, and locate all required evidence without undocumented local files
  or credentials.
- **SC-015**: Synthetic preview input tests reproduce identical camera matrices
  for identical event/delta sequences; formal Vulkan and Metal execution prove
  byte-identical View and Projection inputs for Deferred and Forward, and 100%
  of invalid or caller-overridden formal camera attempts fail before native
  submission.
- **SC-016**: Report validation proves that 100% of hosted RSS/timing records are
  marked observed/operational, 100% of maintainer-local Metal RSS/image decisions
  include successful authority preflight, and no aggregate can convert an
  observation into a hard pass or failure.
- **SC-017**: Every workload semantic probe passes its accepted image and rejects
  the existing semantic mutations plus an intentional one-pixel translation;
  bounded edge perturbations that preserve the required region statistics do
  not fail solely because one sampled pixel crosses a primitive edge. Every
  formal candidate/reference pair is exactly 512-by-512, and 100% of mismatched
  formal extents fail before comparison.

## Assumptions

- The project maintainers authorize the bounded regular-validation package for
  the repository outside this feature; Git LFS is not required for the bounded
  gate, and automated validation neither proves nor enforces that authorization.
- Larger medium-corpus bytes may remain outside Git when a pinned source URL,
  immutable digest, and reproducible acquisition workflow are recorded; absence
  of optional medium bytes does not turn a required regular gate into a pass.
- Exact artist-authored packages are selected during planning research from
  glTF/GLB sources; source quality, representativeness, and reproducibility take
  precedence over visual novelty, while licensing remains an out-of-band
  maintainer decision.
- Existing Features 020-027 contracts are reused. This phase may repair defects
  exposed by production content but does not redesign those subsystems without
  a documented requirement conflict and migration decision.
- The current static-model scope excludes skinning and animation. Production
  packages may contain out-of-scope optional data only when the existing
  importer rejects or ignores it according to a documented deterministic rule.
- The maintainer's physical M4 Pro Metal device is the only available Feature
  028 physical authority. Windows Vulkan, macOS Vulkan, Linux visible manual
  presentation, Android, and cross-device qualification are deferred.
- Screenshots capture only the application window or validated render surface,
  never the full desktop.
- Hosted timing limits are operational cancellation bounds and hosted memory is
  observation-only. The 16 MiB RSS threshold is an acceptance budget only for
  the maintainer-local Metal environment after authority preflight; neither is
  a general engine performance guarantee.
- New source formats, animation, editor workflows, hot reload, packaging,
  streaming, Meshlets, virtual geometry, ray tracing, and broad visual redesign
  remain assigned to later roadmap work.
- The calibration free camera is private Feature 028 validation tooling, not a
  reusable gameplay/editor camera system or a promise of runtime navigation.
