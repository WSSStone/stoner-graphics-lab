# Data Model: Production Content Integration & Acceptance

## 1. Production Content Package

Represents one admitted artist-authored package.

| Field | Type | Rules |
|---|---|---|
| `PackageId` | canonical string | Unique, stable, lowercase kebab-case |
| `WorkName` | string | Human-readable source work name |
| `PackageName` | string | Exact selected variant name |
| `Publisher` | string or absent | Informational provenance only |
| `SourceLocation` | HTTPS URL | Stable upstream repository/location |
| `Revision` | 40-char Git SHA or immutable release ID | Required and immutable within corpus revision |
| `AcquiredOn` | ISO date | Frozen after admission |
| `Tier` | `regular` or `medium` | Regular bytes exist in checkout; medium may be externally staged |
| `PackageRoot` | normalized relative path | Package directory below the production-content root |
| `SourcePath` | normalized relative path | Exact package directory below the pinned source repository |
| `RootAssetId` | typed AssetId string | Must be `StaticModel` and canonical |
| `RootFile` | normalized relative path | Must be declared in `Files` |
| `Files` | ordered file records | Sorted by normalized path; no missing/extra/path escape/collision |
| `CoverageClaims` | ordered claim IDs | Every claim resolves to declared evidence |

There is intentionally no license, policy, approval, SPDX, or legal-state
field. Additional unknown properties are rejected so policy data cannot
silently become part of automated acceptance. Maintainer-owned attribution or
license notes may exist only in
`Content/ProductionAcceptance/MAINTAINER_NOTES.md`, outside this model, the
package inventory, package roots, and every validation input; validators never
open, parse, hash, or reject that note.

## 2. Corpus File Record

| Field | Type | Rules |
|---|---|---|
| `Path` | normalized UTF-8 relative path | NFC, `/` separators, no `.`/`..`, absolute path, case-only or normalization collision |
| `Sha256` | lowercase 64-hex string | Digest of exact distributed bytes |
| `SizeBytes` | non-negative integer | Must equal observed file length |
| `Role` | enum | `model`, `buffer`, `image`, or `other-source` |

State: `Declared -> Verified` or `Declared -> Rejected`. No import begins until
all records are `Verified` and directory enumeration contains no extra file.

## 3. Corpus Manifest

| Field | Type | Rules |
|---|---|---|
| `Schema` | constant | `stoner.production-corpus` |
| `SchemaVersion` | integer | Starts at 1 |
| `CorpusRevision` | canonical string | Changes for any package, file, root, tier, or coverage change |
| `Packages` | ordered package list | Sorted by `PackageId`; at least two independent source works |
| `CoverageClaims` | ordered claim list | Complete required coverage with no dangling package/subresource |

Identity is the SHA-256 of canonical JSON. The manifest is authoritative for
technical provenance and integrity only.

## 4. Coverage Claim

| Field | Type | Rules |
|---|---|---|
| `ClaimId` | enum/string | One required geometry/material/texture/container characteristic |
| `PackageId` | string | Must identify an admitted package |
| `Subject` | stable package-relative subresource | Mesh/material/image/node/variant identity |
| `Evidence` | bounded scalar/object | Expected count, dimension, channels, semantic, or dependency form |

Required initial claims include indexed triangles, multiple primitives,
multiple materials, local hierarchy, shared dependency, external dependencies,
embedded dependencies, 1K/2K dimensions, RGB/RGBA, color, tangent-space normal,
and non-color material-data textures.

## 5. Production Validation Profile

| Field | Type | Rules |
|---|---|---|
| `ProfileId` | enum | `regular`, `medium`, or `hardware` |
| `CorpusRevision` | string | Exact manifest revision |
| `PackageIds` | ordered list | Regular is bounded; medium/hardware include declared packages |
| `TargetProfiles` | ordered list | Exact AssetCooker profile IDs/digests |
| `PackageLifecycles` | ordered records | One exact record per `PackageIds` entry in the same order; each record contains `PackageId`, `Purpose`, `Cycles`, and included `WarmupCycles` |
| `MaxRssGrowthBytes` | integer | 16 MiB when RSS disposition is `required` on maintainer-local Metal; retained as a diagnostic comparison value but not applied to `observed` hosted or Windows Vulkan RSS |
| `EnvironmentPolicy` | authority-policy ID | Repository-owned mapping for hosted, maintainer-local Metal, maintainer-local Windows Vulkan, and local diagnostic execution |
| `RequiredGates` | ordered enum list | Corpus/import/cook/runtime/realization/render/image/lifecycle as applicable |
| `TimeBudgetSeconds` | integer | 900 regular package, covering clean/warm/strict/native work while native remains independently capped; 2,400 hosted medium package operational timeout; 3,600 serialized visible hardware lane |
| `ProfileTimeBudgetSeconds` | integer | 1,200 regular, reserving 300 seconds beyond the package for target-toolchain/orchestration work; 2,700 hosted medium package shard; 7,800 serialized two-package hardware profile |
| `NativeTimeBudgetSeconds` | integer | 600 regular; 1,800 hosted medium operational timeout; 3,600 hardware; always capped by the enclosing lane deadline |

