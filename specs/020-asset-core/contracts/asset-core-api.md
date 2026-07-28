# Contract: Asset Core Public API

**Feature**: 020-asset-core
**Date**: 2026-07-28

## Public Boundary

Public headers live under `Source/Asset/Public/Asset/` and are included as
`Asset/<Header>.h`. They may include Core public headers and C++ standard
library headers. They must not expose `utf8proc`, RHI, Renderer, Application,
Backend, Tools, editor, platform graphics, or concrete asset-format types.

The public namespace is:

```cpp
namespace Stoner::Asset
{
}
```

## Header Contract

| Header | Public deliverables |
|---|---|
| `AssetMinimal.h` | Stable aggregate for Asset public vocabulary |
| `EAssetResult.h` | Stable operation result categories |
| `FAssetId.h` | Canonical typed identity and ordering |
| `FAssetDigest.h` | Algorithm-tagged digest representation |
| `FAssetVersion.h` | Source/content/cook version evidence |
| `FAssetDependency.h` | Required/soft dependency and resolution state |
| `FAssetMetadata.h` | Provenance, attributes, dependencies |
| `FAssetParticipant.h` | Stable participant identity and producer version tokens |
| `TSoftAssetRef.h` | Typed unloaded identity reference |
| `FAssetDiagnostics.h` | Stable diagnostics and normalized formatting |
| `FAssetInspection.h` | Public deterministic value and snapshot inspection |
| `FAssetRegistry.h` | Atomic metadata mutation and deterministic query |
| `FAssetSource.h` | Storage-independent locator, descriptor, and bounded reader |
| `FAssetPayload.h` | Typed CPU payload base |
| `IAssetResolver.h` | Source-resolution strategy contract |
| `IAssetImporter.h` | Probe and multi-output import contract |
| `IAssetLoader.h` | CPU payload loading contract |
| `IAssetCooker.h` | Target artifact cooking contract |
| `FAssetExtensionRegistry.h` | Scoped registrations, dispatch, and execution leases |

## Result Categories

`EAssetResult` includes at least:

- `Success`
- `InvalidIdentity`
- `InvalidUtf8`
- `IdentityTooLong`
- `TypeMismatch`
- `NotFound`
- `AccessDenied`
- `MalformedSource`
- `TransientFailure`
- `AlreadyExists`
- `Conflict`
- `UnresolvedDependency`
- `DependencyCycle`
- `IncompleteRegistry`
- `NoMatchingResolver`
- `AmbiguousResolver`
- `NoMatchingImporter`
- `AmbiguousImporter`
- `Unsupported`
- `InvalidInput`
- `DependencyFailure`
- `ProcessingFailure`
- `RegistrationInactive`
- `CapacityExceeded`

An operation returns one primary category. Diagnostics add stage and subject
detail without changing category meaning across platforms.

## Identity Contract

### Create

**Given** a type, raw logical path, and optional subresource
**When** identity creation succeeds
**Then** the result stores valid NFC canonical components and can round-trip
through stable diagnostic text.

Creation is fallible. Invalid input never produces a partially valid ID.

### Canonicalization

- Asset type matches `[A-Za-z][A-Za-z0-9_.-]*`, is case-sensitive, is at most
  255 bytes, and is not Unicode-normalized or case-folded.
- NFC applies to logical path and subresource.
- Comparison is case-sensitive.
- `\` becomes `/`; repeated separators collapse; `.` segments are removed.
- Roots, drive/UNC prefixes, `..`, control/NUL, reserved `:`/`#`, invalid UTF-8,
  empty required components, and configured length excess fail.
- Canonicalization is idempotent.

### Equality and Order

- Equality compares type, path, and optional subresource in full.
- Lookup hashes are optimization only and may collide.
- Public ascending order is type, path, subresource absence/presence, then
  subresource text.

## Typed Soft Reference Contract

`TSoftAssetRef<T>` may be empty or hold one `FAssetId`.

- `T` declares a stable expected asset type through an Asset type trait.
- Mismatched construction or resolution reports `TypeMismatch`.
- Holding a reference does not require registry membership or payload loading.
- This feature does not expose a blocking or asynchronous load operation on the
  soft reference.

## Digest and Version Contract

- Initial available digest algorithm is SHA-256 with 32 bytes.
- Digests parse and format lowercase 64-digit hexadecimal.
- Source, content, and cook digests remain independent.
- Unavailable digest is explicit and does not equal an available digest.
- Version changes do not alter identity.
- SHA-256 implementation passes published NIST vectors.
- Full version equality compares every field and every digest byte. Tests force
  collisions only in internal lookup hashers; equal algorithm and digest bytes
  are equal revision evidence.

## Metadata and Dependency Contract

- Metadata contains identity, version, source locator, producer, attributes, and
  dependencies only.
- Source schemes canonicalize to lowercase `[a-z][a-z0-9+.-]*` with a 63-byte
  limit. Locator text is NFC, case-sensitive UTF-8, 1–1,024 bytes, and excludes
  NUL/control characters.
