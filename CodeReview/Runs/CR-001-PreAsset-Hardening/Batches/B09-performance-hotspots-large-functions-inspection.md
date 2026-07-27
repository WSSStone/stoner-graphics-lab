# B09-S10 Inspection: Performance Hotspots And Large Functions

## Scope

Inspected production-code size and hotspot candidates for cross-cutting maintainability and performance risk. This step intentionally focused on production code, not test file size.

Files inspected:

- `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp`
- `Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp`
- `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- `Source/Application/Private/FWorld.cpp`
- `Source/Renderer/Private/FRenderGraphCompiler.cpp`

## Static Metrics

Largest production files by local line count:

- `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp`: 2129 LOC
- `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`: 1756 LOC
- `Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp`: 1737 LOC
- `Source/Application/Private/FWorld.cpp`: 776 LOC
- `Source/Backend/Vulkan/Private/FVulkanCommandBuffer.cpp`: 694 LOC
- `Demo/StonerDemo/Private/FStonerDemoApplication.cpp`: 532 LOC
- `Source/Renderer/Private/FRenderGraphCompiler.cpp`: 464 LOC
- `Source/Renderer/Private/FDeferredFrameExecutor.cpp`: 435 LOC

Largest brace-block candidates:

- `FVulkanNativeOffscreenSession.cpp:1047-1654`: roughly 608 lines inside deferred native validation execution.
- `FVulkanNativeOffscreenSession.cpp:1342-1630`: roughly 289-line nested `ExecuteConvention` lambda.
- `FVulkanNativeContext.cpp:1671-2018`: roughly 348 lines inside native graphics pipeline creation.
- `FVulkanNativeContext.cpp:1219-1414`: roughly 196 lines inside visible triangle preparation.
- `FVulkanNativeContext.cpp:923-1107`: roughly 185 lines inside offscreen triangle execution.

## Positive Evidence

- The large Vulkan native paths already have significant behavioral coverage from B05/B08/B09 findings and native integration tests. No current S0-S2 correctness finding was discovered by this size/hotspot inspection.
- Renderer and Application files below the top Vulkan cluster are comparatively smaller and scoped around declared responsibilities; `FWorld` centralizes ECS state but does not contain similarly large individual execution bodies.
- Previous B09 work already accepted `CR001-B09-F003` for focused test execution debt; this inspection did not duplicate that finding.

## Finding

Accepted `CR001-B09-F005` as S3: `Native deferred validation is concentrated in oversized execution functions`.

The current implementation of `FVulkanNativeOffscreenSession::Execute()` combines resource creation, descriptor setup, pipeline setup, command recording, readback mapping, probe decoding, and oracle composition in one large method with a large nested lambda. This is not an immediate behavior failure, but it is a real pre-Asset maintainability risk because future asset, streaming, deferred, and backend-validation changes will keep touching this same monolithic path.

## Recommended Fix Direction

Do not fix this inside B09-S10. A low-risk follow-up should split the native deferred validation path along existing responsibilities:

- resource/image/buffer setup;
- descriptor and pipeline setup;
- command recording for each convention;
- readback mapping/decoding;
- probe expectation/oracle construction;
- report finalization.

Keep the existing native validation tests as the behavioral gate while splitting. If this remains out of scope for CR-001 closeout, defer `CR001-B09-F005` explicitly to a post-CR native validation refactor phase.