### Package Lifecycle Record

| Field | Type | Rules |
|---|---|---|
| `PackageId` | canonical string | Must equal the package in the same position of `PackageIds`; no missing, duplicate, or extra record |
| `Purpose` | enum | `bounded-regression`, `endurance`, `scale-lifecycle`, or `physical-authority` |
| `Cycles` | integer | `20` for bounded regression, `1,000` for endurance/physical authority, `100` for scale lifecycle |
| `WarmupCycles` | integer | Included in `Cycles`; exactly `2`, `20`, or `10` for the corresponding purpose |

The canonical profiles assign regular Lantern `bounded-regression` 20/2;
hosted medium assigns Lantern `endurance` 1,000/20 and Sponza
`scale-lifecycle` 100/10; hardware assigns both packages
`physical-authority` 1,000/20. A consumer may not replace these records with a
single profile-wide scalar or infer one package's purpose from another.

### Environment Measurement Authority

| Field | Type | Rules |
|---|---|---|
| `ExecutionClass` | enum | `github-hosted`, `maintainer-local-metal`, `maintainer-local-windows-vulkan`, or `local-diagnostic`; each dedicated local flag is a narrow target authority assertion, not a generic class token |
| `Preflight` | enum + evidence | `passed`, `failed`, or `not-required`; each local physical authority requires its native host/backend, exact target/device class, exclusive lock, clean committed revision, default allocator, and sample/presentation evidence |
| `RssDisposition` | enum | `required` only for preflighted maintainer-local Metal; `observed` for hosted, maintainer-local Windows Vulkan, and local diagnostics |
| `TimingDisposition` | enum | `operational` for Feature 028 timeouts; elapsed time is not a hosted performance qualification |
| `ImageDisposition` | enum | `required` only for a profile/device class with accepted physical image authority; otherwise `not-required` |
| `ReplacementLane` | stable token or null | Required when a requested physical authority preflight is unavailable or fails |

`required` participates in the result, `operational` fails only when work does
not complete before its bounded timeout, and `observed` is always serialized but
cannot independently change Passed to Failed or vice versa. Aggregation rejects
unknown or conflicting policies and cannot promote an observation.

## 6. Production Asset Closure

One immutable result from development import or strict-cooked loading.

| Field | Type | Rules |
|---|---|---|
| `Root` | `TAssetHandle<FStaticModelAsset>` | Exact requested root |
| `GenerationIdentity` | SHA-256 digest | Cooked generation digest or normalized development closure digest |
| `Meshes` | AssetId -> typed handle | Complete required set |
| `Materials` | AssetId -> typed handle | Complete required set |
| `Shaders` | AssetId -> typed handle | Complete selected target set |
| `Textures` | AssetId -> typed handle | Development `FTextureAsset`; cooked `FKTX2TextureArtifact` |
| `Dependencies` | ordered edge list | Stable role/order; no unresolved required edge |
| `ExecutionEvidence` | counters | Strict mode proves zero source resolver/importer/decoder/fallback calls |

State: `Requested -> Loading -> Complete` or `Failed/Cancelled`. A closure is
not observable as complete until every required typed handle is valid.

## 7. Texture Cook Selection

| Field | Type | Rules |
|---|---|---|
| `SourceTexture` | `FTextureAsset` | Valid semantic, color space, mip description |
| `CookerId` | participant ID | Must equal `cooker.ktx2` for production texture cooks |
| `Parameters` | `FTextureCookParameters` | Derived from source semantic and target profile |
| `TargetDecision` | existing decision | Capability-compatible format or explicit fallback |
| `DerivedKeyEvidence` | existing evidence | Includes producer/settings/profile/source/dependencies |
| `Artifact` | `FKTX2TextureArtifact` | Valid KTX2 bytes, same AssetId, complete mip evidence |

