# Data Model: Asset Cooker, Manifest & Derived Data

## Model Rules

- `FAssetId` is the only logical Asset identity. Digests, target profiles,
  source locators, DDC paths, payload locators, and generation IDs never replace
  it.
- All persisted text is strict canonical UTF-8 JSON. All bulk payload integers
  use explicit little-endian encoding. No native struct layout is persisted.
- All sizes and counts are unsigned, checked before conversion/allocation, and
  validated against request and compiled limits.
- Collections whose order is not semantic are normalized before hashing or
  writing. Manifest records sort by `FAssetId`; dependencies sort by role then
  `FAssetId`; evidence fields have a fixed schema order.
- Mutation is confined to request-owned planning/execution state and staging.
  Valid DDC entries and published generations are immutable.

## 1. `FAssetTargetProfile`

Versioned target-ready payload policy.

| Field | Type | Rules |
|---|---|---|
| Schema | canonical token | Exactly `stoner.asset-target-profile` |
| SchemaVersion | uint32 | Exactly 1 for Feature 025 |
| DisplayName | UTF-8 string | Required, 1-128 bytes; excluded from effective digest |
| Platform | enum | Windows, MacOS, Linux, Android, IOS, or registered extension |
| CpuArchitecture | enum | X86_64, Arm64, or registered extension |
| GraphicsBackend | enum/token | Vulkan, Metal, DirectX12, OpenGL, GLES, or registered extension |
| ShaderPayloadChoices | ordered array | At least one backend/profile/format tuple; unique; order is fallback priority |
| TextureCapabilities | ordered array | Supported target texture families/formats in preference order; unique |
| TextureFallback | enum | Fail, Uncompressed, or an explicitly supported portable fallback |
| BuildPolicy | object | Global optimization/debug/validation plus unique sorted schema-versioned ProducerSettings records |
| Limits | object | May lower but not exceed compiled safety maxima |
| RequiredExtensions | sorted tokens | Unknown token fails |
| OptionalExtensions | sorted tokens | Unknown body ignored unless declared semantic by a registered extension |

### Effective Identity

1. Parse to the typed model and apply explicit schema defaults.
2. Remove `DisplayName` and presentation-only extension metadata.
3. Canonically encode every semantic field.
4. Compute SHA-256 as `EffectiveProfileDigest`.

Each cooker supplies `GetRelevantProfileEvidence(Profile)`. Its result is a
validated canonical projection and `RelevantProfileDigest`. Two profile names
with the same effective configuration have equal effective digests. Profiles
that differ only in fields outside one cooker's projection share that cooker's
relevant digest.

## 2. `FAssetCookSelection`

Defines the root set before dependency closure.

| Field | Type | Rules |
|---|---|---|
| Mode | enum | ExplicitRoots or CookAll |
| ExplicitRoots | sorted unique `FAssetId[]` | Non-empty only for ExplicitRoots |
| SourceScopes | sorted unique canonical locator[] | Non-empty; all discovery remains contained within these scopes |
| DiscoveryRulesVersion | digest/token | Included in plan evidence |

`ExplicitRoots` and CookAll are mutually exclusive. CookAll derives root Assets
from every supported output discovered under `SourceScopes`; the final manifest
records the scopes and the normalized selected roots.

## 3. `FDiscoveredAssetSource`

Request-local source-catalog entry.

| Field | Type | Rules |
|---|---|---|
| Locator | `FAssetSourceLocator` | Canonical, contained, no traversal |
| PhysicalEvidence | normalized path evidence | Never hashed as an absolute host path |
| SourceVersion | `FAssetVersion` | Digest of bounded authoritative source bytes |
| FormatHint | optional token | Non-authoritative dispatch hint |
| SourceBytes | immutable lease/shared bytes | Request-bounded and pinned during cook |
| Outputs | sorted `FAssetImportOutput[]` | Unique typed IDs; importer result is atomic |
| Importer | participant ID/version | Included in derived evidence |
| SourceManifest | normalized records | Every external/embedded dependency that affected outputs |

Catalog uniqueness is checked by canonical locator and by typed `FAssetId`.
Case/Unicode aliases that refer to different bytes fail before graph selection.

## 4. `FCookInputSnapshot`

Complete source state actually processed by one request.

| Field | Type | Rules |
|---|---|---|
| Records | sorted `FCookInputRecord[]` | One per authoritative source/dependency locator |
| AggregateBytes | uint64 | Checked against request limit |
| SnapshotDigest | SHA-256 | Canonical records; no host path or timestamp |

