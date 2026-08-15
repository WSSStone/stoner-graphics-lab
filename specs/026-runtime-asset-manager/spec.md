# Feature Specification: Runtime Asset Manager

**Feature Branch**: `026-runtime-asset-manager`  
**Created**: 2026-08-15  
**Status**: Draft  
**Input**: User description: "为 roadmap 下一阶段指定 spec"

## Clarifications

### Session 2026-08-15

- Q: How are asynchronous request completions delivered, and what thread-affinity guarantee applies? → A: Request state is pollable; completion callbacks are queued and dispatched in deterministic order only by an explicit completion pump, on the thread calling that pump. Worker threads never invoke consumer callbacks directly.
- Q: What retention and unload policy applies after the final external typed Asset handle is released? → A: Use reference-driven deterministic unload. Remove manager cache ownership immediately after all external handles, request interests, and required-dependency retentions reach zero; a later request reloads the Asset.
- Q: Must generation reader leases be visible across processes to future pruning and packaging tools? → A: Yes. Use local-machine cross-process shared reader ownership at generation granularity; operating-system ownership releases crashed readers automatically, and maintenance cannot acquire exclusive generation ownership while any reader remains.
- Q: How does development mode handle source changes during or after a load? → A: Pin source and version evidence when loading begins; fail without automatic retry if that evidence changes during the load. Published handles remain immutable without hot reload, and a request made after deterministic unload resolves the current source version.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Load Typed Assets Through One Runtime Contract (Priority: P1)

An engine subsystem requests a typed Asset by stable identity and receives the
same validated semantic content whether the manager is configured for
development source loading or strict cooked-generation loading. The subsystem
does not need to know which physical representation supplied the Asset.

**Why this priority**: A uniform source/cooked loading contract is the minimum
runtime value of the Asset pipeline and the direct consumer of Features 020-025.

**Independent Test**: Request every representative Feature 021-024 Asset family
once in development mode and once in cooked mode, then compare identity, type,
dependency roles, versions, and normalized semantic content.

**Acceptance Scenarios**:

1. **Given** a valid source-backed Asset and development mode, **When** its typed
   identity is requested, **Then** the complete validated Asset and all required
   dependencies become available through a typed handle.
2. **Given** a valid published cooked generation and strict cooked mode, **When**
   the same identity is requested, **Then** the matching payload is loaded only
   from the validated generation with no source fallback.
3. **Given** a requested type that does not match the Asset identity or payload,
   **When** the request is processed, **Then** it fails without publishing a
   partially typed handle.
4. **Given** a missing, corrupt, incompatible, or unlisted cooked payload,
   **When** it is requested in strict cooked mode, **Then** the request fails
   closed and no development importer is invoked.
5. **Given** a development source changes during loading, **When** the manager
   validates the pinned source and version evidence, **Then** the request fails
   without automatic retry or partial publication; a later request may load the
   current source version.

---

### User Story 2 - Share Concurrent Work Without Sharing Cancellation (Priority: P1)

Multiple engine systems may request the same Asset concurrently. Equivalent
requests share one dependency and loading operation, while each caller retains
an independent request handle and may cancel its own interest without
incorrectly cancelling other callers.

**Why this priority**: Duplicate loading, race-dependent publication, and
cross-caller cancellation are the principal correctness risks of a runtime
manager.

**Independent Test**: Issue at least eight simultaneous requests for one root
Asset with a non-trivial dependency closure, cancel selected callers at every
request state, and verify one load operation, consistent results for remaining
callers, and no partial publication.

**Acceptance Scenarios**:

1. **Given** equivalent in-flight requests, **When** they overlap, **Then** they
   share one underlying load and each caller receives an independently owned
   request result.
2. **Given** two callers share one load, **When** one caller cancels, **Then** the
   other caller can still complete successfully.
3. **Given** all callers cancel before a load is required by another root,
   **When** cancellation is observed, **Then** unnecessary pending work is
   cancelled and no successful handle is published to the cancelled callers.
4. **Given** a required dependency fails, **When** the root request resolves,
   **Then** the root fails with the dependency chain and all caller-visible
   states reach one terminal outcome.

---

### User Story 3 - Retain Safe Typed Handles and Unload Deterministically (Priority: P2)

Engine systems keep immutable typed Asset handles across frames without owning
loader internals. Handles keep their referenced payload alive, stale request
identities cannot alias new requests, and manager shutdown or unloading never
invalidates memory still retained by a valid handle.

