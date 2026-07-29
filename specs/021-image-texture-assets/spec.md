# Feature Specification: Image & Texture Asset Foundation

**Feature Branch**: `021-image-texture-assets`
**Created**: 2026-07-28
**Status**: Draft
**Input**: User description: "根据 Roadmap 2.1 的下一阶段，建立 PNG、JPEG、HDR 图像导入与 CPU 侧 2D Texture Asset 基础"

## Clarifications

### Session 2026-07-29

- Q: PNG、JPEG 和 HDR 解码后，Feature 021 的 FImageAsset/FTextureAsset 应使用哪组规范化 CPU texel formats？ → A: 使用有限 canonical set，但保留 RGB：LDR 支持 R8、RG8、RGB8、RGBA8；HDR 支持 RGB32F、RGBA32F，并允许 RGBA16F。
- Q: 在调用者没有显式配置时，单张源图像应采用什么默认尺寸与内存上限？ → A: 默认最大边长为 16,384 像素、源文件为 256 MiB、单个 mip 为 512 MiB、完整 decoded mip chain 为 1 GiB；限制可显式提高，但不可关闭溢出检查。
- Q: 当调用者未指定 mip 设置时，导入的 2D texture 是否应默认生成完整 mip chain？ → A: 默认生成完整 mip chain；调用者可显式请求 base-only。
- Q: PNG/JPEG/HDR 导入后应采用哪一个 canonical texture origin？ → A: 使用 DX/Unreal 风格的 top-left origin；UV(0,0) 对应左上 texel，导入时应用必要 orientation 变换，Renderer 不按格式额外翻转 V。
- Q: HDR source 未指定输出精度时，应如何在 RGBA16F 与 RGBA32F 之间选择？ → A: 默认 RGBA16F；调用者可显式请求 RGBA32F。RGB32F 保留为显式无 alpha layout。

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Import Source Images as Stable Assets (Priority: P1)

An engine developer can import supported PNG, JPEG, and HDR source images and
obtain validated CPU-side image and 2D texture assets whose identities,
metadata, pixel interpretation, and dependencies integrate with Feature 020.

**Why this priority**: Materials and model packages cannot consume real image
content until supported source files become stable, typed Asset payloads.

**Independent Test**: Import a deterministic fixture corpus containing supported
channel layouts, dimensions, alpha content, and source encodings, then inspect
the discovered outputs and decoded payloads without creating a GPU resource.

**Acceptance Scenarios**:

1. **Given** a valid PNG, JPEG, or HDR source and an intended texture semantic,
   **When** import is requested, **Then** one image asset and one dependent 2D
   texture asset are discovered with stable typed identities, metadata, version
   evidence, dimensions, texel interpretation, and base-level content.
2. **Given** the same source bytes, identity, semantic, and import settings,
   **When** import is repeated on any supported platform, **Then** discovery,
   canonical metadata, decoded content, and diagnostics are identical.
3. **Given** a source extension that is absent or misleading but whose bounded
   content probe uniquely identifies a supported format, **When** import is
   requested, **Then** the content-selected importer handles it without changing
   the source asset identity.
4. **Given** one source image imported for two distinct semantic uses, **When**
   outputs are discovered, **Then** their texture identities and interpretation
   metadata remain distinct while retaining the same source provenance.

---

### User Story 2 - Preserve Texture Meaning Across Mip Levels (Priority: P1)

An engine developer can declare whether a texture contains color, tangent-space
normal, or generic data and can generate a complete deterministic mip chain
without applying color transfer rules to normal or data textures.

**Why this priority**: A byte-identical downsample is not semantically correct
for every texture type; incorrect color transfer or normal filtering produces
visible rendering errors and contaminates later cooking.

**Independent Test**: Generate mip chains from color, normal, and multi-channel
data fixtures, inspect every level, and compare semantic invariants and output
digests over repeated runs.

**Acceptance Scenarios**:

1. **Given** an sRGB color texture, **When** mips are generated, **Then**
   filtering occurs in linear-light meaning and each stored level retains the
   declared sRGB interpretation.
2. **Given** a tangent-space normal texture, **When** mips are generated,
   **Then** samples are treated as vectors, normalized output remains valid,
   and no sRGB transfer is applied.
