# Research: Runtime Asset Manager

## 1. Execution And State Ownership

**Decision**: Use a manager-owned fixed worker pool with bounded FIFO admission.
One private state mutex protects request slots, load-operation membership,
dependency retention counts, cache ownership, shutdown state, and completion
enqueue sequence. Filesystem I/O, extension calls, import, decode, and validation
run outside that mutex. Immutable scratch results commit in a short locked step.

**Rationale**: The repository has no general task graph. A bounded private
executor is portable and sufficient for complete-payload loading. Centralized
state ownership makes cancellation/coalescing invariants auditable.

**Alternatives considered**: `std::async` lacks scheduling control; one thread
per request violates bounds; a new global job system exceeds this feature;
synchronous loading contradicts the spec.

## 2. Completion Delivery

**Decision**: Polling never executes user code. Optional callbacks receive a
monotonic enqueue sequence after terminal commit. `PumpCompletions(MaxCount)`
detaches a bounded prefix under lock and invokes it unlocked on the pump thread.
Recursive pumping of the same manager fails; queries and new requests from
callbacks are allowed. Admission atomically reserves one queue slot for every
callback-bearing request and fails before acceptance when no slot is available;
dispatch or request-interest release consumes the reservation exactly once. No
public blocking wait is added.

**Rationale**: This implements the clarification, prevents worker affinity and
lock reentrancy surprises, and gives Application an explicit frame integration
point without an Application dependency.

**Alternatives considered**: Worker callbacks are nondeterministic; a dedicated
callback thread has the wrong affinity; blocking waits create dependency/pump
deadlock risks.

## 3. Request And Coalescing Identity

**Decision**: `FAssetRequestHandle` contains manager lifetime identity, slot
index, and generation. Each caller owns one request interest. Private
`FAssetLoadKey` contains AssetId, expected type, manager mode, target evidence
digest, and cooked generation ID when applicable. Only equal complete keys share
one physical operation.

**Rationale**: Slot generations prevent ABA after reuse. Separating caller
interest from physical work permits independent cancellation.

**Alternatives considered**: AssetId-only keys mix types/targets/generations;
shared caller handles couple cancellation; never recycling slots grows without
bound.

## 4. Dependency Scheduling

**Decision**: Build required closures incrementally from validated metadata.
Operations form a DAG keyed by `FAssetLoadKey`; roots retain all required nodes
until release. Use explicit DFS color/path state for cycles, checked depth/edge
limits, and AssetId ordering for ready work and normalized evidence. Soft
dependency failure is tolerated only when the owning payload contract explicitly
defines the selected fallback.

**Rationale**: Shared nodes avoid duplicate work across roots while root-specific
paths preserve actionable failure chains.

**Alternatives considered**: Recursive futures can deadlock a bounded pool;
loading every manifest record defeats on-demand behavior; tree ownership
duplicates shared dependencies.

## 5. Development Loading And Mutation

**Decision**: Use the 020 extension registry and dispatch contracts. Resolve
authoritative bytes, capture complete source-version evidence, import every
output into scratch storage, validate the atomic output set, then re-resolve and
re-hash all evidence before publication. Changed evidence returns stable
`SourceChanged` without retry. Existing handles remain immutable; a request
after unload resolves current bytes.

**Rationale**: This mirrors 025's proven input-snapshot rule and prevents one
changing source from publishing mixed subresources.

**Alternatives considered**: Retry can starve during edits; source locks are
unfriendly and non-portable; in-place mutation violates typed-handle safety.

## 6. Cooked Startup And On-Demand Payloads

**Decision**: Extend `FPublishedGenerationValidationRequest` with a policy whose
default remains full payload validation. Runtime startup uses `IndexAndLayout`:
validate pointer/manifest, generation identity, target evidence, canonical
unique records, safe payload locators, and bounded layout without decoding every
payload. Each request reads only its manifest locator and uses the existing
bounded typed codec, verifying size, digest, identity, type, codec/schema,
target, and dependencies before publication.

