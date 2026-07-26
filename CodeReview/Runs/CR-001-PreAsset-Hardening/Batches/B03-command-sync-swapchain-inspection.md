# B03-S04: Commands, Queues, Synchronization, And Swapchain Inspection

## Inspection Budget

The inspection covered one RHI lifecycle responsibility domain and eight
production headers, totaling 453 lines:

1. `Source/RHI/Public/RHI/IRHIDevice.h`
2. `Source/RHI/Public/RHI/IRHICommandBuffer.h`
3. `Source/RHI/Public/RHI/IRHICommandQueue.h`
4. `Source/RHI/Public/RHI/IRHIFence.h`
5. `Source/RHI/Public/RHI/IRHISemaphore.h`
6. `Source/RHI/Public/RHI/IRHISwapchain.h`
7. `Source/RHI/Public/RHI/IRHIPresentationSurface.h`
8. `Source/RHI/Public/RHI/FRHISwapchainDesc.h`

Supporting evidence included Features 007 and 018 specifications, data models,
contracts and tasks, `Tests/RHICoreTests.cpp`, focused symbol/call-site
searches, and a standalone C++20 probe. A targeted symbol search surfaced the
current Vulkan device's legacy one-argument swapchain definition; it was not
line-by-line inspected and remains in B04's backend scope. No production or
maintained test implementation changed.

## Requirement Mapping

- `007-FR-003` through `007-FR-007`: command buffers expose the specified
  idle, recording, completed, submitted and resettable states; queue
  submission accepts compatible completed work and provides wait-idle
  completion.
- `007-FR-008`, `007-FR-009`, and `007-SC-003`: fences and semaphores expose
  backend-neutral transitions, but failed composite operations are not
  failure-atomic in the default swapchain adapters or mock queue acceptance
  path.
- `007-FR-010`: the legacy headless swapchain contract and its success,
  invalid-state, resize-required and unavailable outcomes are covered.
- `018-FR-003`, `018-FR-008`, `018-FR-009`, `018-T008`, `018-T012`, and
  `018-T014`: explicit surface/swapchain, clear-value, and synchronized frame
  contracts exist in public vocabulary, but three compatibility defaults can
  report false success or return failure after observable state mutation.
- The Feature 018 runtime contract requires surface-compatible extent and
  format, imported images, clear values, acquire signaling, queue waits and
  signals, completion fences, and presentation waits to form one valid frame
  order. Silent headless fallback and partial synchronization transitions
  violate that boundary.

## Reproduction

The retained standalone probe produced:

```text
surface_invalid_desc_result=0 object=1
acquire_result=1 swapchain_state=1 image_index=0
present_result=1 semaphore_state=2 swapchain_state=1
clear_overload_result=0 legacy_called=1
partial_wait_result=4 first_wait_state=2 command_state=2 submitted_count=0
failed_signal_result=1 command_state=3 submitted_count=1 fence_signaled=0
```

Full command, enum decoding, source, and interpretation are retained in
`Evidence/b03-command-sync-swapchain-probes.md` and
`Evidence/Probes/b03-command-sync-swapchain-probe.cpp`.

## Findings

### CR001-B03-F003 - Accepted S2

`IRHIDevice::CreateSwapchain(surface, desc)` ignores the surface, extent,
format and VSync request, then forwards only `FramesInFlight` to the legacy
headless factory. A null surface and invalid descriptor therefore return a
successful object through a legacy implementation.

### CR001-B03-F004 - Accepted S2

Composite synchronization operations can return failure after committing
partial state. The default synchronized swapchain overloads can acquire an
image before failing to signal or consume a semaphore before rejecting
presentation. The mock queue can consume an early wait dependency before a
later wait fails, or mark work submitted before output signaling fails.

### CR001-B03-F005 - Accepted S2

The clear-value `BeginRenderPass` compatibility overload discards the supplied
clear values and delegates to the legacy overload. A legacy implementation can
therefore return `Success` while performing no contracted clear semantics.

## Confirmed Strengths

- Queue submission has explicit wait-semaphore, signal-semaphore and completion
  fence parameters; the previously suspected missing synchronization
  vocabulary is not a finding.
- The public command lifecycle enumerates every Feature 007 state and the mock
  tests cover double begin, end without begin, record after end, submit while
  recording, incompatible queues, submitted reset rejection and wait-idle
  reset eligibility.
- Fence nonblocking wait, timed wait, signal, waited and reset behavior is
  explicit. Semaphore signal, consume, double-signal rejection and reset are
  explicit.
- The headless swapchain state machine rejects double acquire, present without
  acquire, stale frame indices and repeated present, and distinguishes resize
  and unavailable outcomes.
- Newer command capabilities that cannot be emulated return `Unsupported`;
  the clear-value overload is the exception because it currently reports
  success while losing requested semantics.
- Public RHI interfaces expose only Core/RHI types and no Vulkan, GLFW, or
  platform-native handles.

## Coverage Gaps

- Maintained RHI core tests do not invoke the surface-aware device overload
  through `IRHIDevice&`.
- Synchronized acquire/present overloads have no maintained direct call site
  or negative-path test.
- Queue synchronization tests cover only one fully valid submission; they do
  not cover multiple waits, null dependencies, duplicate/already-signaled
  outputs, or failure-state invariants.
- Explicit render-pass clears are tested only on implementations that override
  the new overload, so the semantic-loss compatibility default is invisible.

## B03-S05 Fix Packet

The next packet may repair only these three Accepted findings:

1. Make surface-aware swapchain creation fail closed unless an implementation
   explicitly supports and validates the surface plus complete descriptor;
   add a base-reference regression test.
2. Make queue and swapchain synchronization failure paths validate all
   dependencies before observable state transition, or report `Unsupported`
   where the base interface cannot provide a failure-atomic composition.
3. Make explicit clear values fail closed unless the implementation handles
   them; preserve the existing overriding mock and Vulkan behavior.

The fix must add happy-path and negative-path state assertions and must not
inspect or refactor Vulkan backend internals reserved for B04/B05.

