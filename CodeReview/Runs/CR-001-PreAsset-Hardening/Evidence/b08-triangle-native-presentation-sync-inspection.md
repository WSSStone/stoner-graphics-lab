# B08-S04 Evidence: Triangle Native Presentation And Synchronization Inspection

Step: `B08-S04`.

Files inspected:

- `Demo/StonerDemo/Private/FStonerDemoApplication.cpp`
- `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanNativeContext.h`
- `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp`
- `Tests/TriangleDemoIntegrationTests.cpp`
- `Tests/VulkanNativeIntegrationTests.cpp`
- `specs/018-triangle-demo-integration/spec.md`
- `specs/018-triangle-demo-integration/data-model.md`
- `specs/018-triangle-demo-integration/contracts/triangle-demo-runtime-contract.md`

Requirement evidence:

- `specs/018-triangle-demo-integration/spec.md:124` requires each drawable
  frame to process events, acquire, prepare commands, submit, coordinate
  completion, and present in deterministic order.
- `specs/018-triangle-demo-integration/spec.md:125` requires frame resources
  and presentation images not to be reused while prior work owns them.
- `specs/018-triangle-demo-integration/spec.md:126` requires non-zero size
  changes and stale-presentation conditions to refresh presentation-dependent
  state before rendering resumes.
- `specs/018-triangle-demo-integration/spec.md:127` requires zero drawable
  size to keep event processing while skipping draw submission and presentation.
- `specs/018-triangle-demo-integration/spec.md:128` requires rendering to
  resume after a valid drawable size returns without restart or invalid
  presentation-dependent resources.
- `specs/018-triangle-demo-integration/data-model.md:174` states presentation
  completion signal is selected by acquired image index, not frame slot.
- `specs/018-triangle-demo-integration/contracts/triangle-demo-runtime-contract.md:145`
  through `specs/018-triangle-demo-integration/contracts/triangle-demo-runtime-contract.md:163`
  define the visible frame order and resize/recovery contract.

Finding evidence:

- `Demo/StonerDemo/Private/FStonerDemoApplication.cpp:231` through
  `Demo/StonerDemo/Private/FStonerDemoApplication.cpp:247` only calls
  `PrepareVisibleTriangle` during initialization when the initial drawable
  width and height are non-zero.
- `Demo/StonerDemo/Private/FStonerDemoApplication.cpp:252` through
  `Demo/StonerDemo/Private/FStonerDemoApplication.cpp:255` leaves
  `PresentationState.bInitialized=false` and generation zero when visible
  initialization starts with a zero drawable extent.
- `Demo/StonerDemo/Private/FStonerDemoApplication.cpp:299` through
  `Demo/StonerDemo/Private/FStonerDemoApplication.cpp:314` sets
  `PresentationState.bInitialized=true` when a paused demo sees a non-zero
  drawable extent.
- `Demo/StonerDemo/Private/FStonerDemoApplication.cpp:378` through
  `Demo/StonerDemo/Private/FStonerDemoApplication.cpp:393` calls
  `NotifyDrawableExtent(Width, Height)` before checking
  `PresentationState.bInitialized`, causing startup-zero recovery to choose
  `RecreateVisiblePresentation` instead of first `PrepareVisibleTriangle`.
- `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp:1533` through
  `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp:1541` requires saved
  visible shader paths before recreation and returns `InvalidState` when a
  prior visible prepare never happened.
- `Tests/TriangleDemoIntegrationTests.cpp:125` through
  `Tests/TriangleDemoIntegrationTests.cpp:152` covers the synthetic
  presentation recovery timer state machine but not visible native startup-zero
  first preparation.

Synchronization evidence:

- `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp:1416` through
  `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp:1447` waits the
  selected frame-slot fence, acquires an image, records acquired image index
  and frame slot, rejects overlapping acquired frames, and binds the acquired
  image framebuffer into the command binding.
- `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp:1454` through
  `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp:1497` resets the
  selected slot fence, submits waiting on the frame-slot acquire semaphore,
  signals `VisibleRenderFinished[Bindings.ImageIndex]`, and presents waiting
  on that same image-indexed semaphore.
- `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp:738` through
  `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp:769` abandons an
  acquired visible frame and clears acquired-frame state for failure paths.
- `Tests/VulkanNativeIntegrationTests.cpp:70` through
  `Tests/VulkanNativeIntegrationTests.cpp:89` cover synthetic native visible
  acquire-suboptimal, record, and submit-after-fence-reset failure lifecycle.

Finding recorded:

- `CR001-B08-F003`: Visible startup-zero recovery selects recreate before first
  presentation prepare.
- Severity: S2.
- Disposition: Accepted.

Next step:

- Fix `CR001-B08-F003` by separating presentation lifecycle notification from
  successful resource initialization, then add deterministic coverage for the
  startup-zero first-prepare branch without requiring a real visible window.