3. **Given** a generic data texture, **When** mips are generated, **Then** each
   channel is filtered as linear data without color conversion or vector
   renormalization.
4. **Given** odd or non-square dimensions, **When** a complete chain is
   requested, **Then** every next level has deterministic clamped dimensions
   until a single 1x1 level is reached.

---

### User Story 3 - Realize Texture Assets Through Renderer and RHI (Priority: P2)

An engine developer can pass a validated texture asset to Renderer and receive
a compatible RHI texture populated with the requested mip levels, without the
Asset layer knowing about RHI, a native graphics API, or device lifetime.

**Why this priority**: CPU assets become useful to the existing rendering stack
only through a dependency-safe GPU realization boundary.

**Independent Test**: Feed representative validated texture assets to a mock RHI
adapter, verify resource descriptions and upload regions, and run supported
native readback smoke coverage where the platform graphics path is available.

**Acceptance Scenarios**:

1. **Given** a validated 2D texture asset and a compatible RHI device, **When**
   Renderer realizes it, **Then** the created texture description and each
   upload region match the asset's extent, mip count, texel interpretation, and
   content.
2. **Given** an asset representation the selected device cannot realize,
   **When** creation is requested, **Then** realization fails with an actionable
   compatibility diagnostic and does not publish a partial GPU resource.
3. **Given** GPU resource destruction or device invalidation, **When** the
   realized texture is released, **Then** the CPU asset remains valid and no
   GPU ownership is retained by Asset.
4. **Given** an upload failure at any mip level, **When** realization aborts,
   **Then** all resources created for that request are released and the caller
   receives the failed level and operation stage.

---

### User Story 4 - Reject Unsafe Inputs and Extend Formats Safely (Priority: P3)

An engine maintainer can diagnose missing, malformed, unsupported, or excessive
image inputs and can add future source-image strategies without changing the
core image, texture, registry, or Renderer contracts.

**Why this priority**: Source decoders process untrusted structured bytes, and
the roadmap requires later TGA, cubemap, array, and volume support without
turning this feature into a format-specific monolith.

**Independent Test**: Run malformed, truncated, contradictory, oversized, and
missing fixtures through importer selection and decoding; register a synthetic
future importer; verify stable failures, atomic publication, and unchanged
production dependency boundaries.

**Acceptance Scenarios**:

1. **Given** a missing source or unsupported format, **When** import is
   requested, **Then** the result distinguishes resolution failure from
   unsupported content and identifies the logical source.
2. **Given** malformed, truncated, overflow-inducing, non-finite, or excessive
   image data, **When** it is decoded, **Then** processing stops within declared
   limits, reports format and stage, and publishes no partial metadata or
   payload.
3. **Given** two supported image importers with equal strongest content claims,
   **When** selection occurs, **Then** Feature 020 ambiguity behavior is
   preserved rather than selecting by registration order or file extension.
4. **Given** a synthetic future image format or texture shape, **When** its
   importer is registered, **Then** it can participate in discovery without
   modifying existing PNG, JPEG, HDR, image-payload, or registry code.

### Edge Cases

- A file has no extension, an uppercase extension, or an extension that
  contradicts its content signature.
- A source is empty, truncated at each structural boundary, contains trailing
  bytes, has invalid checksums, or claims unsupported format features.
- Width, height, row pitch, mip byte size, or total decoded size is zero,
  excessive, or overflows intermediate arithmetic.
- A valid image has a single pixel, one-pixel-wide dimensions, odd dimensions,
  extreme aspect ratio, grayscale, grayscale-plus-alpha, RGB, or RGBA content.
- A JPEG source carries orientation or color metadata; an image declares a
  transfer/profile the feature cannot represent as explicit linear or sRGB.
- A PNG contains palette, transparency, interlace, or higher-precision source
  data that must be normalized or rejected explicitly rather than guessed.
- An HDR source contains negative, NaN, infinite, or extremely large channel
  values.
- Alpha is fully opaque, fully transparent, or varies independently from color;
  import and mip generation must not silently change its association model.
- A normal sample decodes to a zero-length vector, lies outside the representable
  unit range, or averages to an indeterminate direction.
- Mip generation receives a partial chain, a chain with inconsistent extents or
  texel interpretations, or a request that includes/excludes the base level.
- The same source is imported as color, normal, and data, producing different
  texture interpretations without identity collision.
