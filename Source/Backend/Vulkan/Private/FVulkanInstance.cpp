#include "VulkanRHI/FVulkanInstance.h"

namespace Stoner::Backend::Vulkan
{

namespace
{

[[nodiscard]] Stoner::Core::TArray<FVulkanAdapterCandidate> MakeDefaultCandidates()
{
    return {
        {
            "Stoner Vulkan Compatible Adapter",
            EVulkanPhysicalDeviceType::Discrete,
            true,
            {true, true, true, true},
            true,
            {true, true},
            0,
            "",
        },
    };
}

} // namespace

bool FVulkanInstance::IsInitialized() const noexcept
{
    return bInitialized;
}

const FVulkanDiagnostics& FVulkanInstance::GetDiagnostics() const noexcept
{
    return Diagnostics;
}

const FVulkanAdapterSelection& FVulkanInstance::GetAdapterSelection() const noexcept
{
    return AdapterSelection;
}

Stoner::RHI::ERHIResult FVulkanInstance::Initialize(const FVulkanInstanceDesc& Desc)
{
    if (bInitialized)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }

    Diagnostics = {};
    Diagnostics.bValidationRequested = Desc.bRequestValidation;
#if defined(STONER_VULKAN_SDK_AVAILABLE) && STONER_VULKAN_SDK_AVAILABLE
    Diagnostics.bUsedSdkHeaders = true;
#else
    Diagnostics.bUsedRuntimeFallback = true;
#endif

    if (Desc.bForceUnsupportedRuntime)
    {
        MarkUnsupportedRuntime(Diagnostics, "forced unsupported Vulkan runtime");
        AdapterSelection = {};
        return Stoner::RHI::ERHIResult::Unsupported;
    }

    if (Desc.bRequestValidation)
    {
        if (Desc.bForceValidationUnavailable)
        {
            MarkValidationUnavailable(Diagnostics, "validation support unavailable");
        }
        else
        {
            Diagnostics.Validation = EVulkanValidationState::Enabled;
        }
    }

    Stoner::Core::TArray<FVulkanAdapterCandidate> Candidates = Desc.SyntheticCandidates.empty()
        ? MakeDefaultCandidates()
        : Desc.SyntheticCandidates;
    AdapterSelection = SelectBestAdapter(std::move(Candidates));
    if (!AdapterSelection.bSucceeded)
    {
        Diagnostics.Availability = EVulkanBackendAvailability::MissingRequiredCapability;
        Diagnostics.UnsupportedRuntimeReason = AdapterSelection.Reason;
        return Stoner::RHI::ERHIResult::Unsupported;
    }

    bInitialized = true;
    MarkSelectedAdapter(Diagnostics, AdapterSelection.Reason);
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanInstance::Shutdown() noexcept
{
    if (!bInitialized)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }

    bInitialized = false;
    return Stoner::RHI::ERHIResult::Success;
}

} // namespace Stoner::Backend::Vulkan
