# B08-S04: Triangle Native Presentation And Synchronization Inspection

## Scope

Inspected Feature 018 visible native frame ordering, zero-drawable recovery,
swapchain presentation synchronization, and related deterministic/native test
coverage.

Production files read:

- `Demo/StonerDemo/Private/FStonerDemoApplication.cpp`
- `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanNativeContext.h`
- `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp`

Supporting files read:

- `Tests/TriangleDemoIntegrationTests.cpp`
- `Tests/VulkanNativeIntegrationTests.cpp`
- `specs/018-triangle-demo-integration/spec.md`
- `specs/018-triangle-demo-integration/data-model.md`
- `specs/018-triangle-demo-integration/contracts/triangle-demo-runtime-contract.md`

## Requirements Checked

- Feature 018 FR-009: visible frames process events before acquire, command
  preparation, submission, completion coordination, and presentation.
- Feature 018 FR-010: frame resources and presentation images are not reused
  while owned by incomplete prior work.
- Feature 018 FR-011: non-zero size changes and stale presentation conditions
  refresh presentation-dependent state before rendering resumes.
- Feature 018 FR-012: zero drawable size pauses drawing while preserving event
  processing.
- Feature 018 FR-013: rendering resumes after a valid drawable size returns
  without restarting or retaining invalid presentation-dependent resources.
- Feature 018 FR-015: non-recoverable frame failure releases owned resources in
  dependency-safe order.
- Runtime contract: presentation completion signal is selected by acquired
  image index, not frame slot.
- Runtime contract: successful recreation returns to `Running`; failed
  recreation becomes the primary failure and begins shutdown.

## Finding

### `CR001-B08-F003`

`FStonerDemoApplication::NotifyDrawableExtent` sets
`PresentationState.bInitialized` when a paused visible demo observes a non-zero
drawable extent. `RunVisible` calls that notification before choosing whether
to call the first visible `PrepareVisibleTriangle` or
`RecreateVisiblePresentation`.

If the visible window starts with a zero drawable extent, `Initialize` leaves
`PresentationState.bInitialized` false and does not call
`PrepareVisibleTriangle`. On the first non-zero extent, `NotifyDrawableExtent`
sets the flag true, so `RunVisible` selects `RecreateVisiblePresentation`.
The Vulkan native context recreate path requires shader paths saved by an
earlier successful `PrepareVisibleTriangle`, so the startup-zero resume path can
fail with `InvalidState` instead of creating first-generation presentation
resources.

Impact: a visible demo launched minimized or with an initially zero drawable
surface can fail recovery after the drawable becomes valid. This violates the
Feature 018 pause/resume contract and makes real-window validation sensitive to
startup window timing.

Status: Accepted, S2.

## Non-Findings

- `RunVisible` polls window and input events before close handling and frame
  acquisition.
- Zero drawable extent skips acquire, submit, and present while sleeping
  briefly before polling again.
- `AcquireVisibleFrame` waits for the selected frame slot fence before
  acquisition and rejects a new acquire while another frame is already acquired.
- `SubmitAndPresentVisibleFrame` signals `VisibleRenderFinished` by acquired
  image index and presents using that same image-indexed semaphore.
- Native submit failure after fence reset recreates a signaled frame-slot fence,
  abandons the acquired image, and advances the frame slot for reuse.
- Native present out-of-date, suboptimal present, and suboptimal acquire are
  surfaced as `ResizeRequired` after acquired-frame state is cleared.
- Existing tests cover native context acquired-state release for synthetic
  acquire, record, and submit failure points, but they do not cover the demo
  visible loop startup-zero branch recorded in `CR001-B08-F003`.

## Observation For Future Review

`RunVisible` has no public native-context abandon call if renderer command
recording fails after `AcquireVisibleFrame` succeeds. This was not recorded as
a finding in this step because the failure is non-recoverable in the demo loop
and `Run` proceeds to `Shutdown`; however, the private helper
`FVulkanNativeContext::DrawVisibleFrame` does abandon on record failure. If a
future visible loop tries to recover from renderer recording errors, the public
native frame API should expose an explicit abandon/release operation or use a
scope guard.