- Import succeeds but registry publication conflicts with an existing output;
  all outputs from the request remain atomic.
- The selected decoder fails after discovery or while another thread unregisters
  its importer; Feature 020 execution-lease guarantees remain intact.
- Renderer receives an empty chain, unsupported texel interpretation, invalid
  row pitch, incompatible device capability, or injected upload failure.
- Multiple threads import the same immutable source and settings concurrently;
  payloads and diagnostics remain deterministic and do not share mutable decoder
  state unsafely.

## Architecture & Design Constraints *(mandatory)*

- **Asset Boundary**: Production code owned by Asset MUST depend only on Core.
  It MUST NOT include or link RHI, Renderer, Application, Backend, Tools, a
  platform graphics API, or device-specific format enums.
- **CPU-Side Ownership**: Asset owns source-image interpretation, validated CPU
  payloads, semantic metadata, and mip descriptions. It MUST NOT create, cache,
  retain, or destroy GPU resources.
- **GPU Realization Boundary**: Texture-to-RHI conversion MUST be owned by
  Renderer and expressed through backend-neutral RHI contracts. It MUST NOT call
  Vulkan or another native graphics API directly.
- **Responsibility Separation**: Format probing, source decoding, payload
  validation, semantic mip generation, asset publication, and GPU realization
  MUST remain independently testable responsibilities.
- **Feature 020 Integration**: Concrete importers MUST use existing stable
  identity, metadata, dependency, dispatch, registration, and execution-lease
  contracts rather than creating a parallel image catalog.
- **Format Extensibility**: PNG, JPEG, and HDR support MUST use replaceable
  importer/decoder strategies. Public image and texture contracts MUST leave
  room for later source formats and texture shapes without claiming those shapes
  are supported in Feature 021.
- **Semantic Integrity**: Color transfer, normal-vector treatment, and generic
  data treatment MUST be explicit. No code path may infer sRGB solely from a
  channel count or apply it to normal/data textures.
- **Determinism**: Equivalent source bytes and import settings MUST produce
  byte-identical normalized CPU payloads, metadata, mip outputs, version
  evidence, and stable diagnostics on Windows, macOS, and Linux.
- **Bounded Processing**: Probing and decoding MUST validate all dimensions and
  byte arithmetic before allocation, enforce configurable resource limits, and
  fail atomically when those limits are exceeded.
- **Current Texture Scope**: Only complete, single-sample 2D texture payloads
  are supported. Cubemaps, arrays, volumes, sparse textures, and runtime partial
  residency remain future extensions.
- **Future Rendering Compatibility**: Payload semantics and mip descriptions
  MUST remain suitable as inputs to later material, KTX2, streaming, mesh/model,
  ray-tracing, and GI phases without embedding those systems here.
- **Naming Conventions**: Public code design MUST follow PascalCase,
  Unreal Engine-style naming conventions.
- **Cross-Platform Compatibility**: Import, validation, mip generation, and
  mock-RHI realization MUST compile and run on Windows, macOS, and Linux.
  Platform-specific codec or graphics behavior MUST remain behind the owning
  implementation boundary.
- **Automated Cross-Platform Validation**: Windows, macOS, and Linux automation
  MUST build and run focused deterministic image/texture tests and preserve the
  existing regression suite. Native visual validation is not required for
  source decoding; available native texture readback MAY supplement mock-RHI
  evidence.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST provide validated CPU-side image assets containing
  typed identity, source provenance, dimensions, channel/texel interpretation,
  color-space declaration, alpha interpretation, row layout, and immutable
  base-level content. Canonical LDR texel interpretations MUST include R8, RG8,
  RGB8, and RGBA8; canonical HDR interpretations MUST include RGB32F and
  RGBA32F and MUST also permit RGBA16F.
- **FR-002**: The system MUST provide CPU-side 2D texture assets containing a
  typed identity, semantic usage, declared color space, ordered mip records,
  source-image dependency, and version evidence without containing a GPU handle.
- **FR-003**: Image and texture payloads MUST reject zero dimensions, unsupported
  texel interpretations, inconsistent row or slice layout, insufficient or
  excess payload bytes, arithmetic overflow, and mip records inconsistent with
  the declared base extent.
