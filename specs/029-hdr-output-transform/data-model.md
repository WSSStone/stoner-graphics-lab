# Data Model: Renderer HDR Post-Processing & Output Transform

## 1. HDR SceneColor Handoff

One immutable producer endpoint from Forward or Deferred.

| Field | Type | Rules |
|---|---|---|
| `SceneColorId` | stable resource identity | Unique for view/frame |
| `RendererStrategy` | `Forward` or `Deferred` | Diagnostic only; cannot change output policy |
| `ViewId`, `FrameToken` | stable identities | Nonzero and same as formal output |
| `Extent` | uint32 pair | Nonzero, bounded, exact drawable/readback extent |
| `Format` | `RGBA16F` | No encoded or 8-bit handoff |
| `SampleCount` | integer | Exactly 1 in Feature 029 |
| `Primaries` | enum | `Rec709` |
| `WhitePoint` | enum | `D65` |
| `Transfer` | enum | `Linear` |
| `AlphaMode` | enum | `OpaqueOne` |
| `Resource` | Render Graph handle | Written once by producer, read by output pipeline |

State: `Declared -> ProducerBound -> Produced -> Consumed`; any invalid metadata,
duplicate producer, stale frame, or missing resource moves directly to `Failed`.

## 2. Output Transform Settings

| Field | Type | Rules |
|---|---|---|
| `ManualExposureStops` | float | Finite, `[-16,+16]`, negative zero normalized |
| `DynamicRange` | `SDR` or `HDR` | Selects exactly one transform family |
| `SDRToneMapVersion` | version ID or null | Required only for SDR; default resolves explicitly to `Sdr.KhronosPbrNeutral.v1` |
| `HDRViewingVersion` | version ID or null | Required only for HDR; initial value is `Hdr.ACES2.0.0_2025-04-04.Rec2020D65.v1` |
| `OutputDeviceProfileId` | stable profile ID | Exact match to one admitted profile |
| `PreTonemapOperations` | ordered declarations | Max 16 |
| `PostTonemapOperations` | ordered declarations | Max 16 |
| `bRequirePresentation` | bool | At least one output requirement true |
| `bRequireReadback` | bool | At least one output requirement true |
| `DiagnosticBypass` | optional stage selector | Produces non-authoritative output only |

Resolved settings are immutable. State: `Authored -> Validating -> Resolved` or
`Rejected`. `Resolved` stores every default as an explicit version ID.

## 3. Output Device Profile

| Field | Type | Rules |
|---|---|---|
| `ProfileId`, `ProfileVersion` | stable strings | Unique; semantic change requires new version |
| `DynamicRange` | enum | SDR or HDR |
| `TargetPrimaries`, `WhitePoint` | enums | Rec.709/D65 or Rec.2020/D65 |
| `ReferenceWhiteNits` | positive float | Fixed for SDR/scRGB; native-resolved for Metal EDR |
| `TargetPeakNits` | positive float | 100 for SDR policy, 1000/2000 for HDR |
| `Transfer` | enum | sRGB, BT.709 OETF, gamma 2.2, ST-2084, scRGB80, or Metal EDR linear |
| `StorageClass` | enum | UNorm8, Packed10UNorm, or Float16 |
| `NativeColorSpace` | RHI enum | Exact required interpretation |
| `MetadataPolicy` | enum | None, HDR10 static content intent resolved per backend, or EDR state with `EDRMetadata=nil`; Vulkan may apply native HDR10 metadata, Metal never maps it to `CAEDRMetadata` |
| `ComparisonDomain` | enum | Linear Rec.709 or absolute nits/XYZ |
| `Tolerances` | immutable policy | SDR `2/255`; HDR `max(0.02 nit,0.25%,M*Qnative)` with PQ `M=1.5`, FP16 `M=2.0`, and matrix-propagated XYZ; never a visual score |

The seven Renderer profiles are immutable configuration. One linear HDR logical
profile resolves to either `scRGB80` or `MetalEDR` native encoding; that resolved
choice is recorded rather than inferred from the format.

## 4. Transform Strategy

An immutable Strategy implementation selected by version identity.

| Field | Type | Rules |
|---|---|---|
| `VersionId` | stable ID | Exact registry match |
| `Family` | SDR tone map, HDR viewing, or output encoding | One responsibility |
| `InputDomain`, `OutputDomain` | color-domain descriptors | Must compose exactly |
| `ConstantsDigest` | SHA-256 | Binds equations/constants/provenance |
| `ShaderVariantId` | stable ID | Offline-cooked target-aware program |
| `ReferenceVectorSet` | digest | CPU oracle authority |

Unknown, duplicate, or deprecated-without-explicit-selection versions fail.

