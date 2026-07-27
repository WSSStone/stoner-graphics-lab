# B05-S13: Native Context Execution Inspection

## Scope

This packet inspected one Feature 011/012 native execution responsibility
domain. Primary files:

- `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp`
- `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanNativeContext.h`
- `Tests/VulkanNativeIntegrationTests.cpp`
- `Tests/DeferredNativeIntegrationTests.cpp`
- `Tests/TriangleDemoIntegrationTests.cpp`

Supporting context came from Feature 011/012 spec, contracts, quickstarts, and
the prior B05 command, queue, shader, and pipeline inspection/verification
packets. No production source or maintained test changed.

## Authority And Invariants

Feature 011 requires real runtime submission when available, explicit failure
results, consistent queue/completion observability, rejection of invalid
command/submission states, and no usable partial state after invalid
transitions. Feature 012 requires native shader/pipeline ownership to remain
explicit and owner-safe.

The inspected native context covers two execution paths:

- offscreen native triangle validation using one command buffer/fence;
- visible swapchain acquisition, command recording, queue submission, and
  presentation through `AcquireVisibleFrame`, `DrawVisibleFrame`, and
  `SubmitAndPresentVisibleFrame`.

## Finding

### CR001-B05-F013 - S2 Accepted

`AcquireVisibleFrame` treats `VK_SUBOPTIMAL_KHR` as `ResizeRequired` but updates
`AcquiredImageIndex`, `AcquiredFrameSlot`, and `bFrameAcquired=true` first.
`DrawVisibleFrame` returns immediately for any non-success acquire result, so a
suboptimal-but-acquired image remains internally acquired with no submit,
present, release, or recreate handoff. The next acquire is rejected by the
`bFrameAcquired` guard.

`SubmitAndPresentVisibleFrame` resets the frame-slot fence before
`vkQueueSubmit`. If submit fails after the reset, the code clears
`bFrameAcquired` and returns `Failed`, but the fence remains unsignaled. The
next acquire for that frame slot waits on the unsignaled fence until timeout,
turning a failed submit into a persistent frame-slot poison.

Both paths violate the Feature 011 requirement that failed submissions and
invalid transitions avoid partially published state and preserve consistent
completion observation. Existing tests cover successful visible/native paths,
runtime-unavailable diagnostics, deferred failure lifecycle, and triangle demo
injected high-level failures, but they do not inject `VK_SUBOPTIMAL_KHR` acquire
or post-reset submit failure against this state machine.

## Non-Finding Notes

- `DestroyFrameResources` and `Shutdown` release current offscreen, visible, and
  RHI-owned native pipeline/shader resources in a plausible device-safe order.
- RHI-owned graphics and compute native pipelines added in B05-S11 have
  rollback and explicit invalidation coverage verified in B05-S12.
- The formal `tests` gate still fails only on known Feature 019 deferred native
  readback checks. That remains a B08 issue, not a new B05 finding.

## Handoff To B05-S14

B05-S14 should repair F013 with one visible native frame-state migration:

1. represent acquired image ownership explicitly, including suboptimal acquire;
2. either continue to submit/present a suboptimal acquire or transfer directly
   into a recreate path that does not leave `bFrameAcquired` set;
3. on submit failure after fence reset, restore or recreate a signalable
   completion state so the next frame slot cannot block until timeout;
4. add deterministic tests or a small injectable native-frame adapter for
   suboptimal acquire and post-reset submit failure;
5. keep the existing successful visible-frame and native pipeline tests passing.

No build, test, remote CI, debugger, or network operation was required for this
inspection packet.