- **FR-004**: Concrete source importers MUST support PNG, JPEG, and Radiance HDR
  content and MUST use Feature 020 hint filtering plus bounded content probing;
  an extension MUST be treated only as a hint and MUST NOT override a unique
  content-based result.
- **FR-005**: Import discovery MUST identify at least one image output and one
  dependent 2D texture output with stable typed identities before any registry
  mutation. All outputs from one import request MUST publish atomically.
- **FR-006**: Import settings MUST explicitly declare color, tangent-space
  normal, or generic data semantic and the intended linear or sRGB
  interpretation where applicable. Missing or contradictory settings MUST
  produce a stable validation result rather than a heuristic guess.
- **FR-007**: Color textures MAY be linear or sRGB. Normal and generic data
  textures MUST be linear and MUST reject an sRGB declaration.
- **FR-008**: PNG and JPEG color content without a representable explicit
  transfer declaration MUST default to sRGB; Radiance HDR content MUST default
  to linear. A source profile that cannot be represented as linear or sRGB MUST
  be reported as unsupported rather than silently approximated.
- **FR-009**: Source alpha MUST be represented as straight/unassociated alpha.
  Importers MUST preserve alpha where present, declare when it is absent, and
  MUST NOT silently premultiply or discard it.
- **FR-010**: Source orientation and origin conventions MUST be normalized to
  the DX/Unreal-style top-left asset-space convention, where `UV(0,0)` samples
  the top-left texel. The applied transformation, including any source
  orientation metadata, MUST be deterministic and inspectable so texture
  sampling does not vary by source format or host platform. Renderer MUST NOT
  apply a second format-specific vertical flip.
- **FR-011**: Importers MUST normalize supported source layouts into the feature's
  declared CPU texel interpretations without changing semantic meaning, and
  MUST report unsupported precision or channel layouts explicitly. Three-channel
  RGB source content MUST remain RGB in the CPU asset rather than being expanded
  solely for GPU alignment. HDR imports MUST default to RGBA16F; an explicit
  high-precision import setting MUST select RGBA32F, while RGB32F is permitted
  only through an explicit no-alpha layout setting.
- **FR-012**: The system MUST generate a complete optional mip chain from the
  validated base level down to 1x1, with each next dimension equal to
  `max(1, floor(previous / 2))`. Full-chain generation MUST be the default;
  callers MAY explicitly select a base-only texture.
- **FR-013**: sRGB color mip generation MUST filter RGB in linear-light meaning
  and preserve the declared storage interpretation; linear color and alpha MUST
  be filtered without an sRGB transfer applied to alpha.
- **FR-014**: Tangent-space normal mip generation MUST decode samples as signed
  vectors, filter them as vectors, renormalize each output sample, and use one
  documented deterministic fallback for an indeterminate zero-length result.
- **FR-015**: Generic data mip generation MUST filter channels independently as
  linear numeric data without color conversion, vector renormalization, or
  channel-role assumptions.
- **FR-016**: Mip generation MUST be deterministic for odd, non-square, and
  one-pixel-wide extents and MUST produce the same bytes, level dimensions, and
  diagnostics across supported platforms.
- **FR-017**: Import and mip operations MUST expose immutable settings in their
  version evidence so a semantic, transfer, orientation, alpha, or mip-policy
  change, including an explicit base-only selection or HDR precision/layout
  selection, invalidates imported content without changing stable identity.
- **FR-018**: Probing and decoding MUST enforce configurable maximum width,
  height, source-file bytes, per-level bytes, and total decoded-chain bytes;
  defaults MUST be 16,384 pixels per dimension, 256 MiB source bytes, 512 MiB
  per mip, and 1 GiB for the complete decoded chain. Callers MAY explicitly
  raise a limit, but MUST NOT disable limits or checked size arithmetic before
  reading, allocation, or indexing.
- **FR-019**: Missing, inaccessible, unsupported, malformed, truncated,
  excessive, non-finite HDR, and processing-failure inputs MUST produce distinct
  stable result categories and actionable diagnostics identifying source,
  format participant, stage, and relevant limit or field.
- **FR-020**: A failed probe, decode, validation, mip operation, or registry
  publication MUST publish no partial asset records and MUST release all
  temporary payload storage.
