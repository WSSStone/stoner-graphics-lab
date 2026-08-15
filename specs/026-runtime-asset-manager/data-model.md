# Data Model: Runtime Asset Manager

## Model Rules

- `FAssetId` remains logical identity; mode, target, expected type, and
  generation participate only in load-operation equivalence.
- Payloads are immutable and published atomically only after their complete
  dependency and validation contract succeeds.
- Every count, byte size, queue, path, and diagnostic list is bounded before
  allocation or admission.
- State and inspection order are deterministic; worker completion order is not
  observable in normalized evidence.
- Public handles carry no filesystem, scheduler, native lease, or graphics type.

## 1. `FAssetManagerConfig`

| Field | Type | Rules |
|---|---|---|
| Mode | `EAssetManagerMode` | DevelopmentSource or StrictCooked; immutable |
| ExtensionRegistry | shared registry reference | Required in development; cooked codec registrations remain required |
| SourceRoot/ResolverContext | Asset resolver configuration | Development only |
| PublicationRoot | UTF-8 path | Cooked only; readable, validated, contained, and may be read-only |
| LeaseCoordinationRoot | UTF-8 path | Cooked only; explicit, pre-provisioned, writable, local-machine coordination root |
| TargetEvidence | immutable profile evidence | Required; participates in load key |
| WorkerCount | uint32 | 1-32 |
| Limits | `FAssetManagerLimits` | Non-zero and <= compiled maxima |
| LeaseTimeoutMs | uint64 | Cooked startup only; <=600000 |
| ExtensionDeadlineMs | uint64 | Monotonic cooperative deadline; non-zero and bounded |

Development and cooked fields are mutually exclusive. The coordination root is
namespaced by domain-separated SHA-256 v1 over the Core-canonical absolute UTF-8
publication path (resolved symlinks; normalized separators and case-folding on
Windows; case preserved on POSIX) so unrelated roots do not collide. The root
must exist; manager creation may create only its digest directory and lease
files below it. Invalid, absent, read-only, or ambiguously aliased coordination
configuration fails before worker creation or request admission.

## 2. `FAssetRequestHandle`

| Field | Type | Rules |
|---|---|---|
| ManagerLifetime | uint64 opaque token | Unique for live manager instances |
| Slot | uint32 | Bounded request table index |
| Generation | uint32 | Non-zero; increments on slot reuse |

The tuple is the only request capability. Query/cancel validates every field.
Stale or foreign handles return `InvalidHandle` and cannot touch a reused slot.

## 3. `FAssetRequestRecord`

| Field | Type | Rules |
|---|---|---|
| HandleIdentity | tuple above | Unique live request |
| RootKey | `FAssetLoadKey` | Complete equivalence key |
| State | `EAssetRequestState` | State machine below |
| Interest | enum | Active, CancelRequested, Released |
| RootOperation | operation reference | Present after admission |
| Result | optional `FAssetRequestResult` | Written once at terminal commit |
| Callback | optional bounded callback | Never invoked by workers |
| CompletionReservation | optional reservation token | Acquired atomically at admission when Callback exists |
| CompletionSequence | optional uint64 | Assigned once after terminal commit |
| DiagnosticIds | bounded array | Stable references, not native data |

### State Machine

```text
Accepted -> WaitingForDependencies -> Loading -> Ready
    |                 |                 |
    +-----------------+-----------------+-> Failed
    +-----------------+-----------------+-> Cancelled
```

Only one terminal transition is legal. Cancellation after Ready does not revoke
the published handle. Query is idempotent in every state.

## 4. `FAssetLoadKey`

| Field | Type | Rules |
|---|---|---|
| AssetId | `FAssetId` | Valid canonical identity |
| ExpectedType | canonical type token | Must equal AssetId type |
| Mode | manager mode | Required |
| TargetEvidenceDigest | SHA-256 | Complete target meaning |
| GenerationId | optional digest | Required only in cooked mode |

Equality is field-for-field, not lookup-hash equality. The key is process-local
and is not a replacement for Asset identity.

## 5. `FSharedAssetLoadOperation`

| Field | Type | Rules |
|---|---|---|
| Key | `FAssetLoadKey` | Unique in operation table |
| State | operation state | Queued, Resolving, Waiting, Loading, Publishing, terminal |
| CallerInterests | sorted request identities | Independent cancellation |
| RootRetentions | checked count | Roots still requiring this node |
| DependencyRetentions | checked count | Loaded roots retaining this payload |
| Dependencies | sorted operation keys | Required/explicit optional edges |
| Dependents | sorted operation keys | Derived reverse edges |
| PayloadCandidate | immutable scratch result | Invisible before commit |
| Failure | stable category/path | Written once |

An operation is cancellable only when no caller, root, or dependency interest
remains. Physical work may finish after cancellation observation, but cannot
publish to cancelled interests.

## 6. `FAssetDependencyClosure`

