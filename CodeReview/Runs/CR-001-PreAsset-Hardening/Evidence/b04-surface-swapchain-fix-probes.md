# B04-S05 Surface And Swapchain Fix Evidence

## Retained Fix Verifier

Source:

`Evidence/Probes/b04-surface-swapchain-fix-probe.cpp`

The final verifier was compiled against the sanitizer-instrumented strict
Debug libraries:

```text
clang++ -std=c++20 -Wall -Wextra -Werror \
  -fsanitize=address,undefined \
  -fno-omit-frame-pointer \
  -fno-sanitize-recover=all \
  -ISource/Core/Public \
  -ISource/RHI/Public \
  -ISource/Backend/Vulkan/Public \
  Evidence/Probes/b04-surface-swapchain-fix-probe.cpp \
  -LBuild/Mac/Debug/Backend/Vulkan \
  -LBuild/Mac/Debug/RHI \
  -LBuild/Mac/Debug/Core \
  -lVulkanRHI -lRHI -lCore \
  -framework Cocoa -framework IOKit -framework CoreFoundation \
  -o /tmp/cr001-b04-s05-surface-swapchain-fix-probe-sanitized

env ASAN_OPTIONS=halt_on_error=1 \
  UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  /tmp/cr001-b04-s05-surface-swapchain-fix-probe-sanitized
```

Observed output:

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

The process exited zero. ASan and UBSan reported no error.

## Contract Assertions

The verifier enters through `IRHIDevice&` and proves:

1. the current surface and descriptor-based swapchain factories return usable
   objects;
2. the swapchain exposes exactly two imported images with requested extent and
   Present usage;
3. synchronized acquire signals only after successful frame preflight;
4. wrong-frame present preserves the acquired frame and signaled semaphore;
5. successful present consumes the semaphore and returns the swapchain to
   Ready;
6. a foreign device rejects the owner device's surface;
7. an inactive surface factory clears a previously valid legacy output;
8. zero-frame device and concrete-constructor paths return invalid-state;
9. surface loss makes the dependent swapchain unavailable and blocks
   recreation;
10. shutdown invalidates the surface, swapchain, and retained imported image.

## Maintained Regression Trace

The expanded `TestSurfaceSwapchain` block exercises the same public contracts
with additional boundary cases:

- invalid, unsupported, and foreign descriptors;
- image replacement and old-generation invalidation;
- already-signaled acquire failure without partial state;
- legacy compatibility helpers using the same owner state;
- owner shutdown with externally retained object references.

The complete maintained executable passed in both the `fallback-strict` and
`sanitizers` profiles.

## Recorded Gates

Authoritative records:

- `Evidence/gate-strict-debug.json`: passed at
  `2026-07-26T13:10:55+00:00`;
- `Evidence/gate-strict-release.json`: passed at
  `2026-07-26T13:11:08+00:00`;
- `Evidence/gate-fallback-strict.json`: build and complete tests passed at
  `2026-07-26T13:11:41+00:00`;
- `Evidence/gate-sanitizers.json`: ASan/UBSan build and maintained tests passed
  at `2026-07-26T13:12:42+00:00`.

The sanitizer test command sets
`STONER_SKIP_OPTIONAL_DEFERRED_NATIVE=1` for accepted
`CR001-B08-F001`. Native Vulkan instance/device and offscreen triangle tests
still execute and pass.

## Native-Proof Boundary

This migration provides deterministic backend-neutral presentation contract
coverage. It does not create a native Vulkan surface, import native swapchain
images, or replace `FVulkanNativeContext`. Device diagnostics and the runtime
mode remain deterministic fallback, so no result in this packet can satisfy a
native presentation gate. Consolidation of the real visible facade remains
explicit B08 review work.
