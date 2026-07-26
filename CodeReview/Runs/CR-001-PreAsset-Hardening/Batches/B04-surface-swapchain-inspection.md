# B04-S04: Surface And Swapchain Lifecycle Inspection

## Inspection Budget

The inspection covered one presentation-contract responsibility domain and
eight production files. Seven files were read in full and three bounded slices
of `FVulkanDevice.cpp` were read, totaling 647 selected lines:

1. `Source/RHI/Public/RHI/IRHIDevice.h`
2. `Source/RHI/Public/RHI/IRHISwapchain.h`
3. `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDevice.h`
4. `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`, lines 150-214,
   350-363, and 682-758
5. `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanSurface.h`
6. `Source/Backend/Vulkan/Private/FVulkanSurface.cpp`
7. `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanSwapchain.h`
8. `Source/Backend/Vulkan/Private/FVulkanSwapchain.cpp`

Feature 009 and Feature 018 specifications/contracts, descriptor headers,
`FPlatformWindow`, maintained tests, call sites, and Git history were
supporting reads rather than additional line-by-line production inspection.
No production or maintained test implementation changed.

## Requirement Mapping

- `009-FR-011` through `FR-016` require a valid Core window-backed surface,
  mutually compatible surface/device/swapchain inputs, explicit acquire and
  present results, and recreation only after valid presentation inputs return.
- `009-FR-017`, the Surface/Swapchain Contracts, and `007-FR-002a` require
  device-owned lifetime and terminal invalidation for presentation objects.
- `009-SC-006` requires Renderer-facing interaction through RHI contracts.
  Feature 018 subsequently introduced `IRHIPresentationSurface`,
  `FRHISwapchainDesc`, imported swapchain images, and synchronized
  acquire/present overloads as the current backend-neutral presentation
  contract.
- `009-FR-001`, `FR-012`, `FR-013`, and `FR-015` require explicit,
  deterministic negative results that leave no usable partial output.

## Reproduction

The retained standalone probe was linked against the current strict
ASan/UBSan Debug libraries. It produced:

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

The process exited zero because the probe is a defect reproducer. ASan and
UBSan reported no memory error. The complete maintained sanitizer test
executable also exited zero with the separately tracked optional
deferred-native case skipped, proving these behaviors are absent from current
regression coverage.

Detailed commands, output, source traces, and interpretation are retained in
`Evidence/b04-surface-swapchain-probes.md` and the probe source.

## Findings

### CR001-B04-F004 - Accepted S2

`FVulkanDevice` implements only the legacy frame-count swapchain factory and
backend-specific `FVulkanSurface` helpers. Through an active `IRHIDevice`
reference, both the current RHI presentation-surface factory and the
surface/descriptor swapchain factory return `Unsupported`. `FVulkanSwapchain`
also inherits null image access and unsupported synchronized acquire/present
defaults. The backend-specific deterministic path succeeds at the same time.

This leaves the current Vulkan device unable to satisfy the backend-neutral
presentation contract and contributes to duplicated presentation ownership in
the later native facade.

### CR001-B04-F005 - Accepted S2

`FVulkanSurface` retains only a borrowed opaque pointer and has no device
identity or shared lifecycle state. `FVulkanDevice` tracks swapchains but not
surfaces. The probe creates a surface on Device A, shuts A down, observes the
surface still valid, and successfully creates a swapchain from it on Device B.
After that surface is invalidated, the Device B swapchain can move from
`Unavailable` back to `Ready` through `Recreate` without restored input.

Device/surface compatibility and parent-child invalidation are therefore not
enforceable, and the ready state does not prove a valid presentation target.

### CR001-B04-F006 - Accepted S2

`FVulkanDevice::CreateSurface` returns early for an inactive device without
resetting `OutSurface`. A failed call can therefore return `InvalidState` while
leaving a previously valid output usable. Swapchain creation also groups zero
frame count with unsupported capabilities and returns `Unsupported`, while the
public concrete constructor silently normalizes zero to one. Generic RHI
coverage classifies zero frame count as invalid input.

Creation failure behavior is not atomic and its result category depends on the
entry point.

## Confirmed Strengths

- A normal active-device invalid-window call resets the output and reports a
  non-empty presentation diagnostic.
- The swapchain rejects double acquire, present without acquire, wrong-frame
  present, and old-generation present without mutating the successful frame
  sequence.
- Failed zero-count recreation preserves frame count and generation.
- Successful recreation resets the frame index, clears acquired generation,
  increments generation, and returns to `Ready`.
- Device shutdown invalidates every swapchain retained by that device, and
  later acquire returns `InvalidState`.
- Failed acquire paths do not overwrite the caller's output frame index.

## Coverage Gaps

- Maintained Vulkan tests use only backend-specific presentation methods; they
  do not call the current RHI surface/descriptor factories or synchronized
  swapchain overloads.
- No maintained test covers surface ownership, cross-device use, device
  shutdown invalidation, surface loss during recreation, or output atomicity.
- No maintained Vulkan test distinguishes zero-frame invalid input from
  unsupported frame-count capability.
- The direct `FVulkanSwapchain(0)` constructor path is untested.

## B04-S05 Fix Packet

B04-S05 may treat the three findings as one coherent presentation API
migration:

1. implement the current backend-neutral surface and descriptor-based
   swapchain factories on `FVulkanDevice`, with deterministic fallback visibly
   distinct from native execution;
2. bind surfaces and surface-backed swapchains to device-owned lifecycle and
   provenance, reject stale/cross-device inputs, and require a valid associated
   surface before successful recreation;
3. make failure outputs atomic and classify malformed descriptors/frame counts
   as `InvalidState`, while preserving explicit `Unsupported` for valid but
   unavailable capabilities;
4. add maintained tests for backend-neutral dispatch, ownership, invalidation,
   recreation, output atomicity, image/synchronization behavior delivered by
   this device path, and negative result categories.

Do not claim deterministic fallback as native presentation proof. Any required
consolidation with the real visible native facade must remain explicit and
traceable for B08 rather than being hidden by a simulated surface.
