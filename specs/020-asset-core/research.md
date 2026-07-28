# Research: Asset Core, Identity & Registry

## Decision 1: Asset Layer and Build Boundary

**Decision**: Add `Source/Asset` as a static engine layer whose only engine-layer
dependency is Core. Build it after Core and before RHI. Renderer and Application
may link Asset in later features; Feature 020 tests link it directly.

**Rationale**: The constitution defines Asset as a CPU-side sibling of RHI, not
as Renderer utility code. A dedicated build target lets SCons enforce forbidden
include directions before concrete formats arrive.

**Alternatives considered**:

- Put identity and registry in Core: rejected because format dispatch, cooking,
  loading, and metadata are a distinct subsystem that would bloat Core.
- Put assets in Renderer: rejected because non-rendering assets and offline
  tools must reuse the same identity and metadata contracts.

## Decision 2: Unicode NFC Implementation

**Decision**: Vendor `utf8proc 2.11.3` under `ThirdParty/utf8proc`, compile it
privately into Core, and expose only UTF-8 validation and stable NFC composition
through a focused `Core/FUnicode.h` API implemented in `FUnicode.cpp`. Asset
calls Core and never includes or links `utf8proc` directly. Record upstream
version and license. Compile third-party source with warning isolation while
keeping project-owned wrapper code under strict warnings.

**Rationale**: C++20 does not provide Unicode normalization. Unicode UAX #15
requires canonically equivalent strings to receive the same normalized binary
form and provides conformance data. `utf8proc` is a small cross-platform C
library, uses an MIT-style license plus the Unicode data license, supports
Windows/macOS/Linux, and version 2.11.3 carries Unicode 17 data and post-2.11
correctness fixes. Reimplementing normalization tables and Hangul/composition
rules would add high correctness risk unrelated to this engine's learning goals.
Putting the wrapper in Core preserves the literal `Asset -> Core` dependency
rule and makes canonical Unicode text available without public third-party
leakage.

**Alternatives considered**:

- Preserve raw UTF-8 bytes: rejected by the accepted NFC clarification.
- ICU: rejected because its footprint and broad internationalization surface
  are disproportionate to one normalization operation.
- Native Windows/macOS/Linux APIs: rejected because results and dependencies
  would vary by platform.
- Hand-written normalization: rejected because UAX #15 conformance and Unicode
  data maintenance are substantial.

**References**:

- https://unicode.org/reports/tr15/
- https://juliastrings.github.io/utf8proc/releases/
- https://github.com/JuliaStrings/utf8proc

## Decision 3: Canonical Identity Grammar

**Decision**: `FAssetId` contains an explicit symbolic asset type, a relative
logical path, and an optional subresource. Asset type text follows the
case-sensitive ASCII grammar `[A-Za-z][A-Za-z0-9_.-]*`, is at most 255 bytes,
and is not Unicode-normalized or case-folded. Path/subresource canonicalization:

1. Validates UTF-8 and normalizes path and subresource to NFC.
2. Converts `\` to `/`, collapses repeated separators, and removes `.` segments.
3. Rejects leading/trailing separators, empty result, `..`, drive/UNC roots,
   control characters, NUL, and reserved `:` or `#` delimiter use.
4. Preserves case.
5. Limits canonical identity text to 1,024 UTF-8 bytes; each path segment and
   subresource is limited to 255 bytes.

The stable diagnostic form is `Type:Logical/Path` with optional
`#Subresource`. Public equality compares all canonical components; internal
hashes never substitute for full equality.

**Rationale**: A platform-independent relative grammar remains manually
editable and resolver-neutral. Explicit limits prevent malformed input from
driving unbounded normalization or diagnostics.

**Alternatives considered**:

- Native filesystem paths: rejected because separators, roots, normalization,
  and case semantics differ by host.
- Case folding: rejected by the clarified case-sensitive identity contract.
- Percent escaping arbitrary delimiters: deferred because rejecting ambiguous
  reserved characters keeps v1 round-trips simple and testable.

## Decision 4: Version Digest

**Decision**: Represent source, imported-content, and cooked evidence as
independent optional `FAssetDigest` values. The initial digest algorithm is
SHA-256 with a 32-byte value and explicit algorithm tag. Provide a private,
portable SHA-256 implementation verified against NIST vectors; do not expose a
security/authentication claim. Equality compares availability, algorithm, size,
and all digest bytes. Diagnostic text uses lowercase hexadecimal. Tests
deliberately collide only internal lookup hashers and then prove full-value
comparison still distinguishes unequal values. Two available digests with the
same algorithm and bytes are equal revision evidence; Feature 020 does not
retain payload bytes to detect a genuine cryptographic collision.

**Rationale**: SHA-256 is standardized, cross-platform, widely interoperable,
and sufficient for content-change and derived-data keys. An algorithm tag
allows later migration. Full byte comparison and full identity equality keep
hash collisions from becoming identity collisions.

**Alternatives considered**:

- `std::hash`: rejected because it is an in-process lookup optimization without
  cross-run digest guarantees.
