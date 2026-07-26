# B04-S06 Surface And Swapchain Verification Evidence

## Exact-Parent Snapshot

The fix parent was exported without changing the review worktree:

```text
mkdir /tmp/cr001-b04-s06-parent-7c9a3df
git archive --format=tar \
  --output=/tmp/cr001-b04-s06-parent-7c9a3df.tar 7c9a3df
tar -xf /tmp/cr001-b04-s06-parent-7c9a3df.tar \
  -C /tmp/cr001-b04-s06-parent-7c9a3df

conda run -n stoner-cr scons \
  config=debug strict=1 graphics=disabled -j8
```

The strict parent build passed. The retained inspection probe was compiled
against that snapshot:

```text
clang++ -std=c++20 -Wall -Wextra -Werror \
  -ISource/Core/Public \
  -ISource/RHI/Public \
  -ISource/Backend/Vulkan/Public \
  CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/Probes/\
b04-surface-swapchain-inspection-probe.cpp \
  -LBuild/Mac/Debug/Backend/Vulkan \
  -LBuild/Mac/Debug/RHI \
  -LBuild/Mac/Debug/Core \
  -lVulkanRHI -lRHI -lCore \
  -framework Cocoa -framework IOKit -framework CoreFoundation \
  -o /tmp/cr001-b04-s06-parent-inspection-probe

/tmp/cr001-b04-s06-parent-inspection-probe
```

Observed output:

```text
devices_ready=1
surface_ready=1
backend_neutral_presentation_unsupported=1
legacy_surface_free_swapchain_available=1
original_swapchain_ready=1
surface_survives_owner_shutdown=1
cross_device_stale_surface_accepted=1
recreate_ignores_lost_surface=1
failed_factory_preserves_usable_output=1
zero_frame_classified_unsupported=1
classification=surface-swapchain-contract-defects
```

The process exited zero as an exact-parent defect reproducer.

## Independent Current Verifier

Source:

`Evidence/Probes/b04-surface-swapchain-verification-probe.cpp`

After the fresh sanitizer gate, the verifier was compiled and linked against
the resulting current libraries:

```text
clang++ -std=c++20 -Wall -Wextra -Werror \
  -fsanitize=address,undefined \
  -fno-omit-frame-pointer \
  -fno-sanitize-recover=all \
  -ISource/Core/Public \
  -ISource/RHI/Public \
  -ISource/Backend/Vulkan/Public \
  CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/Probes/\
b04-surface-swapchain-verification-probe.cpp \
  -LBuild/Mac/Debug/Backend/Vulkan \
  -LBuild/Mac/Debug/RHI \
  -LBuild/Mac/Debug/Core \
  -lVulkanRHI -lRHI -lCore \
  -framework Cocoa -framework IOKit -framework CoreFoundation \
  -o /tmp/cr001-b04-s06-current-verifier-fresh

env ASAN_OPTIONS=halt_on_error=1 \
  UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  /tmp/cr001-b04-s06-current-verifier-fresh
```

Observed output:

```text
backend_neutral_dispatch=1
descriptor_and_image_contract=1
generation_replacement=1
synchronized_failure_atomicity=1
result_classification=1
factory_failure_atomicity=1
provenance_and_shared_loss=1
shutdown_cascade=1
deterministic_not_native=1
classification=surface-swapchain-contracts-verified
```

The process exited zero. ASan and UBSan reported no error.

## Source And Call-Site Search

The verification search covered production, maintained tests, demo code, and
retained probes:

```text
rg -n \
  "CreatePresentationSurface|CreateSwapchainForSurface|CreateSwapchain\\(" \
  Source Tests Demo \
  CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/Probes

rg -n \
  "FVulkanNativeContext|DeterministicFallback|bUsedRuntimeFallback" \
  Source/Backend/Vulkan/Private/FVulkanDevice.cpp \
  Source/Backend/Vulkan/Private/FVulkanInstance.cpp \
  Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp \
  Tests/VulkanBackendTests.cpp
```

The Vulkan backend implements the modern overrides, maintained tests dispatch
through `IRHIDevice&`, and legacy call sites remain identifiable adapters. The
runtime search confirms that `FVulkanDevice` is explicitly deterministic and
that native execution remains owned by `FVulkanNativeContext`.

## Fresh Maintained Gates

Authoritative records:

- `Evidence/gate-fallback-strict.json`: passed at
  `2026-07-26T13:23:42+00:00`;
- `Evidence/gate-sanitizers.json`: passed at
  `2026-07-26T13:24:45+00:00`.

The fallback gate performed a strict graphics-disabled Debug build and ran the
complete maintained test executable with exit code zero. The sanitizer gate
performed a strict ASan/UBSan Debug build and ran the maintained test executable
with exit code zero and `STONER_SKIP_OPTIONAL_DEFERRED_NATIVE=1`.

That skip is scoped to accepted `CR001-B08-F001`. Native Vulkan instance/device
and offscreen triangle tests still executed and passed.
