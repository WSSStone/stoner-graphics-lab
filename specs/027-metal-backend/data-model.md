# Data Model: Native Metal Backend

## 1. `FMetalAdapterCandidate`

Stable discovery record used before a device becomes public.

| Field | Meaning |
|---|---|
| `RegistryId` | Native registry identity, serialized as fixed-width hexadecimal |
| `Name` | Normalized diagnostic name |
| `GpuFamilies` | Sorted supported Metal GPU-family identifiers |
| `bLowPower`, `bRemovable`, `bUnifiedMemory` | Selection and memory-policy inputs |
| `CapabilitySummary` | Backend-neutral limits and format support |
| `Compatibility` | Compatible or rejected with stable reasons |
| `SelectionRank` | Canonical tuple used after explicit selection |

**Validation**: Registry identity must be nonzero and unique in one discovery
snapshot. Capability records are sorted and duplicate-free. Native enumeration
order is never persisted as rank.

## 2. `FMetalDeviceOwnerState`

Shared owner identity retained by every device child.

| Field | Meaning |
|---|---|
| `DeviceIdentity` | Stable process-local generation-safe owner ID |
| `Adapter` | Immutable selected adapter record |
| `Lifecycle` | Device state |
| `SubmissionGeneration` | Monotonic accepted-submission sequence |
| `LiveObjectCounts` | Per-kind atomic object counts |
| `InFlightSubmissionCount` | Native work not yet completed |
| `FirstFailure` | Stable terminal failure, if any |

**States**: `Constructing -> Ready -> Draining -> Invalidated`. Any setup
failure transitions `Constructing -> Failed -> Invalidated`. No transition may
return to `Ready`.

## 3. `FMetalCapabilitySnapshot`

Immutable conversion from native device facts to RHI capability vocabulary.

Fields include queue support, format/usage/sample support, maximum resource
dimensions and byte sizes, per-stage buffer/texture/sampler binding limits,
constant-data limits, threadgroup dimensions and thread count, maximum in-flight
frames, presentation support, and required synchronization support.

**Validation**: Every advertised limit is positive and internally consistent.
Unsupported formats or semantics are absent or explicitly false; they are never
emulated silently. The normalized dump is stable for an unchanged device.

## 4. `FMetalResourceRecord`

Common lifecycle evidence embedded by buffers, textures, samplers, staging
allocations, descriptor snapshots, render passes, and framebuffers.

| Field | Meaning |
|---|---|
| `ObjectIdentity` | Stable kind plus monotonic ID, never a native address |
| `OwnerIdentity` | Creating device owner |
| `Kind` | Buffer, texture, sampler, descriptor, pass, framebuffer, or staging |
| `DescriptorDigest` | Canonical creation-description digest |
| `StoragePolicy` | Shared, managed, or private plus coherency actions |
| `Lifecycle` | Object state |
| `LastUseSubmission` | Latest submission retaining native ownership |
| `Failure` | Stable creation/use failure if present |

**States**: `Allocated -> Ready -> PendingDestroy -> Destroyed`. Creation may
transition directly to `Failed`. `PendingDestroy` remains natively retained
until every referencing submission completes.

## 5. Shader Native Binding Evidence And RHI Map

`FShaderNativeBindingEvidence` is immutable Asset-owned cooked evidence generated
by the Tools-only canonical policy implementation. `FRHINativeBindingMap` is the
backend-neutral value copy produced by Renderer for pipeline creation. Metal's
`FMetalBindingMapValidator` validates and consumes it; Backend never imports
Tools or recomputes assignments.

| Field | Meaning |
|---|---|
| `PolicyVersion` | `metal-direct-binding-v1` for Feature 027 |
| `Entries` | Sorted shader-stage/set/binding/type/element mappings |
| `ReservedRanges` | Vertex and constant buffer native index ranges |
| `LimitSnapshot` | Target profile limits used during derivation |
| `CanonicalDigest` | SHA-256 over canonical policy and entries |

Each entry contains source binding identity and exactly one native buffer,
texture, or sampler index. Duplicate source keys, duplicate incompatible native
indices, array overflow, and target-limit overflow are invalid.

**Transfer rule**: Asset codec validation proves the evidence digest; Renderer
copies every field into `FRHINativeBindingMap`; RHI validates canonical ordering
and digest without knowing Asset; Metal compares the map against pipeline layout
and live capability limits. Any missing entry or digest mismatch fails before
native pipeline creation.

## 6. `FRHIShaderPayloadDesc`

Backend-neutral replacement for the SPIR-V-word-only payload description.

| Field | Meaning |
|---|---|
| `Format` | `SPIRV` or `MetalLibrary` in Feature 027 |
| `Bytes` | Exact immutable payload bytes |
| `PayloadIdentity` | Stable Shader Asset payload identity |
| `TargetProfile` | Exact cooked target profile identity/version |
| `PayloadDigest` | SHA-256 over exact bytes |

