# B04-S05: Surface And Swapchain Lifecycle Fix

## Scope

This packet repaired the three Accepted findings authorized by B04-S04 as one
presentation API migration:

- `CR001-B04-F004`: backend-neutral presentation dispatch was unsupported;
- `CR001-B04-F005`: surfaces had no device provenance or lifecycle ownership;
- `CR001-B04-F006`: creation failures were not atomic or consistently
  classified.

The implementation commit is `6119322`. The migration changes the
deterministic `FVulkanDevice` presentation contract; it does not claim native
Vulkan surface/swapchain execution and does not refactor
`FVulkanNativeContext`, which remains in B08's integration scope.

## Backend-Neutral Presentation

`FVulkanDevice` now overrides the current RHI factories:

- `CreatePresentationSurface(FRHIPresentationSurfaceDesc)`;
- `CreateSwapchain(IRHIPresentationSurface, FRHISwapchainDesc)`.

The legacy `FVulkanSurface` and frame-count helpers remain compatibility
adapters and delegate to the same validated surface-backed swapchain path.
Surface-backed deterministic swapchains expose one imported `IRHITexture`
wrapper per image with the requested extent, color format, and Present usage.
Recreation invalidates the previous image generation before publishing the
replacement.

`FVulkanSwapchain` also implements semaphore-aware acquire and present.
Validation occurs before either the frame state or semaphore changes. A failed
acquire preserves the caller's image index and semaphore; a failed present
preserves the acquired frame and wait signal.

## Provenance And Lifetime

Each device owns a presentation-owner token. Every surface created by that
device stores a weak reference to the token and a shared lifecycle record:

- copies of the legacy value surface and RHI object reference share validity;
- foreign-device surfaces fail swapchain creation;
- explicit invalidation is visible to every copy and dependent swapchain;
- surface loss makes image access, acquire/present, and recreation unavailable;
- device shutdown invalidates swapchains, imported images, surfaces, and the
  owner token;
- `FVulkanDevice` is non-copyable so presentation identity cannot be aliased by
  a copied device object.

The swapchain retains its associated surface. A local `Ready` state therefore
cannot override a lost parent presentation target.

## Failure Atomicity

`CreateSurface` resets its output before checking device state, so every failed
call leaves an invalid destination. Result categories are now separated:

- zero frame count, zero extent, invalid format, and depth presentation format
  return `InvalidState`;
- valid requests beyond the selected device's frame-count or format capability
  return `Unsupported`;
- inactive-device creation returns `InvalidState`;
- stale, missing, and foreign surfaces return `InvalidState`;
- a lost associated surface returns `Unavailable` from frame operations and
  recreation.

The public concrete swapchain constructor no longer normalizes zero frames to
one; it creates a terminal unavailable object whose work methods return
`InvalidState`.

## Maintained Coverage

`Tests/VulkanBackendTests.cpp` now covers:

- output clearing for invalid-window and inactive-device surface creation;
- backend-neutral surface and descriptor-based swapchain dispatch;
- imported image count, extent, format, usage, generation replacement, and
  shutdown invalidation;
- legacy compatibility adapter behavior;
- stale/foreign-device rejection;
- invalid versus unsupported descriptor classification;
- synchronized acquire/present success and failure atomicity;
- surface loss blocking image access and recreation;
- shutdown invalidation of surfaces, swapchains, and imported images;
- direct zero-frame constructor rejection.

All pre-existing ordering, wrong-frame, double-acquire, generation, resize,
unavailable, and shutdown assertions remain covered.

## Validation

The final source state passed:

- strict real-graphics Debug build at `2026-07-26T13:10:55+00:00`;
- strict real-graphics Release build at `2026-07-26T13:11:08+00:00`;
- strict graphics-disabled Debug build and complete maintained test executable
  at `2026-07-26T13:11:41+00:00`;
- strict ASan/UBSan Debug build and maintained test executable at
  `2026-07-26T13:12:42+00:00`, with only the separately tracked optional
  deferred-native test skipped;
- a sanitizer-linked retained fix verifier covering all three findings.

The retained verifier produced:

```text
backend_neutral_contract=1
synchronized_transitions=1
foreign_surface_rejected=1
failed_factory_clears_output=1
invalid_input_classification=1
surface_loss_blocks_recovery=1
shutdown_invalidates_ownership=1
classification=surface-swapchain-contracts-fixed
```

The sanitizer gate still executed and passed native Vulkan instance/device,
offscreen triangle, frame-local release, and zero-live-object tests. The
optional deferred-native skip belongs to accepted `CR001-B08-F001`; it is not
waived or attributed to this packet.

## Handoff To B04-S06

`CR001-B04-F004` through `F006` are `Fixed`, not yet `Verified`. Independent
verification must:

1. reproduce the original behaviors against exact parent `7c9a3df`;
2. invoke the current presentation factories through `IRHIDevice&`, not only
   backend-specific helpers;
3. independently test imported image generations, synchronization atomicity,
   foreign/stale surface rejection, output clearing, and shutdown;
4. rerun focused maintained regressions and required local gates;
5. mark the findings Verified only if deterministic presentation remains
   visibly distinct from native execution.
