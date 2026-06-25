#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIFormat.h"
#include "RHI/ERHIQueueType.h"

#include <algorithm>

namespace Stoner::RHI
{

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

    Stoner::Core::TArray<ERHIFormat> SupportedFormats;

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

    [[nodiscard]] bool SupportsFormat(ERHIFormat Format) const
    {
        return std::find(SupportedFormats.begin(), SupportedFormats.end(), Format) != SupportedFormats.end();
    }
};

} // namespace Stoner::RHI
