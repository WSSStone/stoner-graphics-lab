# Feature Specification: KTX2 Cooking & Compression

**Feature Branch**: `022-ktx2-cooking-compression`
**Created**: 2026-07-29
**Status**: Draft
**Input**: User description: "根据 Roadmap 2.1 的下一阶段，制定 KTX2 Cooking & Compression 规格"

## Clarifications

### Session 2026-07-29

- Q: Feature 022 中，同一个 LDR Texture Asset 应采用哪一种 KTX2 artifact 作为权威 cooked output？ → A: 每种 Basis compression policy（ETC1S 或 UASTC）生成一个平台无关的 Basis KTX2 artifact，运行时根据 RHI capability 转码；Uncompressed policy 生成规范的未压缩 KTX2，平台预转码变体留给 Feature 025。
- Q: Generic Data texture 默认是否允许有损 Basis 压缩？ → A: 默认使用 uncompressed KTX2；只有调用者显式声明允许有损时才可使用 UASTC，Generic Data 禁止使用 ETC1S。
- Q: Feature 022 首期应要求 RHI 与 Vulkan 支持到什么粒度的 BC、ETC2/EAC、ASTC 格式矩阵？ → A: 支持 BC1/BC3/BC4/BC5/BC7、ETC2 RGB/RGBA、EAC R/RG 和 ASTC 4x4；color-capable formats 同时提供 linear/sRGB 变体。
- Q: 设备不支持兼容压缩格式时，LDR Basis KTX2 的 uncompressed fallback payload 从哪里获得？ → A: 运行时直接从权威 Basis payload 转码生成；artifact 不保存重复的未压缩副本，也不产生独立 fallback artifact。
- Q: Feature 022 是否应跨 realization request 缓存 Basis 转码后的 payload？ → A: 不跨请求缓存；转码结果仅在当前 realization request 生命周期内存在，跨请求缓存与生命周期管理留给 Feature 026。
- Q: `Balanced` 与 `High` quality 在 portable profile v1 中如何保持确定性？ → A: ETC1S Balanced 使用 quality 192 / compression level 2，ETC1S High 使用 quality 255 / compression level 2；UASTC Balanced 使用 level 2，UASTC High 使用 level 3；两者均固定单 worker，UASTC RDO 禁用。
- Q: Authoritative encoder WebAssembly 与 host container codec 的职责边界是什么？ → A: WebAssembly 接收 canonical request 与 ordered raw mips，并返回最终完整的 ETC1S/UASTC KTX2 bytes；host 不重写 compressed artifact，只执行 preflight、libktx reopen、normalized validation 与 digest。Uncompressed KTX2 由 host canonical writer 生成。

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Cook Portable Texture Artifacts (Priority: P1)

An engine developer can cook a validated Feature 021 texture into a portable
KTX2 artifact whose identity, complete mip payload, semantic meaning, color
space, orientation, alpha interpretation, producer version, and content
evidence are deterministic and inspectable.

**Why this priority**: Later manifests and runtime asset management need one
stable cooked texture contract before they can package, cache, or load texture
content.

**Independent Test**: Cook a representative corpus of color, normal, data, and
HDR texture assets under each supported policy, validate every artifact with an
independent KTX2 validator, reopen it, and compare its metadata and decoded
levels with the source contract.

**Acceptance Scenarios**:

1. **Given** a valid color texture with a complete mip chain, **When** it is
   cooked with a supported portable compression policy, **Then** the result is
   one valid KTX2 artifact with the same logical identity, ordered mip extents,
   sRGB or linear meaning, top-left orientation, alpha declaration, and
   deterministic cooked revision evidence.
2. **Given** a normal or generic-data texture, **When** it is cooked, **Then**
   the artifact remains linear, records the original semantic, and never
   acquires an sRGB interpretation through compression or container metadata.
3. **Given** an HDR texture, **When** it is cooked, **Then** it is stored in a
   supported uncompressed floating-point KTX2 representation without silently
   reducing it to an 8-bit Basis payload.
4. **Given** identical source assets, settings, and portable cook profile,
   **When** cooking is repeated on any supported host platform, **Then**
   artifact bytes, metadata, diagnostics, and cooked digest are identical.

---

