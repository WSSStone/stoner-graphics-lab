# B04-S07: Memory Allocation, Buffers, And Textures Inspection

## Scope

This packet inspected one Feature 010 responsibility domain across eight
production files, 439 lines total:

- `FVulkanMemoryAllocator.h/.cpp`;
- `FVulkanResourceAllocation.h/.cpp`;
- `FVulkanBuffer.h/.cpp`;
- `FVulkanTexture.h/.cpp`.

The review also read the bounded `FVulkanDevice` factory, configuration, and
shutdown call sites, the RHI buffer/texture descriptions and validators,
Feature 010 authority documents, and maintained Vulkan resource tests. No
production source or maintained test implementation changed in this packet.

## Authority And Expected Invariants

Feature 010 FR-007 through FR-010, SC-002/SC-003, its allocation contract, and
US2 tasks require:

- one owned allocation record per successful buffer or texture;
- deterministic budget and count limits;
- explicit failure without a usable partial resource;
- safe repeated cleanup and zero stale usable resources after shutdown;
- texture footprint estimation suitable for allocation limits;
- explicit RHI failures for invalid or oversized resource operations.

The roadmap now makes these invariants prerequisites for texture assets,
compressed formats, model realization, and GPU residency. Approximate or
corrupt accounting cannot be deferred as a harmless fallback detail.

## Findings

### CR001-B04-F007 — S2 Accepted

`FVulkanMemoryAllocator::Allocate` checks budget subtraction safely, but the
unlimited-path accumulation at line 129 is unchecked. RHI and Vulkan resource
capabilities also publish no buffer-size ceiling. A `UINT64_MAX` buffer followed
by a two-byte buffer wraps `AllocatedBytes` to one; setting a two-byte budget
then permits another allocation. The snapshot reports two bytes for three live
resources.

### CR001-B04-F008 — S2 Accepted

`FVulkanResourceAllocation` is a public copyable value. Its release bit belongs
to each copy, while `FVulkanMemoryAllocator::Release` trusts the supplied value
as the authority to decrement global counters. Releasing two copies of one
token succeeds twice, hides a still-live second allocation, and permits a new
full-budget allocation. Repeated cleanup is therefore not idempotent at the
ownership level.

### CR001-B04-F009 — S2 Accepted

`EstimateTextureBytes`:

- defaults `R32G32_Float` and `R32G32B32_Float` to four bytes;
- ignores `SampleCount`;
- multiplies the full base extent by the mip count instead of summing mip
  extents;
- performs unchecked products.

The retained probe demonstrates both budget undercount and overcount with valid
RHI texture descriptions.

### CR001-B04-F010 — S2 Accepted

The public buffer and texture constructors unconditionally begin in `Valid`
state even when supplied a failed allocation record. A buffer constructed from
`BudgetExceeded` ownership accepts a host upload, while a matching texture also
reports valid. Through the normal device path, a `UINT64_MAX` host-visible
buffer is accepted and `Upload` lets `std::vector::resize` throw
`std::length_error` instead of returning an RHI result.

## Coverage Gap

The maintained suite covers one ordinary budget failure, one count-limit
failure, one explicit buffer invalidation, and shutdown invalidation. It does
not cover the boundaries claimed complete by T046–T050:

- allocation token copy/release identity;
- repeated create/release cycles with concurrent live allocations;
- checked accumulated-byte overflow;
- format/sample/mip texture footprints;
- usable wrappers around failed allocations;
- oversized host-visible upload storage failure.

Fresh sanitizer maintained tests remain green, demonstrating that existing
coverage does not detect the accepted findings.

## Impact Analysis

CodeGraph reports allocator release callers in `FVulkanBuffer::Invalidate` and
`FVulkanTexture::Invalidate`; texture estimation flows only through
`AllocateTexture`. `FVulkanResourceAllocation` also affects both wrapper public
contracts. The index had 340 files, 4,472 nodes, and 12,443 edges with only the
new evidence probe pending sync.

The repair is authorized as one allocation ownership/resource-construction API
migration in B04-S08, despite four separately traceable findings. It must not
introduce Asset code or native Vulkan allocation work.

## Validation

The sanitizer-linked retained inspection probe reproduced all ten defect
signals and exited zero. A fresh `sanitizers` gate passed at
`2026-07-26T13:32:57+00:00`; the optional deferred-native case remained skipped
under accepted `CR001-B08-F001`, while native instance/device and offscreen
triangle tests still ran and passed.

## Handoff To B04-S08

B04-S08 should fix F007–F010 as one coherent migration:

1. define checked buffer/texture size and accumulation rules plus explicit
   backend limits;
2. replace copy-local release authority with one allocator-owned identity or
   shared release state;
3. calculate format-, sample-, layer-, depth-, and mip-correct texture costs;
4. prevent failed ownership from producing valid wrappers;
5. map host storage size/allocation failures to explicit RHI results;
6. add maintained boundary and lifecycle regressions before verification.

No push or GitHub Actions run occurred in this packet.