SPIR-V bytes must be nonempty, four-byte aligned, and pass structural/entry
validation before Vulkan use. Metal libraries must be nonempty and match the
selected profile and evidence before native library creation. The RHI never
parses Asset manifests or owns Asset handles.

## 7. `FMetalShaderDerivationEvidence`

Cross-platform deterministic record for one stage/entry derivation.

Fields: evidence schema, Shader Asset ID/version, authoritative GLSL and SPIR-V
digests, stage, entry point, interface digest, SPIRV-Cross commit/options,
binding-policy version, canonical ordered entries and digest, MSL language/
deployment target, normalized MSL digest, and producer identity/version.

**Identity**: Canonical JSON SHA-256. Host OS, working path, timestamps, and
temporary filenames are forbidden from identity-bearing fields.

## 8. `FMetalNativeLibraryEvidence`

macOS-only finalization record extending one derivation record.

Fields: derivation evidence digest, target profile, architecture, Apple compiler
and Xcode build, SDK identity/version, deployment target, MSL language version,
argument vector digest, native library digest/size, and finalizer identity.

**Validation**: A final Metal payload is publishable only when this evidence is
complete and target-compatible. Source-only MSL cannot satisfy this entity.

**Repeatability identity**: For one exact tuple of CPU architecture, deployment
target, Xcode build, SDK, Metal compiler, target profile, and authoritative
inputs, the metallib byte digest, DDC key, and native evidence digest are all
identical across twenty runs. Across different tuples only derivation evidence
identity is compared; native byte digests and DDC keys are expected to differ.

## 9. `FMetalCommandRecord` And `FMetalSubmissionRecord`

`FMetalCommandRecord` stores validated, immutable RHI command intent and retained
resource identities. It has states `Initial -> Recording -> Executable ->
Submitted -> Completed`; reset is legal only from `Completed` or a never-
submitted `Executable` state according to the existing RHI contract.

`FMetalSubmissionRecord` stores queue identity, monotonic sequence, command
records, wait/signal sync points, retained native objects, terminal status, and
completion diagnostics. It transitions `Accepted -> Encoded -> Committed ->
Completed` or to `Failed`; retained ownership is released only at a terminal
state.

## 10. `FMetalSyncPoint`

Logical fence or semaphore state backed by a monotonic native event value and a
CPU-visible completion condition.

Fields: object/owner identity, kind, current epoch, signaled/completed value,
pending waiters, lifecycle, and failure. Reset advances the logical epoch and
cannot make an old completion satisfy a new wait. Foreign-device and stale-epoch
operations fail before native encoding.

## 11. `FMetalPresentationContext`

Non-owning window bridge and owning Metal layer state.

| Field | Meaning |
|---|---|
| `BorrowedWindowIdentity` | Validated Application platform-window identity |
| `LayerIdentity` | Backend-owned stable layer ID |
| `LogicalExtent`, `DrawableExtent`, `DisplayScale` | Current presentation sizing |
| `Format`, `MaxDrawableCount` | Swapchain-equivalent configuration |
| `Lifecycle` | Presentation state |
| `FrameSlots` | Bounded frame-scoped drawable/submission ownership |

**States**: `Detached -> Attaching -> Ready`; `Ready <-> Paused` for zero extent
or temporary drawable absence; `Ready/Paused -> Reconfiguring -> Ready/Paused`
for resize; any live state may enter `Draining -> Detached`; unrecoverable errors
enter `Failed -> Draining -> Detached`.

## 12. `FMetalValidationRecord`

Normalized evidence for one validation execution.

Fields: schema, Git revision, tier, workload, backend, host architecture/OS,
native device and capability digest where applicable, shader payload/evidence
digests, frame/cycle/iteration counts, semantic probe results, tolerance set,
ownership counters, diagnostic digest, artifact paths/digests, result, failure
category, and for lifecycle stress the complete RSS sample series, warm-up and
sample intervals, first/final medians, absolute/relative growth, threshold, and
pass/fail decision.

**Tier rules**:

- `deterministic`: no native-execution claim.
- `native-offscreen`: real Metal command completion and GPU readback required.
- `visible-manual`: native presentation capture plus lifecycle log required.
- `cross-backend`: independently native Metal and Vulkan evidence required.

## Relationships

```text
FMetalAdapterCandidate -> FMetalDeviceOwnerState -> FMetalCapabilitySnapshot
FMetalDeviceOwnerState -> FMetalResourceRecord[*]
FMetalDeviceOwnerState -> FMetalCommandRecord[*] -> FMetalSubmissionRecord[*]
FMetalSubmissionRecord -> FMetalSyncPoint[*]
FMetalDeviceOwnerState -> FMetalPresentationContext[0..1]
FMetalShaderDerivationEvidence -> FMetalNativeLibraryEvidence
FShaderNativeBindingEvidence -> FMetalShaderDerivationEvidence
FShaderNativeBindingEvidence -> FRHINativeBindingMap -> FMetalBindingMapValidator
FRHIShaderPayloadDesc -> FMetalNativeLibraryEvidence (validated association)
FMetalValidationRecord -> device/capability/shader/submission/presentation evidence
```
