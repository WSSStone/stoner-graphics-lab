# Feature 028 US1 Production Corpus Gate

Captured on 2026-08-22 from branch
`028-production-content-acceptance` after completing T020-T034.

## Admitted Corpus

| Package | Tier | Files | Bytes | Imported structure |
|---|---:|---:|---:|---|
| Khronos Lantern GLB | regular | 2 | 9,564,715 | 1 model, 3 meshes/primitives, 2 materials, 4 images/textures, 4 nodes |
| Khronos Sponza glTF | medium | 71 | 52,686,624 | 1 model/mesh, 103 primitives, 26 materials, 69 images/textures, 1 node |

Lantern is checked in at the pinned Khronos revision. Sponza was acquired into
the ignored external staging root and verified against the same immutable
upstream revision. Both packages use the existing resolver and glTF/GLB
importer; no package-specific parsing branch was added.

The canonical corpus manifest digest is
`7c337f69e3614bd31925a992a9067a3e31f34d7065b36f1d6bd796da0623526d`.
The canonical coverage digest is
`f87bed92e799443283329127e3612c3cf54ae5009d1049ad7e565704597bc749`.

## Integrity And Negative Evidence

Eight verifier tests and five acquisition tests passed. They cover strict
schema fields and ordering, package independence, coverage closure, immutable
pinned acquisition, interruption cleanup, unavailable upstream content, wrong
revision/hash, cache quarantine, and exact file inventory/size/digest checks.

The verifier rejects absolute, dot-segment, mixed-separator, percent-escaped,
case-colliding, non-NFC, duplicate, missing, extra, changed, and symlink-escaping
paths before import. The negative catalog contains 14 stable first-failure
cases. `MAINTAINER_NOTES.md` is neither opened nor hashed and changing it leaves
canonical acceptance output unchanged.

## Determinism And Regression Gates

| Gate | Result |
|---|---|
| Regular clean-checkout corpus verification | PASS |
| Full regular plus staged-medium verification | PASS |
| Python verifier/acquisition unit tests, 13 total | PASS |
| Lantern and Sponza typed importer structural gate | PASS |
| Canonical verifier repetition, 20 runs | PASS, byte-identical |
| Typed import identity/dependency repetition, 20 runs per package | PASS |
| `Tests/verify_architecture.py` | PASS |
| `Tests/verify_asset_layer.py` | PASS |

The real-package repetition remained CPU-bound and completed successfully. It
re-imported the authoritative GLB and external glTF package on every iteration;
the result was not served by a semantic oracle or package summary cache.