- **FR-021**: Decoder and mip-generation operations MUST be safe for concurrent
  requests against immutable inputs and MUST NOT depend on process-global
  mutable state or caller registration order.
- **FR-022**: Renderer MUST provide a texture realization adapter that maps a
  validated texture asset to a backend-neutral RHI texture description and
  complete ordered upload plan without adding an RHI dependency to Asset. When
  the target RHI cannot realize a canonical three-channel Asset format directly,
  any required channel expansion MUST occur in Renderer realization and MUST
  leave the CPU asset unchanged.
- **FR-023**: The Renderer adapter MUST validate RHI format and extent
  compatibility before creating a resource, upload every requested mip with
  matching extent and layout, and return the created resource only after all
  required uploads succeed.
- **FR-024**: A failed realization MUST release resources created by that
  request, identify the failed stage and mip when applicable, and leave the CPU
  image and texture assets valid. Cancellation is outside this synchronous
  realization API and belongs to Feature 026's asynchronous asset lifecycle.
- **FR-025**: Asset inspection MUST provide deterministic human-readable image,
  texture, semantic, color-space, alpha, mip, importer, and diagnostic summaries
  without dumping arbitrary source bytes or native addresses.
- **FR-026**: Focused automated tests MUST cover all supported formats,
  semantics, representative channel layouts, alpha, odd dimensions, complete
  mip chains, malformed/truncated/excessive inputs, importer ambiguity,
  atomic publication, deterministic repetition, and mock-RHI realization and
  rollback.
- **FR-027**: Windows, macOS, and Linux automation MUST build the feature, run
  focused image/texture tests, run architecture-boundary validation, and retain
  all existing regression outcomes.
- **FR-028**: KTX2, block compression, device-specific compressed-format
  negotiation, runtime mip streaming, virtual textures, cubemaps, arrays,
  volumes, sparse textures, initial TGA support, persistent asset management,
  and editor UI MUST remain outside this feature.

### Key Entities

- **Image Asset**: Validated immutable CPU image payload with source provenance,
  dimensions, texel interpretation, color space, alpha interpretation, layout,
  and base-level content.
- **Texture Asset**: Semantic 2D texture description containing an ordered mip
  chain and dependency on its image source, but no GPU ownership.
- **Image Mip**: One immutable level with extent, validated layout, and texel
  content in the texture's declared interpretation.
- **Texture Semantic**: Explicit meaning of the samples: color, tangent-space
  normal, or generic data.
- **Color Space**: Explicit linear or sRGB interpretation governing color
  conversion and filtering behavior.
- **Alpha Interpretation**: Declaration of absent or straight/unassociated alpha.
- **Image Import Settings**: Immutable semantic, transfer, orientation, alpha,
  and mip policy that contributes to content version evidence.
- **Source Image Importer**: Registered Feature 020 participant responsible for
  format probing, decoding, discovery, and stable failure reporting.
- **Texture Realization Request**: Renderer-owned request that converts a
  validated texture asset into an RHI texture and upload plan.
- **Image Diagnostic**: Stable operation stage, category, source, importer,
  field/limit, and actionable reason for a failed image or texture operation.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A fixture corpus containing at least 12 valid files across PNG,
  JPEG, and HDR, including grayscale, alpha, odd dimensions, and HDR range,
  produces the expected image and texture outputs in 100% of cases on Windows,
  macOS, and Linux.
- **SC-002**: Across 20 repeated imports of every valid fixture, normalized
  metadata, decoded base-level bytes, mip bytes, version evidence, and
  diagnostics are identical in all runs and across supported platforms.
- **SC-012**: Every HDR fixture imported without an explicit precision setting
  produces RGBA16F; the same fixture imported with explicit RGBA32F or RGB32F
  settings produces that exact requested canonical layout, and each layout
  selection has distinct imported-content version evidence.
- **SC-003**: For every color, normal, and data fixture, 100% of generated mip
  chains follow the required dimension recurrence and terminate at exactly one
  1x1 level without a missing or duplicate extent.
- **SC-004**: Semantic validation rejects 100% of tested sRGB normal/data
  declarations; decoded accepted normal-map mip samples are finite and satisfy
  `abs(length - 1.0) <= 0.015`, including the documented +Z fallback. CPU
  color/data fixtures match their canonical reference bytes exactly.
