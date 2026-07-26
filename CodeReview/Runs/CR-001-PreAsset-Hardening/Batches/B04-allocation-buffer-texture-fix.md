# B04-S08: Allocation, Buffer, And Texture Fix

## Scope

This packet repaired the four Accepted S2 findings from B04-S07 as one public
allocation API migration:

- `CR001-B04-F007`: allocation counters could overflow and bypass budgets;
- `CR001-B04-F008`: copied allocation records could release accounting twice;
- `CR001-B04-F009`: texture estimates ignored exact format/sample/mip cost;
- `CR001-B04-F010`: failed wrappers remained usable and host mirrors attempted
  full logical-buffer allocation.

The implementation commit is `f1a3329`. These findings are `Fixed`, not yet
`Verified`; B04-S09 owns independent verification.

## Checked Allocation Accounting

`FVulkanMemoryAllocator` now validates public buffer/texture descriptions and
checks every byte/count addition before mutating allocator state. Arithmetic
overflow has its own failure category. A failed request preserves allocated
bytes and live count, so later budget enforcement cannot be bypassed.

Texture footprint calculation now sums every mip's reduced extent and includes
depth, array layers, exact format width, and sample count. Unsupported or
unrepresentable footprints produce a zero estimate and explicit failure.

## Unique Ownership

`FVulkanResourceAllocation` is now a no-allocation, move-only release ticket.
Each ticket binds to:

- a stable, non-copyable allocator object;
- a process-unique allocator identity;
- the allocator reset epoch;
- one allocation ID.

Moving transfers release authority and leaves the source inert. Foreign,
stale, moved-from, and repeated releases fail before counters change. The
ticket itself performs no heap allocation, preserving allocator `noexcept`
behavior.

## Resource Construction And Upload

Only `FVulkanDevice` can construct concrete buffers and textures. Wrappers
validate allocation kind and exact byte size before taking ownership. Wrapper
allocation or device-tracking failure releases the temporary allocation and
returns an explicit result instead of publishing a partial object. Valid
wrappers also release through their destructor if an owning path exits early.

Fallback host-visible buffers now grow their CPU mirror only through the end of
the requested upload. Unrepresentable size, `length_error`, and `bad_alloc`
map to `Unavailable`; previous bytes and lifecycle remain unchanged.

## Specification Alignment

Feature 010's spec, plan, data model, contract, and task history contain a
dated CR-001 amendment for checked arithmetic, exact texture footprints,
move-only allocation ownership, device-only wrapper construction, sparse host
mirrors, and maintained regression coverage. The amendment does not add Asset
scope or conceal the historical implementation.

## Maintained Coverage

`Tests/VulkanBackendTests.cpp` now covers:

- compile-time non-copyable allocation/allocator identity and private wrapper
  construction;
- foreign, repeated, and stale-epoch release rejection;
- exact multisample, RGB32F, and mip-chain texture footprints;
- texture budget decisions and unrepresentable footprint rejection;
- `UINT64_MAX` allocation accounting and post-failure budget integrity;
- sparse one-byte upload to an extreme logical buffer and explicit impossible
  storage failure;
- zero allocation accounting after shutdown.

## Validation

The final source state passed:

- strict graphics-disabled Debug build and complete maintained tests at
  `2026-07-26T13:54:17+00:00`;
- strict real-graphics Debug build at `2026-07-26T13:54:46+00:00`;
- strict real-graphics Release build at `2026-07-26T13:55:02+00:00`;
- strict ASan/UBSan build and complete maintained tests at
  `2026-07-26T13:56:14+00:00`;
- an independently compiled, sanitizer-linked public-API fix probe at
  `Evidence/Probes/b04-allocation-buffer-texture-fix-probe.cpp`.

The fix probe exited zero and produced:

```text
allocation_overflow_rejected=1
post_overflow_budget_preserved=1
unique_release_enforced=1
texture_footprints_exact=1
device_only_construction=1
sparse_upload_explicit_failure=1
shutdown_accounting_zero=1
classification=allocation-resource-contract-hardened
```

The sanitizer gate executed native Vulkan instance/device, offscreen triangle,
frame-local release, and zero-live-object tests. Its optional deferred-native
skip remains owned by accepted `CR001-B08-F001` and is not waived here.

## Handoff To B04-S09

Independent verification must:

1. reproduce all four original defects against exact parent `cf74523`;
2. audit move-only ownership, allocator lifetime identity, reset epochs, and
   every failure rollback without relying only on maintained tests;
3. independently check all current RHI format widths, sample counts, mip,
   depth, and layer arithmetic, including overflow;
4. exercise sparse upload preservation and shutdown accounting under
   sanitizers;
5. rerun focused regressions and required local gates before marking F007-F010
   `Verified`.
