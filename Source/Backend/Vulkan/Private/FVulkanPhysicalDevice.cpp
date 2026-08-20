#include "VulkanRHI/FVulkanPhysicalDevice.h"

#include "RHI/FRHIFormatInfo.h"

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

[[nodiscard]] Stoner::RHI::FRHIFormatCapabilities
MakeSyntheticFormatCapabilities(
    Stoner::RHI::ERHIFormat Format) noexcept
{
    using namespace Stoner::RHI;
    ERHIFormatCapability Capabilities =
        ERHIFormatCapability::SampledImage |
        ERHIFormatCapability::CopySource |
        ERHIFormatCapability::CopyDestination;
    if (IsDepthStencilFormat(Format))
    {
        Capabilities |=
            ERHIFormatCapability::DepthStencilAttachment;
    }
    else if (!GetRHIFormatInfo(Format).bCompressed)
    {
        Capabilities |=
            ERHIFormatCapability::ColorAttachment;
    }
    return {Format, Capabilities};
}

void NormalizeFormatCapabilities(
    Stoner::Core::TArray<
        Stoner::RHI::FRHIFormatCapabilities>& Records)
{
    Records.erase(
        std::remove_if(
            Records.begin(),
            Records.end(),
            [](const Stoner::RHI::FRHIFormatCapabilities& Record) {
                return !Record.IsValid();
            }),
        Records.end());
    std::sort(
        Records.begin(),
        Records.end(),
        [](const auto& Left, const auto& Right) {
            return Left.Format < Right.Format;
        });
    Stoner::Core::TArray<
        Stoner::RHI::FRHIFormatCapabilities> Normalized;
    Normalized.reserve(Records.size());
    for (const auto& Record : Records)
    {
        if (!Normalized.empty() &&
            Normalized.back().Format == Record.Format)
        {
            Normalized.back().Capabilities |=
                Record.Capabilities;
        }
        else
        {
            Normalized.push_back(Record);
        }
    }
    Records = std::move(Normalized);
}

} // namespace

FVulkanFormatSupport::FVulkanFormatSupport(
    bool bIncludeColorFormats,
    bool bIncludeDepthFormats)
{
    if (bIncludeColorFormats)
    {
        const Stoner::RHI::ERHIFormat ColorFormats[] = {
            Stoner::RHI::ERHIFormat::R8_UNorm,
            Stoner::RHI::ERHIFormat::R8G8_UNorm,
            Stoner::RHI::ERHIFormat::R8G8B8A8_UNorm,
            Stoner::RHI::ERHIFormat::R8G8B8A8_sRGB,
            Stoner::RHI::ERHIFormat::B8G8R8A8_UNorm,
            Stoner::RHI::ERHIFormat::R16G16B16A16_Float,
            Stoner::RHI::ERHIFormat::R32_Float,
            Stoner::RHI::ERHIFormat::R32G32_Float,
            Stoner::RHI::ERHIFormat::R32G32B32_Float,
            Stoner::RHI::ERHIFormat::R32G32B32A32_Float,
            Stoner::RHI::ERHIFormat::BC1_RGBA_UNorm,
            Stoner::RHI::ERHIFormat::BC1_RGBA_sRGB,
            Stoner::RHI::ERHIFormat::BC3_RGBA_UNorm,
            Stoner::RHI::ERHIFormat::BC3_RGBA_sRGB,
            Stoner::RHI::ERHIFormat::BC4_R_UNorm,
            Stoner::RHI::ERHIFormat::BC5_RG_UNorm,
            Stoner::RHI::ERHIFormat::BC7_RGBA_UNorm,
            Stoner::RHI::ERHIFormat::BC7_RGBA_sRGB,
            Stoner::RHI::ERHIFormat::ETC2_RGB8_UNorm,
            Stoner::RHI::ERHIFormat::ETC2_RGB8_sRGB,
            Stoner::RHI::ERHIFormat::ETC2_RGBA8_UNorm,
            Stoner::RHI::ERHIFormat::ETC2_RGBA8_sRGB,
            Stoner::RHI::ERHIFormat::EAC_R11_UNorm,
            Stoner::RHI::ERHIFormat::EAC_RG11_UNorm,
            Stoner::RHI::ERHIFormat::ASTC_4x4_RGBA_UNorm,
            Stoner::RHI::ERHIFormat::ASTC_4x4_RGBA_sRGB,
        };
        for (Stoner::RHI::ERHIFormat Format : ColorFormats)
        {
            FormatCapabilities.push_back(
                MakeSyntheticFormatCapabilities(Format));
        }
    }
    if (bIncludeDepthFormats)
    {
        FormatCapabilities.push_back(
            MakeSyntheticFormatCapabilities(
                Stoner::RHI::ERHIFormat::D24_UNorm_S8_UInt));
        FormatCapabilities.push_back(
            MakeSyntheticFormatCapabilities(
                Stoner::RHI::ERHIFormat::D32_Float));
    }
    NormalizeFormatCapabilities(FormatCapabilities);
}

