#include "FVulkanLabDeviceStartup.h"

#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE

#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

namespace Stoner::Backend::Vulkan::Private
{

namespace
{

using Stoner::Core::uint32;

constexpr const char* SwapchainExtension = "VK_KHR_swapchain";
constexpr const char* OptionalMaintenanceExtension =
    "VK_EXT_swapchain_maintenance1";
constexpr const char* PromotedMaintenanceExtension =
    "VK_KHR_swapchain_maintenance1";
constexpr uint32 MaxCreateInfoExtensionNames = 4096U;
constexpr uint32 MaxCreateInfoPNextNodes = 64U;

[[nodiscard]] bool HasExtensionName(
    const VkDeviceCreateInfo& CreateInfo,
    const char* Name) noexcept
{
    if (CreateInfo.ppEnabledExtensionNames == nullptr)
    {
        return false;
    }
    for (uint32 Index = 0; Index < CreateInfo.enabledExtensionCount; ++Index)
    {
        const char* Candidate = CreateInfo.ppEnabledExtensionNames[Index];
        if (Candidate != nullptr && std::strcmp(Candidate, Name) == 0)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool HasAnyOptionalMaintenanceExtension(
    const VkDeviceCreateInfo& CreateInfo) noexcept
{
    return HasExtensionName(CreateInfo, OptionalMaintenanceExtension) ||
        HasExtensionName(CreateInfo, PromotedMaintenanceExtension);
}

[[nodiscard]] bool IsValidExtensionArray(
    const VkDeviceCreateInfo& CreateInfo) noexcept
{
    if (CreateInfo.enabledExtensionCount > MaxCreateInfoExtensionNames ||
        (CreateInfo.enabledExtensionCount != 0 &&
            CreateInfo.ppEnabledExtensionNames == nullptr))
    {
        return false;
    }
    for (uint32 Index = 0; Index < CreateInfo.enabledExtensionCount; ++Index)
    {
        if (CreateInfo.ppEnabledExtensionNames[Index] == nullptr)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool ContainsOptionalFeatureStruct(
    const VkDeviceCreateInfo& CreateInfo) noexcept
{
    const VkBaseInStructure* Node = reinterpret_cast<const VkBaseInStructure*>(
        CreateInfo.pNext);
    for (uint32 Count = 0; Node != nullptr && Count < MaxCreateInfoPNextNodes;
         ++Count)
    {
#if defined(VK_EXT_swapchain_maintenance1)
        if (Node->sType ==
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT)
        {
            return true;
        }
#endif
        Node = Node->pNext;
    }
    return Node != nullptr;
}

[[nodiscard]] bool IsValidBaseCreateInfo(
    const VkDeviceCreateInfo& CreateInfo) noexcept
{
    if (CreateInfo.sType != VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO ||
        CreateInfo.queueCreateInfoCount == 0 ||
        CreateInfo.pQueueCreateInfos == nullptr ||
        !IsValidExtensionArray(CreateInfo) ||
        !HasExtensionName(CreateInfo, SwapchainExtension) ||
        HasAnyOptionalMaintenanceExtension(CreateInfo) ||
        ContainsOptionalFeatureStruct(CreateInfo))
    {
        return false;
    }
    return true;
}

[[nodiscard]] FVulkanLabPresentationPolicySelection MakeRejectedSelection(
    const FVulkanLabPresentationPolicyCandidate& Candidate) noexcept
{
    FVulkanLabPresentationPolicySelection Selection;
    Selection.Reason = Candidate.Reason;
    Selection.bExtensionAdvertised = Candidate.bExtensionAdvertised;
    Selection.bFeatureAdvertised = Candidate.bFeatureAdvertised;
    Selection.bInstanceDependenciesAvailable =
        Candidate.bInstanceDependenciesAvailable;
    Selection.bEntryPointsResolved = false;
    Selection.NativeQueryResult = Candidate.NativeQueryResult;
    try
    {
        Selection.QueryFailure = Candidate.QueryFailure;
    }
    catch (const std::bad_alloc&)
    {
        // Scalar failure evidence remains authoritative.
    }
    return Selection;
}

void MarkEntryPointFailure(FVulkanLabDeviceStartupResult& Result) noexcept
{
    FVulkanLabPresentationPolicySelection& Selection = Result.Selection;
    Selection.Mode = Stoner::RHI::ERHIPresentationRetirementMode::Unknown;
    Selection.Reason =
        Stoner::RHI::ERHIPresentationRetirementReason::EntryPointsUnavailable;
    Selection.bEntryPointsResolved = false;
    Selection.bOptionalEnabled = false;
    Selection.bRetryPending = false;
    Selection.bRetryWithoutOptional = false;
    Result.bMandatoryPresentationEntryPointMissing = true;
}

[[nodiscard]] bool ResolvePresentEntryPoint(VkDevice Device) noexcept
{
    if (Device == VK_NULL_HANDLE)
    {
        return false;
    }
    return vkGetDeviceProcAddr(Device, "vkQueuePresentKHR") != nullptr;
}

void DestroyDevice(VkDevice& Device) noexcept
{
    if (Device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(Device, nullptr);
        Device = VK_NULL_HANDLE;
    }
}

[[nodiscard]] bool CopyExtensionNames(
    const VkDeviceCreateInfo& BaseCreateInfo,
    std::vector<const char*>& OutNames) noexcept
{
    try
    {
        OutNames.reserve(static_cast<std::size_t>(
            BaseCreateInfo.enabledExtensionCount) + 1U);
        for (uint32 Index = 0; Index < BaseCreateInfo.enabledExtensionCount;
             ++Index)
        {
            OutNames.push_back(BaseCreateInfo.ppEnabledExtensionNames[Index]);
        }
        if (!HasExtensionName(BaseCreateInfo, OptionalMaintenanceExtension))
        {
            OutNames.push_back(OptionalMaintenanceExtension);
        }
        return true;
    }
    catch (const std::bad_alloc&)
    {
        OutNames.clear();
        return false;
    }
    catch (const std::length_error&)
    {
        OutNames.clear();
        return false;
    }
}

[[nodiscard]] bool TryCreateDevice(
    VkPhysicalDevice PhysicalDevice,
    const VkDeviceCreateInfo& CreateInfo,
    VkDevice& OutDevice,
    VkResult& OutResult) noexcept
{
    OutDevice = VK_NULL_HANDLE;
    OutResult = vkCreateDevice(
        PhysicalDevice, &CreateInfo, nullptr, &OutDevice);
    return OutResult == VK_SUCCESS && OutDevice != VK_NULL_HANDLE;
}

[[nodiscard]] FVulkanLabPresentationPolicySelection CompleteOrdinarySelection(
    const FVulkanLabPresentationPolicyCandidate& Candidate) noexcept
{
    return FVulkanLabPresentationPolicy::CompleteEnablement(
        Candidate,
        EVulkanLabOptionalEnablementState::NotAttempted,
        false,
        false);
}

} // namespace

FVulkanLabDeviceStartupResult FVulkanLabDeviceStartup::Create(
    VkPhysicalDevice PhysicalDevice,
    const VkDeviceCreateInfo& BaseCreateInfo,
    const FVulkanLabPresentationPolicyRequest& PolicyRequest,
    const FVulkanLabPresentationCapabilityObservation& Observation) noexcept
{
    FVulkanLabDeviceStartupResult Result;
    FVulkanLabPresentationPolicyCandidate Candidate =
        FVulkanLabPresentationPolicy::SelectCandidate(
            PolicyRequest, Observation);

    if (PhysicalDevice == VK_NULL_HANDLE ||
        !IsValidBaseCreateInfo(BaseCreateInfo))
    {
        Result.bInputRejected = true;
        Result.Selection = MakeRejectedSelection(Candidate);
        return Result;
    }

    const bool bMandatoryPresentationEntryPoint =
        HasExtensionName(BaseCreateInfo, SwapchainExtension);

    if (Candidate.Candidate == EVulkanLabPresentationCandidate::None)
    {
        Result.Selection = CompleteOrdinarySelection(Candidate);
        return Result;
    }

    auto CompleteOrdinaryDevice =
        [&](bool bIsFallbackRetry) noexcept -> bool
        {
            if (bIsFallbackRetry)
            {
                Result.bFallbackCreationAttempted = true;
            }
            else
            {
                Result.bBaselineCreationAttempted = true;
            }
            VkDevice Device = VK_NULL_HANDLE;
            VkResult Creation = VK_NOT_READY;
            if (!TryCreateDevice(
                    PhysicalDevice, BaseCreateInfo, Device, Creation))
            {
                if (bIsFallbackRetry)
                {
                    Result.FallbackCreationResult = Creation;
                }
                Result.CreationResult = Creation;
                if (Device != VK_NULL_HANDLE)
                {
                    DestroyDevice(Device);
                }
                return false;
            }
            if (bIsFallbackRetry)
            {
                Result.FallbackCreationResult = Creation;
            }
            Result.CreationResult = Creation;

            if (bMandatoryPresentationEntryPoint)
            {
                Result.bUsedPresentEntryPointResolved =
                    ResolvePresentEntryPoint(Device);
                Result.Selection.bEntryPointsResolved =
                    Result.bUsedPresentEntryPointResolved;
                if (!Result.bUsedPresentEntryPointResolved)
                {
                    DestroyDevice(Device);
                    MarkEntryPointFailure(Result);
                    return false;
                }
            }
            Result.Device = Device;
            return true;
        };

    if (Candidate.Candidate == EVulkanLabPresentationCandidate::AcquireHistory)
    {
        Result.Selection = CompleteOrdinarySelection(Candidate);
        if (!CompleteOrdinaryDevice(false))
        {
            Result.Selection.Mode =
                Stoner::RHI::ERHIPresentationRetirementMode::Unknown;
            Result.Selection.bRetryPending = false;
            Result.Selection.bRetryWithoutOptional = false;
            return Result;
        }
        Result.bSucceeded = Result.Selection.IsUsable();
        return Result;
    }

    FVulkanLabPresentationPolicySelection RetrySelection;
    bool bOptionalCreatePrepared = false;
#if defined(VK_EXT_swapchain_maintenance1)
    std::vector<const char*> OptionalExtensionNames;
    VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT OptionalFeatures{};
    VkDeviceCreateInfo OptionalCreateInfo = BaseCreateInfo;
    if (CopyExtensionNames(BaseCreateInfo, OptionalExtensionNames))
    {
        OptionalFeatures.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT;
        // Vulkan declares this feature-chain pNext as mutable although
        // vkCreateDevice only reads it.  Preserve the caller's read-only
        // chain without mutating any base node.
        OptionalFeatures.pNext = const_cast<void*>(BaseCreateInfo.pNext);
        OptionalFeatures.swapchainMaintenance1 = VK_TRUE;
        OptionalCreateInfo.enabledExtensionCount = static_cast<uint32>(
            OptionalExtensionNames.size());
        OptionalCreateInfo.ppEnabledExtensionNames =
            OptionalExtensionNames.data();
        OptionalCreateInfo.pNext = &OptionalFeatures;
        bOptionalCreatePrepared = true;
    }
#endif

#if defined(VK_EXT_swapchain_maintenance1)
    if (bOptionalCreatePrepared)
    {
        VkDevice OptionalDevice = VK_NULL_HANDLE;
        VkResult OptionalCreation = VK_NOT_READY;
        Result.bOptionalCreationAttempted = true;
        if (TryCreateDevice(
                PhysicalDevice,
                OptionalCreateInfo,
                OptionalDevice,
                OptionalCreation))
        {
            Result.OptionalCreationResult = OptionalCreation;
            Result.CreationResult = OptionalCreation;
            Result.bUsedPresentEntryPointResolved =
                ResolvePresentEntryPoint(OptionalDevice);
            if (Result.bUsedPresentEntryPointResolved)
            {
                Result.Selection = FVulkanLabPresentationPolicy::CompleteEnablement(
                    Candidate,
                    EVulkanLabOptionalEnablementState::Succeeded,
                    true,
                    false);
                Result.Device = OptionalDevice;
                Result.bSucceeded = Result.Selection.IsUsable();
                return Result;
            }

            DestroyDevice(OptionalDevice);
            RetrySelection = FVulkanLabPresentationPolicy::CompleteEnablement(
                Candidate,
                EVulkanLabOptionalEnablementState::Succeeded,
                false,
                false);
        }
        else
        {
            Result.OptionalCreationResult = OptionalCreation;
            Result.CreationResult = OptionalCreation;
            if (OptionalCreation == VK_ERROR_DEVICE_LOST)
            {
                Result.Selection = MakeRejectedSelection(Candidate);
                Result.Selection.Reason =
                    Stoner::RHI::ERHIPresentationRetirementReason::OptionalEnablementFailed;
                return Result;
            }
            RetrySelection = FVulkanLabPresentationPolicy::CompleteEnablement(
                Candidate,
                EVulkanLabOptionalEnablementState::Failed,
                false,
                false);
        }
    }
    else
    {
        RetrySelection = FVulkanLabPresentationPolicy::CompleteEnablement(
            Candidate,
            EVulkanLabOptionalEnablementState::Failed,
            false,
            false);
    }
#else
    (void)bOptionalCreatePrepared;
    RetrySelection = FVulkanLabPresentationPolicy::CompleteEnablement(
        Candidate,
        EVulkanLabOptionalEnablementState::Failed,
        false,
        false);
#endif

    // The policy object keeps this selection unusable until ordinary device
    // creation succeeds.  A single fallback call is the only retry allowed.
    Result.bRetryConsumed = true;
    Result.Selection = std::move(RetrySelection);
    if (!CompleteOrdinaryDevice(true))
    {
        if (Result.bMandatoryPresentationEntryPointMissing)
        {
            return Result;
        }
        Result.Selection = FVulkanLabPresentationPolicy::CompleteFallbackRetry(
            Result.Selection,
            EVulkanLabFallbackDeviceCreationState::Failed);
        return Result;
    }

    Result.Selection.bEntryPointsResolved =
        Result.bUsedPresentEntryPointResolved;

    if (!Result.bUsedPresentEntryPointResolved &&
        bMandatoryPresentationEntryPoint)
    {
        // CompleteOrdinaryDevice already destroyed the handle and marked the
        // typed entry-point failure.  Keep the selection unusable.
        return Result;
    }

    Result.Selection = FVulkanLabPresentationPolicy::CompleteFallbackRetry(
        Result.Selection,
        EVulkanLabFallbackDeviceCreationState::Succeeded);
    Result.bSucceeded = Result.Selection.IsUsable();
    return Result;

}

} // namespace Stoner::Backend::Vulkan::Private

#endif
