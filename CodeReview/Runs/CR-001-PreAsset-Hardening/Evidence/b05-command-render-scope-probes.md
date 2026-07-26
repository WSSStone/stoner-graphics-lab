# B05-S01 Command And Render-Scope Probe Evidence

## Build And Runtime

The retained inspection probe was compiled against the current strict
ASan/UBSan Debug libraries with project warnings treated as errors:

```text
clang++ -std=c++20 -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -ISource/Backend/Vulkan/Public -ISource/RHI/Public -ISource/Core/Public \
  Evidence/Probes/b05-command-render-scope-inspection-probe.cpp \
  Build/Mac/Debug/Backend/Vulkan/libVulkanRHI.a \
  Build/Mac/Debug/RHI/libRHI.a Build/Mac/Debug/Core/libCore.a
```

The executable used `ASAN_OPTIONS=halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1`. It exited zero without a sanitizer report and
printed:

```text
oversized_mip_texture_copy_accepted=1
oversized_mip_readback_accepted=1
incompatible_format_copy_accepted=1
submission_state_forgeable=1
unsupported_direct_pool_allocation_succeeded=1
invalid_render_pass_constructed_valid=1
invalid_framebuffer_constructed_valid=1
classification=command-transfer-factory-contract-defects
```

The zero exit means every expected contract-defect signal was reproduced. It
is not a pass verdict for production behavior.

## Lifetime Evidence

The device/command-buffer lifetime issue is supported by source control flow,
not by an intentionally failing runtime probe:

- `FVulkanDevice` has a default constructor and no destructor that calls
  `Shutdown`;
- `CreateCommandBuffer` returns shared command-buffer ownership to callers;
- each command buffer stores a raw pointer to the device's `Diagnostics`
  member;
- `Invalidate` does not detach that pointer;
- `MarkRecordingDiagnostic` dereferences the pointer on normal and rejected
  recording transitions.

Therefore either implicit device destruction or an explicitly shut-down and
then destroyed device can leave a retained command buffer with stale
diagnostic ownership. No fault-triggering probe was run; B05-S02 must add a
safe maintained lifetime regression after repairing the ownership model.

## Maintained-Suite Contrast

The Feature 011 block in `Tests/VulkanBackendTests.cpp` covers ordinary
device-mediated command allocation, base-mip transfers, render-scope ordering,
capacity exhaustion, upload scheduling, and explicit shutdown invalidation.
It does not cover:

- negative transfer regions at nonzero mips;
- texture-copy format or sample compatibility;
- checked texture-to-buffer footprint arithmetic;
- closure of command-pool, command-buffer, render-pass, and framebuffer
  construction;
- queue/submission-only authority for submitted/completed transitions;
- retained command-buffer behavior after the device object's lifetime ends;
- allocation failure rollback in command and render-object factories.

This contrast establishes coverage gaps rather than contradicting the focused
probe.
