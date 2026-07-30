#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIFormat.h"
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
};

} // namespace Stoner::RHI
