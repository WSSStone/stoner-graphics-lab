# Contract: Feature 027 RHI Operation Matrix

## Frozen Baseline

This inventory is frozen from `Source/RHI/Public/RHI/IRHI*.h` at Feature 027
planning time. Every listed method is semantically applicable to Metal. Query
methods must return truthful state; mutating methods must execute natively or
return `Unsupported` only when the invocation exceeds a capability published by
the selected device. An implementation-progress `Unsupported` is forbidden.

Virtual destructors are covered by lifecycle and ownership tests rather than a
separate call row. Overloads are counted independently by the verifier.

| Interface | Frozen public operations |
|---|---|
| `IRHIDevice` | `GetState`, `GetCapabilities`, `IsActive`, `GetRuntimeMode`, `GetRuntimeSnapshot`, `Shutdown`, `CreateCommandQueue`, `CreateCommandBuffer`, `CreateFence`, `CreateSemaphore`, both `CreateSwapchain` overloads, `CreateBuffer`, `UploadBuffer`, `CreateTexture`, `UploadTexture`, `CreateSampler`, `CreateShaderModule`, `CreatePipelineLayout`, `CreateDescriptorSet`, `CreateGraphicsPipeline`, `CreateComputePipeline`, `CreateRenderPass`, `CreateFramebuffer`, `CreatePresentationSurface` |
| `IRHICommandBuffer` | `GetState`, `GetCompatibleQueueType`, `GetRecordedCommandCount`, `Begin`, `End`, `Reset`, `RecordDraw`, both `RecordDrawIndexed` overloads, `RecordDispatch`, `BindGraphicsPipeline`, `BindComputePipeline`, both `RecordBarrier` overloads, `RecordBufferCopy`, `RecordTextureCopy`, `RecordLayoutTransition`, both `BeginRenderPass` overloads, `EndRenderPass`, `BindVertexBuffer`, `BindIndexBuffer`, `BindDescriptorSet`, `RecordTextureToBufferCopy`, `SetViewport`, `SetScissor` |
| `IRHICommandQueue` | `GetQueueType`, `GetSubmittedCommandBufferCount`, `Submit`, `WaitIdle` |
| `IRHIFence` | `GetState`, `IsSignaled`, `Wait`, `Reset`, `Signal` |
| `IRHISemaphore` | `GetState`, `IsSignaled`, `Signal`, `Consume`, `Reset` |
| `IRHISwapchain` | `GetState`, `GetFrameCount`, `GetCurrentFrameIndex`, both `AcquireNextFrame` overloads, both `Present` overloads, `GetImage`, `GetGeneration` |
| `IRHIBuffer` | `GetDesc`, `GetSizeInBytes`, `GetUsage`, `GetLifecycleState`, `Invalidate`, `Upload` |
| `IRHITexture` | `GetDesc`, `GetDimension`, `GetFormat`, `GetUsage`, `GetLifecycleState`, `Invalidate` |
| `IRHISampler` | `GetDesc`, `GetLifecycleState`, `Invalidate` |
| `IRHIShaderModule` | `GetDesc`, `GetStage`, `GetLifecycleState`, `Invalidate` |
| `IRHIPipelineLayout` | `GetDesc`, `GetSetCount`, `FindBinding`, `GetLifecycleState`, `Invalidate` |
| `IRHIDescriptorSet` | `GetSetIndex`, `GetPipelineLayout`, `GetBoundResourceKind`, `GetBoundResourceCount`, `GetLifecycleState`, `UpdateBuffer`, `UpdateTexture`, `UpdateSampler`, `UpdateCombinedTextureSampler`, `Invalidate` |
| `IRHIGraphicsPipeline` | `GetDesc`, `GetPipelineLayout`, `GetLifecycleState`, `Invalidate` |
| `IRHIComputePipeline` | `GetDesc`, `GetPipelineLayout`, `GetLifecycleState`, `Invalidate` |
| `IRHIRenderPass` | `GetDesc`, `GetAttachmentCount`, `GetAttachment`, `GetLifecycleState`, `Invalidate` |
| `IRHIFramebuffer` | `GetDesc`, `GetRenderPass`, `GetWidth`, `GetHeight`, `GetAttachmentCount`, `GetLifecycleState`, `Invalidate` |
| `IRHIPresentationSurface` | `GetDesc`, `IsValid`, `Invalidate` |

## Classification And Change Rule

The generated validation matrix has exactly one row per overload with interface,
signature digest, requirement, Metal test, Vulkan regression, status, capability
predicate, and evidence path. Initial status is `required-native`; a row may
become `capability-limited` only with a backend-neutral capability field, a real
device snapshot, and a negative conformance test. No row may be omitted.

Before any Metal implementation task, the verifier extracts the headers and
compares names, overload counts, and normalized signature digests with this
contract. A public RHI change blocks later tasks until this file, the generated
matrix seed, affected backends, and tests are updated together.
