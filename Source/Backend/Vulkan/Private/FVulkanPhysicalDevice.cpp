#include "VulkanRHI/FVulkanPhysicalDevice.h"

#include <algorithm>
#include <string_view>

namespace Stoner::Backend::Vulkan
{

namespace
{

[[nodiscard]] Stoner::Core::int32 DeviceTypeScore(EVulkanPhysicalDeviceType DeviceType) noexcept
{
    switch (DeviceType)
    {
    case EVulkanPhysicalDeviceType::Discrete:
        return 1000;
    case EVulkanPhysicalDeviceType::Integrated:
        return 650;
    case EVulkanPhysicalDeviceType::Virtual:
        return 300;
    case EVulkanPhysicalDeviceType::Cpu:
        return 100;
    case EVulkanPhysicalDeviceType::Unknown:
        return 0;
    }
    return 0;
}

} // namespace

bool PassesRequiredCapabilityGate(const FVulkanAdapterCandidate& Candidate) noexcept
{
    return Candidate.bPassesRequiredGate && Candidate.Queues.bGraphics && Candidate.Queues.bTransfer && Candidate.Formats.bColor;
}

Stoner::Core::int32 ScoreAdapterCandidate(const FVulkanAdapterCandidate& Candidate) noexcept
{
    if (!PassesRequiredCapabilityGate(Candidate))
    {
        return -1;
    }

    Stoner::Core::int32 Score = DeviceTypeScore(Candidate.DeviceType);
    Score += Candidate.Queues.bGraphics ? 100 : 0;
    Score += Candidate.Queues.bCompute ? 75 : 0;
    Score += Candidate.Queues.bTransfer ? 50 : 0;
    Score += Candidate.Queues.bPresent ? 50 : 0;
    Score += Candidate.bPresentationSupported ? 75 : 0;
    Score += Candidate.Formats.bColor ? 40 : 0;
    Score += Candidate.Formats.bDepth ? 20 : 0;
    return Score;
}

FVulkanAdapterSelection SelectBestAdapter(Stoner::Core::TArray<FVulkanAdapterCandidate> Candidates)
{
    FVulkanAdapterSelection Selection;
    Selection.Candidates = std::move(Candidates);

    for (FVulkanAdapterCandidate& Candidate : Selection.Candidates)
    {
        Candidate.Score = ScoreAdapterCandidate(Candidate);
        if (!PassesRequiredCapabilityGate(Candidate))
        {
            Candidate.RejectionReason = Candidate.RejectionReason && Candidate.RejectionReason[0] != '\0'
                ? Candidate.RejectionReason
                : "missing required graphics, transfer, or color format support";
        }
    }

    auto BestIt = std::max_element(
        Selection.Candidates.begin(),
        Selection.Candidates.end(),
        [](const FVulkanAdapterCandidate& A, const FVulkanAdapterCandidate& B) {
            if (A.Score != B.Score)
            {
                return A.Score < B.Score;
            }
            return std::string_view(A.Name) > std::string_view(B.Name);
        });

    if (BestIt == Selection.Candidates.end() || BestIt->Score < 0)
    {
        Selection.bSucceeded = false;
        Selection.Reason = "no adapter passed required capability gates";
        return Selection;
    }

    Selection.bSucceeded = true;
    Selection.Selected = *BestIt;
    Selection.Reason = "selected highest scoring compatible adapter";
    return Selection;
}

Stoner::Core::TArray<Stoner::RHI::ERHIFormat> GetDefaultVulkanSupportedFormats()
{
    return {
        Stoner::RHI::ERHIFormat::R8G8B8A8_UNorm,
        Stoner::RHI::ERHIFormat::B8G8R8A8_UNorm,
        Stoner::RHI::ERHIFormat::R16G16B16A16_Float,
        Stoner::RHI::ERHIFormat::R32_Float,
        Stoner::RHI::ERHIFormat::R32G32_Float,
        Stoner::RHI::ERHIFormat::R32G32B32_Float,
        Stoner::RHI::ERHIFormat::D24_UNorm_S8_UInt,
        Stoner::RHI::ERHIFormat::D32_Float,
    };
}

} // namespace Stoner::Backend::Vulkan
