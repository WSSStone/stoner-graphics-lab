# B04-S07 Allocation, Buffer, And Texture Probe Evidence

## Retained Inspection Probe

Source:

`Evidence/Probes/b04-allocation-buffer-texture-inspection-probe.cpp`

The probe was compiled against the current sanitizer-instrumented strict Debug
libraries:

```text
clang++ -std=c++20 -Wall -Wextra -Werror \
  -fsanitize=address,undefined \
  -fno-omit-frame-pointer \
  -fno-sanitize-recover=all \
  -ISource/Core/Public \
  -ISource/RHI/Public \
  -ISource/Backend/Vulkan/Public \
  CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/Probes/\
b04-allocation-buffer-texture-inspection-probe.cpp \
  -LBuild/Mac/Debug/Backend/Vulkan \
  -LBuild/Mac/Debug/RHI \
  -LBuild/Mac/Debug/Core \
  -lVulkanRHI -lRHI -lCore \
  -framework Cocoa -framework IOKit -framework CoreFoundation \
  -o /tmp/cr001-b04-s07-allocation-inspection-probe-fresh

env ASAN_OPTIONS=halt_on_error=1 \
  UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  /tmp/cr001-b04-s07-allocation-inspection-probe-fresh
```

Observed output:

```text
allocation_counter_overflow=1
overflow_budget_bypass=1
oversized_upload_throws=1
duplicate_release_accepted=1
duplicate_release_budget_bypass=1
multisample_footprint_undercounted=1
wide_format_footprint_undercounted=1
mip_chain_footprint_overcounted=1
failed_allocation_buffer_usable=1
failed_allocation_texture_valid=1
classification=allocation-buffer-texture-contract-defects
```

The process exited zero because this is a defect reproducer. ASan and UBSan
reported no memory error; the oversized upload exception was caught and
recorded deliberately.

## Probe Construction

### Checked-accounting boundary

The probe creates a valid fallback device, then requests `UINT64_MAX` and two
bytes through the normal `IRHIDevice` buffer factory. Both succeed and the
allocator snapshot wraps to one byte with two live allocations. A later
two-byte budget accepts a third one-byte resource.

### Allocation ownership boundary

A standalone allocator creates two 32-byte records under a 64-byte budget. The
first record is copied; both values release successfully and decrement the same
global accounting twice. The second token remains successful, yet the snapshot
is zero and a new 64-byte request succeeds.

### Texture footprint boundary

All three probe descriptions pass `IsValidRHITextureDesc`:

- 4x4 RGBA8, four samples: estimator 64, logical texel footprint 256, accepted
  under 64;
- 4x4 RGB32F: estimator 64, logical texel footprint 192, accepted under 64;
- 8x8 RGBA8, four mips: estimator 1024, exact texel footprint 340, rejected
  under 340.

### Partial-resource and host-storage boundary

Public wrappers constructed from explicit failed allocation records report
valid lifecycle state. The buffer accepts a one-byte upload. A normal
device-created `UINT64_MAX` host-visible buffer throws from full-size CPU mirror
allocation when asked to upload one byte.

## CodeGraph And Source Search

Commands:

```text
codegraph status . --no-color
codegraph callers FVulkanMemoryAllocator::Release --no-color
codegraph callers EstimateTextureBytes --no-color
codegraph callers FVulkanBuffer::Upload --no-color

rg -n \
  "FVulkanMemoryAllocator|FVulkanResourceAllocation|FVulkanBuffer|FVulkanTexture" \
  Source/Backend/Vulkan Tests
```

CodeGraph identified buffer and texture invalidation as the two allocator
release callers, and `AllocateTexture` as the sole texture-estimator caller. It
found no static caller for the virtual buffer upload override; the retained
probe exercises it through `IRHIBuffer`.

## Maintained Gate

Authoritative record:

- `Evidence/gate-sanitizers.json`: passed at
  `2026-07-26T13:32:57+00:00`.

The gate performed a strict ASan/UBSan Debug build and ran the maintained test
executable with exit code zero and
`STONER_SKIP_OPTIONAL_DEFERRED_NATIVE=1`. That skip belongs to accepted
`CR001-B08-F001`; this packet neither waives nor verifies it.