**Why this priority**: Correct lifetime separation lets Renderer and Application
consume CPU Assets safely before later streaming and GPU-residency features.

**Independent Test**: Repeatedly load, share, release, reload, cancel, and shut
down a manager while retaining selected typed handles; verify stable payloads,
balanced lifetime accounting, and deterministic terminal states.

**Acceptance Scenarios**:

1. **Given** multiple typed handles to one loaded Asset, **When** some handles are
   released, **Then** remaining handles continue to expose the same immutable
   payload.
2. **Given** the final external handle is released, **When** the manager applies
   its retention policy, **Then** it immediately removes cache ownership once no
   request interest or loaded root retains that Asset as a required dependency;
   a later request performs a new load.
3. **Given** a manager shutdown begins, **When** new and in-flight requests
   interact with it, **Then** new requests are rejected, active requests reach
   deterministic terminal states, and already published typed handles remain
   memory-safe until released.
4. **Given** a prior request handle has reached a terminal state and its internal
   slot is reused, **When** the stale handle is queried or cancelled, **Then** it
   cannot observe or mutate the newer request.

---

### User Story 4 - Protect the Active Cooked Generation (Priority: P2)

A strict cooked-mode manager validates one published generation before serving
requests and retains reader evidence for as long as that generation may be read.
Future maintenance tooling can distinguish live generations from candidates for
safe pruning without giving the runtime ownership of pruning policy.

**Why this priority**: Feature 025 publishes immutable generations, so Feature
026 must establish reader lifetime before later packaging or cleanup can safely
remove old output.

**Independent Test**: Open a generation, serve concurrent payload requests,
attempt a competing maintenance observation, release all manager readers, and
verify that live-generation evidence follows the complete reader lifetime.

**Acceptance Scenarios**:

1. **Given** a valid current pointer, manifest, and payload set, **When** a strict
   cooked manager starts, **Then** it validates and binds one immutable
   generation before accepting requests.
2. **Given** a manager is serving a generation, **When** the current pointer is
   later replaced, **Then** the existing manager continues to read its bound
   generation and does not perform implicit hot reload.
3. **Given** a generation remains bound or a payload read is active, **When**
   liveness is inspected from another process, **Then** that generation reports
   shared reader ownership; exclusive maintenance ownership cannot be acquired
   until every reader releases or exits.
4. **Given** generation validation fails during manager startup, **When** a
   request is attempted, **Then** no payload from that generation is served.
5. **Given** a valid read-only publication root and a separate pre-provisioned
   writable lease-coordination root, **When** a cooked manager starts, **Then**
   it acquires generation ownership without modifying the publication root and
   exposes only normalized coordination identity in diagnostics.

---

### User Story 5 - Observe and Stress Runtime Loading (Priority: P3)

Developers can inspect request, dependency, cache, cancellation, failure,
generation, and shutdown behavior through bounded deterministic diagnostics.
The manager remains predictable under repeated and concurrent workloads.

**Why this priority**: Runtime ownership bugs are difficult to reproduce without
stable state evidence and repeatable stress scenarios.

**Independent Test**: Run deterministic request traces and bounded stress
workloads in development and cooked modes, compare normalized reports over
twenty repetitions, and verify all resource and request counts return to their
expected terminal values.

**Acceptance Scenarios**:

1. **Given** a completed or failed request, **When** it is inspected, **Then** the
   report identifies stable Asset identity, mode, state transitions, dependency
   chain, coalescing decision, cache decision, and actionable result category.
2. **Given** equivalent request traces, **When** they run repeatedly with
   different worker completion orders, **Then** normalized outcomes and
   diagnostics are identical.
3. **Given** a request completes away from its consumer, **When** its state is
   queried, **Then** the terminal state is observable without dispatching user
   code; **When** the consumer explicitly pumps completions, **Then** queued
   callbacks run in deterministic order on the thread calling the pump, and
   worker threads never invoke consumer callbacks directly.

### Edge Cases

- The requested root exists but a required dependency is absent, corrupt,
  cancelled, cyclic, type-incompatible, or outside the bound generation.
- Source-backed loading emits multiple subresources while only one typed
  identity was requested.
- A development source changes while its load is in flight or while a published
  typed handle still retains the prior immutable payload.
- Equivalent requests arrive while the first request is resolving, loading,
  publishing, cancelling, or already terminal.
