#pragma once

#include "Core/CoreMinimal.h"
#include "FVulkanLabPresentationPolicy.h"

#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
#include "FVulkanStruct.h"
#endif

namespace Stoner::Backend::Vulkan::Private
{

enum class EVulkanLabCapabilityQueryMechanism
{
    Unavailable,
    Core11,
    KHRProperties2
};

struct FVulkanLabCapabilityQueryFacts
{
    // This is the effective API version negotiated for the selected
    // instance/device pair.  The native caller must cap it to both actual
    // instance negotiation and selected-device applicability; this helper
    // never infers it from the loader's maximum version.
    Stoner::Core::uint32 NegotiatedApiVersion = 0;
    bool bKHRGetPhysicalDeviceProperties2Enabled = false;
    bool bSurfaceMaintenance1Enabled = false;
    // Query precedes logical-device creation: this is the required extension
    // requested for that creation, not evidence that a device enabled it.
    // Query also checks its selected-device advertisement independently.
    bool bKHRSwapchainRequested = false;
};

class FVulkanLabCapabilityQuery final
{
public:
    [[nodiscard]] static constexpr Stoner::Core::uint32 MakeApiVersion(
        Stoner::Core::uint32 Major,
        Stoner::Core::uint32 Minor,
        Stoner::Core::uint32 Patch = 0) noexcept
    {
        return (Major << 22U) | (Minor << 12U) | Patch;
    }

    [[nodiscard]] static EVulkanLabCapabilityQueryMechanism
    SelectQueryMechanism(const FVulkanLabCapabilityQueryFacts& Facts) noexcept;

#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
    [[nodiscard]] static FVulkanLabPresentationCapabilityObservation Query(
        VkInstance Instance,
        VkPhysicalDevice PhysicalDevice,
        const FVulkanLabCapabilityQueryFacts& Facts) noexcept;
#else
    [[nodiscard]] static FVulkanLabPresentationCapabilityObservation Query(
        const FVulkanLabCapabilityQueryFacts& Facts) noexcept;
#endif
};

} // namespace Stoner::Backend::Vulkan::Private
