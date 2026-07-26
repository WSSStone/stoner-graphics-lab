# B04-S10 Descriptor, Sampler, And Upload Probe Evidence

## Build And Runtime

The retained inspection probe was compiled against the current strict
ASan/UBSan Debug libraries with project warnings treated as errors:

```text
clang++ -std=c++20 -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -ISource/Backend/Vulkan/Public -ISource/RHI/Public -ISource/Core/Public \
  Evidence/Probes/b04-descriptor-sampler-upload-inspection-probe.cpp \
  Build/Mac/Debug/Backend/Vulkan/libVulkanRHI.a \
  Build/Mac/Debug/RHI/libRHI.a Build/Mac/Debug/Core/libCore.a
```

macOS does not support ASan leak detection, so the executable used
`ASAN_OPTIONS=halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1`. It exited zero without a sanitizer report and
printed:

```text
unallocated_set_released_another_reservation=1
descriptor_capacity_overcommit_enabled=1
invalid_sampler_constructed_valid=1
oversized_mip_region_accepted=1
underfilled_rgba_region_accepted=1
classification=descriptor-sampler-upload-contract-defects
```

The zero exit means every expected defect signal was reproduced; it is not a
pass verdict for production behavior.

## Maintained-Suite Contrast

The allow-listed `sanitizers` gate passed at
`2026-07-26T14:30:29+00:00`. Its maintained Vulkan tests cover ordinary
descriptor exhaustion, device-mediated invalid sampler rejection, one base-mip
texture region rejection, and ordinary upload success. They do not exercise:

- reservation ownership or factory bookkeeping allocation failure;
- direct backend-wrapper construction closure;
- nonzero-mip upload extents;
- source byte footprints derived from region and format.

This contrast establishes a coverage gap rather than contradicting the focused
probe.