| Field | Type | Rules |
|---|---|---|
| Root | load key | Required |
| Nodes | sorted operation references | <=MaxAssets |
| Edges | role/strength/version records | <=MaxDependencyEdges |
| VisitColor | per-node enum | White/Gray/Black |
| FailurePath | ordered AssetId list | Root-to-failing node, bounded depth |

Required nodes must be Ready before root publication. Cycle, missing/type/version
failure, or limit overflow fails the root. Sharing an operation does not merge
root-specific path evidence.

## 7. `FAssetRuntimeCacheEntry`

| Field | Type | Rules |
|---|---|---|
| Key | load key | Unique cache index |
| Metadata | validated immutable metadata | Agrees with payload |
| Payload | `shared_ptr<const FAssetPayload>` | Non-null and typed |
| ExternalHandles | uint64 | Checked |
| RequestInterests | uint64 | Active or completion-undelivered |
| DependencyRetentions | uint64 | Required loaded-root ownership |
| PayloadBytes | uint64 | Included in aggregate limit |

Entry removal occurs synchronously when all three counts reach zero. Removal
drops manager ownership and accounting; handle-owned payload/control blocks may
outlive both entry and manager.

## 8. `TAssetHandle<T>`

| Field | Type | Rules |
|---|---|---|
| Payload | shared immutable typed payload | Dynamic type and trait token validated before construction |
| Identity | `FAssetId` | Stable inspection value |
| Version | `FAssetVersion` | Published payload evidence |
| RetentionControl | shared private control | Releases external count exactly once |

Copying shares retention; moving transfers it. Destruction never calls back into
a destroyed manager: the retention control owns the minimum independent state
needed to release safely.

## 9. `FBoundCookedGeneration`

| Field | Type | Rules |
|---|---|---|
| GenerationId | digest | From validated pointer/manifest |
| Directory | canonical contained path | Immutable while bound |
| ManifestDigest | digest | Agrees with pointer |
| TargetEvidence | immutable profile evidence | Manager-compatible |
| RecordsById | sorted/indexed manifest records | Unique and bounded |
| ReaderLease | `FGenerationReaderLease` | Shared, generation-scoped |

Binding order is pointer read -> shared lease -> exact generation validation ->
index publication. `Current.json` changes do not mutate a bound generation.

## 10. `FGenerationReaderLease`

| Field | Type | Rules |
|---|---|---|
| GenerationId | digest | Required |
| LeasePath | canonical Core path | CoordinationRoot/PublicationDigest/GenerationId.lease |
| NativeLease | private Core lease | Shared mode |
| State | Empty/Held/Released | Move-only, idempotent release |

Multiple shared readers coexist. Exclusive maintenance acquisition fails/times
out while any reader is held. Native release on process death is authoritative;
file contents are diagnostic only.

## 11. Loading Strategy Result

Both private strategies return the same scratch model:

| Field | Type | Rules |
|---|---|---|
| Result | stable category | Success or terminal failure |
| Outputs | sorted metadata/payload pairs | Atomic development multi-output or one cooked record |
| SourceEvidence | normalized records | Complete and pinned |
| DependencyDecisions | normalized records | Required or owning-payload-validated optional fallback policy |
| Diagnostics | bounded records | Stable ordering |

The scheduler, not a strategy, owns operation state, coalescing, cancellation,
cache publication, dependency retention, and completion delivery.

Each optional dependency decision records target, role, declared fallback token,
owning payload type, validation outcome, and stable reason. A strategy may emit
an optional decision only after the owning payload's type-specific validator has
confirmed that the immutable payload contains a satisfiable fallback. Missing,
undeclared, or unsatisfied fallback becomes a required root failure. No generic
manager heuristic may infer fallback from dependency strength alone.

## 12. Completion Queue And Inspection

`FAssetCompletionRecord` contains sequence, request identity, terminal state,
callback, and its admission reservation. The queue is ordered by sequence and
bounded. Callback-bearing admission fails when a reservation cannot be acquired;
dispatch or request release consumes it exactly once. Pump detaches first, then
invokes unlocked. Inspection snapshots copy bounded request, operation,
cache, generation, limit, and aggregate counters; they never expose mutable
containers, absolute temporary paths, addresses, timestamps, or thread IDs.

## Invariants

1. Every accepted request reaches exactly one terminal state.
2. One complete load key has at most one active operation and cache entry.
3. No callback or extension implementation executes while manager state is locked.
4. Ready roots retain every required dependency until root retention ends.
5. No cache entry remains with all retention classes zero.
6. Cooked mode performs zero resolver/importer fallback.
7. A source mutation publishes neither old/new mixed output nor partial cache.
8. Shutdown leaves no worker or manager-owned operation alive.
9. Every accepted callback-bearing request owns exactly one completion
   reservation until dispatch or request-interest release.

## Runtime Extension Control

`FAssetRuntimeExecutionContext` carries a shared cooperative cancellation token
and monotonic deadline into resolver/importer/loader request context. A runtime-
compatible extension checks the token at bounded work boundaries and returns no
later than its declared deadline. Manager shutdown never detaches or forcibly
terminates extension code; non-return is an extension contract violation.