- **SC-005**: At least 30 malformed, truncated, contradictory, overflow-
  inducing, non-finite, missing, unsupported, and excessive fixtures produce
  the expected stable result category in 100% of cases and publish zero partial
  registry records. Boundary tests accept values at every default limit and
  reject the first value above each limit before a payload allocation occurs.
- **SC-006**: Importer dispatch tests covering correct, absent, uppercase,
  misleading, and ambiguous extensions select the unique content match or the
  expected ambiguity/unsupported result in 100% of cases regardless of
  registration order.
- **SC-007**: Mock-RHI tests realize representative linear, sRGB, normal, data,
  single-level, and complete-chain assets with 100% agreement between asset and
  created resource extents, mip counts, texel interpretations, and upload
  regions.
- **SC-008**: Failure injection at every realization stage and at every mip
  upload publishes zero partial GPU resources, releases every request-owned
  resource exactly once, and leaves the source CPU asset inspectable.
- **SC-009**: Automated architecture validation reports zero Asset production
  dependencies on RHI, Renderer, Application, Backend, Tools, native graphics
  APIs, or device-specific format constants.
- **SC-010**: Under at least 8 concurrent import or mip requests for immutable
  fixtures, all operations complete without crash, hang, ThreadSanitizer report
  in the Linux focused-concurrency gate, or divergence from single-request
  output.
- **SC-011**: Windows, macOS, and Linux automated jobs pass focused
  image/texture tests and the existing full regression suite. Available native
  texture-readback smoke evidence compares UNorm channels within one LSB
  (`1 / 255`), FP16 channels within `max(1e-3, abs(expected) * 1e-3)`, and FP32
  channels within `max(1e-6, abs(expected) * 1e-6)`; it is not required on a
  platform without native graphics capability.

## Assumptions

- This feature serves engine developers and later material/model/cooking
  systems; it does not expose an end-user asset browser or editing workflow.
- PNG, JPEG, and Radiance HDR are source interchange formats. They do not become
  the cooked runtime texture container; KTX2 and compression belong to Feature
  022.
- The exact codec library, mip reconstruction filter, normal fallback vector,
  and comparison tolerances are planning decisions. They must satisfy the
  observable semantic, determinism, safety, and cross-platform requirements in
  this specification.
- Default import guardrails are 16,384 pixels per dimension, 256 MiB source
  bytes, 512 MiB per mip, and 1 GiB for a complete decoded mip chain. Callers
  may explicitly raise them for controlled tools, but cannot disable limits or
  checked arithmetic.
- Canonical CPU texel formats are R8, RG8, RGB8, and RGBA8 for LDR, plus
  RGB32F, RGBA32F, and RGBA16F for HDR. Retaining RGB is an Asset contract;
  Renderer may expand unsupported three-channel formats during realization
  without mutating the source asset.
- HDR imports default to RGBA16F. RGBA32F is an explicit high-precision choice,
  and RGB32F is an explicit no-alpha choice; all three layouts are distinct
  import settings and versioned payload contracts.
- Import callers explicitly provide semantic intent. Defaults for representable
  source transfer are sRGB for PNG/JPEG color and linear for HDR, while normal
  and data usages are always linear.
- Alpha remains straight/unassociated throughout Feature 021. Premultiplied
  storage and alpha-coverage-preserving mip policies are future explicit
  extensions rather than implicit behavior.
- A complete mip chain is generated by default. A valid base-only texture is
  permitted only when callers explicitly disable generation, and that setting
  contributes to the imported-content version evidence.
- Source orientation is normalized during import to the DX/Unreal-style top-left
  asset-space convention, where `UV(0,0)` addresses the top-left texel.
  Renderer does not add a second format-specific vertical flip.
- Feature 020 identity, registry, dependency, importer-dispatch, registration,
  execution-lease, diagnostic, and inspection contracts are authoritative and
  are extended rather than duplicated.
- RHI Feature 008 provides backend-neutral uncompressed 2D texture and upload
  contracts sufficient for Feature 021. Any compressed format or capability
  expansion is deferred to Feature 022.
- Native visual proof is required only on Windows and macOS when a later feature
  changes visible rendering. For this CPU-heavy foundation, three-platform
  deterministic tests and mock-RHI evidence are the primary gate; native
  readback is supplementary where available.
