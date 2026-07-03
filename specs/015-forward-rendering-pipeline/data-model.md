# Data Model: Forward Rendering Pipeline

## Forward Renderer

**Purpose**: Coordinates validation and preparation of one forward-rendered frame plan from view, light, material, draw, environment, and output inputs.

**Key fields**:

- `Name`: Optional deterministic debug name.
- `Configuration`: Forward renderer settings, including point light limit and fallback policies.
- `FrameInputs`: Current view, output, draw, light, and environment inputs.
- `PreparedPlan`: Most recent prepared forward frame plan.
- `Diagnostics`: Validation, culling, sorting, fallback, and declaration diagnostics.

**Validation rules**:

- A renderer can prepare a frame only when view data and output target summary are valid.
- Default point light limit is 4 unless explicitly configured.
- Preparing equivalent inputs must produce byte-stable diagnostics and dumps.
- The renderer must not require real GPU execution or presentation to prepare a frame.

## Forward Frame Plan

**Purpose**: Represents one prepared Renderer-level workload for the forward pipeline.

**Key fields**:

- `FrameName`: Stable frame/debug identifier.
- `ViewData`: Validated active view.
- `OutputTarget`: Abstract final color/depth target summary.
- `PassOrder`: Depth, opaque, sky/background, and transparent stage records.
- `AcceptedOpaqueDraws`: Draw commands accepted for opaque rendering.
- `AcceptedTransparentDraws`: Draw commands accepted for transparent rendering.
- `AcceptedLights`: Directional light and selected point lights.
- `ResourceDeclarations`: Render graph-compatible pass/resource declaration summary.
- `Diagnostics`: Accepted, rejected, limited, and fallback records.
- `DebugDump`: Deterministic human-readable frame inspection text.

**Validation rules**:

- Depth declaration precedes opaque declaration when opaque draws exist.
- Transparent declaration follows opaque and sky/background declarations.
- Plans with no renderable geometry remain valid and contain clear/background behavior.
- Plans with geometry and no accepted lights remain valid only with the ambient-only fallback diagnostic.

## Forward Renderer Configuration

**Purpose**: Captures user-tunable forward preparation settings.

**Key fields**:

- `PointLightLimit`: Number of point lights accepted per frame; default 4.
- `AmbientFallback`: Constant ambient-only fallback policy for no accepted lights.
- `TransparentSortPolicy`: Camera-space depth descending, material id, object id.
- `bEnableSkyBackground`: Whether sky/background declaration is enabled.

**Validation rules**:

- Point light limit must be non-negative.
- A zero point light limit rejects all point lights deterministically while preserving directional light behavior.
- Transparent sort policy is fixed for this feature and must not depend on caller submission order.

## Forward View Data

**Purpose**: Represents camera and viewport inputs needed to prepare view-dependent work.

**Key fields**:

- `ViewName`: Required debug name.
- `ViewTransform`: Camera transform or equivalent world-to-view information.
- `ViewProjection`: View-projection information.
- `CameraPosition`: Camera world position.
- `ViewportWidth`: Positive viewport width.
- `ViewportHeight`: Positive viewport height.

**Validation rules**:

- View name must be present.
- View-projection and camera position must be valid and finite.
- Viewport dimensions must be positive.
- Invalid view data prevents frame preparation.

## Forward Output Target

**Purpose**: Describes the abstract frame output without owning backend resources.

**Key fields**:

- `ColorTargetName`: Stable abstract color target identifier.
- `DepthTargetName`: Optional stable abstract depth target identifier.
- `FormatSummary`: Renderer/RHI-compatible output format intent.
- `Extent`: Width and height expected for the frame.

**Validation rules**:

- Color target name and extent must be present.
- Extent must be compatible with view viewport dimensions.
- Output target summaries must not hold backend-specific resources or swapchain handles.

## Forward Light Set

**Purpose**: Owns validated directional and point light data selected for one frame.

**Key fields**:

- `DirectionalLight`: Optional primary directional light.
- `SubmittedPointLights`: Ordered input point light records.
- `AcceptedPointLights`: Point lights selected after deterministic influence ordering.
- `RejectedPointLights`: Point lights excluded by validation or configured limit.
- `AmbientFallbackRecord`: Present when no lights are accepted.
- `Diagnostics`: Light validation, selection, and fallback messages.

**Validation rules**:

- At most one primary directional light is accepted.
- Multiple directional light candidates produce a deterministic diagnostic.
- Point lights are sorted by deterministic influence such as distance and effectiveness before the front N are accepted.
- Accepted and rejected counts must be reported when submitted point lights exceed the configured limit.

## Directional Light

**Purpose**: Represents the primary directional light contribution for a frame.

**Key fields**:

- `LightId`: Stable identifier.
- `Direction`: Finite direction vector.
- `Color`: Color/intensity contribution.
- `Intensity`: Non-negative scalar intensity.

**Validation rules**:

