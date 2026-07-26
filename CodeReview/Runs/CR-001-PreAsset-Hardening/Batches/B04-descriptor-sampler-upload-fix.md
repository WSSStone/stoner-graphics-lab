# B04-S11: Descriptor, Sampler, And Upload Staging Fix

## Repair Target

Implementation commit `5830901` repairs:

- `CR001-B04-F011`: forgeable descriptor quota and non-atomic allocation;
- `CR001-B04-F012`: sampler factory validation bypass;
- `CR001-B04-F013`: invalid mip regions and source byte footprints accepted by
  texture upload staging.

The repair is one ownership/validation migration. It does not introduce Asset
code, native descriptor allocation, compressed texture formats, streaming, or
command execution.

## Descriptor Reservation Ownership

`FVulkanDescriptorPool` no longer exposes public scalar `Allocate` or
`Release`. The device-owned pool issues a `FVulkanDescriptorReservation` that:

- is default-inert, move-only, and not copyable;
- holds the pool alive while active;
- transfers its sole release authority on move;
- returns capacity exactly once on reset or destruction.

Pool and descriptor-set constructors are private to `FVulkanDevice`.
`FVulkanDescriptorSet` owns the reservation directly, releases it on explicit
invalidation, and otherwise relies on RAII destruction. An unallocated set can
no longer be manufactured to decrement another set's quota.

The device factory catches pool object/control-block, set wrapper/control-
block, and tracking-vector failures. Local or wrapper-owned reservations roll
back according to the exact failure point before `Unavailable` is returned.

## Sampler Boundary

`FVulkanSampler` is now device-factory-only. The device remains the single
sampler description validator and catches wrapper, control-block, and tracking
allocation failures. Failed tracking invalidates the transient object before
returning; no unsupported sampler can be publicly constructed in `Valid`
state.

## Upload Validation

`GetRHIFormatByteSize` centralizes exact widths for every current uncompressed
RHI format and returns zero for `Unknown`. Both full texture allocation and
subresource staging use this mapping.

Buffer request creation now requires:

- a valid destination with `CopyDestination` usage;
- a nonzero in-bounds range;
- exact source-byte/range equality;
- representable staging storage.

Texture request creation now requires:

- a valid one-sample destination with `CopyDestination` usage;
- a valid selected mip and array layer;
- offsets and extents bounded by `GetRHIMipExtent`, not base dimensions;
- checked `width * height * depth * formatBytes` arithmetic;
- exact source-byte/region-footprint equality.

Request construction is private to validated static factories. Wrapper,
control-block, staging-vector, and device-tracking failures map to
`Unavailable`; no partial pending record escapes.

## Tests And Evidence

Maintained regressions prove API closure, all format widths, reservation reuse,
exact buffer and texture bytes, nonzero-mip validation, and unsupported transfer
paths. The retained allocation-failure probe covers twelve distinct failure
points and reports a zero descriptor count after every descriptor failure.

The original defect reproducer now fails syntax compilation because the raw
pool allocation and direct wrapper construction mechanisms are absent.

Fresh fallback-strict, real strict Debug, strict Release, and ASan/UBSan gates
all pass. CodeGraph is current at 368 files, 5,035 nodes, and 15,321 edges; the
only production format-width callers are the allocator and upload validator,
and reservation creation remains inside the device/pool/set ownership chain.

## Finding State

- `CR001-B04-F011`: Fixed at `5830901`.
- `CR001-B04-F012`: Fixed at `5830901`.
- `CR001-B04-F013`: Fixed at `5830901`.

B04-S12 must independently verify the exact parent defect signals, current API
closure, failure rollback, format/mip matrices, full gates, and cross-platform
behavior before transitioning any finding to Verified.

No push or GitHub Actions run occurred in this packet.