## 5. Post-Process Insertion Operation

| Field | Type | Rules |
|---|---|---|
| `OperationId` | stable ID | Unique in one frame plan |
| `InsertionPoint` | PreTonemap or PostTonemap | Fixes color domain |
| `OrderKey` | int32 | Unique at insertion point |
| `DependsOn` | ordered IDs | Same insertion point, acyclic |
| `Reads`, `Writes` | bounded resource declarations | No undeclared hazard |
| `Extent`, `SampleCount` | inherited invariants | Cannot change |
| `Strategy` | operation interface | No ownership of tone map/ODT/formal output |

The validated Composite sorts by dependency then order key with stable ID as a
diagnostic tie-breaker only; duplicate order keys are rejected rather than
silently ordered.

## 6. Output Transform Plan

| Field | Type | Rules |
|---|---|---|
| `PlanId` | stable ID | Unique per view/frame |
| `SceneColor` | handoff snapshot | Valid and current |
| `ResolvedSettings` | immutable settings | Complete |
| `Stages` | canonical ordered list | Exactly one exposure, one tone/view, one output encoding |
| `FormalOutputId` | stable ID | Single writer |
| `OutputDesc` | typed extent/format/domain | Agrees with profile/native request |
| `PlanFingerprint` | SHA-256 | Canonical handoff/settings/stage/profile digest |
| `Diagnostics` | bounded ordered records | No native pointers or sensitive values |

State: `Preparing -> Validated -> GraphDeclared -> Bound -> Executing ->
Completed -> Published`; failure from any pre-publication state becomes
`Failed -> Released` and publishes no formal output.

## 7. Output Graph Stage

| Field | Type | Rules |
|---|---|---|
| `StageId`, `Name` | stable identity | Unique |
| `Kind` | producer/exposure/insertion/tone/view/ODT/copy/present | Canonical order |
| `Reads`, `Writes` | typed graph handles | Declared before compile |
| `InputDomain`, `OutputDomain` | descriptors | Exact compatibility |
| `bExternalSideEffect` | bool | True for requested readback/presentation |
| `Transitions` | compiled RHI transitions | Interleaved before the owning pass |

The compiled schedule is immutable. Cycles, read-before-write, duplicate formal
writers, illegal domain/extent/sample changes, or culling an observed result fail
before native recording.

## 8. Presentation Capability Snapshot

Surface-specific, immutable result of one capability generation.

| Field | Type | Rules |
|---|---|---|
| `SurfaceId`, `CapabilityGeneration` | identities | Generation monotonic |
| `SupportedPairs` | unique format/color-space records | No blanket device inference |
| `bSupportsHDRMetadata` | bool | Native mechanism only |
| `bSupportsExtendedRange` | bool | Surface/display current state |
| `NativeReferenceWhiteNits` | optional positive float | Required for Metal EDR encoding |
| `CurrentHeadroom`, `PotentialHeadroom` | optional floats >=1 | Evidence, not peak claim |
| `CapabilityDigest` | SHA-256 | Canonical, redacted snapshot |

Moving display, changing mode, or losing a surface invalidates the snapshot.

## 9. Resolved Presentation State

| Field | Type | Rules |
|---|---|---|
| `ModeGeneration` | uint64 | Nonzero, monotonic |
| `ActualExtent` | uint32 pair | Native drawable pixels, not requested logical size |
| `Format`, `ColorSpace` | exact resolved pair | Must exist in capability snapshot |
| `MetadataDigest` | optional SHA-256 | Digest of metadata actually applied through the native presentation API; null on both Metal HDR paths |
| `DisplayAdaptation` | enum | `None` or Metal PQ `SystemColorManagement`; the latter means Core Animation color matching for the declared PQ colorspace, never `CAEDRMetadata` system tone mapping |
| `NativeEncoding` | enum | SDRExplicit, PQ, scRGB80, or MetalEDR |
| `ReferenceWhiteNits`, `TargetPeakNits` | floats | Decode provenance |
| `SwapchainImageGeneration` | uint64 | Stale images rejected |

State: `Requested -> Resolving -> Ready -> Acquired -> Submitted -> Presented`.
Resize/profile/display change transitions to `ReconfigureRequired`; zero extent
is `Paused`; native inability is `Unsupported`; failure is terminal for the
frame but recoverable for a later generation.

## 10. Formal Frame Output

Published only after all requested terminal operations succeed.