- FNV or CRC as version evidence: rejected because collision resistance is too
  weak for long-lived derived-data keys.
- BLAKE3: attractive for later cooker throughput, but adding another dependency
  is unnecessary before Feature 025 measures hashing cost.

**Reference**:

- https://csrc.nist.gov/pubs/fips/180-4/upd1/final

## Decision 5: Registry Concurrency and Atomic Batches

**Decision**: `FAssetRegistry` owns a `std::shared_mutex`. Queries acquire shared
access and return values/copies, never references or iterators into registry
storage. Writers are internally serialized. A mutation batch is normalized,
validated, cycle-checked, and staged before exclusive commit; commit updates
record, type, source, forward-edge, and reverse-edge indexes as one reader-
invisible critical section. Failure before commit leaves all indexes unchanged.

**Rationale**: This implements concurrent readers plus serialized atomic writes
without requiring callers to coordinate locks. Returning snapshots avoids
borrowed references surviving a mutation.

**Alternatives considered**:

- Caller-owned locking: rejected by clarification.
- Fully concurrent writes: rejected as unnecessary before runtime manager work.
- Immutable full-registry copy per mutation: rejected as excessive for future
  large registries.

## Decision 6: Dependency Resolution and Cycles

**Decision**: Required edges to absent targets are stored as `Unresolved`.
Registering a target changes matching reverse-indexed edges to `Resolved`.
Removing a target changes them back to `Unresolved`. Before a batch commits,
cycle detection runs over the proposed graph of known required edges. If a
newly resolved target would close a self-cycle or multi-asset cycle, the whole
batch fails. Soft references do not create graph edges unless metadata
explicitly declares them as dependencies.

**Rationale**: Discovery order remains independent while graph completeness is
inspectable. Rejecting cycles when they become knowable prevents a latent
invalid required graph.

**Alternatives considered**:

- Reject missing targets: rejected by clarification because it forces import
  order and oversized batches.
- Placeholder metadata: rejected because fabricated metadata obscures
  provenance and producer ownership.
- Accept required cycles as invalid state: rejected because later schedulers
  need an acyclic prerequisite graph.

## Decision 7: Resolver and Importer Dispatch

**Decision**:

- Resolver eligibility is based on declared logical domain/scheme.
- One unique highest priority resolver wins.
- Equal highest-priority resolvers return `AmbiguousResolver`, sorted by
  participant identity for diagnostics.
- Importer format hints create a candidate set when a hint exists; candidates
  receive a read-only prefix of at most 64 KiB, bounded further by their
  declared probe limit.
- After hint filtering, at most 64 eligible importers may be probed. More than
  64 returns `CapacityExceeded` before source bytes are read or any probe
  callback runs. Registration itself is not capped because capabilities may be
  disjoint.
- Probe confidence is an integer `0..100`; `0` means no match. One unique
  highest nonzero score wins. Equal leaders return `AmbiguousImporter`.
- A declared hint with no accepting candidate fails rather than probing
  unrelated formats. Sources without a hint may probe all eligible importers.
- Registration order is never a functional tie-breaker.

**Rationale**: Explicit priorities support development/cooked overlays. Bounded
content confirmation avoids extension-only false positives and unbounded reads.
The candidate cap bounds one dispatch to at most 4 MiB of aggregate probe input.
Ambiguity failures reveal configuration errors rather than hiding them.

**Alternatives considered**:

- First registered wins: rejected as initialization-order dependent.
- Try resolvers until success: rejected because it masks corruption and
  permission failures as fallback behavior.
- Probe complete files: rejected as unsafe and unnecessary for format
  identification.

## Decision 8: Extension Ownership and Execution Leases

**Decision**: `FAssetExtensionRegistry` returns a move-only scoped registration
token. Dispatch acquires a copyable execution lease that retains shared
ownership of the selected extension before invoking it. Releasing the token
atomically marks the record inactive and removes it from future candidate
snapshots. Existing leases remain valid; the instance is destroyed once after
the last lease releases.

**Rationale**: Registration lifetime and in-flight execution lifetime are
different facts. This design is non-blocking at unregistration, prevents stale
callbacks, and does not introduce Feature 026 cancellation policy.

**Alternatives considered**:

- Blocking token destruction: rejected because callback re-entry can deadlock.
- Forced cancellation: rejected because current interfaces are synchronous and
  cancellation belongs to the runtime manager.
- Raw extension pointers: rejected because they cannot prove in-flight
  lifetime.

## Decision 9: CPU Payload Boundary

**Decision**: Define a minimal Asset-owned `FAssetPayload` polymorphic base with
virtual destruction and explicit asset type. Import and load results may retain
`TSharedPtr<const FAssetPayload>`. Synthetic payloads validate the contract in
Feature 020; concrete `FImageAsset`, `FTextureAsset`, material, shader, and mesh
payloads derive from it later. Cook results use a separate byte artifact plus
target profile and digest because cooked output is serialized delivery data.

**Rationale**: A typed CPU-side base avoids `void*`, backend handles, and raw
destroy callbacks while allowing later payload classes without changing
extension signatures.

