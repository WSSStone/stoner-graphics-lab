# Data Model: Deferred Rendering Pipeline

## Overview

Feature 019 owns no persistent scene or asset database. Its model represents one prepared deferred frame, its semantic surface layout, deterministic draw/light decisions, render-graph declaration, transient execution bindings, native readback probes, diagnostics, and one optional forward/deferred comparison report. Stable IDs and normalized fields may appear in dumps; native graphics handles never do.

## Enumerations

### `EDeferredResult`

- `Success`
- `InvalidConfiguration`
- `InvalidView`
- `InvalidOutput`
- `InvalidSurfaceLayout`
- `InvalidDraw`
- `InvalidMaterial`
- `InvalidLight`
- `InvalidBinding`
- `GraphCompilationFailed`
- `RecordFailed`
- `SubmitFailed`
- `ReadbackFailed`
- `ValidationFailed`
- `ComparisonInvalid`

### `EDeferredPassStage`

- `SurfaceData`
- `DirectionalLighting`
- `PointLightVolumes`
- `SpotLightVolumes`
- `Composition`
- `ForwardTransparency`
- `ValidationReadback`

The canonical order follows this enumeration. Empty directional/local/transparent stages may be omitted, but `SurfaceData` precedes every lighting stage and `Composition` follows all lighting stages.

### `EDeferredSurfaceSemantic`

- `BaseColor`
- `ViewNormal`
- `Metallic`
- `Roughness`
- `Emissive`
- `AmbientOcclusion`
- `Depth`

### `EDeferredLightType`

- `Directional`
- `Point`
- `Spot`

### `EDeferredLightAcceptance`

- `AcceptedFullView`
- `AcceptedVolumeOutsideCamera`
- `AcceptedVolumeCameraInside`
- `AcceptedVolumeNearPlaneIntersection`
- `CulledOutsideView`
- `RejectedInvalid`

### `EDeferredExecutionState`

- `Uninitialized`
- `BindingsValidated`
- `Recording`
- `Recorded`
- `Submitted`
- `Completed`
- `ReadbackReady`
- `Failed`
- `Released`

## Entity: Deferred Renderer Configuration

Defines immutable planning policy for one renderer instance.

| Field | Type | Default | Validation |
|-------|------|---------|------------|
| `SurfaceLayout` | Surface Data Layout | Initial layout below | Must include every required semantic exactly once |
| `bEnableMaskedGeometry` | boolean | `true` | Alpha controls coverage only |
| `bEnableForwardTransparencyHandoff` | boolean | `true` | Transparent draws are never accepted into surface data |
| `bCullLocalLightsOutsideView` | boolean | `true` | Required for deterministic omission records |
| `LightOrderPolicy` | canonical policy | type/influence/identity | Stable identity is final tie-breaker |
| `SampleCount` | integer | `1` | Exactly one in Feature 019 |

## Entity: Surface Data Layout

Defines one compatibility identity shared by the surface, lighting, and validation stages.

| Field | Type | Initial value | Rule |
|-------|------|---------------|------|
| `LayoutId` | stable value identity | derived from ordered semantics/formats | Equal layouts produce equal IDs |
| `Extent` | positive width/height | active output extent | All attachments must match |
| `SampleCount` | integer | `1` | All attachments must match |
| `BaseColorAOFormat` | RHI format | `R8G8B8A8_UNorm` | RGB base color; A AO |
| `NormalRoughnessFormat` | RHI format | `R16G16B16A16_Float` | XYZ view normal; W roughness |
| `EmissiveMetallicFormat` | RHI format | `R16G16B16A16_Float` | RGB emissive; A metallic |
| `DepthFormat` | RHI format | `D32_Float` | Normalized depth |
| `NormalSpace` | coordinate-space enum | view space | Must match lighting reconstruction |
| `DepthConvention` | canonical summary | active RHI normalized convention | Must match view matrices |
| `ClearValues` | semantic values | research table | Empty pixels remain distinguishable |

Relationships:

- One layout owns four ordered attachment declarations.
- One deferred frame plan references exactly one layout.
- Every bound surface texture must match the layout identity, extent, sample count, format, and required usage.

## Entity: Deferred Frame Inputs

| Field | Type | Rule |
|-------|------|------|
| `View` | existing forward-compatible view data | Finite matrices, camera position, viewport, and positive extent |
| `OutputTarget` | stable output summary | Valid identity, positive extent, final color compatibility |
| `OpaqueDraws` | ordered draw candidates | Opaque or compatible masked material only |
| `TransparentDraws` | ordered draw candidates | Passed to existing forward-transparent ordering only |
| `DirectionalLights` | array | Every valid view-affecting light represented |
| `PointLights` | array | No count cap; invalid/outside-view records retained |
| `SpotLights` | array | No count cap; invalid/outside-view records retained |
| `AmbientContribution` | finite linear color/intensity | May be zero |
| `StableFrameId` | stable value | Must not contain a native handle |

