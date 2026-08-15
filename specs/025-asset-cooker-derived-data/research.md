# Research: Asset Cooker, Manifest & Derived Data

## Offline Delivery Model

### Decision: Implement deterministic cook-by-the-book only

Feature 025 provides a standalone `StonerAssetCooker` command that discovers
source assets, builds a dependency graph, cooks one explicit target profile,
validates an immutable generation, and publishes it. It supports clean,
incremental, plan-only, inspection, published-output validation, and strict
cache validation modes. It does not start a cook server and does not serve
runtime requests.

**Rationale**: Cooking converts authoring formats into target-ready runtime
formats. Unreal separates advance "by the book" cooking from networked cook on
the fly; this feature needs the former contract before Feature 026 can own
runtime requests. Keeping network and runtime scheduling out also preserves the
roadmap split.

**Alternatives considered**:

- Cook on the fly server: rejected as Feature 026/runtime and network scope.
- Integrate cooker into the demo executable: rejected because offline tooling
  must not require Application, Renderer, RHI, or Backend.
- Build only a library with no CLI: rejected because clean-machine automation,
  stable exit categories, and standalone validation require a user entry point.

**Sources**:

- [Unreal Engine content cooking](https://dev.epicgames.com/documentation/unreal-engine/cooking-content-in-unreal-engine)
- [Unreal build operations and cook-by-the-book](https://dev.epicgames.com/documentation/unreal-engine/build-operations-cooking-packaging-deploying-and-running-projects-in-unreal-engine)

## Layer Ownership

### Decision: Put reusable data contracts and codecs in Asset; orchestration stays in Tools

Asset owns `FAssetTargetProfile`, derived-key evidence, cooked payload envelope,
manifest models/codecs, payload cooker/loader registrations, and validation
that Feature 026 will later consume. `Tools/AssetCooker` owns source-root
enumeration, root selection, graph scheduling, filesystem DDC policy, staging,
publication, reports, and CLI parsing. Core gains only generally reusable
filesystem primitives: bounded recursive enumeration, canonical path checks,
atomic same-volume move/replace, safe removal, durable write, and an RAII
cross-process file lease.

**Rationale**: Runtime modules may depend on Asset but never on Tools. Putting
manifest or cooked decoding in Tools would force Feature 026 to violate that
boundary or duplicate the format. Putting offline graph coordination in Asset
would make the runtime layer own build-system policy.

**Alternatives considered**:

- Put all code under Asset: rejected because publication and CLI workflow are
  offline tool responsibilities.
- Put all code under Tools: rejected because runtime manifest and payload
  consumption must not depend on Tools.
- Add a database-backed catalog layer: rejected by the local, manifest-only
  scope and roadmap.

## Source Selection And Discovery

### Decision: Scan declared roots, then select explicit Asset roots or cook-all

The tool builds a request-local source catalog by bounded, stable recursive
enumeration of declared source roots. Built-in source adapters recognize the
Feature 021-024 source suffixes and use public resolver/importer contracts to
produce typed outputs. The default mode requires one or more canonical
`FAssetId` roots after catalog construction and closes over required
dependencies. `--cook-all` selects every discovered typed output. The two modes
are mutually exclusive.

**Rationale**: Feature 020 has no persistent catalog or reverse AssetId-to-file
index, so discovery must remain request-local. Explicit roots model Unreal's
map/primary-content-oriented cook while cook-all provides the full-project mode
selected during clarification.

**Alternatives considered**:

- Infer one source path directly from every `FAssetId`: rejected because one
  glTF source produces several typed subresources and identities are not paths.
- Always cook every file: rejected by clarification and because unused source
  content should not enter default delivery.
- Persist discovery in SQLite: rejected because a persistent editor database is
  out of scope.

## Target Profile

### Decision: Canonical effective configuration is identity; display name is metadata

`FAssetTargetProfile` v1 contains schema identity, display name, target
platform, CPU architecture, graphics backend, ordered shader payload choices,
ordered texture capability/fallback choices, payload-affecting build policy,
limits, and optional extension declarations. `BuildPolicy` contains unique
sorted records of producer ID, settings schema version, and bounded scalar
settings. Every selected cooker requires and strictly validates its own record,
then projects only fields that can affect its output. Validation converts input
to a typed normalized model. Its effective digest excludes display name and
non-semantic presentation fields.

**Rationale**: Renaming a profile must not invalidate all data, while changing
an actual target capability must. A per-cooker projection prevents an
irrelevant field from destroying valid cache reuse.

**Alternatives considered**:

- Profile name as identity: rejected by clarification.
- Platform/backend pair only: rejected because shader profile, texture
  capability, and fallback can change bytes.
- Hash the complete profile for every asset: rejected because irrelevant fields
  would cause avoidable invalidation.

## Text Contracts

### Decision: Reuse the project's strict typed canonical JSON profile

Target profiles, manifests, current-generation pointers, DDC entry metadata,
cook reports, and validation reports use strict UTF-8 RFC 8259 JSON. Parsers
use the existing Asset-private yyjson integration with bounded input, depth,
value count, duplicate-key rejection, and project-owned typed models. Writers
emit fields in schema order, sort semantic sets by canonical token or
`FAssetId`, use two-space indentation, LF, and one final newline. Unknown
required extensions fail; unknown optional extensions are retained only when
their contract declares them semantic.

Tools and future runtime consumers access profile, envelope, and manifest
parse/write/validate operations through the Asset-public typed
`FAssetCookContractCodec` facade. The facade owns no policy and exposes no
yyjson/native types; concrete JSON and binary codecs remain Asset-private.

**Rationale**: Feature 023 already established and tested this profile. Reuse
avoids a second JSON canonicalization dialect and a new dependency. Human
readability matters for target profiles, manifests, and CI evidence.

**Alternatives considered**:

- RFC 8785/JCS as a new profile: rejected because the project already owns a
  typed canonical writer and does not need arbitrary-DOM canonicalization.
- CBOR/MessagePack for all metadata: rejected because manual inspection and
  checked-in contract fixtures are important.
- Ad hoc line-oriented text: rejected because nested dependencies and extension
  evidence need a typed schema.

## Cooked Payload Format

### Decision: Use one versioned binary envelope with type-specific deterministic bodies

Every published payload is a `.sgasset` file. The `SGCOOK01` v1 envelope uses
explicit little-endian integer encoding and contains container version,
canonical AssetId, asset type, payload codec token and revision, payload schema
revision, flags, body byte count, and body SHA-256. The whole envelope SHA-256
is recorded in the manifest. Bodies are written by registered type-specific
codecs using checked length-prefixes and fixed field order. Existing canonical
JSON is retained for material/shader definitions where it is already the
payload contract; KTX2 bytes remain KTX2; images, textures, meshes, and models
use bounded binary bodies.

**Rationale**: Large mesh/image arrays should not expand into JSON, while a
common envelope gives standalone substitution, schema, type, and digest
validation. Explicit little-endian encoding is stable across supported hosts
and future target platforms.

**Alternatives considered**:

- One canonical JSON file per asset: rejected for bulk geometry and image size.
- Copy in-memory struct bytes: rejected because layout, padding, pointers,
  endianness, and ABI are not stable contracts.
- FlatBuffers/Protobuf: rejected for this phase because explicit bounded codecs
  are small, no schema migration engine is yet needed, and a new dependency
  would add build and audit cost.
- Pak/archive aggregation: rejected by roadmap scope; generation files remain
  independently inspectable.

## Derived Key

### Decision: Hash a domain-separated canonical binary evidence stream

`FAssetDerivedKey` is SHA-256 over a tagged, length-delimited stream containing
key-format revision, typed AssetId, source version, normalized source manifest,
sorted dependency AssetId/version/role evidence, importer identity and
revision, selected cooker identity and revision, payload schema/codec revision,
the cooker-specific schema-versioned producer-settings projection from
target-profile `BuildPolicy` and the remaining effective target-profile
projection digest. Every tag and byte length is encoded explicitly. Feature 025
has no separate host-local or implicit processing-settings channel.

The display profile name, host path, output/DDC root, timestamps, process ID,
worker count, discovery order, and diagnostic text are excluded. The complete
normalized evidence remains inspectable beside the DDC entry.

**Rationale**: Derived data is valid only when every byte-affecting input is
identical. Domain separation and length delimiters prevent boundary ambiguity;
project SHA-256 evidence already exists in Feature 020.

**Alternatives considered**:

- Hash only source bytes: rejected because dependencies, tools, profile
  `BuildPolicy` producer settings, and target capabilities affect output.
- Use `FAssetId` as the cache key: rejected because identity is stable across
  versions.
- Include the whole target profile: rejected because irrelevant fields would
  force unrelated recooks.

## DDC Layout And Authority

### Decision: Local immutable directory entries with atomic put-if-absent

The initial local DDC layout is:

```text
<ddc-root>/
├── Entries/<first-two-key-chars>/<full-derived-key>/
│   ├── Entry.json
│   └── Payload.sgasset
├── Locks/<first-two-key-chars>/<full-derived-key>.lock
├── Quarantine/<full-derived-key>/<failure-evidence-digest>/
│   ├── Entry.json
│   ├── Payload.sgasset
│   └── Failure.json
└── Staging/<request-token>/...
```

An entry is built in DDC-local staging and validated. The writer then acquires
a short-lived per-key native file lease, re-queries the final key, and moves the
staged directory to its absent final location. The lease is held only for final
query/install or quarantine, never for expensive cooking. If another writer
wins, the loser validates the winner and discards its equivalent stage.
Existing valid entries are immutable. The quarantine suffix is the digest of
canonical failure evidence, so it remains defined even when payload bytes are
missing. Ordinary cook quarantines invalid entries and rebuilds; strict cache
validation fails after recording the invalid evidence. Cache entries never
become source authority and are not checked into source control.

**Rationale**: Unreal documents DDC content as disposable and regenerable from
source assets. Directory entries keep metadata inspectable and allow one
same-volume publish operation without adding a database.

**Alternatives considered**:

- Mutable files updated in place: rejected because readers could see partial
  metadata or payload.
- One monolithic cache file: rejected because concurrent writes, recovery, and
  targeted quarantine become much harder.
- Remote/shared cache: intentionally deferred; Feature 025 is local-only.

**Source**: [Unreal Engine Derived Data Cache](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-derived-data-cache-in-unreal-engine)

## Dependency Graph And Scheduling

### Decision: Deterministic topological plan with bounded parallel execution

Graph nodes are unique by typed `FAssetId`; edges carry normalized dependency
role and required version. Planning uses Kahn topological ordering with
`FAssetId` as the ready-set tie-breaker. A configurable worker pool executes
ready nodes in parallel, but commits each node result into a preassigned plan
index and emits manifests/reports in plan order. Default workers are
`min(8, available hardware threads)` with a valid range of 1-32. Results from
1 worker and N workers must be byte-identical.

**Rationale**: Asset processing is parallelizable, but thread completion order
must not become semantic. A bounded pool also gives a testable memory ceiling.

**Alternatives considered**:

- Single-thread only: simple but inadequate for target compression and shader
  payload growth.
- Unbounded task per asset: rejected because payload memory and thread count
  would be uncontrolled.
- Sort only after execution: insufficient if completion order affects graph
  mutation, report subjects, or publication paths.

## Input Snapshot

### Decision: Pin bytes per operation and verify all source versions before publication

Discovery/import resolves bounded source leases and records source plus
dependency digests in `FCookInputSnapshot`. A worker only consumes bytes owned
by that snapshot and re-resolves its consumed locator set before committing the
node result. After processing and staged-generation validation, the tool
re-resolves every recorded locator and recomputes its bounded digest. Any
missing or changed input aborts the cook or publication with `SourceChanged`;
no automatic retry and no source-tree lock are used.

**Rationale**: This implements clarification A without preventing developers
from editing Content. The final verification prevents a manifest from claiming
a source state different from the bytes actually processed.

**Alternatives considered**:

- Read files opportunistically: rejected because one generation could mix
  source revisions.
- Automatically retry: rejected by clarification and because repeated edits
  can make completion unbounded.
- Hold exclusive locks on source: rejected because authoring tools must remain
  usable during offline cook.

## Publication Lease

### Decision: Add a Core RAII file lease backed by process-owned native locks

`FPlatformFileLease` opens one stable lease file and attempts an exclusive
non-blocking lock. POSIX uses a descriptor-owned whole-file `flock` so another
descriptor may read owner metadata without releasing the lease;
Windows uses a `CreateFileW` handle whose desired access/share mode excludes
other writers while allowing owner metadata reads. Acquisition retries on a
monotonic schedule until the configured timeout. The held native descriptor or
handle is non-inheritable and releases on close or process termination. After
acquisition, the tool overwrites diagnostic owner metadata; metadata never
decides ownership, so a stale file is harmless.

Default publication-lease timeout is 30 seconds, configurable from 0 to 10
minutes. DDC entries use separate per-key leases for final installation and
quarantine; they never take the target publication lease.

**Rationale**: The user selected bounded waiting. Native descriptor-owned locks
recover from a crash without PID probing, wall-clock expiry, or deleting a lock
owned by another process. Core owns the platform branch so Tools remains
portable.

**Alternatives considered**:

- Lock existence file with `O_EXCL`/`CREATE_NEW`: rejected because a crash
  leaves ambiguous stale ownership requiring unsafe PID/time heuristics.
- In-process mutex: rejected because separate cooker processes can overlap.
- Distributed lease: rejected because network filesystems are out of scope.

**Sources**:

- [POSIX process-owned file lock definition](https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/V1_chap03.html)
- [Windows CreateFile sharing semantics](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew)

## Atomic Publication

### Decision: Publish immutable same-volume generations and atomically replace Current.json

The target output layout is:

```text
<output-root>/
├── .publish.lock
├── Current.json
├── Generations/<generation-id>/
│   ├── Manifest.json
│   └── Payloads/<first-two-digest-chars>/<envelope-digest>.sgasset
└── Staging/<request-token>/...
```

The generation ID is SHA-256 over canonical manifest semantics excluding
display-only profile name, generation ID itself, and physical payload locators.
Payload locator paths are then derived from envelope digest. Expensive cooking
first builds a generation image in request-local scratch outside the output
root. The tool acquires the publication lease before creating or cleaning
output-root staging, copies/finalizes the image there, validates every byte,
re-verifies the input snapshot, and moves the absent generation directory into
`Generations`. `Current.next` is durably written and atomically replaces
`Current.json`. POSIX uses same-filesystem `rename`; Windows uses Core's
`ReplaceFileW` transaction path with a `MoveFileExW` fallback. Failure before pointer
replacement leaves the old generation current.

Successful atomic replacement is the commit point and returns committed
success. A post-commit audit re-read may emit a stable warning but cannot turn
that committed operation into a failed cook. Simulated process interruption at
the replacement boundary may leave either the complete old or complete new
pointer; recovery validates whichever complete pointer is present.

Successful immutable generations are not automatically pruned in Feature 025.
This avoids deleting files while an external validator or future runtime reader
uses them. Feature 026 owns reader leases and live-generation evidence; a future
Tools/Packaging maintenance track owns retention policy, generation pruning,
local DDC garbage collection, and remote-cache evolution. Failed staging is
cleaned best-effort under the publication lease and is never addressable from
`Current.json`.

**Rationale**: One file replacement is the observable commit point; replacing
an entire non-empty directory portably is not. Keeping each generation
self-contained enables validation without the DDC or source tree.

**Alternatives considered**:

- Rewrite one fixed manifest and payload tree in place: rejected because readers
  can observe a mixed generation.
- Pointer symlink: rejected because Windows symlink permissions and replacement
  behavior differ.
- Automatically keep only current plus previous: deferred until reader leases
  exist; deleting a generation safely requires reader lifetime knowledge.

**Sources**:

- [POSIX rename atomicity](https://pubs.opengroup.org/onlinepubs/9799919799/functions/rename.html)
- [Windows ReplaceFile](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilew)
- [Windows MoveFileEx fallback](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexw)

## Manifest And Generation Identity

### Decision: Canonical manifest records sorted by typed identity

`Manifest.json` v1 records schema, effective profile digest/configuration,
selection mode, explicit roots or cook-all source scopes, generation ID,
limits profile, and records sorted by canonical `FAssetId`. Each asset record
contains type, source version, normalized source manifest, importer and cooker
revision, derived key, payload schema/codec, relative payload locator, envelope
size/digest, and dependencies sorted by role then AssetId. The display profile
name is retained as non-authoritative metadata.

Generation identity is computed in two phases: build and canonicalize the
semantic manifest body with no generation ID or physical locator, hash it, then
write the final manifest with that generation ID and deterministic
digest-derived locators. Validation repeats this procedure.

**Rationale**: Excluding physical locator prevents a self-reference cycle and
allows equivalent layouts to share generation identity. Including complete
version/dependency evidence makes standalone validation and later runtime
loading self-sufficient.

**Alternatives considered**:

- Hash final manifest bytes including its own ID: impossible without a
  fixed-point convention.
- Random generation UUID: rejected because repeated cooks would differ.
- Manifest order follows graph completion: rejected because concurrency would
  change bytes.

## Corruption And Quarantine

### Decision: Ordinary cook repairs; strict cache validation fails

Every DDC read validates directory/key agreement, canonical metadata, evidence,
payload envelope, sizes, body digest, full envelope digest, codec/schema, and
effective target compatibility. Ordinary cook atomically moves a bad entry to
`Quarantine`, writes stable failure evidence, reports `CorruptCacheRebuilt`, and
rebuilds. If quarantine move loses a race, the process re-queries the key.
`validate-cache --strict` performs no rebuild and exits with a cache-validation
failure after reporting every bounded error.

Published generation corruption is never repaired from the DDC by standalone
validation; it fails closed. A new cook may publish a new valid generation.

**Rationale**: DDC is disposable, but silent deletion hides storage faults.
Published content is delivery evidence and must not mutate during validation.

**Alternatives considered**:

- Fail every cook on cache corruption: rejected by clarification.
- Delete silently and rebuild: rejected by clarification.
- Repair published generation in place: rejected by immutable publication.

## Limits And Performance Budget

### Decision: Bounded graph, bytes, concurrency, and diagnostics with reference budgets

Default hard limits are 100,000 discovered sources, 100,000 graph assets,
1,000,000 dependency edges, dependency depth 256, 1 GiB per source or cooked
payload, 8 GiB request-owned aggregate source/payload bytes, 256 MiB manifest,
1,024 UTF-8 bytes per logical locator/path component contract, 32 JSON depth,
1,000,000 JSON values for a manifest, 4,096 diagnostics, and 1-32 workers.
Profiles may lower but not exceed compiled safety maxima without a schema
revision.

On the Apple M4 Pro Release reference host, the 1,000-asset/5,000-edge synthetic
corpus must plan within 2 seconds, complete a fully cached incremental cook
within 10 seconds, validate a published generation within 10 seconds, and
clean-cook within 60 seconds. The separate complete representative Feature
021-024 clean corpus must also finish within 60 seconds. Peak process RSS must
remain below 1 GiB for the synthetic corpus and
below the configured 8 GiB aggregate payload ceiling for the representative
clean cook. The reference benchmark exits unsuccessfully when any threshold is
exceeded. CI enforces correctness and separate hard 4x time ceilings for smoke
regression rather than pretending hosted runners meet M4 performance.

**Rationale**: These budgets resolve SC-010 while separating coordinator scale
from codec-heavy source content. Bounded workers and payload ownership keep
memory proportional to active work rather than total graph size.

**Alternatives considered**:

- Unbounded inputs/workers: rejected as unsafe for file-driven tools.
- One strict time budget on hosted CI: rejected because shared runner variance
  would create flaky gates.
- Exclude payload memory from all reporting: rejected because large cookers are
  the dominant memory risk.

## Build And Validation Matrix

### Decision: Build the tool everywhere and run deterministic filesystem integration tests

SCons adds `Tools/AssetCooker/SConscript` after Core and Asset. The tool builds
on Windows, macOS, and Linux and links only Asset and Core. `StonerTest` gains
focused suites for profile/manifest/payload codecs, derived keys, graph
planning, DDC, source snapshots, lease/publication, determinism, concurrency,
and performance. Python verifiers validate schemas, fixture provenance, output
layout, and architecture boundaries without becoming the implementation.

CI runs Debug and strict Release on all three platforms, Linux ASan/UBSan,
Linux TSan for scheduler/cache paths, twenty-repeat determinism, two-process
lease/publication probes, clean-machine cook, unchanged incremental cook,
mutation matrix, at least 15 DDC corruption cases, and a separate set of at
least 30 published-generation corruptions for standalone validation. Normalized
reports are uploaded from `Validation/025`.

**Rationale**: Filesystem replacement, locks, Unicode paths, case behavior, and
CLI exit propagation are platform-sensitive even though the cooker is CPU-only.

**Alternatives considered**:

- Linux-only cooker tests: rejected by constitution and Windows/macOS filesystem
  differences.
- Unit tests only: rejected because cross-process and crash-boundary behavior
  requires executable integration probes.
- Require graphics hardware: rejected because Feature 025 produces CPU-side
  assets and does not own GPU validation.
