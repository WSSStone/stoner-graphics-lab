#pragma once

#include "FVulkanLabPresentationPolicy.h"

#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE

#include "FVulkanStruct.h"

namespace Stoner::Backend::Vulkan::Private
{

// This result belongs to the private startup boundary.  A VkResult is only
// meaningful when its corresponding attempt flag is true; VK_NOT_READY is
// the sentinel for an attempt that did not run.
struct FVulkanLabDeviceStartupResult
{
    VkDevice Device = VK_NULL_HANDLE;
    FVulkanLabPresentationPolicySelection Selection;
    VkResult CreationResult = VK_NOT_READY;
    VkResult OptionalCreationResult = VK_NOT_READY;
    VkResult FallbackCreationResult = VK_NOT_READY;
    bool bBaselineCreationAttempted = false;
    bool bOptionalCreationAttempted = false;
    bool bFallbackCreationAttempted = false;
    bool bRetryConsumed = false;
    bool bUsedPresentEntryPointResolved = false;
    bool bMandatoryPresentationEntryPointMissing = false;
    bool bInputRejected = false;
    bool bSucceeded = false;
};

class FVulkanLabDeviceStartup final
{
public:
    [[nodiscard]] static FVulkanLabDeviceStartupResult Create(
        VkPhysicalDevice PhysicalDevice,
        const VkDeviceCreateInfo& BaseCreateInfo,
        const FVulkanLabPresentationPolicyRequest& PolicyRequest,
        const FVulkanLabPresentationCapabilityObservation& Observation)
        noexcept;
};

} // namespace Stoner::Backend::Vulkan::Private

#endif