- A cancelled dependency is still required by another non-cancelled root.
- A caller releases its request handle before completion but retains a typed
  Asset handle obtained earlier.
- The manager reaches configured request, dependency-depth, diagnostic, byte,
  or concurrent-work limits.
- Shutdown races with source resolution, dependency completion, cooked payload
  reads, callback/poll delivery, and final handle release.
- A cooked runtime process exits normally or crashes while holding generation
  reader ownership; stale metadata exists without a live operating-system owner.
- The publication root is readable but not writable, or the separately
  configured lease-coordination root is absent, read-only, ambiguously shared
  with another publication namespace, or outside its configured limits.
- The current pointer changes or disappears after a cooked manager has bound its
  immutable generation.
- A cooked manifest is valid but names an unsupported codec, payload schema,
  required extension, or target profile.
- A stale request handle refers to a reused internal slot or prior manager
  lifetime.
- Two logically equal requests use incompatible expected types, target evidence,
  modes, or bound generations and therefore must not coalesce.

## Architecture & Design Constraints *(mandatory)*

- **Layer Boundary**: Runtime Asset Manager ownership MUST remain in `Asset ->
  Core`. Asset MUST NOT depend on Tools, RHI, Renderer, Application, Backend, or
  any graphics API. Offline Tools MAY consume the same public Asset contracts.
- **Runtime/Tool Separation**: The manager MUST consume public identity,
  resolver/importer/loader, cooked payload, manifest, and published-generation
  validation contracts without linking or reaching into `Tools/AssetCooker`.
- **CPU/GPU Ownership**: The manager owns CPU payload request and cache lifetime
  only. Renderer remains responsible for RHI realization and GPU residency.
- **Responsibility Separation**: Request state, dependency scheduling, source
  and cooked loading strategies, cache ownership, handle lifetime, generation
  reading, and diagnostics MUST remain separable responsibilities; no manager
  god-class or giant orchestration function is permitted.
- **Advanced Asset Readiness**: Typed handles and dependency scheduling MUST
  permit later meshlet, ray-tracing, SDF, shader-backend, and chunked payloads
  without making those derived Assets a second source authority.
- **Naming Conventions**: New engine-facing contracts MUST follow PascalCase and
  Unreal Engine-style naming conventions.
- **Cross-Platform Compatibility**: Source-backed and cooked loading,
  cancellation, shutdown, synchronization, and normalized diagnostics MUST run
  on Windows, macOS, and Linux. Platform-specific behavior MUST remain behind
  Core abstractions.
- **Automated Cross-Platform Validation**: Windows, macOS, and Linux automated
  validation MUST cover focused runtime-manager behavior and strict Release;
  applicable sanitizer coverage MUST exercise concurrency and lifetime paths.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST provide one runtime Asset Manager whose operating
  mode is selected explicitly for its lifetime as development source-backed or
  strict cooked-generation loading.
- **FR-002**: A manager MUST reject an absent, contradictory, malformed, or
  unsupported operating configuration before accepting requests.
- **FR-003**: Strict cooked mode MUST validate and bind one immutable published
  generation through the shared Asset generation-validation contract before
  accepting requests.
- **FR-004**: Strict cooked mode MUST NOT invoke source resolvers, importers, or
  implicit development fallback after startup or request failure.
- **FR-005**: Development mode MUST resolve and load Assets through registered
  public Asset contracts without depending on one repository path or concrete
  source format.
- **FR-006**: Both modes MUST preserve the same canonical `FAssetId`, requested
  type, dependency roles, source/version evidence, target-profile meaning, and
  normalized typed payload semantics for equivalent content.
- **FR-007**: Every request MUST identify a valid typed Asset identity and MUST
  reject type mismatch before publishing a typed handle.
- **FR-008**: The manager MUST support non-blocking requests whose observable
  states distinguish at least accepted, waiting for dependencies, loading,
  ready, failed, and cancelled outcomes.
- **FR-009**: Every accepted request MUST reach exactly one terminal state and
  terminal state observation MUST be idempotent.
- **FR-010**: Request handles MUST be generation-safe so stale handles cannot
  inspect, cancel, or retain a reused request slot or another manager lifetime.
- **FR-011**: Equivalent overlapping requests MUST coalesce into one underlying
  dependency/load operation. Equivalence MUST include Asset identity, expected
  type, operating mode, target evidence, and bound cooked generation where
  applicable.
- **FR-012**: Non-equivalent requests MUST NOT coalesce even when they reference
  the same logical Asset identity.
