#include "FVulkanLabCapabilityQuery.h"

#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>
#endif

namespace Stoner::Backend::Vulkan::Private
{

namespace
{

using Stoner::Core::uint32;

constexpr uint32 VulkanMajorShift = 22U;
constexpr uint32 VulkanMinorShift = 12U;
constexpr uint32 VulkanMajorMask = 0x7fU;
constexpr uint32 VulkanMinorMask = 0x3ffU;

[[nodiscard]] bool IsAtLeastVulkan11(uint32 Version) noexcept
{
    const uint32 Major = (Version >> VulkanMajorShift) & VulkanMajorMask;
    const uint32 Minor = (Version >> VulkanMinorShift) & VulkanMinorMask;
    return Major > 1U || (Major == 1U && Minor >= 1U);
}

#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE

constexpr const char* SwapchainMaintenance1Extension =
    "VK_EXT_swapchain_maintenance1";

constexpr uint32 MaxExtensionEnumerationAttempts = 3U;

[[nodiscard]] bool HasExtension(
    const std::vector<VkExtensionProperties>& Extensions,
    const char* Name) noexcept
{
    for (const VkExtensionProperties& Extension : Extensions)
    {
        if (std::strcmp(Extension.extensionName, Name) == 0)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] const char* VkResultName(VkResult Result) noexcept
{
    switch (Result)
    {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_NOT_READY: return "VK_NOT_READY";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_EVENT_SET: return "VK_EVENT_SET";
    case VK_EVENT_RESET: return "VK_EVENT_RESET";
    case VK_INCOMPLETE: return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    default: return "VK_ERROR_UNKNOWN";
    }
}

void SetNativeFailure(
    FVulkanLabPresentationCapabilityObservation& Observation,
    VkResult Result,
    const char* Operation) noexcept
{
    Observation.QueryState = EVulkanLabCapabilityQueryState::Failed;
    Observation.NativeQueryResult = static_cast<Stoner::Core::int32>(Result);
    try
    {
        Observation.QueryFailure = std::string(Operation) + " returned " +
            VkResultName(Result);
    }
    catch (...)
    {
        // Keep QueryState and NativeQueryResult authoritative if diagnostic
        // storage itself is exhausted.  A second string assignment here
        // could throw again inside this noexcept helper.
        Observation.QueryFailure.Clear();
    }
}

void SetQueryUnavailable(
    FVulkanLabPresentationCapabilityObservation& Observation,
    const char* Reason) noexcept
{
    Observation.QueryState = EVulkanLabCapabilityQueryState::Unavailable;
    Observation.NativeQueryResult = 0;
    try
    {
        Observation.QueryFailure = Reason;
    }
    catch (...)
    {
        Observation.QueryFailure.Clear();
    }
}

void SetLocalFailure(
    FVulkanLabPresentationCapabilityObservation& Observation,
    const char* Operation) noexcept
{
    Observation.QueryState = EVulkanLabCapabilityQueryState::Failed;
    // This failure is local input validation/allocation; no Vulkan call
    // returned a VkResult to report.
    Observation.NativeQueryResult = 0;
    try
    {
        Observation.QueryFailure = Operation;
    }
    catch (...)
    {
        Observation.QueryFailure.Clear();
    }
}

[[nodiscard]] bool EnumerateDeviceExtensions(
    VkPhysicalDevice PhysicalDevice,
    std::vector<VkExtensionProperties>& OutExtensions,
    FVulkanLabPresentationCapabilityObservation& Observation) noexcept
{
    for (uint32 Attempt = 0; Attempt < MaxExtensionEnumerationAttempts;
         ++Attempt)
    {
        uint32 Count = 0;
        const VkResult CountResult = vkEnumerateDeviceExtensionProperties(
            PhysicalDevice, nullptr, &Count, nullptr);
        if (CountResult != VK_SUCCESS && CountResult != VK_INCOMPLETE)
        {
            SetNativeFailure(
                Observation, CountResult,
                "vkEnumerateDeviceExtensionProperties(count)");
            return false;
        }
        if (CountResult == VK_INCOMPLETE &&
            Attempt + 1U < MaxExtensionEnumerationAttempts)
        {
            continue;
        }
        if (CountResult == VK_INCOMPLETE)
        {
            SetNativeFailure(
                Observation, CountResult,
                "vkEnumerateDeviceExtensionProperties(count)");
            return false;
        }

        if (Count == 0)
        {
            // Do not pass a null pProperties pointer as though it were a
            // storage buffer.  A second count query catches a list that grew
            // between the two calls without publishing uninitialized data.
            uint32 ConfirmedCount = 0;
            const VkResult ConfirmResult =
                vkEnumerateDeviceExtensionProperties(
                    PhysicalDevice, nullptr, &ConfirmedCount, nullptr);
            if (ConfirmResult == VK_SUCCESS && ConfirmedCount == 0)
            {
                OutExtensions.clear();
                return true;
            }
            if (ConfirmResult == VK_INCOMPLETE &&
                Attempt + 1U < MaxExtensionEnumerationAttempts)
            {
                continue;
            }
            if (ConfirmResult == VK_INCOMPLETE)
            {
                SetNativeFailure(
                    Observation, ConfirmResult,
                    "vkEnumerateDeviceExtensionProperties(recount)");
                return false;
            }
            if (ConfirmResult != VK_SUCCESS &&
                ConfirmResult != VK_INCOMPLETE)
            {
                SetNativeFailure(
                    Observation, ConfirmResult,
                    "vkEnumerateDeviceExtensionProperties(recount)");
                return false;
            }
            Count = ConfirmedCount;
            if (Count == 0)
            {
                SetNativeFailure(
                    Observation, VK_INCOMPLETE,
                    "vkEnumerateDeviceExtensionProperties(recount)");
                return false;
            }
        }

        // A driver supplied count is native input.  Keep the allocation
        // bounded and avoid a platform-dependent vector length_error.
        constexpr uint32 MaxReportedExtensionCount = 16'384U;
        if (Count > MaxReportedExtensionCount)
        {
            SetLocalFailure(
                Observation,
                "vkEnumerateDeviceExtensionProperties(count exceeds bound)");
            return false;
        }

        try
        {
            OutExtensions.resize(Count);
        }
        catch (const std::bad_alloc&)
        {
            SetLocalFailure(
                Observation,
                "vkEnumerateDeviceExtensionProperties(storage)");
            return false;
        }
        catch (const std::length_error&)
        {
            SetLocalFailure(
                Observation,
                "vkEnumerateDeviceExtensionProperties(storage)");
            return false;
        }

        const VkResult PropertiesResult =
            vkEnumerateDeviceExtensionProperties(
                PhysicalDevice, nullptr, &Count, OutExtensions.data());
        if (PropertiesResult == VK_SUCCESS)
        {
            if (Count <= OutExtensions.size())
            {
                return true;
            }
            if (Attempt + 1U < MaxExtensionEnumerationAttempts)
            {
                continue;
            }
            SetNativeFailure(
                Observation, VK_INCOMPLETE,
                "vkEnumerateDeviceExtensionProperties(properties)");
            return false;
        }
        if (PropertiesResult != VK_INCOMPLETE)
        {
            SetNativeFailure(
                Observation, PropertiesResult,
                "vkEnumerateDeviceExtensionProperties(properties)");
            return false;
        }
        if (Attempt + 1U >= MaxExtensionEnumerationAttempts)
        {
            SetNativeFailure(
                Observation, PropertiesResult,
                "vkEnumerateDeviceExtensionProperties(properties)");
            return false;
        }
    }

    SetNativeFailure(
        Observation, VK_INCOMPLETE,
        "vkEnumerateDeviceExtensionProperties(properties)");
    return false;
}

#endif

} // namespace

EVulkanLabCapabilityQueryMechanism
FVulkanLabCapabilityQuery::SelectQueryMechanism(
    const FVulkanLabCapabilityQueryFacts& Facts) noexcept
{
    // NegotiatedApiVersion is supplied by the native caller after it has
    // intersected the actual instance and selected-device applicability.  It
    // is intentionally not discovered from a loader-global maximum here.
    if (IsAtLeastVulkan11(Facts.NegotiatedApiVersion))
    {
        return EVulkanLabCapabilityQueryMechanism::Core11;
    }
    if (Facts.bKHRGetPhysicalDeviceProperties2Enabled)
    {
        return EVulkanLabCapabilityQueryMechanism::KHRProperties2;
    }
    return EVulkanLabCapabilityQueryMechanism::Unavailable;
}

#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE

FVulkanLabPresentationCapabilityObservation
FVulkanLabCapabilityQuery::Query(
    VkInstance Instance,
    VkPhysicalDevice PhysicalDevice,
    const FVulkanLabCapabilityQueryFacts& Facts) noexcept
{
    FVulkanLabPresentationCapabilityObservation Observation;
    if (Instance == VK_NULL_HANDLE || PhysicalDevice == VK_NULL_HANDLE)
    {
        SetLocalFailure(
            Observation,
            "FVulkanLabCapabilityQuery invalid native handle");
        return Observation;
    }

    const EVulkanLabCapabilityQueryMechanism Mechanism =
        SelectQueryMechanism(Facts);
    std::vector<VkExtensionProperties> Extensions;
    if (!EnumerateDeviceExtensions(
            PhysicalDevice, Extensions, Observation))
    {
        return Observation;
    }

    Observation.bExtensionAdvertised = HasExtension(
        Extensions, SwapchainMaintenance1Extension);
    Observation.bInstanceDependenciesAvailable =
        Mechanism != EVulkanLabCapabilityQueryMechanism::Unavailable &&
        Facts.bSurfaceMaintenance1Enabled && Facts.bKHRSwapchainRequested &&
        HasExtension(Extensions, "VK_KHR_swapchain");

    if (Mechanism == EVulkanLabCapabilityQueryMechanism::Unavailable)
    {
        SetQueryUnavailable(
            Observation,
            "no applicable Vulkan 1.1 or enabled KHR properties2 query path");
        return Observation;
    }

    if (!Observation.bExtensionAdvertised)
    {
        // The query path itself worked; there is simply no optional device
        // extension to feature-query on this selected device.
        Observation.QueryState = EVulkanLabCapabilityQueryState::Succeeded;
        return Observation;
    }

#if !defined(VK_EXT_swapchain_maintenance1)
    SetQueryUnavailable(
        Observation,
        "Vulkan headers have no swapchain maintenance1 feature structure");
    return Observation;
#else
    VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT MaintenanceFeatures{};
    MaintenanceFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT;
    VkPhysicalDeviceFeatures2 Features2{};
    Features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    Features2.pNext = &MaintenanceFeatures;

    PFN_vkGetPhysicalDeviceFeatures2 GetFeatures2 = nullptr;
    const char* FunctionName = "vkGetPhysicalDeviceFeatures2";
    if (Mechanism == EVulkanLabCapabilityQueryMechanism::Core11)
    {
#if defined(VK_VERSION_1_1)
        GetFeatures2 = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
            vkGetInstanceProcAddr(Instance, "vkGetPhysicalDeviceFeatures2"));
#else
        SetQueryUnavailable(
            Observation,
            "Vulkan headers cannot call the negotiated core 1.1 feature query");
        return Observation;
#endif
    }
    else
    {
        FunctionName = "vkGetPhysicalDeviceFeatures2KHR";
#if defined(VK_KHR_get_physical_device_properties2)
        GetFeatures2 = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
            vkGetInstanceProcAddr(
                Instance, "vkGetPhysicalDeviceFeatures2KHR"));
#else
        SetQueryUnavailable(
            Observation,
            "Vulkan headers cannot call the enabled KHR properties2 query");
        return Observation;
#endif
    }
    if (GetFeatures2 == nullptr)
    {
        try
        {
            Observation.QueryFailure = std::string(FunctionName) +
                " is unavailable for the negotiated query path";
        }
        catch (...)
        {
            Observation.QueryFailure.Clear();
        }
        Observation.QueryState = EVulkanLabCapabilityQueryState::Unavailable;
        return Observation;
    }

    GetFeatures2(PhysicalDevice, &Features2);
    Observation.bFeatureAdvertised =
        MaintenanceFeatures.swapchainMaintenance1 == VK_TRUE;
    Observation.QueryState = EVulkanLabCapabilityQueryState::Succeeded;
    return Observation;
#endif
}

#else

FVulkanLabPresentationCapabilityObservation
FVulkanLabCapabilityQuery::Query(
    const FVulkanLabCapabilityQueryFacts&) noexcept
{
    FVulkanLabPresentationCapabilityObservation Observation;
    Observation.QueryState = EVulkanLabCapabilityQueryState::Unavailable;
    try
    {
        Observation.QueryFailure =
            "Vulkan native support is disabled for this build";
    }
    catch (...)
    {
        Observation.QueryFailure.Clear();
    }
    return Observation;
}

#endif

} // namespace Stoner::Backend::Vulkan::Private
