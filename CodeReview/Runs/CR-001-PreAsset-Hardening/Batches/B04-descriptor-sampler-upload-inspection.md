# B04-S10: Descriptors, Samplers, And Upload Staging Inspection

## Scope

This packet inspected one Feature 010 responsibility domain across exactly
eight production files, 528 lines total:

- `FVulkanDescriptorPool.h/.cpp`;
- `FVulkanDescriptorSet.h/.cpp`;
- `FVulkanSampler.h/.cpp`;
- `FVulkanUploadStaging.h/.cpp`.

The review also read bounded device factory/shutdown and command scheduling
call sites, RHI contracts, Feature 008/010/011 authority documents, and
maintained tests. No production source or maintained test changed.

## Authority And Invariants

Feature 010 FR-005, FR-011 through FR-017, SC-004/SC-005, its data model and
contracts require:

- unsupported sampler descriptions to fail without a usable object;
- descriptor sets to consume one fixed-capacity pool reservation and reclaim
  exactly that reservation;
- descriptor allocation exhaustion and partial failure to return explicit
  results without changing existing sets or leaking capacity;
- upload requests to validate the selected mip/layer region and source data's
  format-compatible byte footprint when the request is created;
- invalid requests to fail before a pending record can reach scheduling.

The device-owned factory rule from Feature 008 makes wrapper constructors part
of those invariants rather than an optional convention.

## Findings

### CR001-B04-F011 — S2 Accepted

Descriptor pool capacity is represented only by a mutable scalar counter.
`FVulkanDescriptorSet` has a public constructor and assumes it owns one pool
slot without receiving reservation authority. Its destructor therefore calls
`Release` even for a set that was never allocated. The retained probe uses that
path to release another live reservation and then allocate the supposedly full
slot again.

The normal factory also calls `Allocate` before `MakeShared` and
`DescriptorSets.push_back`. Either allocation may throw; neither path catches
the failure or rolls the reservation back. The fixed-capacity and explicit-
result contracts are therefore not failure-atomic.

### CR001-B04-F012 — S2 Accepted

`FVulkanSampler` exposes a public constructor that performs no validation. The
probe passes the same compare-mode plus no-mip-filter description rejected by
`FVulkanDevice::CreateSampler`; the directly constructed object reports a
`Valid` lifecycle. Unsupported state can therefore enter descriptor records
without crossing the device capability boundary.

### CR001-B04-F013 — S2 Accepted

Texture upload creation compares every region against the base texture extent,
not `GetRHIMipExtent` for the selected mip. It also accepts any positive source
byte count and has no format-byte-width or checked region-footprint rule. The
probe demonstrates both defects:

- an 8x8 region is accepted for mip 1 of an 8x8 texture, whose extent is 4x4;
- one byte is accepted for a 4x4 `R8G8B8A8_UNorm` region requiring 64 bytes.

The command scheduler rechecks mip bounds and rejects the first record later,
but Feature 010 requires rejection at request creation. It does not repair the
underfilled record, which can be marked scheduled and leaves future execution
without enough source bytes.

## Sampler And Descriptor Update Review

Within factory-mediated valid objects, binding lookup, descriptor type and
array bounds, retained weak records, resource lifecycle checks, and shutdown
invalidation follow the clarified contract. Sampler state preservation and
idempotent invalidation are locally consistent. No separate finding was opened
for those paths.

Map allocation during descriptor updates can still throw, but the current
authority documents do not define an allocation-failure result for update
bookkeeping. It is recorded as residual risk for B09's cross-cutting exception
policy rather than expanded into an unsupported B04 finding.

## Coverage Gap And Impact

Fresh maintained ASan/UBSan tests pass while the focused sanitizer probe
reproduces every defect signal. Existing tests cover only device-mediated
sampler creation, ordinary capacity exhaustion, base-mip region bounds, and a
16-byte upload whose byte count happens to match a 4x4 one-byte format fixture.

These gaps matter before the Asset roadmap introduces image formats, mips,
compressed payloads, and asynchronous manager bookkeeping. Invalid upload
records or non-atomic descriptor quota cannot become implicit preconditions of
Features 021–025.

## Validation

- retained strict sanitizer probe: five defect signals, zero exit, no
  ASan/UBSan report;
- fresh `sanitizers` gate: passed at
  `2026-07-26T14:30:29+00:00`;
- CodeGraph was current before the evidence probe and is synchronized again at
  packet closure;
- no production or maintained-test source changed.

## Handoff To B04-S11

B04-S11 should repair the three findings as one factory/validation migration:

1. replace scalar descriptor reservation authority with an unforgeable,
   exactly-once reservation owned by a factory-created set;
2. make descriptor creation rollback-safe for wrapper/control-block/tracking
   allocation failure and map it to `Unavailable`;
3. close sampler and descriptor-set direct construction while preserving
   device factories;
4. add checked, mip-aware texture-region byte-footprint validation for every
   current RHI format and reject unsupported transfer shapes explicitly;
5. add maintained regressions and deterministic failure injection before
   B04-S12 verification.

No push or GitHub Actions run occurred in this packet.
