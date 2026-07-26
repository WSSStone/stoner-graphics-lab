# B03-S04 Command, Synchronization, And Swapchain Probes

## Purpose

This evidence reproduces public RHI compatibility-path behavior without
modifying repository production or test sources. The probe includes the
maintained `RHICoreTests.cpp` mock acceptance implementation in one standalone
translation unit, then exercises the public interfaces through base
references.

The retained source is:

`Evidence/Probes/b03-command-sync-swapchain-probe.cpp`

## Command

```text
clang++ -std=c++20 \
  -I. \
  -ISource/Core/Public \
  -ISource/RHI/Public \
  -ITests \
  Evidence/Probes/b03-command-sync-swapchain-probe.cpp \
  -o /tmp/cr001_b03_s04_probe

/tmp/cr001_b03_s04_probe
```

## Observed Output

```text
surface_invalid_desc_result=0 object=1
acquire_result=1 swapchain_state=1 image_index=0
present_result=1 semaphore_state=2 swapchain_state=1
clear_overload_result=0 legacy_called=1
partial_wait_result=4 first_wait_state=2 command_state=2 submitted_count=0
failed_signal_result=1 command_state=3 submitted_count=1 fence_signaled=0
```

The relevant enum values are:

- `ERHIResult::Success = 0`
- `ERHIResult::InvalidState = 1`
- `ERHIResult::NotReady = 4`
- `ERHISwapchainState::Acquired = 1`
- `ERHISemaphoreState::Consumed = 2`
- `ERHICommandBufferState::Completed = 2`
- `ERHICommandBufferState::Submitted = 3`

## Interpretation

1. Calling the surface-aware device overload through `IRHIDevice&` with a
   null surface and an invalid zero-extent descriptor returns `Success` and a
   non-null headless swapchain.
2. Acquiring with an already-signaled semaphore returns `InvalidState`, but
   the swapchain has already transitioned to `Acquired` and exposed image
   index zero.
3. Presenting the wrong image returns `InvalidState`, but the wait semaphore
   has already transitioned to `Consumed`.
4. A legacy command buffer that implements only the old render-pass overload
   returns `Success` for the clear-value overload, including null render-pass
   and framebuffer arguments, without observing the clear values.
5. A mock queue submission with one ready and one unready wait semaphore
   returns `NotReady` after consuming the first semaphore.
6. A mock queue submission with an already-signaled output semaphore returns
   `InvalidState` after marking the command buffer `Submitted` and incrementing
   the submitted count, while leaving the completion fence unsignaled.

The probe exits zero because it records the current behavior rather than
asserting the repaired contract. B03-S06 must use a verdict-bearing parent
versus current probe.