FVulkanFormatSupport::FVulkanFormatSupport(
    Stoner::Core::TArray<Stoner::RHI::ERHIFormat> InSupportedFormats)
{
    FormatCapabilities.reserve(InSupportedFormats.size());
    for (Stoner::RHI::ERHIFormat Format : InSupportedFormats)
    {
        if (Stoner::RHI::IsValidRHIFormat(Format))
        {
            FormatCapabilities.push_back(
                MakeSyntheticFormatCapabilities(Format));
        }
    }
    NormalizeFormatCapabilities(FormatCapabilities);
}

FVulkanFormatSupport::FVulkanFormatSupport(
    Stoner::Core::TArray<
        Stoner::RHI::FRHIFormatCapabilities>
        InFormatCapabilities)
    : FormatCapabilities(std::move(InFormatCapabilities))
{
    NormalizeFormatCapabilities(FormatCapabilities);
}

bool FVulkanFormatSupport::SupportsFormat(Stoner::RHI::ERHIFormat Format) const noexcept
{
    return Stoner::RHI::IsValidRHIFormat(Format) &&
        std::any_of(
            FormatCapabilities.begin(),
            FormatCapabilities.end(),
            [Format](const auto& Record) {
                return Record.Format == Format;
            });
}

bool FVulkanFormatSupport::SupportsFormatUsage(
    Stoner::RHI::ERHIFormat Format,
    Stoner::RHI::ERHIFormatCapability Required) const noexcept
{
    const auto Found = std::find_if(
        FormatCapabilities.begin(),
        FormatCapabilities.end(),
        [Format](const auto& Record) {
            return Record.Format == Format;
        });
    return Found != FormatCapabilities.end() &&
        Required !=
            Stoner::RHI::ERHIFormatCapability::None &&
        (Found->Capabilities & Required) == Required;
}

bool FVulkanFormatSupport::SupportsColor() const noexcept
{
    return std::any_of(FormatCapabilities.begin(), FormatCapabilities.end(), [](const auto& Record) {
        return Stoner::RHI::IsValidRHIFormat(Record.Format) && !Stoner::RHI::IsDepthStencilFormat(Record.Format);
    });
}

bool FVulkanFormatSupport::SupportsDepth() const noexcept
{
    return std::any_of(
        FormatCapabilities.begin(),
        FormatCapabilities.end(),
        [](const auto& Record) {
            return Stoner::RHI::IsDepthStencilFormat(
                Record.Format);
        });
}

const Stoner::Core::TArray<
    Stoner::RHI::FRHIFormatCapabilities>&
FVulkanFormatSupport::GetFormatCapabilities() const noexcept
{
    return FormatCapabilities;
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

Stoner::RHI::FRHIDeviceCapabilities MakeVulkanBaselineDeviceCapabilities()
{
    using namespace Stoner::RHI;
    FRHIDeviceCapabilities Capabilities;
    Capabilities.bSupportsGraphicsQueue = true;
    Capabilities.bSupportsComputeQueue = true;
    Capabilities.bSupportsTransferQueue = true;
    Capabilities.bSupportsSynchronization = true;
    Capabilities.MaxInFlightFrames = 3;
    Capabilities.MaxCommandBuffersPerQueue = 64;
    Capabilities.MaxQueuesPerType = 1;
    Capabilities.MaxBufferSizeBytes = 128ULL * 1024ULL * 1024ULL;
    Capabilities.MaxResourceSizeBytes = 128ULL * 1024ULL * 1024ULL;
    Capabilities.MaxTextureDimension1D = 8192;
    Capabilities.MaxTextureDimension2D = 8192;
    Capabilities.MaxTextureDimension3D = 2048;
    Capabilities.MaxTextureArrayLayers = 256;
    Capabilities.MaxPerStageBufferBindings = 12;
    Capabilities.MaxPerStageTextureBindings = 16;
    Capabilities.MaxPerStageSamplerBindings = 16;
    Capabilities.MaxConstantRangeBytes = 128;
    Capabilities.MaxConstantDataBytesPerStage = 128;
    Capabilities.MaxComputeThreadgroupSizeX = 128;
    Capabilities.MaxComputeThreadgroupSizeY = 128;
    Capabilities.MaxComputeThreadgroupSizeZ = 64;
    Capabilities.MaxComputeThreadsPerThreadgroup = 128;
    Capabilities.MaxComputeDispatchGroupsX = 65535;
    Capabilities.MaxComputeDispatchGroupsY = 65535;
    Capabilities.MaxComputeDispatchGroupsZ = 65535;
    Capabilities.SupportedSampleCounts =
        static_cast<Stoner::Core::uint32>(ERHISampleCount::One) |
        static_cast<Stoner::Core::uint32>(ERHISampleCount::Two) |
        static_cast<Stoner::Core::uint32>(ERHISampleCount::Four) |
        static_cast<Stoner::Core::uint32>(ERHISampleCount::Eight);
    return Capabilities;
}

} // namespace Stoner::Backend::Vulkan