- Participant identity matches `[A-Za-z][A-Za-z0-9_.-]*` up to 127 bytes;
  producer version matches `[A-Za-z0-9][A-Za-z0-9_.+-]*` up to 64 bytes.
- Runtime payload is not embedded in metadata.
- Required absent targets are accepted as `Unresolved`.
- Known required cycles reject the proposing batch.
- Soft references do not become dependency edges implicitly.
- Dependency resolution state is registry-derived.

## Registry Mutation Contract

### Register Batch

**Given** new valid metadata
**When** registration commits
**Then** records and all indexes become visible atomically.

- Canonically value-equivalent re-registration is idempotent. Equality compares
  every semantic field, treats attributes as a key/value set, sorts dependency
  declarations by target/role/strength, and ignores memory layout, insertion
  order, and registry-derived resolution state.
- Conflicting existing records require explicit replacement.
- Duplicate output IDs, malformed metadata, or known required cycles reject the
  whole batch.
- Missing required targets remain unresolved and do not reject the batch.

### Replace Batch

**Given** existing IDs selected for explicit replacement
**When** replacement commits
**Then** source, type, forward, and reverse indexes reflect only the new
metadata, and affected resolution states update atomically.

### Remove Batch

**Given** existing target records
**When** removal commits
**Then** records disappear, reverse dependents remain, and affected required
edges become unresolved atomically.

### Failure Guarantee

Any validation or staging failure before commit preserves all prior records,
indexes, dependency states, and registry revision.

## Registry Query Contract

Queries support:

- exact identity;
- all IDs for one asset type;
- all IDs originating from one source;
- direct dependencies;
- reverse dependents;
- completeness validation;
- normalized full-registry inspection.

Queries return owned snapshots. Multi-result output uses canonical identity as
the final ordering key. Callers never retain registry iterators or references.

Concurrent readers are supported. Writers are internally serialized. A reader
sees complete pre-batch or post-batch state only.

## Resolver Contract

`IAssetResolver` declares participant identity, integer priority, and supported
domains/schemes.

Resolution:

1. Captures eligible active registrations.
2. Finds highest priority.
3. Acquires an execution lease only for a unique leader.
4. Returns `AmbiguousResolver` for equal leaders.
5. Invokes the resolver through its lease.

Resolver output is a source descriptor/reader and does not modify `FAssetId`.
Platform storage failures map as follows:

| Resolver outcome | `EAssetResult` |
|---|---|
| Source absent | `NotFound` |
| Permission or access denied | `AccessDenied` |
| Malformed source/descriptor data | `MalformedSource` |
| Retryable or temporarily unavailable storage | `TransientFailure` |

## Importer Contract

`IAssetImporter` declares participant identity, producer version, format hints,
and probe limit.

### Probe

- Receives a read-only prefix no larger than 64 KiB.
- Returns confidence `0..100` plus stable diagnostic detail.
- Must be side-effect free.

### Select

- A supplied format hint restricts candidates to importers declaring that hint.
- Without a hint, all eligible importers may probe.
- If hint filtering leaves more than 64 eligible importers, selection returns
  `CapacityExceeded` before reading the source or invoking any probe.
- Unique highest nonzero confidence wins.
- Equal highest confidence returns `AmbiguousImporter`.
- Registration order never breaks a tie.

### Discover

The selected importer may return multiple typed output records. All IDs,
metadata, dependencies, and payload types validate before any registry batch is
submitted. Discovery failure makes no registry change.

## Loader and Cooker Contract

`IAssetLoader` returns an immutable Asset-owned CPU payload or one explicit
failure category. `IAssetCooker` returns a target-tagged byte artifact and cook
digest or one explicit failure category.

Feature 020 requires synthetic contract tests only. Concrete codecs, source
formats, target payload schemas, Tools, manifests, and cache publication are
outside scope.

## Registration and Lease Contract

- Registration tokens are move-only scoped owners.
- Duplicate participant IDs within one extension kind are rejected.
- Dispatch snapshots only active registrations.
- Acquiring a lease retains shared extension ownership before callback.
- Releasing a token immediately prevents new selection.
- Existing leases are not cancelled and may finish.
- The extension instance remains alive until the last lease releases.
- No callback may execute through a stale raw pointer.

## Diagnostics and Inspection Contract

`FAssetInspection` is the public formatting entry point. It returns owned
`FString` output for identities, digests, versions, metadata, dependency edges,
owned registry snapshots, registered extension capabilities, and ambiguity
candidate lists. It accepts no private registry iterator or mutable borrowed
state.

Normalized diagnostics identify:

- stage;
- result category;
- stable code;
- asset subject when available;
- participant when available;
- actionable reason.

Output excludes addresses, thread IDs, registration order, native error text,
and unstable timing. Equivalent operation histories produce byte-identical
normalized output.

## Exclusions

- Concrete PNG, JPEG, HDR, KTX2, glTF, material, shader, or mesh payloads.
- Persistent registry, manifests, derived-data cache, or database.
- Asynchronous requests, cancellation, coalescing, hot reload, or residency.
- Tools runtime dependency.
- RHI resources, backend objects, GPU handles, or graphics execution.