State: `Selected -> Keyed -> Reused` or `Cooked -> Validated -> Published`.
Any generic raw typed-texture payload in a production cooked generation is a
type/producer failure.

## 8. Static Model Realization Request

| Field | Type | Rules |
|---|---|---|
| `Device` | `IRHIDevice&` | Live device from selected backend |
| `Model` | `FStaticModelAsset` | Valid root matching closure |
| `Meshes` | typed immutable dependency map | Every referenced mesh exactly once |
| `Materials` | typed immutable dependency map | Every slot resolves |
| `Shaders` | selected shader snapshots/payloads | Compatible backend/profile/interface |
| `Textures` | KTX2 artifacts and target decisions | Every material texture resolves |
| `Limits` | bounded realization limits | Counts/bytes/descriptors/pipelines |

Validation rejects missing, extra ambiguous, stale, duplicate-conflicting,
wrong-type, or target-incompatible dependencies before commit.

## 9. Static Model Render Snapshot

Immutable Renderer result published atomically after transaction commit.

| Field | Type | Rules |
|---|---|---|
| `RootAssetId` | AssetId | Matches request model |
| `SnapshotGeneration` | monotonic process-local generation | Stale handles cannot alias recreation |
| `Nodes` | ordered node transforms | Parent-before-child, source-stable identity |
| `Draws` | ordered primitive records | Mesh/index ranges, material binding, bounds |
| `Materials` | immutable Renderer snapshots | No Asset mutation after publication |
| `Resources` | owned RHI handles | Buffers/textures/descriptors/pipelines needed by draws |
| `Inspection` | bounded stable summary | Counts/identities; no native pointers |

State: `Preparing -> Realizing -> ReadyToCommit -> Published`. Failure from any
pre-commit state transitions to `RollingBack -> Failed`; resources release in
reverse dependency order exactly once. Destroying a published snapshot moves
it through `Releasing -> Released`.

## 10. Asset-Backed Composition

| Field | Type | Rules |
|---|---|---|
| `WorkloadRevision` | canonical string | Changes for camera/light/transform/render policy changes |
| `RenderSnapshot` | immutable snapshot | Must be published and current |
| `Camera` | backend-neutral view/projection data | Same values for Vulkan/Metal |
| `Transforms` | normalized composition transforms | No backend branch |
| `Lights` | deterministic light records | Same ordering/values for both backends |
| `FrameState` | deterministic frame token/time | Proves current frame |
| `Paths` | Deferred full + Forward smoke | Both use same root/composition inputs |

## 10A. Production Camera Preset

| Field | Type | Rules |
|---|---|---|
| `WorkloadRevision` | canonical string | Exact unique authority; no nearest/fallback selection |
| `View` | 16 float32 row-major values | Finite, affine, orthonormal, no scale/shear, invertible |
| `Projection` | 16 float32 row-major values | Finite, invertible, positive-X-forward StandardZ perspective |
| `CameraPosition` | derived `FVector3` | Inverse-View origin; never independently authored |
| `ViewProjection` | derived matrix | `Projection * View` |
| `InverseViewProjection` | derived matrix | Must exist and remain finite |

State: `Candidate -> Frozen -> Superseded`. Candidate records are bounded
calibration output only. Formal rendering consumes only the code-owned Frozen
record selected by exact workload revision. A camera change creates a new
workload revision and forces semantic/image recalibration.

The calibration controller owns transient position, orientation, velocity,
FOV, input events, and frame delta. None of those transient fields are report
identity or formal gate inputs; only an explicit snapshot can propose the two
matrices above.

## 11. Device Class Registry

| Field | Type | Rules |
|---|---|---|
| `Schema`, `SchemaVersion` | constants | `stoner.production-device-class-registry`, version 1 |
| `RegistryVersion` | integer | Versioned with every class/signature policy change |
| `Classes` | ordered records | Strictly sorted by class token; class tokens and complete signatures are each unique |
| `DeviceClass` | canonical token | Stable baseline key; never supplied authoritatively by a caller |
| `CapabilitySignature` | canonical object | Registry version, backend implementation, CPU architecture, adapter family, shader profile, color/depth formats, sample count, and texture-format family |