- **FR-013**: Each coalesced caller MUST retain an independent cancellation and
  result-observation contract.
- **FR-014**: Cancelling one caller MUST NOT cancel work still required by
  another caller, loaded root, or non-cancelled dependency closure.
- **FR-015**: Work with no remaining caller or dependency interest SHOULD be
  cancelled before publication when cancellation is still possible, and MUST
  otherwise complete without publishing success to cancelled callers.
- **FR-016**: Cancellation MUST be idempotent and a late cancellation MUST NOT
  convert an already published successful typed handle into invalid memory.
- **FR-017**: The manager MUST resolve the complete required transitive
  dependency closure before declaring a root ready.
- **FR-018**: Dependency scheduling MUST reject missing, self-referential,
  cyclic, type-incompatible, version-incompatible, or limit-exceeding closure
  with a stable root-to-failure identity chain.
- **FR-019**: One dependency operation MAY be shared across multiple roots, but
  failure or cancellation accounting MUST remain correct for every interested
  root.
- **FR-020**: A root MUST NOT become ready until all required dependencies are
  valid and retained for the root's published payload contract.
- **FR-021**: Optional dependencies MAY fail without failing the root only when
  the owning Asset contract explicitly defines that fallback; the decision MUST
  be inspectable. An undeclared fallback or a failed optional dependency whose
  declared fallback cannot be satisfied MUST fail the root.
- **FR-022**: Development mode MUST publish all typed subresources emitted by a
  source import atomically into the request context while returning only the
  requested typed identity to that caller.
- **FR-023**: Cooked mode MUST load only manifest-listed payload locators and
  MUST verify identity, type, codec revision, payload schema, size, digest,
  target compatibility, and dependency agreement before publication.
- **FR-024**: Unsupported required extensions, codecs, schemas, targets, or
  payload types MUST fail closed without a partially visible cache entry.
- **FR-025**: A typed Asset handle MUST expose immutable CPU payload ownership
  without exposing loader, scheduler, filesystem, or manager-internal state.
- **FR-026**: Multiple typed handles for one loaded payload MUST share ownership
  safely and MUST preserve that payload until the final retaining handle is
  released.
- **FR-027**: Loaded-Asset retention MUST be reference-driven. The manager MUST
  remove its cache ownership immediately when external typed handles, active or
  undelivered request interests, and required-dependency retentions all reach
  zero. It MUST NOT retain an unreferenced payload until trim, timeout, capacity
  pressure, or shutdown; a later request performs a new load.
- **FR-028**: Reloading an unloaded Asset MUST never revive stale handles or
  mutate a payload retained by a prior valid handle.
- **FR-029**: The manager MUST provide explicit cache and request inspection
  without exposing mutable internal containers.
- **FR-030**: Manager shutdown MUST reject new requests, resolve or cancel
  accepted requests deterministically, stop manager-owned work, and release all
  manager-owned payload references exactly once. Runtime-compatible resolver,
  importer, and loader calls MUST receive a cooperative cancellation token and
  monotonic deadline and MUST return within that declared bound; the manager
  MUST NOT detach or forcibly terminate extension code.
- **FR-031**: Shutdown MUST NOT invalidate payload memory retained by typed
  handles that were successfully published before shutdown.
- **FR-032**: Manager destruction MUST NOT require callers to destroy successful
  typed handles first.
- **FR-033**: A cooked manager MUST require an explicit, pre-provisioned,
  writable lease-coordination root distinct from the potentially read-only
  publication root. It MUST derive a collision-resistant publication namespace
  from the canonical publication identity and retain local-machine cross-process
  shared reader ownership for its bound generation within that namespace from
  successful startup until no manager operation can read that generation.
  Multiple readers of the same generation MUST be able to coexist.
- **FR-034**: A separate process MUST be able to determine through ownership
  acquisition whether a generation has live readers. Exclusive maintenance
  ownership MUST remain unavailable while any reader is live; normal release or
  process exit MUST release ownership automatically without PID probing,
  wall-clock expiry, or forced process intervention. The runtime MUST NOT gain
  generation pruning authority.
- **FR-035**: A cooked manager MUST continue using its bound immutable
  generation when `Current.json` changes; automatic generation switching and
  hot reload are excluded.