## Entity: Deferred Draw Record

| Field | Type | Rule |
|-------|------|------|
| `StableObjectId` | stable identity | Final draw-order tie-breaker |
| `StableMaterialId` | stable identity | References shared material/instance semantics |
| `MaterialBinding` | compatible material binding | Provides required surface values and shader permutation |
| `WorldTransform` | finite transform | Must produce a valid drawable transform |
| `Bounds` | finite sphere or box | Used for view rejection only |
| `BlendMode` | opaque/masked/transparent | Transparent never enters surface stage |
| `MaskedCutoff` | normalized scalar | Used only for masked draws |
| `Acceptance` | accepted/rejected record | Includes stable reason |

Ordering:

1. Accepted opaque/masked draws use stable material/pipeline compatibility order.
2. Stable object/entity identity is the final tie-breaker.
3. Transparent draws preserve the existing camera-space forward ordering after composition.

## Entity: Deferred Light Record

Shared fields:

| Field | Type | Rule |
|-------|------|------|
| `StableLightId` | stable entity identity | Final ordering tie-breaker |
| `Type` | `EDeferredLightType` | One canonical value |
| `Color` | finite non-negative linear RGB | At least one positive channel for accepted light |
| `Intensity` | finite non-negative scalar | Zero may be omitted with a stable reason |
| `Acceptance` | `EDeferredLightAcceptance` | Exactly one result |
| `InfluenceKey` | deterministic tuple | No pointer/address components |

Directional fields: normalized non-degenerate direction.

Point fields: finite world position and positive finite range; influence shape is a sphere.

Spot fields: finite world position, normalized non-degenerate direction, positive finite range, and finite inner/outer cone angles where `0 <= Inner <= Outer < 90 degrees`; influence shape is a cone.

Ordering:

1. Directional, then point, then spot.
2. Deterministic influence key within type.
3. Stable entity identity as final tie-breaker.

## Entity: Deferred Frame Plan

| Field | Type | Rule |
|-------|------|------|
| `FrameId` | stable identity | Copied from inputs |
| `ValidationState` | valid/invalid | Invalid plans cannot execute |
| `SurfaceLayout` | Surface Data Layout | Exactly one valid layout |
| `AcceptedDraws` | ordered array | Opaque/masked only |
| `RejectedDraws` | ordered array | Stable reason per item |
| `AcceptedLights` | ordered array | Every valid view-affecting light |
| `CulledLights` | ordered array | Outside-view or zero-contribution reason |
| `RejectedLights` | ordered array | Invalid-input reason |
| `Passes` | ordered pass records | Canonical dependency order |
| `TransparentHandoff` | optional forward plan fragment | Runs after composition |
| `Diagnostics` | ordered records | First actionable failure owns result |
| `InputFingerprint` | normalized hash/value identity | Used by comparison validation |

Validity:

- View, output, surface layout, and all required material bindings are compatible.
- Every accepted draw/light appears exactly once.
- No rejected draw/light appears in executable pass counts.
- Pass dependencies are acyclic and composition produces exactly one final output.

## Entity: Deferred Pass Record

| Field | Type | Rule |
|-------|------|------|
| `Stage` | `EDeferredPassStage` | Canonical order |
| `PassId` | stable identity | Unique within frame plan |
| `ReadResources` | ordered semantic IDs | Explicit and duplicate-free |
| `WriteResources` | ordered semantic IDs | Explicit and duplicate-free |
| `DrawCount` | non-negative integer | Matches accepted stage work |
| `LightCount` | non-negative integer | Matches accepted stage lights |
| `CullEligible` | boolean | Empty local/transparent/readback work may be culled |

## Entity: Deferred Graph Declaration

Contains render graph-compatible virtual/imported resources, accesses, pass dependencies, transition requirements, culling decisions, and exactly one final output mapping. Surface color/depth and lighting accumulation are transient unless imported explicitly for validation readback.

Rules:

- Every pass read has a prior write or imported-resource declaration.
- Surface attachments share extent/sample count.
- Lighting reads all required surface semantics and writes accumulation.
- Composition reads accumulation plus required emissive/base surface data and writes final output.
- Readback depends on the last writer of each requested target.

## Entity: Deferred Frame Execution Bindings

Groups backend-neutral RHI objects by role.