### User Story 2 - Select a Supported Runtime Representation (Priority: P1)

An engine developer can present a cooked texture and a backend-neutral device
capability set to the realization path and receive one deterministic,
semantic-compatible BC, ETC2/EAC, ASTC, or uncompressed representation.

**Why this priority**: A portable artifact has no runtime value unless each
device receives a representation it can sample correctly.

**Independent Test**: Evaluate a matrix of cooked artifacts, texture semantics,
alpha layouts, color spaces, and synthetic capability sets, then verify that
selection and transcoded output are stable and never choose an unsupported or
semantically invalid format.

**Acceptance Scenarios**:

1. **Given** a portable compressed color artifact and a device supporting
   multiple compatible families, **When** realization is requested, **Then**
   the documented target-profile order selects exactly one supported format and
   reports the decision.
2. **Given** a normal or data artifact, **When** target selection runs, **Then**
   it chooses only a linear representation compatible with the required
   channel count and semantic policy.
3. **Given** no supported compressed target, **When** uncompressed fallback is
   allowed, **Then** a deterministic uncompressed representation is transcoded
   from the authoritative Basis payload and realized without changing the
   source asset or stable identity.
4. **Given** no compatible target and fallback is forbidden or unsupported,
   **When** realization is requested, **Then** the operation fails before
   resource creation with an actionable capability diagnostic.

---

### User Story 3 - Realize Compressed Textures Through RHI (Priority: P2)

An engine developer can create and upload a block-compressed 2D texture through
the existing Renderer-to-RHI boundary, including every mip, without exposing a
graphics API to Asset code.

**Why this priority**: KTX2 and transcoding remain CPU-only utilities until the
resulting blocks can be represented, validated, uploaded, and sampled by a
backend.

**Independent Test**: Use mock capabilities and upload capture to realize
representative BC, ETC2/EAC, ASTC, and fallback payloads, then supplement the
mock evidence with Vulkan format and native readback or sampling validation
where the host supports it.

**Acceptance Scenarios**:

1. **Given** valid block-compressed mip data and matching capabilities,
   **When** Renderer realizes the texture, **Then** RHI receives the selected
   format, exact mip extents, block-aware row/slice layout, and all payload
   bytes in order.
2. **Given** a format whose final mip dimensions are smaller than one
   compression block, **When** it is uploaded, **Then** layout validation uses
   one complete block rather than rejecting the logical mip extent.
3. **Given** a failure during creation or any mip upload, **When** realization
   stops, **Then** every request-owned resource is released exactly once, no
   partial resource is returned, and the cooked CPU artifact remains valid.
4. **Given** a native Vulkan device, **When** capabilities are queried and a
   supported compressed texture is realized, **Then** the reported support,
   selected format, upload behavior, and validation result agree.

---

### User Story 4 - Reject Corrupt or Unsafe Cooked Content (Priority: P2)

An engine developer can inspect or load KTX2 content from an untrusted local
file without crashes, unbounded allocation, partial publication, or ambiguous
failure results.

**Why this priority**: Cooked artifacts become package inputs and therefore
must be treated as data, not as trusted process state.

**Independent Test**: Mutate container headers, level indexes, metadata, Basis
payloads, dimensions, offsets, lengths, and DFD/supercompression declarations,
then verify bounded rejection and stable diagnostics before publication or GPU
allocation.

**Acceptance Scenarios**:

1. **Given** a truncated, overlapping, misaligned, excessive, or contradictory
   container, **When** it is inspected or loaded, **Then** it is rejected with
   a stable container-stage result and no cooked payload publication.
2. **Given** malformed supercompressed content, **When** transcoding is
   requested, **Then** failure identifies the level and transcode stage without
   returning partial target blocks.
3. **Given** metadata that contradicts the source texture semantic, transfer,
   orientation, alpha, or mip layout, **When** validation runs, **Then** the
   artifact is rejected rather than silently reinterpreted.
4. **Given** an input at a configured limit and another immediately above it,
   **When** both are inspected, **Then** the boundary input is accepted and the
   excessive input is rejected before an excessive allocation occurs.

### Edge Cases

- A valid 1x1 or explicitly base-only texture has exactly one level.
- Odd and non-square mip extents become smaller than a compressed block while
  their stored level size still covers complete blocks.
