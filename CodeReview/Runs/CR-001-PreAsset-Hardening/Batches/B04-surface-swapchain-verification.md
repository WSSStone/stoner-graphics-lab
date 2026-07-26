# B04-S06: Surface And Swapchain Lifecycle Verification

## Verification Target

This packet independently verifies the repairs committed at `6119322`:

- `CR001-B04-F004`: backend-neutral presentation dispatch and imported images;
- `CR001-B04-F005`: surface provenance and shared lifecycle ownership;
- `CR001-B04-F006`: atomic factory failure and result classification.

No production source or maintained test implementation changed during this
verification packet.

## Exact-Parent Reproduction

The exact fix parent `7c9a3df` was exported with `git archive` into an isolated
temporary directory and built with the strict graphics-disabled Debug profile.
The retained B04-S04 inspection probe was then linked against those parent
libraries. It reproduced every recorded defect signal:

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

The process exited zero because the retained probe is a defect reproducer. This
establishes that the verification method distinguishes the parent behavior from
the repaired behavior.

## Independent Current-Head Verifier

The B04-S06 verifier was written independently from the fix verifier. It enters
through `IRHIDevice&` for modern surface and swapchain dispatch and adds checks
for:

- descriptor preservation and imported color/present image usage;
- three-image recreation, generation increment, and invalidation of both
  retained old images;
- already-signaled acquire failure without frame or semaphore mutation;
- unsignaled present failure without consuming the acquired frame;
- invalid-input versus unsupported-capability classification;
- active-device legacy factory output clearing;
- foreign-device rejection and invalidation through a copied surface value;
- shutdown invalidation of retained surface, swapchain, and image references;
- deterministic fallback diagnostics that cannot satisfy native proof.

The verifier was linked against the freshly generated ASan/UBSan strict Debug
libraries and produced:

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

## Call-Site And Authority Review

- `FVulkanDevice` overrides both current backend-neutral presentation factories.
- Maintained presentation creation enters through `IRHIDevice&`; the legacy
  helpers remain explicit compatibility adapters.
- The current factories validate a concrete Vulkan surface owner token before
  creating a surface-backed deterministic swapchain.
- Surface copies share validity, and device shutdown invalidates swapchains
  before surfaces and then deactivates the owner token.
- `FVulkanDevice` explicitly reports `DeterministicFallback` and directs native
  execution to `FVulkanNativeContext`; this packet does not claim native surface
  or swapchain execution.

## Fresh Gate Evidence

The current source state passed:

- `fallback-strict` at `2026-07-26T13:23:42+00:00`: strict
  graphics-disabled Debug build and complete maintained test executable;
- `sanitizers` at `2026-07-26T13:24:45+00:00`: strict ASan/UBSan Debug build
  and maintained test executable with the separately tracked optional
  deferred-native case skipped.

The sanitizer run still executed and passed native Vulkan instance/device,
offscreen triangle, frame-local release, and zero-live-object tests. The
optional deferred-native skip belongs to accepted `CR001-B08-F001`; it is not
waived or verified by this packet.

## Finding Decisions

- `CR001-B04-F004`: Verified.
- `CR001-B04-F005`: Verified.
- `CR001-B04-F006`: Verified.

The next packet may inspect the next B04 responsibility domain. No push or
GitHub Actions run occurred in this packet.
