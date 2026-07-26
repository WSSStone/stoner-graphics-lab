# B04-S04 Surface And Swapchain Probe Evidence

## Retained Probe

Source:

`Evidence/Probes/b04-surface-swapchain-inspection-probe.cpp`

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
  Evidence/Probes/b04-surface-swapchain-inspection-probe.cpp \
  -LBuild/Mac/Debug/Backend/Vulkan \
  -LBuild/Mac/Debug/RHI \
  -LBuild/Mac/Debug/Core \
  -lVulkanRHI -lRHI -lCore \
  -framework Cocoa -framework IOKit -framework CoreFoundation \
  -o /tmp/cr001-b04-s04-surface-swapchain-probe

env ASAN_OPTIONS=halt_on_error=1 \
  UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  /tmp/cr001-b04-s04-surface-swapchain-probe
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

The probe exits zero only when every inspected behavior is reproduced. ASan
and UBSan reported no runtime error.

## Maintained Test Control

The current sanitizer-built maintained test executable was run with:

```text
env ASAN_OPTIONS=halt_on_error=1 \
  STONER_SKIP_OPTIONAL_DEFERRED_NATIVE=1 \
  UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  Build/Mac/Debug/Tests/StonerTest
```

It exited zero. Existing Vulkan surface/swapchain tests passed their
backend-specific happy path, ordering, generation, unavailable, and shutdown
checks. The optional deferred-native skip belongs to accepted
`CR001-B08-F001` and does not exercise this deterministic presentation slice.

The control result establishes a maintained-coverage gap: all reproduced
behaviors coexist with a green test executable.

## Backend-Neutral Dispatch Trace

Focused search shows:

- `IRHIDevice` declares `CreatePresentationSurface` and descriptor-based
  `CreateSwapchain` compatibility methods;
- `FVulkanDevice` overrides only `CreateSwapchain(uint32)`;
- its presentation methods accept `FVulkanSurface` directly and are absent
  from Renderer/Application call sites;
- `FVulkanSwapchain` does not override `GetImage` or the semaphore-aware
  acquire/present overloads.

The probe invokes the current methods through `IRHIDevice&` and receives
`Unsupported`/null from both, while backend-specific creation succeeds on the
same active deterministic device.

## Ownership And Recovery Trace

`FVulkanDevice` retains `Swapchains` and invalidates them during shutdown. It
has no corresponding surface collection. `FVulkanSurface` contains only:

```text
void* NativeHandle
const char* DiagnosticReason
```

`CreateSwapchainForSurface` checks only `Surface.IsValid()` and delegates to
the surface-free frame-count factory. `FVulkanSwapchain` retains no surface or
owner identity, so `Recreate` can validate only its own boolean and frame
count. The probe consequently demonstrates all of these independent states:

1. Device A shutdown invalidates its swapchain but not its surface value;
2. Device B accepts Device A's stale surface;
3. invalidating that surface does not affect Device B's swapchain;
4. recreation returns the detached swapchain to `Ready`.

## Failure Contract Trace

`FVulkanDevice::CreateSurface` checks `IsActive()` before calling
`FVulkanSurface::Create`, and only the latter resets the output. The inactive
early return therefore preserves an old valid surface.

`FVulkanDevice::CreateSwapchain` uses one `Unsupported` branch for missing
presentation capabilities, zero frame count, and frame counts above the
device maximum. `FVulkanSwapchain` separately converts constructor input zero
to one. Generic RHI tests expect a zero-frame factory request to return
`InvalidState`, so the Vulkan result and concrete constructor are inconsistent
with the portable negative-path convention.

## Interpretation

The evidence separates three related defects for one B04-S05 migration:

1. the Vulkan device does not implement the current backend-neutral
   presentation contract;
2. the backend-specific surface/swapchain path has no parent provenance or
   surface-dependent recovery invariant;
3. creation failures are not output-atomic and conflate invalid inputs with
   unsupported capabilities.

The probe remains a defect reproducer so B04-S05 can invert the assertions and
B04-S06 can verify the migration independently.