- RGB source content requires a four-channel compressed target while alpha is
  absent and must remain semantically absent.
- Straight alpha contains fully transparent texels with nonzero color values.
- A color artifact declares linear storage, or sRGB storage with an
  incompatible linear-only target.
- A normal or data artifact contains an sRGB data-format declaration.
- Generic Data requests ETC1S, or requests UASTC without an explicit
  lossy-allowed declaration.
- A source texture is HDR, uses RGBA16F, RGBA32F, or RGB32F, and cannot enter an
  8-bit ETC1S/UASTC policy.
- A declared target profile contains duplicate, unknown, or semantically
  incompatible format preferences.
- Capability data advertises a format but lacks required sampled-image,
  transfer-destination, or block-extent support.
- A linear-only BC4, BC5, EAC R, or EAC RG format is incorrectly requested with
  an sRGB interpretation.
- Container dimensions, level count, offsets, lengths, or decompressed sizes
  overflow checked arithmetic or configured limits.
- Level index entries overlap the header, metadata, one another, or the end of
  the source.
- Required KTX2 data-format information, supercompression global data, or
  orientation metadata is absent, duplicated, unsupported, or contradictory.
- The container is valid but an external validator reports a standards
  violation not recognized by the engine validator.
- Cooking, validation, or transcoding is requested concurrently using immutable
  inputs and shared registered participants.
- Registration is removed while an already-started cook or load operation
  holds its Feature 020 execution lease.
- A target device supports no compressed family, or supports a family but not
  the semantic-compatible member needed by the artifact.
- An LDR fallback request must produce uncompressed texels from Basis without
  consulting a duplicated fallback payload or publishing another asset.
- Resource creation succeeds but upload fails on an intermediate or final mip.

## Architecture & Design Constraints *(mandatory)*

- **Asset Boundary**: Asset owns CPU-side KTX2 content, metadata, validation,
  cook/load/transcode contracts, and immutable cooked payloads. Asset MUST
  depend only on Core and MUST NOT consume RHI capability objects, GPU handles,
  Vulkan constants, or backend types.
- **Renderer Boundary**: Renderer owns capability-driven target selection and
  conversion of Asset output into RHI texture descriptions and upload requests.
- **RHI Abstraction**: Backend-neutral compressed format, capability, block
  layout, and upload contracts belong to RHI. Application and Renderer MUST NOT
  call Vulkan or any other graphics API directly.
- **Backend Isolation**: Vulkan owns native format queries, mappings, image
  creation, upload, and native evidence. Asset metadata MUST NOT embed Vulkan
  format values.
- **Focused Cooker Scope**: This feature may exercise the existing per-asset
  cooking contract and produce deterministic KTX2 artifacts. The repository-wide
  cooker executable, manifest, incremental derived-data cache, and target
  package layout belong to Feature 025. Platform-specific pretranscoded KTX2
  variants are likewise Feature 025 outputs rather than authoritative Feature
  022 artifacts.
- **Runtime Lifetime Scope**: Transcoded compressed or fallback payloads are
  request-scoped transient values. Cross-request caching, request coalescing,
  retained handles, cancellation, and deterministic unload belong to Feature
  026.
- **Stable Identity**: KTX2, compression policy, portable cook profile, and
  cooked digest version content but MUST NOT alter the Feature 020 logical
  AssetId. Runtime target preferences do not version the authoritative artifact.
- **Semantic Safety**: Color transfer, normal-vector meaning, generic-data
  channel meaning, straight alpha, and Feature 021 top-left orientation MUST
  remain explicit across cooking, loading, transcoding, and realization.
- **Complete Texture Scope**: The initial contract covers single-sample 2D
  textures with complete or explicitly base-only mip payloads. Cubemaps,
  arrays, volumes, sparse textures, partial residency, and virtual textures are
  excluded.
- **Design Patterns**: Container parsing, policy selection, cooking,
  transcoding, capability selection, and realization MUST remain separable
  responsibilities rather than one KTX2 or Vulkan manager.
- **Advanced Graphics**: Cooked metadata and block layouts MUST remain usable by
  later material, model, streaming, ray-tracing, and GI phases without adding
  those systems here.
