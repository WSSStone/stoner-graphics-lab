# Data Model: Asset Core, Identity & Registry

## Asset Identity

**Public name**: `FAssetId`

**Purpose**: Stable logical identity independent from physical storage, content
revision, target profile, and registration order.

**Fields**:

- `AssetType`: non-empty case-sensitive ASCII symbolic type name.
- `LogicalPath`: canonical relative NFC path.
- `Subresource`: optional canonical NFC name.

**Validation rules**:

- Type, path, and present subresource are non-empty.
- Type matches `[A-Za-z][A-Za-z0-9_.-]*`, is at most 255 bytes, and is neither
  Unicode-normalized nor case-folded.
- Path is valid UTF-8 normalized to NFC, case-sensitive, `/` separated, and at
  most 1,024 bytes in full canonical form.
- `\` normalizes to `/`; repeated separators collapse; `.` segments disappear.
- Leading/trailing separators, `..`, roots, drive prefixes, control/NUL, `:`,
  `#`, invalid UTF-8, overlong components, and empty final paths are rejected.
- Every path segment and subresource is at most 255 UTF-8 bytes.
- Equality compares all canonical components. Lookup hash collisions do not
  imply identity equality.

**Canonical order**:

1. Asset type text.
2. Logical path bytes after NFC.
3. Subresource absence before presence, then subresource bytes.

**Diagnostic form**: `Type:Logical/Path` or
`Type:Logical/Path#Subresource`.

**State transitions**:

- Raw input -> Valid canonical identity.
- Raw input -> Rejected with identity diagnostic.
- Canonical identity is immutable.

## Typed Soft Reference

**Public name**: `TSoftAssetRef<T>`

**Purpose**: Preserve an unloaded identity plus compile-time expected payload
type.

**Fields**:

- `Id`: optional `FAssetId`.
- Expected type trait supplied by `T`.

**Validation rules**:

- Empty/default reference is valid but unresolved.
- A non-empty reference is usable only when `Id.AssetType` equals the stable
  type declared for `T`.
- Constructing a mismatched reference returns or exposes `TypeMismatch`; it
  never casts a payload.

## Asset Digest

**Public name**: `FAssetDigest`

**Purpose**: Cross-run evidence that source, imported, or cooked bytes changed.

**Fields**:

- `Availability`: unavailable or available.
- `Algorithm`: initially `Sha256`.
- `Bytes`: exactly 32 bytes for SHA-256.

**Validation rules**:

- Unavailable digest has no algorithm/value semantics.
- Available SHA-256 digest has exactly 32 bytes.
- Equality compares availability, algorithm, length, and all bytes.
- Lowercase hexadecimal inspection is 64 digits for SHA-256.
- Two available digests with identical algorithm and bytes are equal revision
  evidence. Original payload bytes are not retained for cryptographic-collision
  disambiguation.

## Asset Version

**Public name**: `FAssetVersion`

**Purpose**: Keep independent revision evidence out of stable identity.

**Fields**:

- `SourceDigest`: optional digest of original source.
- `ContentDigest`: optional digest of imported CPU content.
- `CookDigest`: optional digest of target-specific cooked content.
- `ProducerVersion`: stable importer/cooker participant and schema version.
- `TargetProfile`: optional target name for cooked evidence.

**Validation rules**:

- Any digest may be unavailable before that processing stage exists.
- Cook digest requires producer version and target profile.
- Changing any version field never changes `FAssetId`.
- Equality compares every field and every digest byte; test-only lookup hashers
  may be forced to collide without changing value equality.

## Asset Dependency

**Public name**: `FAssetDependency`

**Purpose**: Directed metadata edge from a referring asset to a target.

**Fields**:

- `TargetId`: referenced asset.
- `Role`: source/import, build/cook, or runtime-use prerequisite.
- `Strength`: required or soft.
- `Resolution`: resolved or unresolved.

**Validation rules**:

- Target identity is valid.
- Required self-edges and known required cycles are rejected.
- Required absent targets are retained as unresolved.
- Soft references are not dependency edges unless metadata explicitly declares
  them.
- Resolution is registry-derived, not caller-authored authority.

**State transitions**:

- Absent target -> `Unresolved`.
- Target registered -> `Resolved`, unless proposed graph would form a cycle.
- Target removed -> `Unresolved`.

