# Contract: Cooked Generation Publication v1

## Layout

```text
<output-root>/
├── .publish.lock
├── Current.json
├── Generations/
│   └── <64-lowercase-generation-id>/
│       ├── Manifest.json
│       └── Payloads/
│           └── <digest[0:2]>/
│               └── <64-lowercase-envelope-digest>.sgasset
└── Staging/
    └── <non-semantic-request-token>/...
```

The output root, staging root, and installed generation must remain on one
filesystem/volume. Cross-volume move fallback by copy-and-delete is forbidden
for the publication commit path.

## Lease

- One stable `.publish.lock` per output root.
- Native lock/handle ownership is authoritative; file contents are diagnostic.
- Default wait timeout: 30,000 ms; valid range 0-600,000 ms.
- Acquisition polling uses monotonic time and bounded sleep.
- Native ownership is non-inheritable and released by close or process exit.
- A stale metadata file does not block acquisition and is overwritten after the
  native lock succeeds.
- The lease protects staging cleanup, generation installation, and current
  pointer replacement for this root. It does not lock source or DDC reads.

## Generation Construction

1. Build all payload envelopes and manifest semantics in request-local scratch
   outside the output root; this step MUST NOT create or clean output staging.
2. Acquire the publication lease before mutating output staging/current state.
3. Create a unique non-semantic staging directory under the output root.
4. Copy each distinct envelope digest exactly once to its digest-derived locator.
5. Canonically write `Manifest.json` with sorted records and final locators.
6. Standalone-validate the staged generation using only manifest/profile/payload
   evidence.
7. Re-resolve and verify every snapshotted source/dependency version.
8. Move the complete staging directory to
   `Generations/<generation-id>` with absent-destination semantics.
9. If that generation already exists, standalone-validate it and accept only an
   equivalent valid generation.

## Commit Point

1. Build canonical `Current.next` in the output root with generation ID,
   manifest locator, and manifest digest.
2. Durably write and re-read `Current.next`.
3. Atomically replace `Current.json` with `Current.next`.
4. Re-read `Current.json` and validate it resolves to the installed generation.
5. Release the publication lease.

The observable commit point is step 3. Before it, readers see the previous
complete pointer. After replacement reports success, readers see the new
complete pointer and the operation is committed success. Step 4 is an audit and
cannot reclassify a committed operation as failure. No reader is required to
scan `Generations` or `Staging` to discover current content.

## Failure Rules

| Failure stage | Required state |
|---|---|
| Before lease | Output root unchanged |
| Lease timeout | Output root unchanged; stable timeout result |
| Staging write/validation | Current pointer and all installed generations unchanged |
| Input re-verification | No generation installed or made current |
| Generation install | Current pointer unchanged; partial install not addressable |
| Current.next write | Current pointer unchanged |
| Atomic replace reports failure | Previous pointer remains current; operation reports `PublicationFailure` |
| Process interruption at atomic replace | Recovery observes either complete old or complete new pointer, never partial JSON, then validates the observed pointer |
| Post-commit re-read/audit failure | Replacement remains committed and operation reports `Success` with a stable audit diagnostic; no in-place repair |

Failed staging is cleaned best-effort while the lease is held. Cleanup failure
is diagnostic and cannot make staging current.

## Generation Identity

The generation ID hashes canonical semantic manifest content excluding:

- the generation ID field itself;
- display-only profile name;
- physical payload locators (payload envelope digest is included);
- output/DDC/source host paths;
- timestamps, process/thread/request IDs, worker count, timings, RSS, and logs.

Validation reconstructs the same semantic projection and compares the digest.

## Retention

Feature 025 never automatically deletes a successfully installed generation.
It has no runtime reader lease and therefore cannot prove an old generation is
unused. Reader-aware retention/GC is deferred. Manual filesystem deletion is
not part of the CLI contract and must never remove the generation referenced by
`Current.json`.