- **Naming Conventions**: Public code design MUST follow PascalCase,
  Unreal Engine-style naming conventions.
- **Cross-Platform Compatibility**: Deterministic container, policy, and
  capability tests MUST run on Windows, macOS, and Linux. Native compressed
  texture evidence is supplementary on platforms where the required native
  capability is unavailable.
- **Automated Cross-Platform Validation**: CI MUST retain strict Debug, strict
  Release, focused Feature 022 tests, architecture-boundary validation, full
  regression, Linux sanitizer coverage, and available Linux Vulkan evidence.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST represent a cooked 2D texture as immutable KTX2
  bytes plus stable Asset identity, source/content/cook revision evidence,
  producer version, portable cook profile, semantic, transfer, alpha,
  orientation, dimensions, and ordered mip metadata.
- **FR-002**: Cooking MUST consume only validated Feature 021 texture payloads
  and immutable cook settings. It MUST reject inconsistent identity,
  dimensions, formats, semantics, color space, alpha, or mip layout before
  producing an artifact.
- **FR-003**: The initial compression policies MUST support Basis ETC1S, Basis
  UASTC, and uncompressed KTX2 output. Policy selection MUST be explicit in
  version evidence and deterministic for a given input and settings. Portable
  profile v1 MUST map ETC1S Balanced to quality 192 / compression level 2,
  ETC1S High to quality 255 / compression level 2, UASTC Balanced to level 2,
  and UASTC High to level 3. Every Basis profile uses one logical worker and
  UASTC RDO remains disabled.
- **FR-004**: The default semantic policy MUST use ETC1S for ordinary opaque or
  straight-alpha LDR color where the requested quality profile permits it,
  UASTC for LDR normal maps, and uncompressed storage for HDR and lossless-
  required generic data. Callers MAY explicitly request UASTC for higher-quality
  color. Generic Data MUST default to uncompressed storage and MAY use UASTC
  only when the caller explicitly declares lossy compression acceptable;
  ETC1S MUST NOT be accepted for Generic Data.
- **FR-005**: HDR inputs MUST remain in a supported uncompressed floating-point
  KTX2 representation. ETC1S or UASTC requests for HDR MUST fail explicitly
  rather than quantizing implicitly.
- **FR-006**: A normal or generic-data artifact MUST remain linear at every
  stage. Only color artifacts MAY carry an sRGB transfer declaration. A
  generic-data lossy-compression opt-in MUST be explicit, immutable, and part
  of cook revision evidence.
- **FR-007**: Cooking MUST preserve Feature 021's DX/Unreal-style top-left
  orientation and straight/unassociated alpha contract in inspectable container
  metadata. Loading MUST reject contradictory orientation or alpha metadata.
- **FR-008**: Cooking MUST preserve every source mip extent and semantic. A
  complete-chain input MUST produce a complete-chain artifact; an explicitly
  base-only input MAY produce one level but MUST remain distinguishable in cook
  settings and version evidence.
- **FR-009**: KTX2 artifacts MUST contain standards-conforming header, level
  index, data-format information, supercompression metadata when applicable,
  key/value metadata, and aligned level payloads sufficient for independent
  validation and reopening.
- **FR-010**: The same logical asset cooked with different compression,
  quality, semantic-loss, mip, producer-version, or portable-cook-profile
  settings MUST retain the same AssetId while producing distinct cook revision
  evidence. Changing only the runtime target preference MUST NOT recook or
  change the authoritative artifact.
- **FR-011**: Cooking MUST be byte-for-byte deterministic across repeated runs
  and supported host platforms for identical source content, settings, and
  producer version. Nondeterministic timestamps, paths, native addresses, or
  registration order MUST NOT enter artifact bytes.
- **FR-012**: The system MUST reopen and inspect repository-produced KTX2
  artifacts without requiring GPU access and report identity, policy, semantic,
  transfer, alpha, orientation, dimensions, levels, stored lengths,
  supercompression, portable cook profile, and digest.
- **FR-013**: Container inspection and loading MUST validate signature, version,
  dimensions, face/layer/depth scope, level count, offsets, lengths, alignment,
  overlap, data-format declarations, supercompression data, metadata
  uniqueness, and configured size limits using checked arithmetic.
