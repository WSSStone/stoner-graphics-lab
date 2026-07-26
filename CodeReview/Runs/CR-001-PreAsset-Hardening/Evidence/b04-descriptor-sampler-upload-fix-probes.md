# B04-S11 Descriptor, Sampler, And Upload Fix Evidence

## Allocation-Failure Probe

The retained fix probe overrides global allocation only after deterministic
fixtures are initialized. It was compiled with strict warnings and first run
against the strict Debug libraries, then rebuilt and rerun against the final
ASan/UBSan libraries. Both runs exited zero. The sanitizer run printed:

```text
pool_object_rollback=1
pool_control_rollback=1
descriptor_wrapper_rollback=1
descriptor_control_rollback=1
descriptor_tracking_rollback=1
sampler_wrapper_failure=1
sampler_control_failure=1
sampler_tracking_failure=1
upload_wrapper_failure=1
upload_control_failure=1
upload_staging_failure=1
upload_tracking_failure=1
classification=descriptor-sampler-upload-fixes-active
```

Each descriptor failure also asserted a zero allocated pool count. All result
paths returned `Unavailable` with a null object; no exception escaped and no
ASan/UBSan diagnostic was emitted.

## Original Mechanism Closure

The B04-S10 defect reproducer no longer compiles against current headers. Its
expected-failure syntax check reports five errors:

- `FVulkanDescriptorPool::Allocate` no longer exists;
- the old pool-based descriptor-set constructor no longer matches;
- `FVulkanSampler` construction is private;
- `FVulkanDescriptorPool` construction is private.

Maintained compile-time assertions additionally prove that descriptor pools,
descriptor sets, samplers, and empty upload requests are not publicly
constructible and that descriptor reservations cannot be copied.

## Maintained Coverage

The full maintained suite now covers:

- exact byte widths for every declared RHI format;
- one reservation returned by descriptor invalidation and subsequent capacity
  reuse;
- exact buffer source/range equality and copy-destination usage;
- exact RGBA texture source bytes;
- selected nonzero-mip bounds and valid nonzero-mip upload;
- underfilled staging data, oversized mip region, non-copy destination, and
  multisampled transfer rejection.

The prior 16-byte source for a 4x4 RGBA upload was corrected to 64 bytes rather
than retained as a false-positive success fixture.

## Gate Results

- `fallback-strict`: passed with full tests at
  `2026-07-26T14:42:27+00:00`;
- `strict-debug`: passed at `2026-07-26T14:46:16+00:00`;
- `strict-release`: passed at `2026-07-26T14:46:55+00:00`;
- `sanitizers`: passed with full tests at
  `2026-07-26T14:48:13+00:00`.

The sanitizer profile intentionally skips only the optional deferred-native
case owned by accepted `CR001-B08-F001`; native Vulkan instance/device,
offscreen triangle, frame-local release, and zero-live-object tests passed.
