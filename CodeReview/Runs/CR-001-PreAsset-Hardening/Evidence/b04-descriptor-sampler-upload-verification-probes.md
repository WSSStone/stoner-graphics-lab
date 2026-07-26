# B04-S12 Descriptor, Sampler, And Upload Verification Evidence

## Exact-Parent Reproduction

Implementation commit `5830901` has exact parent
`7ec5db51c6bfc82f53085f4a37a9b346002dca8c`. The parent was checked out in a
temporary detached worktree, built with strict ASan/UBSan and graphics
disabled, and linked with the retained B04-S10 inspection probe.

The process exited zero because the probe classifies the presence of the
defects. ASan and UBSan emitted no diagnostic. Output was:

```text
unallocated_set_released_another_reservation=1
descriptor_capacity_overcommit_enabled=1
invalid_sampler_constructed_valid=1
oversized_mip_region_accepted=1
underfilled_rgba_region_accepted=1
classification=descriptor-sampler-upload-contract-defects
```

The temporary worktree and parent build were removed after capture.

## Independent Current-Head Verifier

The B04-S12 verifier was written independently from the implementation probe.
It was compiled with strict warnings and ASan/UBSan against the final local
sanitizer libraries. It verifies:

- compile-time closure of pool, descriptor-set, sampler, upload-request, and
  reservation construction/copy mechanisms;
- descriptor capacity exhaustion, invalidation, reuse, move transfer, and
  exactly-once release;
- pool object/control-block and descriptor wrapper/control-block/tracking
  allocation rollback;
- exact buffer upload ranges and copy-destination usage;
- exact byte footprints for every declared RHI format;
- 1D-array, 2D, 3D, cube, nonzero-mip, and array-layer subresources;
- unknown format, multisampling, copy-usage, bounds, byte mismatch, and checked
  arithmetic overflow rejection;
- upload wrapper/control-block/staging/tracking allocation failure mapping.

The verifier exited zero with no sanitizer diagnostic and printed:

```text
factory_api_closure=1
descriptor_state_machine=1
pool_object_failure_rollback=1
pool_control_failure_rollback=1
descriptor_wrapper_failure_rollback=1
descriptor_control_failure_rollback=1
descriptor_tracking_failure_rollback=1
buffer_upload_matrix=1
all_format_footprints=1
subresource_dimension_matrix=1
unsupported_and_overflow_matrix=1
upload_allocation_failure_matrix=1
classification=descriptor-sampler-upload-fixes-independently-verified
```

## Negative API Closure

The original B04-S10 inspection probe was compiled with `-fsyntax-only`
against current headers and failed as required with five errors:

- `FVulkanDescriptorPool::Allocate` is absent;
- direct pool construction is private;
- the old pool-based descriptor-set constructor is absent;
- direct sampler construction is private.

The original quota forgery and validation-bypass paths therefore cannot be
expressed through the current public API.

## Local Gates

- `fallback-strict`: passed with complete maintained tests at
  `2026-07-26T15:00:00+00:00`;
- `strict-debug`: passed at `2026-07-26T15:00:36+00:00`;
- `strict-release`: passed at `2026-07-26T15:01:43+00:00`;
- `sanitizers`: passed with complete maintained tests at
  `2026-07-26T15:02:56+00:00`.

The sanitizer profile passed native Vulkan instance/device, offscreen
triangle, frame-local release, and zero-live-object tests. Its optional
deferred-native skip remains owned by accepted `CR001-B08-F001`.

## CodeGraph

After synchronization the index is current at 369 files, 5,085 nodes, and
15,463 edges. `GetRHIFormatByteSize` is called by the Vulkan allocation
estimator, Vulkan upload footprint validator, maintained regression test, and
independent verifier. Reservation creation remains in the pool/device/set
ownership chain. No contradictory production path was found.

## Remote Evidence

Three-platform GitHub Actions verification is pending at this local evidence
commit. Findings remain Fixed until that run passes.