- **FR-014**: Missing, inaccessible, unsupported, malformed, truncated,
  excessive, contradictory, corrupt, cook-failure, and transcode-failure inputs
  MUST produce distinct stable result categories and diagnostics identifying
  source, participant, stage, level or field, and relevant limit.
- **FR-015**: Failed cooking, loading, validation, or transcoding MUST publish
  no partial registry record or output payload and MUST release all temporary
  storage.
- **FR-016**: Cooking, validation, inspection, and transcoding MUST be safe for
  concurrent immutable requests and MUST honor Feature 020 registration leases
  without relying on process-global mutable codec state.
- **FR-017**: Backend-neutral runtime target formats MUST include BC1, BC3, BC4,
  BC5, BC7, ETC2 RGB, ETC2 RGBA, EAC R, EAC RG, ASTC 4x4, and uncompressed
  fallback. BC1, BC3, BC7, ETC2 RGB, ETC2 RGBA, and ASTC 4x4 MUST provide
  linear and sRGB variants; BC4, BC5, EAC R, and EAC RG MUST remain
  linear-only.
- **FR-018**: RHI MUST expose compressed texture formats and distinguish logical
  texel extent from block extent, bytes per block, minimum stored block count,
  sRGB compatibility, supported usages, and device capability.
- **FR-019**: Capability reporting MUST distinguish whether a format supports
  sampled-image use and transfer-destination upload. Renderer MUST NOT select a
  format lacking either capability required by the realization request.
- **FR-020**: Renderer MUST select one target deterministically from an explicit
  ordered target profile, artifact properties, semantic, channel/alpha needs,
  and RHI capabilities. Internal enumeration order and backend registration
  order MUST NOT affect selection. This runtime profile selects a transient
  representation and MUST NOT redefine the authoritative cooked artifact.
- **FR-021**: The default desktop target profile MUST prefer compatible BC
  formats before ASTC 4x4, then ETC2/EAC, then uncompressed fallback. Explicit
  target profiles MAY change this order without changing Asset identity.
- **FR-022**: A selected compressed target MUST preserve color transfer and the
  semantic-required channels. Selection MUST reject sRGB normal/data formats,
  alpha-dropping formats for alpha-bearing color, and channel-dropping formats
  when the discarded channels are semantically required.
- **FR-023**: When no compressed target is compatible, Renderer MUST either
  produce the documented uncompressed fallback by transcoding the authoritative
  LDR Basis payload or fail before resource creation if fallback is disabled or
  unsupported. The fallback MUST be transient runtime output rather than a
  second cooked Asset or cross-request cache entry.
- **FR-024**: Transcoding MUST validate expected output block count and byte
  length for every mip before publication. A failed level MUST return no
  partial target payload. This applies equally to compressed targets and
  uncompressed fallback texels. Successful target payloads MUST remain scoped
  to the current realization request and be released when that request no
  longer needs them.
- **FR-025**: RHI texture creation and upload validation MUST accept compressed
  formats only when every mip's logical extent, block-rounded row layout, slice
  layout, and payload length agree with the selected format.
- **FR-026**: Vulkan MUST map every advertised Feature 022 RHI format to the
  corresponding native format, query actual usage support, create the matching
  image, and upload complete blocks without exposing native constants outside
  Backend.
- **FR-027**: Renderer realization failure during creation or any mip upload
  MUST release every request-owned resource exactly once, return no partial
  GPU resource, and leave the cooked Asset payload inspectable.
- **FR-028**: Focused tests MUST cover valid ETC1S, UASTC, uncompressed LDR/HDR,
  color, normal, data, alpha, odd extents, small terminal mips, target
  capability matrices, deterministic fallback, and failure rollback.
- **FR-029**: A malformed corpus MUST cover header, level index, offsets,
  lengths, alignment, data-format metadata, supercompression metadata, payload,
  semantic contradiction, overflow, and configured-limit failures.
- **FR-030**: Repository-produced artifacts MUST pass a pinned independent KTX2
  validation workflow. Validator absence on a developer host MAY skip only the
  supplementary local check; CI MUST provide the required validation evidence.
