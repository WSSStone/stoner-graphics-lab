#include "VulkanRHI/FVulkanPhysicalDevice.h"

#include <algorithm>
#include <utility>

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

FVulkanFormatSupport::FVulkanFormatSupport(
    bool bIncludeColorFormats,
    bool bIncludeDepthFormats)
{
    if (bIncludeColorFormats)
    {
        SupportedFormats = {
            Stoner::RHI::ERHIFormat::R8G8B8A8_UNorm,
            Stoner::RHI::ERHIFormat::B8G8R8A8_UNorm,
            Stoner::RHI::ERHIFormat::R16G16B16A16_Float,
            Stoner::RHI::ERHIFormat::R32_Float,
            Stoner::RHI::ERHIFormat::R32G32_Float,
            Stoner::RHI::ERHIFormat::R32G32B32_Float,
        };
    }
    if (bIncludeDepthFormats)
    {
        SupportedFormats.push_back(Stoner::RHI::ERHIFormat::D24_UNorm_S8_UInt);
        SupportedFormats.push_back(Stoner::RHI::ERHIFormat::D32_Float);
    }
}

FVulkanFormatSupport::FVulkanFormatSupport(
    Stoner::Core::TArray<Stoner::RHI::ERHIFormat> InSupportedFormats)
    : SupportedFormats(std::move(InSupportedFormats))
{
    SupportedFormats.erase(
        std::remove_if(SupportedFormats.begin(), SupportedFormats.end(), [](Stoner::RHI::ERHIFormat Format) {
            return !Stoner::RHI::IsValidRHIFormat(Format);
        }),
        SupportedFormats.end());
    std::sort(SupportedFormats.begin(), SupportedFormats.end());
    SupportedFormats.erase(
        std::unique(SupportedFormats.begin(), SupportedFormats.end()),
        SupportedFormats.end());
}

bool FVulkanFormatSupport::SupportsFormat(Stoner::RHI::ERHIFormat Format) const noexcept
{
    return Stoner::RHI::IsValidRHIFormat(Format) &&
        std::find(SupportedFormats.begin(), SupportedFormats.end(), Format) != SupportedFormats.end();
}

bool FVulkanFormatSupport::SupportsColor() const noexcept
{
    return std::any_of(SupportedFormats.begin(), SupportedFormats.end(), [](Stoner::RHI::ERHIFormat Format) {
        return Stoner::RHI::IsValidRHIFormat(Format) && !Stoner::RHI::IsDepthStencilFormat(Format);
    });
}

bool FVulkanFormatSupport::SupportsDepth() const noexcept
{
    return std::any_of(SupportedFormats.begin(), SupportedFormats.end(), Stoner::RHI::IsDepthStencilFormat);
}

const Stoner::Core::TArray<Stoner::RHI::ERHIFormat>&
FVulkanFormatSupport::GetSupportedFormats() const noexcept
{
    return SupportedFormats;
}

bool PassesRequiredCapabilityGate(const FVulkanAdapterCandidate& Candidate) noexcept
{
    return !Candidate.Name.IsEmpty() && Candidate.bPassesRequiredGate &&
        Candidate.Queues.bGraphics && Candidate.Queues.bTransfer && Candidate.Formats.SupportsColor();
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
    Score += Candidate.Formats.SupportsColor() ? 40 : 0;
    Score += Candidate.Formats.SupportsDepth() ? 20 : 0;
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
            if (Candidate.RejectionReason.IsEmpty())
            {
                Candidate.RejectionReason = Candidate.Name.IsEmpty()
                    ? "missing adapter identity"
                    : "missing required graphics, transfer, or color format support";
            }
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
            return A.Name.View() > B.Name.View();
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

} // namespace Stoner::Backend::Vulkan