Each input record contains canonical locator, expected size, source digest,
source role, importer evidence, and immutable bytes or lease. Before
publication, every locator is re-resolved and its bounded size/digest compared.
Missing or changed records transition the request to `SourceChanged`; no retry
or source lock follows.

## 5. `FAssetCookGraph`

Deterministic DAG over selected typed Assets.

### Node

| Field | Type | Rules |
|---|---|---|
| PlanIndex | uint32 | Assigned by deterministic topological order |
| Id | `FAssetId` | Unique graph key |
| Metadata | `FAssetMetadata` | Valid Feature 020 metadata |
| Payload | immutable Asset payload | Type agrees with Id |
| Dependencies | sorted edge indices | Required closure present |
| Dependents | sorted node indices | Derived after validation |
| SourceEvidence | source records | Complete byte-affecting evidence |
| CookerParticipant | ID/version | Exactly one deterministic winner |
| Codec | token/revision/schema | Registered and target-compatible |
| RelevantProfileDigest | SHA-256 | Cooker's semantic projection |
| DerivedKey | `FAssetDerivedKey` | Computed after full evidence exists |
| ReuseEligibility | enum + reason | Eligible for every reachable plan-valid deterministic node with a complete key; otherwise explicit exclusion |
| State | `EAssetCookNodeState` | State machine below |
| Result | optional immutable node result | Published only on success |

### Edge

| Field | Type | Rules |
|---|---|---|
| DependencyId | `FAssetId` | Existing node |
| Role | canonical dependency role | Type-compatible and normalized |
| RequiredVersion | optional version | Must match selected payload when present |

### Graph Validation

- Unique typed IDs, no self-edge, no cycle, complete required closure.
- Count <=100,000, edges <=1,000,000, depth <=256 by default.
- Ready-set ties resolve by `FAssetId`.
- Worker completion order never changes PlanIndex, report order, key evidence,
  manifest order, or diagnostic truncation.

### Node State Machine

```text
Discovered
  -> Planned
  -> WaitingForDependencies
  -> CacheQuery
       -> CacheHit -> Validated
       -> CacheMiss -> Cooking -> CacheStore -> Validated
       -> CacheCorrupt -> Quarantined -> Cooking -> CacheStore -> Validated
  -> Staged
  -> Complete

Any non-terminal state -> Failed
```

Node failure prevents generation publication. Already stored valid DDC entries
remain valid because DDC publication is independent and immutable.

## 6. `FAssetDerivedKeyEvidence`

Inspectable normalized evidence for one derived key.

| Field | Type | Rules |
|---|---|---|
| KeyFormatVersion | uint32 | 1 |
| AssetId | `FAssetId` | Canonical typed identity |
| SourceVersion | `FAssetVersion` | Required |
| SourceManifest | sorted source records | Complete |
| Dependencies | sorted ID/version/role records | Complete direct byte-affecting evidence |
| ImporterId/Version | tokens | Required source-import participant evidence |
| CookerId/Version | tokens | Required |
| CodecId/Version | tokens | Required |
| PayloadSchemaVersion | uint32 | Required |
| EffectiveSettingsDigest | SHA-256 | Cooker projection of its schema-versioned profile BuildPolicy producer-settings record; required |
| RelevantProfileDigest | SHA-256 | Required |

The binary key stream uses a fixed domain tag, numeric field tags, and explicit
lengths before every variable field. `FAssetDerivedKey` is the SHA-256 of that
stream. The evidence JSON is inspectable metadata and must reproduce the key;
the JSON bytes themselves are not the key input.

## 7. `FAssetCookedPayloadEnvelope`

One `.sgasset` file.

### Header

| Field | Encoding | Rules |
|---|---|---|
| Magic | 8 bytes | ASCII `SGCOOK01` |
| ContainerVersion | little-endian uint16 | 1 |
| HeaderBytes | little-endian uint16 | Bounded; covers variable header |
| Flags | little-endian uint32 | Unknown required bit fails |
| AssetIdBytes | length + UTF-8 | Canonical `FAssetId::ToString()` |
| AssetTypeBytes | length + UTF-8 | Must agree with AssetId |
| CodecIdBytes | length + UTF-8 | Canonical registered token |
| CodecVersion | little-endian uint32 | Supported exact/compatible revision |
| PayloadSchemaVersion | little-endian uint32 | Supported revision |
| BodyBytes | little-endian uint64 | <= active payload limit |
| BodyDigest | 32 bytes | SHA-256 of body |

The body begins at `HeaderBytes`, has exactly `BodyBytes`, and no trailing bytes
are permitted. `EnvelopeDigest` is SHA-256 over the complete file and is stored
outside the envelope in DDC metadata and manifest records.