- **FR-036**: Request state MUST be queryable without dispatching user code.
  Completion callbacks MUST be queued and dispatched in deterministic order
  only by an explicit completion pump, MUST run on the thread calling that pump,
  and MUST NOT be invoked directly by worker threads. Callback reentrancy into
  query and new-request operations MUST be defined and safe; recursively pumping
  the same completion queue MUST fail without nested dispatch. Admission of a
  callback-bearing request MUST atomically reserve one bounded completion slot;
  lack of capacity MUST reject admission, and each reservation MUST be released
  exactly once by dispatch or request-interest release.
- **FR-037**: Equivalent ready, failed, cancelled, and shutdown outcomes MUST
  produce deterministic normalized result categories independent of worker
  completion order.
- **FR-038**: Diagnostics MUST identify stable Asset identity, requested type,
  mode, bound generation when applicable, request state, dependency chain,
  coalescing/cache decision, and actionable failure reason.
- **FR-039**: Diagnostic and inspection output MUST be bounded, deterministically
  ordered, and free of raw native handles, absolute temporary paths, scheduler
  addresses, timestamps, and thread identifiers in normalized evidence.
- **FR-040**: The manager MUST enforce explicit finite limits for accepted and
  in-flight requests, dependency count/depth, loaded payload bytes, per-payload
  bytes, diagnostic count, completion reservations, extension deadlines, and
  concurrent work.
- **FR-041**: Limit, allocation, resolution, decoding, cancellation, and shutdown
  failures MUST leave no partially published request result or cache entry.
- **FR-042**: Public request, manager, typed-handle, and inspection contracts MUST
  contain no Tools, RHI, Renderer, Backend, graphics API, or native platform
  types.
- **FR-043**: Offline cooking, generation pruning, DDC garbage collection,
  package construction, budget eviction, chunk streaming, GPU residency, hot
  reload, network storage, and editor database behavior MUST NOT be introduced.
- **FR-044**: The feature MUST include deterministic development/cooked
  equivalence, coalescing, cancellation, dependency, cache, generation,
  repeated-load, shutdown, malformed-input, concurrency, and stress validation.
- **FR-045**: Automated validation MUST build and run focused tests on Windows,
  macOS, and Linux with strict Release and applicable memory, undefined-behavior,
  and thread sanitizer coverage.
- **FR-046**: Each development-mode load MUST pin its source and version evidence
  before publication. If that evidence changes while loading, the operation MUST
  fail terminally without automatic retry or partial publication. Successfully
  published payloads and existing typed handles MUST remain immutable and MUST
  NOT hot reload; after deterministic unload, a later request MUST resolve the
  then-current source version.

### Key Entities

- **Runtime Asset Manager**: Owns one loading mode, accepted requests,
  dependency scheduling, CPU payload cache references, generation binding,
  diagnostics, and orderly shutdown.
- **Asset Request**: One caller's typed interest in an Asset, including its
  identity, expected type, state, cancellation interest, and terminal result.
- **Request Handle**: Generation-safe caller capability for observing or
  cancelling one request interest without exposing shared work internals.
- **Typed Asset Handle**: Immutable typed ownership of one successfully loaded
  CPU Asset payload, independent of manager lifetime.
- **Shared Load Operation**: Coalesced physical resolution/loading work plus the
  set of interested requests and roots.
- **Dependency Operation**: Shared load and retention state for one node in one
  or more required root closures.
- **Runtime Asset Cache**: Manager-owned mapping from complete load identity to
  validated immutable payload and explicit external-handle, request-interest,
  and required-dependency retention evidence; entries are removed as soon as
  all three retention classes reach zero.
- **Loading Strategy**: Development source-backed or strict cooked-generation
  behavior selected explicitly for one manager lifetime.
- **Bound Generation**: One validated immutable Feature 025 generation used by a
  strict cooked manager without implicit switching.
- **Generation Reader Lease**: Local-machine cross-process shared ownership for
  one immutable generation. Multiple readers may coexist, operating-system
  lifetime releases crashed readers, and it conveys no pruning authority.
- **Request Diagnostic Record**: Bounded normalized evidence for state,
  dependency, coalescing, cache, cancellation, generation, and failure behavior.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Every representative Feature 021-024 Asset family loads
  successfully in both development and strict cooked modes with 100% agreement
  in identity, type, dependency roles, version evidence, and normalized semantic
  content.
- **SC-002**: Eight or more simultaneous equivalent requests for the same root
  and dependency closure perform exactly one underlying load per unique node
  and return equivalent successful handles to every non-cancelled caller.