- Direction must be valid and non-zero.
- Intensity must be non-negative.
- Only one primary directional light is accepted per frame.

## Point Light

**Purpose**: Represents one local point light candidate.

**Key fields**:

- `LightId`: Stable identifier.
- `Position`: Finite world position.
- `Color`: Color contribution.
- `Intensity`: Non-negative scalar intensity.
- `Range`: Positive influence range.
- `InfluenceScore`: Deterministic score derived from distance and effectiveness.

**Validation rules**:

- Position must be finite.
- Intensity must be non-negative.
- Range must be positive.
- Influence ordering must be stable for equal scores.

## Mesh Draw Command

**Purpose**: Represents a reusable Renderer-level draw description for a mesh/material/view combination.

**Key fields**:

- `ObjectId`: Stable object/draw identity.
- `MeshId`: Stable mesh identity.
- `MaterialBinding`: Valid material or material instance shader binding summary.
- `PassMask`: Depth, opaque, transparent, or combinations accepted by validation.
- `SortKeys`: Stable opaque and transparent sorting keys.
- `ResourceRequirements`: Material resource requirements needed by graph declarations.
- `ValidationState`: Draft, Accepted, Rejected, or Invalidated.

**Validation rules**:

- Object id and mesh id must be present.
- Material binding must be compatible with the requested pass.
- Opaque and transparent compatibility must follow material domain/blend validation.
- Repeated equivalent mesh/material/view inputs produce stable draw identity and ordering.

## PBR Surface Input Set

**Purpose**: Describes the material inputs a forward opaque or transparent surface must expose.

**Key fields**:

- `BaseColor`
- `Metallic`
- `Roughness`
- `NormalInput`
- `Occlusion`
- `Emissive`
- `Alpha`
- `ExtensionSlots`

**Validation rules**:

- Required fields must be present for forward PBR validation.
- Extension slots must be named deterministically.
- Missing or type-incompatible surface inputs produce deterministic diagnostics.

## Environment Background

**Purpose**: Represents simple sky/background participation in a forward frame.

**Key fields**:

- `BackgroundMode`: Clear, gradient-like sky, or abstract environment reference.
- `BackgroundName`: Stable identifier.
- `ResourceRequirements`: Optional abstract resource references.

**Validation rules**:

- Missing environment data falls back to deterministic clear/background behavior.
- Background contribution must not overwrite accepted opaque geometry.
- Environment resource references must remain Renderer-level abstract identifiers.

## Forward Render Graph Declaration

**Purpose**: Summarizes the render graph-compatible passes and resources produced by frame preparation.

**Key fields**:

- `PassDeclarations`: Depth, opaque, sky/background, and transparent pass summaries.
- `ResourceDeclarations`: Color, depth, material, and environment resource summaries.
- `AccessDeclarations`: Read/write intent for each pass/resource relationship.
- `GraphOutputs`: Abstract final output target summaries.

**Validation rules**:

- Declarations must preserve the prepared pass order.
- Resource requirements from material bindings must be represented without backend handles.
- Declarations are inspectable and testable without executing the graph.

## Forward Diagnostic

**Purpose**: Structured message for frame validation, light selection, draw rejection, fallback, declaration, and dump behavior.

**Key fields**:

- `Severity`: Info, Warning, or Error.
- `Category`: View, Output, Light, Material, Draw, Pass, ResourceDeclaration, Fallback, or Dump.
- `SubjectName`: View, output, light, material, draw, pass, or resource involved.
- `Message`: Human-readable explanation.
- `StableCode`: Deterministic diagnostic identifier.

**Validation rules**:

- Invalid input diagnostics must identify the failing subject.
- No-light frames with valid geometry must produce exactly one ambient-only fallback diagnostic.
- Diagnostic ordering must be deterministic for repeated equivalent inputs.

## Forward Debug Dump

**Purpose**: Deterministic text summary for headless verification and developer inspection.

**Key fields**:

- `FrameSummary`
- `ViewSummary`
- `PassOrder`
- `OpaqueDrawSummary`
- `TransparentDrawSummary`
- `LightSelectionSummary`
- `ResourceDeclarationSummary`
- `Diagnostics`

**Validation rules**:

- Repeated dumps of unchanged inputs must be byte-identical.
- Dumps must include accepted/rejected draw counts, accepted/rejected light counts, pass order, and fallback decisions.
- Dumps must not include pointer values or unordered-container iteration order.

## State Transitions

```text
Draft
  ├── Prepare succeeds -> Prepared
  ├── Prepare fails    -> Failed
  └── Invalidate       -> Invalidated

Prepared
  ├── Reprepare succeeds -> Prepared
  ├── Reprepare fails    -> Failed
  ├── Reset              -> Draft
  └── Invalidate         -> Invalidated

Failed
  ├── Reset      -> Draft
  └── Invalidate -> Invalidated

Invalidated
  └── Reset -> Draft
```