### Type-Specific Bodies

- Image/texture: dimensions, format, color/orientation/semantic metadata, mip
  records, and bounded bytes in fixed order.
- KTX2: validated KTX2 bytes plus semantic/target evidence required by the Asset
  payload contract.
- Material/shader/material-instance: existing canonical definition bytes and
  separately referenced payload/source dependencies according to Feature 023.
- Static mesh: stream descriptors, canonical stream bytes, 16/32-bit index
  bytes, primitives, slots, bounds, and source evidence.
- Static model: scenes, topological nodes, local transforms, mesh references,
  default scene, and source evidence.

Decoding builds scratch typed models, validates the complete model, and only
then returns an immutable payload.

## 8. `FDerivedDataEntry`

Immutable local cache directory.

| Field | Type | Rules |
|---|---|---|
| DerivedKey | SHA-256 | Must equal directory name and recomputed evidence |
| Evidence | `FAssetDerivedKeyEvidence` | Complete and canonical |
| AssetId | `FAssetId` | Agrees with envelope |
| Codec/Schema | tokens/revisions | Agrees with envelope |
| RelevantProfileDigest | SHA-256 | Agrees with evidence |
| PayloadLocator | fixed relative path | `Payload.sgasset` only in v1 |
| PayloadBytes | uint64 | Exact file size |
| EnvelopeDigest | SHA-256 | Exact full-file digest |
| State | implicit by directory | Staging, Valid, or Quarantined |

### Entry Transitions

```text
Absent -> Staging -> Validated -> Valid
Absent -> Staging -> FailedCleanup
Valid -> Quarantined            (only after failed read validation)
Quarantined -> retained/explicit later cleanup
```

No valid entry is updated in place. A concurrent final-directory collision is
prevented by a short-lived per-key native lease. A writer cooks outside the
lease, acquires it only to re-query/install, and validates/uses an equivalent
winner. Quarantine also revalidates while holding the same key lease. The
quarantine subject suffix is a digest of canonical failure evidence, not an
assumed payload digest, so missing-payload cases remain addressable.

## 9. `FAssetCookManifest`

Canonical published-generation index.

### Root Fields

| Field | Type | Rules |
|---|---|---|
| Schema | token | `stoner.asset-cook-manifest` |
| SchemaVersion | uint32 | 1 |
| GenerationId | SHA-256 | Recomputed semantic-manifest digest |
| ProfileDisplayName | string | Informational; excluded from generation identity |
| EffectiveProfile | normalized object/digest | Authoritative target evidence |
| Selection | `FAssetCookSelection` | Includes normalized final roots/scopes |
| SnapshotDigest | SHA-256 | Source state evidence |
| LimitsProfile | canonical object/digest | Validation limits used |
| Records | sorted manifest record[] | One per reachable typed Asset |
| RequiredExtensions | sorted token[] | Unknown fails |

### Asset Record

| Field | Type | Rules |
|---|---|---|
| AssetId | `FAssetId` | Unique, ascending canonical order |
| AssetType | token | Agrees with AssetId/envelope |
| SourceVersion | `FAssetVersion` | Required |
| SourceManifest | sorted records | Complete |
| Importer/Cooker/Codec | ID and revisions | Required |
| DerivedKey | SHA-256 | Recomputable from evidence |
| PayloadSchemaVersion | uint32 | Loader-compatible |
| PayloadLocator | safe relative UTF-8 path | Contained under generation root |
| PayloadBytes | uint64 | Exact |
| EnvelopeDigest | SHA-256 | Exact |
| Dependencies | sorted records | Closed over manifest |

### Generation ID

Construct a semantic manifest projection that excludes `GenerationId`,
`ProfileDisplayName`, payload physical locators, report telemetry, and all host
paths. Include each payload envelope digest. Canonically encode and SHA-256 that
projection. Final locators derive only from envelope digest, so validation can
recreate the projection without a hash cycle.

## 10. `FCurrentGenerationPointer`

Small canonical JSON commit record.

| Field | Type | Rules |
|---|---|---|
| Schema | token | `stoner.asset-current-generation` |
| SchemaVersion | uint32 | 1 |
| GenerationId | SHA-256 | Existing immutable generation directory |
| ManifestLocator | relative path | Exactly `Generations/<id>/Manifest.json` in v1 |
| ManifestDigest | SHA-256 | Exact canonical manifest bytes |

`Current.json` is the only mutable published file. It is replaced atomically
after the immutable generation is installed and validated.

## 11. `FPublicationLease`

Move-only Core handle plus diagnostic metadata.

