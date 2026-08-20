#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIFormat.h"
#include "RHI/ERHIPipelineState.h"
#include "RHI/ERHIQueueType.h"
#include "RHI/ERHIResourceUsage.h"

#include <algorithm>

namespace Stoner::RHI
{

enum class ERHIFormatCapability : Stoner::Core::uint32
{
    None = 0,
    SampledImage = 1u << 0,
    CopySource = 1u << 1,
    CopyDestination = 1u << 2,
    ColorAttachment = 1u << 3,
    DepthStencilAttachment = 1u << 4
};

[[nodiscard]] constexpr ERHIFormatCapability operator|(
    ERHIFormatCapability Left,
    ERHIFormatCapability Right) noexcept
{
    return static_cast<ERHIFormatCapability>(
        RHIToUnderlying(Left) | RHIToUnderlying(Right));
}

[[nodiscard]] constexpr ERHIFormatCapability operator&(
    ERHIFormatCapability Left,
    ERHIFormatCapability Right) noexcept
{
    return static_cast<ERHIFormatCapability>(
        RHIToUnderlying(Left) & RHIToUnderlying(Right));
}

constexpr ERHIFormatCapability& operator|=(
    ERHIFormatCapability& Left,
    ERHIFormatCapability Right) noexcept
{
    Left = Left | Right;
    return Left;
}

inline constexpr ERHIFormatCapability RHIFormatCapabilityValidMask =
    ERHIFormatCapability::SampledImage |
    ERHIFormatCapability::CopySource |
    ERHIFormatCapability::CopyDestination |
    ERHIFormatCapability::ColorAttachment |
    ERHIFormatCapability::DepthStencilAttachment;

struct FRHIFormatCapabilities
{
    ERHIFormat Format = ERHIFormat::Unknown;
    ERHIFormatCapability Capabilities =
        ERHIFormatCapability::None;

    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
        if (!IsValidRHIFormat(Format) ||
            Capabilities == ERHIFormatCapability::None ||
            !HasOnlyRHIFlags(
                Capabilities,
                RHIFormatCapabilityValidMask))
        {
            return false;
        }
        return IsDepthStencilFormat(Format)
            ? (Capabilities &
                ERHIFormatCapability::ColorAttachment) ==
                ERHIFormatCapability::None
            : (Capabilities &
                ERHIFormatCapability::DepthStencilAttachment) ==
                ERHIFormatCapability::None;
    }
};

[[nodiscard]] constexpr FRHIFormatCapabilities
MakeRHIFormatCapabilities(ERHIFormat Format) noexcept
{
    const ERHIFormatCapability Common =
        ERHIFormatCapability::SampledImage |
        ERHIFormatCapability::CopySource |
        ERHIFormatCapability::CopyDestination;
    return {
        Format,
        Common |
            (IsDepthStencilFormat(Format)
                ? ERHIFormatCapability::DepthStencilAttachment
                : ERHIFormatCapability::ColorAttachment)};
}

struct FRHIDeviceCapabilities
{
    bool bSupportsGraphicsQueue = false;
    bool bSupportsComputeQueue = false;
    bool bSupportsTransferQueue = false;
    bool bSupportsPresentQueue = false;

    bool bSupportsPresentation = false;
    bool bSupportsSynchronization = false;

    Stoner::Core::uint32 MaxInFlightFrames = 0;
    Stoner::Core::uint32 MaxCommandBuffersPerQueue = 0;
    Stoner::Core::uint32 MaxQueuesPerType = 0;

    Stoner::Core::uint64 MaxBufferSizeBytes = 0;
    Stoner::Core::uint64 MaxResourceSizeBytes = 0;
    Stoner::Core::uint32 MaxTextureDimension1D = 0;
    Stoner::Core::uint32 MaxTextureDimension2D = 0;
    Stoner::Core::uint32 MaxTextureDimension3D = 0;
    Stoner::Core::uint32 MaxTextureArrayLayers = 0;

    Stoner::Core::uint32 MaxPerStageBufferBindings = 0;
    Stoner::Core::uint32 MaxPerStageTextureBindings = 0;
    Stoner::Core::uint32 MaxPerStageSamplerBindings = 0;
    Stoner::Core::uint32 MaxConstantRangeBytes = 0;
    Stoner::Core::uint32 MaxConstantDataBytesPerStage = 0;

    Stoner::Core::uint32 MaxComputeThreadgroupSizeX = 0;
    Stoner::Core::uint32 MaxComputeThreadgroupSizeY = 0;
    Stoner::Core::uint32 MaxComputeThreadgroupSizeZ = 0;
    Stoner::Core::uint32 MaxComputeThreadsPerThreadgroup = 0;
    Stoner::Core::uint32 MaxComputeDispatchGroupsX = 0;
    Stoner::Core::uint32 MaxComputeDispatchGroupsY = 0;
    Stoner::Core::uint32 MaxComputeDispatchGroupsZ = 0;

