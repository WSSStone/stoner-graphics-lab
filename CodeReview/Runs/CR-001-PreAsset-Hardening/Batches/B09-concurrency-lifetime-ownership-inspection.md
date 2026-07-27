# B09-S07 Inspection: Concurrency, Lifetime, And Ownership Duplication

## Scope

Inspected representative cross-cutting concurrency and ownership/lifetime paths, focusing on objects with explicit owner tokens, RAII reservations, global identity allocation, and thread-sensitive logging.

Files inspected:

- `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp`
- `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanNativeContext.h`
- `Source/Backend/Vulkan/Private/FVulkanDescriptorPool.cpp`
- `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDescriptorPool.h`
- `Source/Application/Private/FWorld.cpp`
- `Source/Core/Private/FLog.cpp`
- `Source/Core/Public/Core/FLog.h`
- `Tests/VulkanNativeIntegrationTests.cpp`
- `Tests/VulkanBackendTests.cpp`
- `Tests/LoggingAssertionTests.cpp`

## Positive Evidence

- Native Vulkan shader and pipeline ownership is tokenized in `FVulkanNativeContext::FImpl` through `OwnedShaderModules`, `OwnedPipelines`, `NextOwnedShaderToken`, and `NextOwnedPipelineToken`. Explicit destroy calls ignore zero/unknown tokens, remove known tokens exactly once, update live-object counters, and shutdown destroys owned pipeline resources before shader modules before device teardown.
- Pipeline creation failure paths consistently call `DestroyOwnedPipelineResources(Resources)` before returning, and `PublishOwnedPipeline()` destroys unpublished resources when token allocation or map insertion fails.
- `FVulkanDescriptorReservation` is move-only, releases through `Reset()`, and moves the shared owner out before calling `ReleaseReservation()`, preventing repeated release through a stale active handle. `FVulkanDescriptorPool::Acquire()` rejects foreign owners, exhausted pools, invalidated pools, and already-active output reservations.
- `FWorld` uses an atomic relaxed counter only for process-local unique world ids; ordering is not part of the contract, so relaxed ordering is sufficient for the current identity use.
- `FLog` uses atomic severity/handler state and a mutex around message/output mutation. Existing tests exercise concurrent assertion-handler replacement, concurrent log emission line integrity, and concurrent runtime threshold reads/writes.

## Test Evidence

- `Tests/VulkanNativeIntegrationTests.cpp` covers native context shutdown to zero live objects, owner-safe native shader runtime enablement, shader execution-model mismatch rejection before runtime object creation, native graphics/compute pipeline retention, explicit shader/pipeline invalidation, device shutdown invalidation, and native runtime teardown.
- `Tests/VulkanBackendTests.cpp` covers descriptor reservation factory-only invariants, fixed-capacity allocation, exhaustion, invalidation returning exactly one reservation, and capacity reuse after reservation release.
- `Tests/LoggingAssertionTests.cpp` covers concurrent assertion handler replacement, non-interleaved multi-thread logging, and valid threshold values under concurrent access.

## Finding

No new S0-S2 finding was accepted in this inspection. The reviewed ownership paths have focused implementation guards and regression coverage. No S3 finding was added because the remaining concurrency surface is intentionally small and aligned with the current single-threaded renderer/application execution model.

## Follow-Up Notes

Future async asset loading and streaming features should re-open this area once cross-thread resource lifetime becomes part of the runtime contract. At that point, native-context and descriptor ownership should be re-audited against explicit thread-safety requirements rather than the current single-thread assumption.