- **SC-003**: A cancellation matrix covering every non-terminal request state,
  single-caller cancellation, all-caller cancellation, and shared-dependency
  cancellation completes 100 repeated runs without cross-caller cancellation,
  partial success, deadlock, or inconsistent terminal state.
- **SC-004**: A dependency corpus containing missing, cyclic, type-mismatched,
  version-mismatched, corrupt, cancelled, and limit-exceeding nodes rejects 100%
  of invalid roots with the expected stable dependency chain.
- **SC-005**: At least 10,000 repeated load/share/release/reload operations return
  all manager-owned request, operation, payload, and byte counts to expected
  terminal values with no stale-handle aliasing.
- **SC-006**: At least 100 manager startup/shutdown iterations with concurrent
  requests and conforming cooperative extensions complete within the configured
  monotonic deadline with no accepted request left non-terminal and no
  manager-owned work surviving shutdown; a deliberately non-conforming bounded
  test extension is diagnosed without detached work or forced termination.
- **SC-007**: Strict cooked mode detects 100% of a corpus covering malformed
  pointers/manifests, unsupported revisions/extensions, unsafe locators,
  missing/substituted payloads, digest/size/type/dependency disagreement, and
  target incompatibility, while invoking zero source fallbacks.
- **SC-008**: In normal release and forced-process-exit tests, cross-process
  shared reader ownership blocks exclusive maintenance acquisition for 100% of
  each bound-generation read lifetime and permits acquisition after every live
  reader has released or exited.
- **SC-009**: Twenty repetitions of equivalent development and cooked request
  traces produce identical normalized terminal states, dependency order,
  coalescing/cache decisions, and diagnostics despite varied worker completion
  order.
- **SC-010**: A representative graph of at least 1,000 Assets and 5,000
  dependency edges completes bounded loading, shared-request, cancellation, and
  shutdown validation within resource budgets established during planning.
- **SC-011**: Architecture checks report zero Asset-to-Tools/RHI/Renderer/
  Application/Backend dependencies and zero public native/platform type leaks.
- **SC-012**: Windows, macOS, and Linux focused Debug and strict Release gates,
  applicable sanitizer suites, and all pre-existing regression suites pass on
  the final feature revision.
- **SC-013**: A development-source mutation matrix covering every pre-publication
  load stage rejects 100% of changed-evidence operations without automatic retry
  or partial publication, preserves every already published handle unchanged,
  and loads the current source version on the first request after unload.

## Assumptions

- Features 020 and 025 public identity, registry, extension, manifest, payload,
  generation validation, and diagnostic contracts remain authoritative.
- Feature 025's published generation is immutable; one manager binds one
  generation and does not observe implicit updates to `Current.json`.
- Development source-backed loading is intended for developer workflows, while
  strict cooked loading is the release behavior. Mode selection is explicit and
  immutable for one manager lifetime.
- Development source mutation is detected through pinned source and version
  evidence. Feature 026 provides neither transparent retry nor hot reload.
- Source and cooked representations may differ physically but expose the same
  normalized typed payload meaning.
- Required dependency closure must be complete before a root becomes ready;
  optional fallback exists only when an owning Asset contract declares it.
- Runtime-compatible extensions accept cancellation/deadline context and return
  within their declared bound. Arbitrary third-party code that never returns is
  an extension contract violation that cannot be safely force-terminated by the
  manager.
- Typed Asset handles own immutable CPU payloads. Renderer may consume them to
  create RHI resources but that GPU lifetime is outside this feature.
- Successful payloads have no unreferenced grace cache in Feature 026. Keeping a
  typed handle is the explicit mechanism for keeping a complete Asset resident.
- Request priorities and partial chunk availability are deferred to Asset
  Streaming & Residency; Feature 026 schedules complete Asset payloads only.
- Generation pruning, DDC cleanup, package archives, and remote cache behavior
  belong to a future Tools/Packaging maintenance track.
- Reader ownership coordinates processes on one machine only. Cross-machine
  distributed coordination remains outside this roadmap stage.
- Cooked deployments provide the same explicit writable lease-coordination root
  to runtime readers and future maintenance tools. The publication root itself
  may remain read-only and is never modified to acquire reader ownership.
- Publication coordination namespace identity is a versioned, domain-separated
  digest of the Core-canonical absolute publication path; normalized diagnostics
  expose only that digest. Manager startup may create its namespaced lease
  directory beneath the pre-provisioned coordination root.