    Stoner::Core::uint32 SupportedSampleCounts = 0;

    Stoner::Core::TArray<FRHIFormatCapabilities> Formats;

    [[nodiscard]] bool SupportsQueue(ERHIQueueType QueueType) const noexcept
    {
        switch (QueueType)
        {
        case ERHIQueueType::Graphics:
            return bSupportsGraphicsQueue;
        case ERHIQueueType::Compute:
            return bSupportsComputeQueue;
        case ERHIQueueType::Transfer:
            return bSupportsTransferQueue;
        case ERHIQueueType::Present:
            return bSupportsPresentQueue;
        }
        return false;
    }

    [[nodiscard]] bool HasValidFormatCapabilities() const noexcept
    {
        for (Stoner::Core::usize Index = 0;
             Index < Formats.size();
             ++Index)
        {
            if (!Formats[Index].IsValid())
            {
                return false;
            }
            for (Stoner::Core::usize Other = Index + 1;
                 Other < Formats.size();
                 ++Other)
            {
                if (Formats[Index].Format ==
                    Formats[Other].Format)
                {
                    return false;
                }
            }
        }
        return true;
    }

    [[nodiscard]] bool SupportsFormat(
        ERHIFormat Format) const noexcept
    {
        if (!IsValidRHIFormat(Format) ||
            !HasValidFormatCapabilities())
        {
            return false;
        }
        return std::any_of(
            Formats.begin(),
            Formats.end(),
            [Format](const FRHIFormatCapabilities& Record) {
                return Record.Format == Format;
            });
    }

    [[nodiscard]] bool SupportsFormatUsage(
        ERHIFormat Format,
        ERHIFormatCapability Required) const noexcept
    {
        if (!IsValidRHIFormat(Format) ||
            Required == ERHIFormatCapability::None ||
            !HasOnlyRHIFlags(
                Required,
                RHIFormatCapabilityValidMask) ||
            !HasValidFormatCapabilities())
        {
            return false;
        }
        const auto It = std::find_if(
            Formats.begin(),
            Formats.end(),
            [Format](const FRHIFormatCapabilities& Record) {
                return Record.Format == Format;
            });
        return It != Formats.end() &&
            (It->Capabilities & Required) == Required;
    }

    [[nodiscard]] bool SupportsSampleCount(
        ERHISampleCount SampleCount) const noexcept
    {
        if (!IsValidRHISampleCount(SampleCount))
        {
            return false;
        }
        return (SupportedSampleCounts &
            static_cast<Stoner::Core::uint32>(SampleCount)) != 0;
    }

    [[nodiscard]] bool HasValidLimits() const noexcept
    {
        if (MaxInFlightFrames == 0 || MaxCommandBuffersPerQueue == 0 ||
            MaxQueuesPerType == 0 || MaxBufferSizeBytes == 0 ||
            MaxResourceSizeBytes < MaxBufferSizeBytes ||
            MaxTextureDimension1D == 0 || MaxTextureDimension2D == 0 ||
            MaxTextureDimension3D == 0 || MaxTextureArrayLayers == 0 ||
            MaxPerStageBufferBindings == 0 ||
            MaxPerStageTextureBindings == 0 ||
            MaxPerStageSamplerBindings == 0 ||
            MaxConstantRangeBytes == 0 ||
            MaxConstantDataBytesPerStage < MaxConstantRangeBytes ||
            !SupportsSampleCount(ERHISampleCount::One))
        {
            return false;
        }
        if (bSupportsComputeQueue &&
            (MaxComputeThreadgroupSizeX == 0 ||
             MaxComputeThreadgroupSizeY == 0 ||
             MaxComputeThreadgroupSizeZ == 0 ||
             MaxComputeThreadsPerThreadgroup == 0 ||
             MaxComputeDispatchGroupsX == 0 ||
             MaxComputeDispatchGroupsY == 0 ||
             MaxComputeDispatchGroupsZ == 0 ||
             static_cast<Stoner::Core::uint64>(MaxComputeThreadgroupSizeX) *
                 MaxComputeThreadgroupSizeY * MaxComputeThreadgroupSizeZ <
                 MaxComputeThreadsPerThreadgroup))
        {
            return false;
        }
        return true;
    }
};

[[nodiscard]] bool IsValidRHIDeviceCapabilities(
    const FRHIDeviceCapabilities& Capabilities) noexcept;
[[nodiscard]] Stoner::Core::FString DumpRHIDeviceCapabilities(
    const FRHIDeviceCapabilities& Capabilities);

} // namespace Stoner::RHI
