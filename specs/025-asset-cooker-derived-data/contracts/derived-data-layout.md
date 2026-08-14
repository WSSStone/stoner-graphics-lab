# Contract: Local Derived Data Store v1

## Authority

The DDC is disposable, local, and reconstructible. A DDC entry can accelerate
cooking but cannot change source Asset identity, satisfy a missing source
dependency, or override newer complete key evidence. It is never required to
validate an already published generation.

## Layout

```text
<ddc-root>/
├── Entries/
│   └── <key[0:2]>/
│       └── <64-lowercase-hex-key>/
│           ├── Entry.json
│           └── Payload.sgasset
├── Locks/
│   └── <key[0:2]>/
│       └── <64-lowercase-hex-key>.lock
├── Quarantine/
│   └── <64-lowercase-hex-key>/
│       └── <64-lowercase-failure-evidence-digest>/
│           ├── Entry.json          # when readable
│           ├── Payload.sgasset     # when present
│           └── Failure.json
└── Staging/
    └── <non-semantic-request-token>/...
```

Only `Entries` is eligible for cache hits. `Locks`, `Quarantine`, and `Staging`
are never searched as data sources. Request tokens and quarantine physical
paths are excluded from normalized evidence. A lock file's contents are
diagnostic; its native lock/handle is ownership authority.

## Entry Metadata

`Entry.json` uses schema `stoner.asset-derived-entry`, version 1, and contains:

- full derived key and complete canonical key evidence;
- canonical AssetId;
- importer, cooker, codec, and schema revisions;
- relevant effective-profile digest;
- fixed payload locator `Payload.sgasset`;
- exact payload byte count and envelope SHA-256;
- required extension declarations.

The recomputed key must equal both metadata and directory name. Metadata and
payload must agree on AssetId, type, codec, schema, size, and digest.

## Query

1. Derive the final directory from the complete lowercase key.
2. Missing directory is `Miss`.
3. Bound-read and strictly parse `Entry.json`.
4. Recompute the key from evidence and compare path/metadata/request key.
5. Bound-read and validate `Payload.sgasset` completely.
6. Verify target projection compatibility with the request.
7. Return immutable `Hit` only after all checks pass.

## Store

1. Build metadata and payload under DDC-local `Staging`.
2. Durably write both, re-open, and validate as a query would.
3. Acquire the short-lived per-key native lease, then re-query the final key.
4. If final is still absent, move the complete staged directory to the final
   key directory while the lease is held.
5. If final already exists, query and use it when valid/equivalent.
6. If a conflicting valid entry has the same key but different bytes/evidence,
   return `CacheFailure`; this indicates a key-contract or digest invariant
   violation and must not be overwritten.

No valid entry is modified in place. No cache-wide or target publication lease
is used; only writers/quarantiners for the same full derived key serialize.

## Corruption And Quarantine

Ordinary cook:

1. Classify and capture bounded stable failure evidence.
2. Compute a canonical failure-evidence digest, which remains available when
   metadata or payload bytes are absent.
3. Acquire the per-key lease, revalidate, and move the still-invalid final entry
   out of `Entries` to the evidence-digest quarantine subject.
4. If another process wins the repair, re-query the final key.
5. Report `CorruptCacheRebuilt`, treat as Miss, rebuild, and store normally.

Strict cache validation:

- validates without rebuild;
- returns `CacheFailure` when any requested entry is invalid;
- may quarantine only when it can preserve evidence atomically;
- never silently deletes an invalid entry.

Missing entries are ordinary misses for cook and explicit failures when a
specific strict-validation key was requested.

## Cleanup

Feature 025 performs best-effort cleanup only for its own failed staging. It
does not implement cache capacity eviction, age-based GC, remote replication,
or automatic quarantine deletion. Those require separate policy and telemetry.