| Field | Type | Rules |
|---|---|---|
| LeasePath | UTF-8 path | Stable `.publish.lock` under output root |
| NativeHandle | opaque Core-private | Non-inheritable, process-released |
| OwnerProcessId | diagnostic uint64 | Not ownership authority |
| OwnerStartToken | diagnostic string | Not used to break locks |
| RequestToken | diagnostic string | Excluded from deterministic artifacts |
| WaitTimeout | monotonic duration | 0-10 minutes; default 30 seconds |
| Acquired | bool | Only true while native lock/handle is held |

States: `Unacquired -> Waiting -> Held -> Released`, with `Waiting -> TimedOut`
or `Waiting -> Failed`. A stale metadata file with no native owner is acquired
and overwritten; no PID kill/probe or wall-clock expiry is used.

## 12. `FPublishedGeneration`

Immutable directory containing one manifest and digest-addressed payloads.

States:

```text
Planned
  -> RequestScratchComplete
  -> PublicationLeaseHeld
  -> Staging
  -> StagedComplete
  -> Validated
  -> InputsReverified
  -> InstalledImmutable
  -> CurrentPointerCommitted

Any state before CurrentPointerCommitted -> FailedNotCurrent
CurrentPointerCommitted -> PostCommitAuditWarning -> CommittedSuccess
```

If an identical `GenerationId` is already installed and validates, installation
is a successful no-op. Successful generations are not automatically deleted in
Feature 025. Failed staging is never referenced by `Current.json`.

## 13. `FAssetCookRequest`

Offline cook/plan request accepted by the public tool runner. Published
validation, DDC validation, and inspect use their own subject-specific request
types and do not fabricate unused source/profile/output fields.

| Field | Type | Rules |
|---|---|---|
| SourceScopes | canonical locator[] | Non-empty, unique |
| Selection | `FAssetCookSelection` | Valid exclusive mode |
| TargetProfilePath/Profile | input + typed model | Required and valid |
| OutputRoot | path | Local, writable, distinct from source/DDC roots |
| DerivedDataRoot | path | Local, writable, distinct from source/output roots |
| Mode | enum | Cook or PlanOnly |
| CleanPolicy | enum | Incremental default or IgnoreExistingDDC |
| WorkerCount | uint32 | 1-32 |
| LeaseTimeout | optional duration | Cook only, 0-10 minutes; forbidden for PlanOnly |
| Limits | `FAssetCookLimits` | <= compiled maxima |
| ReportPath | optional path | Must not alias source, payload, manifest, or pointer |

## 14. `FAssetCookReport`

Canonical deterministic report plus separate host telemetry.

### Deterministic Summary

- Command/mode and stable result category.
- Effective profile digest and selection evidence.
- Snapshot, plan, and generation digests when available.
- Counts: discovered, selected roots, reachable, reuse eligible/ineligible,
  cache hits/misses, corrupt, quarantined, cooked, regenerated, reused, failed,
  invalidated, staged, published. Every ineligible node carries a stable
  exclusion reason.
- Bytes: bounded source, cooked body, envelope, cache read/write, generation.
- Ordered per-node decisions and invalidation reasons by PlanIndex/AssetId.
- Ordered diagnostics with stable subject, dependency chain, stage, category,
  field, and reason.

### Non-Deterministic Telemetry

- Host platform/toolchain label, wall-clock durations, peak RSS, worker timing,
  process ID, and absolute report location.

Telemetry is emitted under a clearly separate field, excluded from the report's
`DeterministicDigest`, and omitted by `--normalized-report` golden output.

## 15. Result Categories

Public tool result categories map one-to-one to CLI exit codes:

| Category | Meaning |
|---|---|
| Success | Requested operation completed; atomic publication may include a non-fatal post-commit audit diagnostic |
| InvalidArguments | CLI/request contract invalid |
| InvalidProfile | Profile/schema/capability invalid |
| DiscoveryFailure | Source scope/enumeration/import catalog failed |
| GraphFailure | Identity/dependency/cycle/limit failure |
| CookFailure | Type-specific processing failed |
| CacheFailure | Strict validation or unrecoverable DDC operation failed |
| SourceChanged | Snapshotted input changed before publication |
| LeaseTimeout | Publication lease unavailable within bound |
| PublishedValidationFailure | Manifest/payload/generation invalid |
| PublicationFailure | Staging/install/current-pointer operation failed before atomic replacement committed |
| IoFailure | Portable filesystem operation failed |
| InternalFailure | Invariant/unexpected failure; no partial publication |

Type-specific Asset diagnostics remain nested evidence, not process exit-code
arithmetic.