**Alternatives considered**:

- `void*` with deleter: rejected as weakly typed and easy to misuse.
- Raw byte arrays for all runtime payloads: rejected because images, materials,
  and meshes need structured CPU data.
- Template-only extension interfaces: rejected because heterogeneous runtime
  registration and dispatch need one stable ABI-level vocabulary.

## Decision 10: Focused Test Suite Selection

**Decision**: Refactor `Tests/Main.cpp` around a small static suite table. The
test executable supports:

- no arguments: run every suite, preserving current CI behavior;
- `--list-suites`: print canonical suite names and exit;
- one or more `--suite <name>` pairs: run only the selected suites in registry
  order, deduplicated;
- `--suite all`: explicitly run all suites;
- unknown/missing/malformed arguments: print usage and return exit code `2`;
- test failure: return `1`; success: return `0`.

Existing logging child-process arguments are recognized before general parsing.
The Asset suite canonical name is `asset`.

Failure propagation is tested through the reusable suite table in process: a
test-only fake callback registered as `asset` returns a failure while counters
prove unselected callbacks did not run. No environment variable, output
filter, hidden CLI option, or change to the real Asset suite is used.

**Rationale**: This closes `CR001-B09-F003` with reusable infrastructure for all
later features rather than another environment-variable skip.

**Alternatives considered**:

- Separate Asset test executable: rejected because it duplicates linking,
  startup, and CI wiring.
- Environment skip variables: rejected by the CR finding.
- Output filtering: rejected because it does not prevent unrelated suites from
  running.

## Decision 11: Validation and CI

**Decision**: Add `StonerTest --suite asset` to each Debug platform job, retain
the no-argument full suite in existing deterministic orchestration, retain
three-platform Release strict builds, and retain Linux ASan/UBSan full-suite
execution. No graphics runtime, screenshot, or native backend artifact is
required for Feature 020.

**Rationale**: Path normalization, thread behavior, C compilation, and static
link order are platform-sensitive even though the feature is graphics-free.
Focused execution proves suite selection while full runs protect regressions.

**Alternatives considered**:

- Linux-only focused tests: rejected by the constitution and SC-010.
- New Asset-specific workflow: rejected because the existing matrix already
  owns compiler and platform setup.

## Decision 12: Source Locator and Resolver Failure Vocabulary

**Decision**: `FAssetSourceLocator` stores a canonical lowercase ASCII scheme
matching `[a-z][a-z0-9+.-]*` with a 63-byte limit and an NFC, case-sensitive
UTF-8 locator of 1–1,024 bytes without NUL/control characters. Equality,
ordering, and hashing compare scheme then locator bytes and never consult native
filesystem behavior. Resolver failures map absent, inaccessible, malformed, and
retryable sources to `NotFound`, `AccessDenied`, `MalformedSource`, and
`TransientFailure`.

**Rationale**: Source lookup is a registry index and therefore needs the same
cross-platform value stability as asset identity. Explicit failure categories
prevent native errno or platform text from becoming public behavior.

**Alternatives considered**:

- Native filesystem path equality: rejected because case and normalization vary
  by host.
- One generic processing failure: rejected because callers need to distinguish
  permanent absence, access policy, malformed data, and retryable storage.

## Decision 13: Participant and Metadata Equivalence

**Decision**: `FAssetParticipantId` uses case-sensitive ASCII
`[A-Za-z][A-Za-z0-9_.-]*` up to 127 bytes. `FAssetProducerVersion` uses
case-sensitive ASCII `[A-Za-z0-9][A-Za-z0-9_.+-]*` up to 64 bytes. Canonical
metadata equality compares identity, complete version, canonical source,
participant/version, attributes as a key/value set, and dependency declarations
sorted by target/role/strength. It ignores memory layout, insertion order, and
registry-derived resolution state.

**Rationale**: Extension uniqueness and diagnostics require stable token
ordering, while idempotent registration must compare semantic metadata rather
than implementation-specific container bytes.

**Alternatives considered**:

- Arbitrary UTF-8 participant names: rejected because these are implementation
  keys rather than user-facing labels and do not need Unicode aliases.
- Raw object-byte comparison: rejected because padding and container layout are
  neither portable nor semantic.

## Decision 14: Opt-In Registry Benchmark

**Decision**: Build `StonerAssetBenchmark` as a standalone opt-in executable
from `Tests/AssetRegistryBenchmark.cpp`. It records the 10,000-record/50,000-edge
sample but is excluded from `StonerTest`, no-argument regression, sanitizer
gates, and CI. Timing text is informational and not normalized evidence.

**Rationale**: This preserves a repeatable engineering sample without adding
unstable timing output or recurring cost to correctness gates.

**Alternatives considered**:

- Run inside the Asset suite: rejected because every focused/full CI run would
  pay the cost and expose timing noise.
- Add timing thresholds: deferred until representative hardware and performance
  budgets exist.

## Open Questions

None. All technical unknowns needed for task generation are resolved.
