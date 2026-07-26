# B04-S12: Descriptor, Sampler, And Upload Staging Verification

## Verification Target

This packet independently verifies the repairs committed at `5830901`:

- `CR001-B04-F011`: descriptor quota ownership and failure atomicity;
- `CR001-B04-F012`: sampler factory validation authority;
- `CR001-B04-F013`: mip-aware texture upload bounds and exact byte footprints.

No production source or maintained test implementation changed during this
verification packet.

## Parent Reproduction

The exact implementation parent
`7ec5db51c6bfc82f53085f4a37a9b346002dca8c` reproduced all five defect signals
under strict ASan/UBSan: forged descriptor release, quota overcommit, valid
construction of an unsupported sampler, oversized nonzero-mip upload, and
underfilled RGBA upload. The temporary detached worktree was removed after
capture.

## Independent Verification

The current-head verifier covers descriptor state transitions and every
allocation rollback point, exact buffer ranges, all RHI format widths,
dimension/mip/layer texture subresources, unsupported transfer cases, checked
overflow, and upload allocation failures. It passed under ASan/UBSan without a
diagnostic.

The original defect probe now fails syntax compilation with five expected
errors because raw pool allocation, direct pool construction, the old
descriptor-set constructor, and direct sampler construction are absent.

Detailed commands, matrices, and output are recorded in
`Evidence/b04-descriptor-sampler-upload-verification-probes.md`.

## Source And Graph Audit

The current CodeGraph index contains 369 files, 5,085 nodes, and 15,463 edges.
The shared RHI format-width helper feeds both full allocation and upload
footprint validation. Descriptor reservation creation is confined to the
device/pool/set ownership chain. No public or production bypass was found.

## Local Gate Evidence

The current source state passed fallback-strict with full tests, real-graphics
strict Debug, strict Release, and strict ASan/UBSan with full tests. The final
independent verifier was rebuilt against the sanitizer gate artifacts and
passed again.

## Pending Boundary Evidence

Three-platform GitHub Actions remains pending. Until it passes:

- `CR001-B04-F011`: Fixed;
- `CR001-B04-F012`: Fixed;
- `CR001-B04-F013`: Fixed.

The packet and B04 batch must not close before remote evidence is recorded.