| Field | Type | Rules |
|---|---|---|
| `FormalOutputId` | stable ID | Matches plan |
| `FrameToken`, `ModeGeneration` | identities | Same for readback/presentation |
| `WorkloadRevision`, `GitRevision` | immutable revisions | Exact evidence provenance |
| `PlanFingerprint` | SHA-256 | Matches resolved settings |
| `Extent`, `Format`, `ProfileId` | resolved values | Exact |
| `Readback` | optional completion record | Same frame token |
| `Presentation` | optional completion record | Same frame token |
| `Result` | enum | Success only when all requested outputs succeed |

Partial/stale outputs are not observable as formal results.

## 11. SDR Image Baseline v3

| Field | Type | Rules |
|---|---|---|
| `BaselineId`, `State` | identity/state | Candidate -> Calibrated -> Reviewed -> Accepted -> Superseded |
| `WorkloadRevision` | string | New v3; never a v2 alias |
| `Backend`, `DeviceClass` | exact authority key | Vulkan/Metal only |
| `OutputDeviceProfileId`, `TransformVersion` | exact policy key | No cross-selection |
| `ExposureStops`, `SettingsDigest` | frozen settings | Exact match |
| `Width`, `Height`, `SampleCount` | constants | 512, 512, 1 |
| `ReferencePath`, digests | PNG + SHA-256 | Lossless, compressed and decoded identity |
| `FlipPolicy`, `CalibrationDigest` | bounded policy/evidence | Mutation gates retained |
| `Acceptance` | maintainer metadata | Required only for Accepted |

The v2 loader and data remain unchanged. Ordinary automation cannot create an
Accepted record.

## 12. HDR Live Review Request

Machine-authored preparation record. It has no visual decision.

| Field | Type | Rules |
|---|---|---|
| `RequestId`, revisions, digests | identity/provenance | Exact frozen run |
| `Backend`, `HostClass` | constants | Metal, maintainer-local macOS |
| `DeviceClass`, `DisplayClass` | redacted stable classes | No serial |
| `ReviewSessionId` | stable ID | One live session |
| `Profiles` | exactly four ordered entries | PQ1000, PQ2000, EDR1000, EDR2000 |
| `State` | enum | not-run/unsupported/failed/ready-for-live-review |
| `FirstFailure` | optional record | Required for failed |

Each profile records mode generation, settled frame tokens, readback digest,
and presentation readiness. The request cannot represent visual acceptance.

## 13. HDR Live View Attestation

Maintainer-authored, immutable statement linked to one request digest.

| Field | Type | Rules |
|---|---|---|
| `AttestationId` | stable ID | Unique |
| `RequestId`, `RequestSha256` | exact link | Request must be ready and matching |
| `Revisions`, `DeviceClass`, `DisplayClass`, `SessionId` | copied identity | Exact match |
| `MaintainerId`, `ReviewedAt` | public stable token/time | No OS username |
| `Observations` | exactly four ordered entries | Each `viewedLive=true`, decision pass/fail |
| `Acknowledgements` | exact required tokens | Live view/no capture substitution/no automation |
| `SupersedesAttestationId` | ID or null | Correction appends; never overwrites |

No overall score, measured peak, image path, threshold, or automatic decision is
allowed. Aggregation quotes decisions but never invents them. A current `fail`
blocks closeout; correction appends a linked superseding record, and all four
profiles require current non-superseded `pass` observations for completion.

## 14. Output Validation Report

One bounded canonical machine report for deterministic, native non-visual, or
SDR image-authority work.

| Field | Type | Rules |
|---|---|---|
| Revisions/strategy/backend/tier/device class | identity | Exact and redacted |
| SceneColor/output settings/profile | canonical objects | Match plan fingerprint |
| Native result | completion/readback/present records | Same frame where applicable |
| `ExecutionResult`, `FirstFailure` | result | Stable first failure |
| `VisualAuthority` | disposition/reason | not-required/manual-review-required/attestation-recorded |
| `Artifacts` | bounded manifest | <=64, each/aggregate policy enforced |

Windows conditional validation rejects any Feature 029 HDR authority claim.
An SDR authority report links exactly four artifacts: immutable Candidate,
lossless PNG, exact-revision cross-process calibration, and native probe. An HDR
native report links exactly one native probe. These sidecars preserve the frozen
SDR v3 baseline schema/key while making software revision and actual artifact
linkage mandatory at aggregation. Tested software revision is distinct from a
later evidence-storage commit; old artifacts are never relabeled.

## Relationships

```text
Forward or Deferred
  -> HDR SceneColor Handoff
  -> Output Transform Settings + Transform Strategies + Insertions
  -> Output Transform Plan
  -> Output Graph Stages
  -> Presentation Capability Snapshot
  -> Resolved Presentation State
  -> Formal Frame Output
       |-> Output Validation Report
       |-> SDR Image Baseline v3
       `-> HDR Live Review Request -> HDR Live View Attestation
```