- **FR-031**: Windows, macOS, and Linux automation MUST build the feature, run
  focused cook/load/transcode/capability/realization tests, run the Asset
  architecture boundary check, and retain the full regression suite and strict
  Release builds.
- **FR-032**: Runtime mip streaming, partial residency, virtual textures,
  cubemaps, arrays, volume textures, sparse textures, repository-wide manifests,
  incremental derived-data caching, package layout, platform-specific
  pretranscoded KTX2 variants, cross-request transcode caching, request
  coalescing, retained runtime handles, editor UI, and vendor-specific source
  formats MUST remain outside this feature.

### Key Entities

- **Cooked Texture Artifact**: Immutable KTX2 bytes and associated stable
  identity, revision evidence, semantic, portable cook profile, and producer
  metadata. For LDR compression it contains the platform-independent Basis
  payload rather than a BC, ETC2/EAC, or ASTC platform variant.
- **Texture Cook Settings**: Immutable compression, quality, semantic-loss, and
  portable-cook-profile choices that contribute to cook evidence. The source
  asset's mip policy contributes to revision evidence but is not overridden by
  cook settings. Runtime fallback and target preferences are not cook settings.
- **Compression Policy**: ETC1S, UASTC, or uncompressed behavior constrained by
  source semantic and precision.
- **KTX2 Level**: One ordered logical mip with extent, stored offset, stored
  length, uncompressed length, and validated payload range.
- **KTX2 Metadata**: Data-format, supercompression, orientation, alpha,
  semantic, identity, producer, and portable-profile declarations needed to
  interpret an artifact. Runtime target preferences are never artifact
  metadata.
- **Target Profile**: Explicit ordered set of acceptable runtime
  representations and fallback policy independent from one graphics backend.
- **Compressed Format Description**: Backend-neutral block extent, bytes per
  block, transfer interpretation, channel/alpha compatibility, and usage
  capability.
- **Transcode Request**: Immutable artifact, chosen target representation, and
  safety limits used to produce complete per-mip request-scoped target blocks.
- **Texture Capability Set**: Device-supported format and usage facts used by
  Renderer selection.
- **Compressed Texture Realization**: Renderer-owned operation that selects,
  transcodes, creates, uploads, and either returns or rolls back one RHI
  texture.
- **KTX2 Diagnostic**: Stable category, stage, participant, source, level,
  field, limit, and actionable explanation for a failed operation.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A corpus of at least 18 valid cooked artifacts spanning ETC1S,
  UASTC, uncompressed LDR/HDR, color, normal, data, straight alpha, odd extents,
  complete chains, and explicit base-only inputs passes both engine validation
  and the pinned independent KTX2 validator in 100% of cases.
- **SC-002**: Across 20 repeated cooks of every valid fixture on Windows,
  macOS, and Linux, artifact bytes, cooked digest, normalized metadata,
  reopened mip descriptions, and diagnostics are identical in all runs.
- **SC-003**: Every valid complete-chain artifact contains exactly the source
  mip count and extents, and every stored level satisfies its compression-block
  size and alignment rules, including all terminal mips smaller than one block.
- **SC-004**: In an aggregate representative LDR color corpus whose base levels
  are at least 64x64, ETC1S artifacts occupy no more than 35% and UASTC
  artifacts no more than 40% of the equivalent uncompressed RGBA8 mip bytes,
  excluding fixed container metadata.
- **SC-005**: Reference quality evaluation reports color PSNR of at least 35 dB
  for ETC1S and 40 dB for UASTC on the declared color corpus; normal-map UASTC
  output has mean angular error no greater than 3 degrees and 99th-percentile
  error no greater than 10 degrees after normalization.
- **SC-006**: Semantic policy tests reject 100% of HDR-to-Basis, sRGB
  normal/data, forbidden-lossy-data, alpha-dropping, and required-channel-
  dropping requests before artifact or resource publication. Generic Data
  ETC1S requests and UASTC requests without explicit lossy opt-in are included
  in this rejection set.
- **SC-007**: A matrix containing at least 36 synthetic capability combinations
  selects the expected BC, ETC2/EAC, ASTC, or uncompressed result in 100% of
  cases, covers every required format member and applicable transfer variant,
  and remains independent of capability enumeration and participant
  registration order. Every uncompressed LDR fallback is derived from Basis
  without reading a duplicate fallback artifact.