The loader rejects unknown fields, duplicate class tokens, duplicate signatures,
non-canonical ordering, and zero or multiple matches for an observed signature.

## 12. Image Acceptance Baseline

| Field | Type | Rules |
|---|---|---|
| `BaselineId` | canonical string | Unique |
| `WorkloadRevision` | string | Exact workload match |
| `Backend` | `vulkan` or `metal` | Exact native backend |
| `DeviceClass` | registry-owned stable token | Derived by exactly one registry match; never accepted from a caller |
| `CapabilitySignature` | canonical object | Registry version, backend implementation, CPU architecture, adapter family, shader profile, color/depth formats, sample count, and texture-format family; excludes marketing name |
| `Width`, `Height` | integer pair | Exactly 512 by 512 for formal calibration captures, accepted references, and candidates; the 1024 preview is excluded |
| `PresentationExtent` | integer pair | Exactly 512 by 512 for the formal native authority drawable; the authority window's logical client extent is boundedly adjusted when display density changes the measured drawable |
| `ColorTransfer` | enum | Canonical comparison transfer |
| `ReferencePath`, `ReferenceSha256` | path + digest | Versioned reviewed lossless PNG and compressed-byte digest; decoder output is proven pixel-equivalent to the reviewed raw capture |
| `FlipPolicy` | thresholds | Mean, p95, max, bad-pixel threshold/fraction |
| `CalibrationEvidence` | digest/reference | 20-repeat noise set and mutation results |

State: `Candidate -> Calibrated -> Reviewed -> Accepted -> Superseded`.
Ordinary CI can consume only `Accepted`. Missing or multiple registry matches
fail before baseline lookup; there is no nearest or fallback device class.
Raw PPM/RGBA captures are transient run artifacts under ignored
`Build/Validation/` and are never registry inputs. Lightweight calibration JSON
may be checked in; expanded run directories, DDC, generations, logs, and raw
readbacks may not. Test fixture manifests are product-independent test inputs
and live with their corpora under `Tests/Fixtures/`, not in `Validation/`.

## 13. Production Acceptance Report

| Field | Type | Rules |
|---|---|---|
| `Deterministic` | canonical object | Corpus, root, target, generation digest or `not-created`, closure, workload, backend, result, evidence digests, and conditional first failure |
| `Authority` | canonical bounded object | Workflow-derived execution class, physical preflight state/evidence, and exact `required`, `operational`, `observed`, or `not-required` disposition for each environment-sensitive measurement; callers and aggregates cannot promote it |
| `Observations` | bounded object | Timing, RSS/task-VM/allocator, exact registered device class, image metric, and artifact observations; values do not carry or create authority |
| `FirstFailure` | object or null | Null for Passed; exactly one stable stage/category/subject/expected/observed/reproduction object for Failed/Unsupported; Unsupported also names missing prerequisite and replacement lane |
| `Artifacts` | ordered digest list | At most 64; every external report/readback/capture has digest and byte size, each at most 64 MiB and aggregate at most 256 MiB |

Result states: `Passed`, `Failed`, or `Unsupported`. `Unsupported` requires a
missing prerequisite and replacement lane; it is never aggregated as Passed.
Native Vulkan/Metal reports require the exact registered device class. A profile
with `ImageDisposition=required` requires measured passing FLIP; profiles that
do not own image authority use a structured `not-required` reason rather than a
fabricated comparison. Failures before a required comparison use structured
`not-run`. Non-native reports use backend
`none`. Passed requires a real generation digest; only Failed/Unsupported may
use `not-created`. Deterministic content must serialize byte-identically for
equivalent inputs. Canonical serialized report JSON is at most 1 MiB.

## Relationships

```text
Corpus Manifest 1 --* Production Content Package 1 --* Corpus File Record
       |                         |
       +--* Coverage Claim       +--1 Production Asset Root
                                          |
Validation Profile --selects--------------+
                                          v
                               Production Asset Closure
                                          |
                         Texture Cook Selection / generation
                                          |
                                          v
                            Static Model Realization Request
                                          | transaction
                                          v
                              Static Model Render Snapshot
                                          |
                                          v
                               Asset-Backed Composition
                                          |
                    Image Baseline --------+
                                          v
                              Production Acceptance Report
```