| Group | Required bindings |
|-------|-------------------|
| Surface | surface textures, depth texture, surface render pass/framebuffer/pipeline, geometry vertex/index/uniform resources |
| Directional | sampled surface descriptors, light/view buffer, accumulation target, pipeline, fullscreen geometry |
| Point volume | sampled surface descriptors, point-light data, accumulation target, sphere vertex/index buffers, outside/inside pipeline variants |
| Spot volume | sampled surface descriptors, spot-light data, accumulation target, cone vertex/index buffers, outside/inside pipeline variants |
| Composition | sampled accumulation/surface descriptors, final output, composition pipeline, fullscreen geometry |
| Transparency | optional existing forward-transparent bindings |
| Readback | copy-source textures and host-visible destination buffers with copy regions |
| Submission | command buffer, graphics queue, completion fence |

Bindings are validated as one set before command recording. A resource cannot appear with an incompatible format, extent, usage, lifecycle state, or layout identity.

## Entity: Deferred Execution Result

| Field | Type | Rule |
|-------|------|------|
| `Result` | `EDeferredResult` | First non-success stage owns it |
| `FinalState` | `EDeferredExecutionState` | Terminal state is completed, failed, or released |
| `LastCompletedStage` | optional pass stage | No later stage may claim success |
| `RecordedPassCount` | integer | Equals non-culled recorded passes |
| `RecordedDrawCount` | integer | Sum of recorded stage draws |
| `RecordedCommandCount` | integer | Stable for equivalent deterministic bindings |
| `ReadbackProbeCount` | integer | Zero unless readback completed |
| `Diagnostics` | ordered records | No native addresses |

State transitions:

```text
Uninitialized -> BindingsValidated -> Recording -> Recorded -> Submitted -> Completed
Completed -> ReadbackReady
Any pre-release state -> Failed
Completed | ReadbackReady | Failed -> Released
```

## Entity: Readback Probe

| Field | Type | Rule |
|-------|------|------|
| `ProbeName` | stable text identity | Unique in validation scene |
| `TargetSemantic` | surface/final semantic | Identifies decode/tolerance policy |
| `PixelX`, `PixelY` | integer coordinates | Inside target extent |
| `ExpectedValue` | finite scalar/vector | Declared by reference scene |
| `ObservedValue` | finite scalar/vector | Decoded from mapped staging bytes |
| `TolerancePolicy` | semantic policy | Color, depth, normal, or scalar |
| `bPassed` | boolean | Derived, not caller supplied |
| `FailureReason` | normalized text | Empty only on success |

Tolerance policies:

- Final LDR color: absolute error `<= 2/255` per channel.
- Normalized depth: absolute error `<= 1e-4`.
- Decoded view normal: normalized dot product `>= 0.999`.
- Metallic, roughness, ambient occlusion: absolute error `<= 1e-3`.
- Any non-finite value: fail.

## Entity: Renderer Comparison Report

| Field | Type | Rule |
|-------|------|------|
| `SceneFingerprint` | normalized identity | Equal for forward/deferred pair |
| `ViewFingerprint` | normalized identity | Equal for pair |
| `MaterialFingerprint` | normalized identity | Equal for pair |
| `LightTier` | integer | One of `0`, `16`, `64`, `256` local lights |
| `WarmupFrameCount` | positive integer | Excluded from statistics |
| `MeasuredFrameCount` | integer | At least `100` |
| `ForwardMedian`, `ForwardP95` | duration | Non-negative finite values |
| `DeferredMedian`, `DeferredP95` | duration | Non-negative finite values |
| `ForwardWorkload` | pass/draw/light counts | Retained for equivalence inspection |
| `DeferredWorkload` | pass/draw/light counts | Surface draw count independent of light tier |
| `CrossoverClassification` | result | Forward faster, deferred faster, equal-within-resolution, or not observed |
| `Validity` | valid/invalid | Any fingerprint/count mismatch invalidates report |

## Entity: Deferred Diagnostic

| Field | Type | Rule |
|-------|------|------|
| `Sequence` | monotonic integer | Starts at one per operation |
| `Stage` | planner/graph/execution/readback/comparison stage | Stable enum |
| `Severity` | info/warning/error | First error owns failure result |
| `Result` | stable result category | No backend-native code |
| `SubjectId` | stable value identity | No pointer/address |
| `Reason` | normalized text | Actionable and deterministic |

## Ownership and Cleanup

- Frame plans, graph declarations, diagnostics, and reports are process-local value data.
- The native offscreen session is the lifecycle Composite root for frame-owned RHI/native objects.
- Readback buffers remain alive until completion is proven and probe decoding finishes.
- Cleanup stops submission, waits for completion when required, unmaps/release staging, then releases descriptors, buffers, framebuffers, render passes, pipelines/layouts/shaders, image views/images/memory, command/fence state, device, and instance in reverse dependency order.
- Final validation requires zero live deferred frame-owned objects even after partial initialization or failed readback.