- **SC-008**: Mock-RHI realization tests report 100% agreement between selected
  format properties and every created/uploaded mip's logical extent, block
  layout, and byte length.
- **SC-009**: Failure injection at resource creation and every mip transcode or
  upload stage returns no partial target payload or GPU resource, releases each
  request-owned resource and transient payload exactly once, creates no
  cross-request cache entry, and leaves source and cooked assets inspectable.
- **SC-010**: At least 40 malformed, truncated, contradictory, overlapping,
  misaligned, overflow-inducing, and excessive artifacts produce their expected
  stable result category in 100% of cases and trigger no excessive allocation
  or registry mutation.
- **SC-011**: Under at least 8 concurrent cook, inspect, load, or transcode
  requests over immutable fixtures, all operations complete without crash,
  hang, sanitizer report, or divergence from single-request output.
- **SC-012**: Automated architecture validation reports zero Asset production
  dependencies on RHI, Renderer, Application, Backend, native graphics APIs, or
  external validation executables.
- **SC-013**: Windows, macOS, and Linux CI pass focused Feature 022 tests, full
  regression, and strict Release builds; Linux sanitizer gates pass, and
  available Vulkan native evidence agrees with advertised compressed-format
  support without treating unavailable native capability as fallback success.

## Assumptions

- Feature 021's validated `FTextureAsset` payload, semantic, color-space, alpha,
  orientation, mip, digest, diagnostics, and Renderer realization contracts are
  the authoritative source inputs and are extended rather than duplicated.
- Feature 020's AssetId, version, registry, cooker/loader participant,
  registration lease, atomic publication, and diagnostics contracts remain
  authoritative.
- The default policy is ETC1S for ordinary LDR color, UASTC for tangent-space
  normals, and uncompressed KTX2 for HDR or generic data requiring lossless
  values. Explicit settings may request higher-quality color. Generic Data may
  use UASTC only through an explicit lossy-allowed setting and never uses
  ETC1S.
- Each compression policy produces one platform-independent authoritative Basis
  KTX2 artifact for LDR content. Renderer transcodes it according to runtime
  RHI capabilities; Feature 022 does not emit platform-specific BC, ETC2/EAC,
  or ASTC KTX2 variants.
- LDR uncompressed fallback is generated transiently by runtime transcoding from
  the authoritative Basis payload. It is not duplicated inside KTX2 and is not
  published as a separate cooked asset.
- All compressed and uncompressed transcode results are request-scoped. Feature
  022 creates no process-level transcode cache; Feature 026 owns later
  cross-request caching, request coalescing, and lifetime policy.
- The initial desktop target order is BC, ASTC 4x4, ETC2/EAC, then
  uncompressed. Profiles for later mobile or web targets may reorder the same
  capability vocabulary.
- The initial compressed-format matrix is BC1/BC3/BC4/BC5/BC7, ETC2
  RGB/RGBA, EAC R/RG, and ASTC 4x4. Color-capable formats have linear and sRGB
  variants; BC4/BC5 and EAC R/RG are linear-only.
- ETC1S and UASTC apply to LDR content. HDR remains uncompressed floating point
  in this phase.
- Complete mip chains remain the default. Explicit base-only Feature 021 assets
  may remain single-level KTX2 artifacts; runtime mip generation is not added.
- Straight alpha and top-left orientation remain unchanged from Feature 021.
- A pinned mature KTX2/Basis implementation and independent Khronos-compatible
  validator may be used. Dependency selection, version, license, build
  integration, and deterministic encoder controls are planning/research work.
- Local native compressed-texture support may vary by device and Vulkan driver.
  Mock capability/realization tests and three-platform deterministic CPU tests
  are mandatory; native evidence supplements them where available.
- The focused per-asset cooker may emit artifact bytes or test files, but
  repository scanning, command-line batch cooking, manifests, incremental
  derived-data storage, target packages, and optional platform-pretranscoded
  variants belong to Feature 025.
- Runtime requests are synchronous and their transcode payloads are transient
  in this phase. Asynchronous scheduling, cancellation, request coalescing,
  cross-request caching, retained handles, and unload belong to Feature 026.