## Asset Metadata

**Public name**: `FAssetMetadata`

**Purpose**: Registry-owned description without embedding runtime payload data.

**Fields**:

- `Id`: asset identity.
- `Version`: current version evidence.
- `Source`: originating logical source locator.
- `Producer`: stable participant identity and producer version.
- `Attributes`: string metadata keyed by stable names.
- `Dependencies`: deterministic dependency declarations.

**Validation rules**:

- Identity, canonical source, participant identity, and producer version are
  valid.
- Dependency targets within one record are unique by target, role, and
  strength.
- Attributes have non-empty unique keys; inspection sorts by key text.
- Canonical metadata equality compares identity, version, canonical source,
  participant identity/version, attributes as a key/value set, and dependency
  declarations sorted by target, role, and strength. It ignores memory layout,
  insertion order, and registry-derived resolution state.
- Conflicting existing metadata requires explicit replacement.

## Asset Source Descriptor

**Public names**: `FAssetSourceLocator`, `FAssetSourceDescriptor`,
`IAssetSource`

**Purpose**: Separate stable asset identity from physical or virtual source
storage.

**Fields**:

- Canonical lowercase ASCII `Scheme` or logical domain.
- NFC, case-sensitive resolver-private `Locator`.
- Optional size and format hint.
- Read-only source lease supporting bounded range/prefix reads.

**Validation rules**:

- Source descriptor does not alter `FAssetId`.
- Scheme matches `[a-z][a-z0-9+.-]*` and is at most 63 bytes; input scheme text
  is ASCII-lowercased before validation.
- Locator is valid NFC UTF-8, is 1–1,024 bytes, preserves case, and rejects
  NUL/control characters.
- Equality, hashing, and ordering compare canonical scheme first and locator
  UTF-8 bytes second; registry source indexes never use native path semantics.
- Platform-native not-found, inaccessible, malformed-source, and retryable
  failures map to `NotFound`, `AccessDenied`, `MalformedSource`, and
  `TransientFailure` respectively.
- Probe reads never exceed the smaller of importer-declared limit and 64 KiB.

## Asset Participant and Producer Version

**Public names**: `FAssetParticipantId`, `FAssetProducerVersion`

**Purpose**: Reusable stable keys for extension uniqueness, deterministic
diagnostics, and producer schema/version evidence.

**Validation rules**:

- Participant identity matches `[A-Za-z][A-Za-z0-9_.-]*`, is case-sensitive,
  and is at most 127 bytes.
- Producer version matches `[A-Za-z0-9][A-Za-z0-9_.+-]*`, is case-sensitive,
  and is at most 64 bytes.
- Neither token is Unicode-normalized or case-folded.
- Equality and ordering compare complete ASCII bytes; participant ordering is
  used only for stable diagnostics and never breaks a functional dispatch tie.

## Registry State

**Public name**: `FAssetRegistry`

**Purpose**: Process-local authoritative metadata and dependency index.

**Indexes**:

- `RecordsById`: exact metadata lookup.
- `IdsByType`: type lookup.
- `IdsBySource`: source lookup.
- `ForwardDependencies`: referring ID to dependency edges.
- `ReverseDependents`: target ID to referring IDs and edge state.
- `Revision`: monotonic successful mutation counter for inspection.

**Concurrency rules**:

- Multiple queries may run concurrently under shared access.
- Mutations are serialized internally.
- Queries return copied snapshots, not borrowed references or iterators.
- One batch publishes records and every index atomically.
- Reader output is either complete pre-batch or complete post-batch state.

**Mutation states**:

1. `Building`: caller adds register, replace, and remove operations.
2. `Normalizing`: all identities and metadata are canonicalized.
3. `Validating`: duplicates, conflicts, dependencies, and proposed cycles are
   checked against current state plus the batch.
4. `Rejected`: no registry/index changes; diagnostics retained by result.
5. `Committed`: all state and indexes update once; revision increments.

**Completeness state**:

- `Complete`: no required dependency is unresolved.
- `Incomplete`: at least one required dependency is unresolved; validation
  reports every referring/target pair.

## Resolver Registration

**Public interface**: `IAssetResolver`

**Capability fields**:

- Participant identity.
- Priority.
- Supported domains or schemes.

**Selection rules**:

- Ineligible domain is excluded.
- Unique highest priority wins.
- Equal highest-priority candidates yield `AmbiguousResolver`.
- Participant identity sorts diagnostics only.

## Importer Registration

**Public interface**: `IAssetImporter`

**Capability fields**:

- Participant identity and producer version.
- Supported format hints.
- Requested probe byte limit, capped at 64 KiB.
- Dispatch candidate count after format-hint filtering, capped at 64.

**Probe result**:

- Confidence `0..100`.
- Stable reason/category.

**Discovery output**:

- One source descriptor.
- One or more `FAssetImportOutput` values containing ID, metadata, dependencies,
  and optional synthetic or later concrete CPU payload.

**Validation rules**:

- Output identities are unique.
- Discovery completes and validates before registry mutation.
- More than 64 eligible candidates yields `CapacityExceeded` before source
  reading or probe invocation.
- One unique highest nonzero confidence wins.
- Equal leaders yield `AmbiguousImporter`.

## CPU Asset Payload

**Public name**: `FAssetPayload`

**Purpose**: Asset-owned polymorphic base for typed CPU content.

**Fields/operations**:

- Virtual destructor.
- Stable asset type query.

**Validation rules**:

- Payload type matches its `FAssetId` and typed soft reference.
- No RHI resource, backend object, graphics handle, or GPU lifetime appears.
- Feature 020 uses synthetic payloads only.

## Loader and Cooker Requests

**Public interfaces**: `IAssetLoader`, `IAssetCooker`

**Loader request fields**:

- Identity, version, metadata, dependencies, source descriptor, and settings.

**Loader result fields**:

- Result category, diagnostics, and optional immutable CPU payload.

**Cook request fields**:

- Identity, version, metadata, dependencies, immutable CPU payload, settings,
  and target profile.

**Cook result fields**:

- Result category, diagnostics, cooked byte artifact, target profile, and cook
  digest.

**Validation rules**:

- Unsupported, invalid input, dependency failure, and processing failure are
  distinct.
- Feature 020 exercises these contracts with synthetic participants only.

## Extension Registration and Execution Lease

**Public names**: `FAssetExtensionRegistry`,
`FAssetExtensionRegistration`, `FAssetExecutionLease<T>`

**Registration fields**:

- Participant identity.
- Extension kind and capabilities.
- Shared extension instance.
- Active flag.

**Registration states**:

- `Active`: eligible for new candidate snapshots.
- `Unregistered`: no longer eligible; existing leases may remain.
- `Retired`: unregistered and no leases remain; instance may be destroyed.

**Execution lease states**:

- `Acquired`: retains a selected active extension.
- `Released`: no longer retains it.

**Validation rules**:

- Participant identity is unique per extension kind.
- Dispatch acquires a lease before callback invocation.
- Token release atomically prevents new leases.
- Existing leases complete without cancellation.
- Instance destruction occurs exactly once after registration ownership and
  all execution leases release.

## Asset Diagnostic

**Public names**: `EAssetResult`, `FAssetDiagnostic`,
`FAssetDiagnostics`

**Fields**:

- Stable result category.
- Operation stage.
- Severity.
- Optional subject asset.
- Optional participant identity.
- Stable diagnostic code.
- Human-readable reason.

**Validation rules**:

- First actionable error names stage and subject/participant when available.
- Normalized output excludes native addresses, thread IDs, unstable timings,
  registration order, and platform-native error strings.
- Multiple diagnostics sort by stage, subject identity, participant, then code.

## Asset Inspection

**Public name**: `FAssetInspection`

**Purpose**: Provide one public, deterministic formatting boundary for Asset
values and subsystem snapshots without exposing private registry storage.

**Operations**:

- Format an identity, digest, version, metadata record, or dependency edge.
- Format an owned registry snapshot in canonical asset identity order.
- Format active extension capabilities and ambiguity candidate lists in stable
  participant identity order.

**Validation rules**:

- Operations return owned `FString` values.
- Inspection accepts public values or owned snapshots only; it never returns a
  registry iterator, borrowed mutable reference, or native address.
- Equivalent values and operation histories produce byte-identical text across
  supported platforms.