**Rationale**: The 025 validator currently decodes every payload. That is right
for offline validation but would eagerly load up to 100,000 Assets at runtime.
Policy separation preserves 025 defaults and strictness.

**Alternatives considered**: Full startup validation is too costly; trusting an
unvalidated index delays structural failures; duplicating a parser risks drift.

## 7. Generation Reader Lease

**Decision**: Add `EPlatformFileLeaseMode::{Shared,Exclusive}` to Core and keep
the old overload equivalent to Exclusive. POSIX uses `flock(LOCK_SH/LOCK_EX)`;
Windows uses shared/exclusive byte-range locks on share-enabled handles.
Process-local coordination uses a shared mutex. Shared readers do not rewrite
metadata; native ownership is authoritative. Cooked configuration supplies a
pre-provisioned writable coordination root independent of the publication root.
The lease path is `<coordination-root>/<publication-identity-digest>/<generation-id>.lease`;
the publication root may remain read-only.

Publication identity digest v1 is domain-separated SHA-256 over the existing
publication root's Core-canonical absolute UTF-8 path: symlinks are resolved,
Windows path spelling is separator-normalized and case-folded, and POSIX case is
preserved. The pre-provisioned coordination root must exist; the manager may
create only its digest-named namespace directory and lease files beneath it.
Normalized diagnostics expose the digest, never the absolute publication path.

Binding reads the pointer, acquires that generation's shared lease, then
validates the exact generation under the lease. Future maintenance must acquire
the same generation's exclusive lease before deletion.

**Rationale**: Generation scope allows new publication while old readers
continue. OS ownership releases on normal close or process death.

**Alternatives considered**: Process counts are invisible externally; PID files
need stale heuristics; locking `Current.json` couples readers to publication; a
lease file inside the generation races with directory removal.

## 8. Cache And Handle Ownership

**Decision**: Cache entries hold immutable payloads plus three explicit counts:
external typed handles, active/undelivered request interests, and required
dependency retentions. `TAssetHandle<T>` owns a shared control block that
releases the external class once. At zero for all classes, manager ownership and
index membership disappear immediately; already published handles may keep
payload memory alive after manager shutdown.

**Rationale**: Explicit classes make FR-027 inspectable and match the project's
existing immutable `shared_ptr<const FAssetPayload>` model.

**Alternatives considered**: LRU/grace retention belongs to streaming; raw
pointers cannot outlive shutdown; manager-bound handles impose destruction order.

## 9. Shutdown And Cancellation

**Decision**: Runtime strategy calls receive a cooperative cancellation token
and monotonic deadline through source-compatible request context. Conforming
extensions explicitly advertise runtime compatibility and must return within
that bound; legacy extensions remain valid for offline use but are rejected by
the runtime manager. Shutdown rejects admission, cancels
interests without published success, drains queued work, lets running extension
calls return into discard-only commit paths, joins all workers, commits every request terminal,
then releases cache and generation lease. Explicit shutdown may leave queued
callbacks pumpable; destruction discards unpumped callbacks without user-code
execution. Published typed handles stay valid.

**Rationale**: Arbitrary C++ extension code cannot be forcibly stopped safely.
The explicit compatibility contract makes the bounded shutdown claim testable;
joining bounded owned workers prevents use-after-free. A never-returning
third-party extension is diagnosed as contract-violating rather than detached or
forcefully terminated.

**Alternatives considered**: Detached workers violate lifetime; destructor
callbacks violate affinity; forceful thread termination is unsafe.

## 10. Validation And CI

**Decision**: Add a Feature 026 workflow with Windows/macOS/Linux Debug and
strict Release plus Linux ASan/UBSan and TSan. Tests are headless. A normalized
Python runner emits equivalence, request, coalescing, process-lease, shutdown,
stress, and timing evidence. Existing general CI remains; unrelated graphics
gates are not duplicated.

**Rationale**: Concurrency and native ownership differ across standard libraries
and OSes. Eight bounded jobs match the established repository pattern.

**Alternatives considered**: One-platform validation misses native semantics;
native graphics jobs consume CI without testing this feature's risk.
