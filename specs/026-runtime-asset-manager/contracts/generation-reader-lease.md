# Contract: Generation Reader Lease v1

## Path And Authority

```text
<publication-root>/
├── Current.json
└── Generations/<generation-id>/...

<writable-coordination-root>/
└── <publication-identity-digest>/
    └── <generation-id>.lease
```

The lowercase 64-hex digest of canonical publication identity selects a
collision-resistant namespace; the generation ID determines the lease filename.
Both roots are explicit cooked-manager configuration. The coordination root
must already exist and be writable; manager startup does not create or modify
the potentially read-only publication root. Native lock ownership is
authoritative; file contents, PID, timestamp, and owner metadata are never
liveness authority.

Publication identity digest v1 is SHA-256 over the fixed domain tag
`stoner.asset-publication-namespace.v1` followed by the Core-canonical absolute
UTF-8 publication path. Canonicalization resolves symlinks, normalizes separators
and case-folds on Windows, and preserves case on POSIX. The manager may create
only the digest-named namespace and lease files beneath the already existing
coordination root. Diagnostics expose the digest rather than the source path.

## Core Primitive

`FPlatformFileLease` gains `EPlatformFileLeaseMode { Shared, Exclusive }`.
The existing Acquire signature delegates to Exclusive for source compatibility.

- Shared acquisitions coexist.
- Exclusive acquisition coexists with neither Shared nor Exclusive.
- Timeout is bounded and uses a monotonic clock.
- Lease is move-only; Release is idempotent.
- Normal destruction and process termination release native ownership.
- POSIX uses one common flock byte/file description policy.
- Windows uses one common byte range with shared/exclusive `LockFileEx`
  semantics and share-enabled file opens.
- In-process arbitration mirrors native shared/exclusive semantics.

Shared acquisition does not truncate or rewrite metadata. Exclusive legacy 025
leases retain their current metadata behavior.

## Runtime Binding Protocol

1. Read and strictly parse `Current.json`.
2. Derive the coordination namespace from canonical publication identity and
   the generation lease path from its generation ID.
3. Acquire Shared with configured timeout.
4. Validate the exact generation directory and manifest under ownership.
5. Store lease in `FBoundCookedGeneration`.
6. Release only after no manager operation can read the generation.

If the pointer changes between steps 1 and 4, binding the originally selected
generation remains valid if its exact directory validates under lease. Missing
or invalid generation, absent/read-only coordination root, namespace collision,
or lease failure fails startup without modifying publication content.

## Future Maintenance Protocol

Before pruning one generation, maintenance must acquire Exclusive on the same
configured coordination path, then re-check that it is not current and validate
its deletion target.
Feature 026 exposes this coordination contract but implements no pruning.

## Required Tests

- multiple same/different-process readers coexist;
- exclusive acquisition times out while any reader lives;
- exclusive acquisition succeeds after all readers release;
- forced reader process exit releases ownership;
- different generation IDs never contend;
- current publication remains independent of old generation readers;
- a read-only publication root works with a writable coordination root;
- missing/read-only/aliased coordination roots fail before request admission;
- old exclusive-only 025 call sites retain behavior on all three platforms.
