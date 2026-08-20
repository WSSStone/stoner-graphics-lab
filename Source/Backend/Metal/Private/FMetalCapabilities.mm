#include "FMetalCapabilities.h"

#include "FMetalFormat.h"

#import <Metal/Metal.h>

#include <algorithm>
#include <limits>

namespace Stoner::Backend::Metal::Private
{

RHI::FRHIDeviceCapabilities QueryMetalCapabilities(
    void* NativeDevice) noexcept
{
    RHI::FRHIDeviceCapabilities Result;
    if (NativeDevice == nullptr) return Result;
    @autoreleasepool
    {
        id<MTLDevice> Device = (__bridge id<MTLDevice>)NativeDevice;
        Result.bSupportsGraphicsQueue = true;
        Result.bSupportsComputeQueue = true;
        Result.bSupportsTransferQueue = true;
        Result.bSupportsPresentQueue = true;
        Result.bSupportsPresentation = true;
        Result.bSupportsSynchronization = true;
        Result.MaxInFlightFrames = 3;
        Result.MaxCommandBuffersPerQueue = 4096;
        Result.MaxQueuesPerType = 1;
        Result.MaxBufferSizeBytes = Device.maxBufferLength;
        Result.MaxResourceSizeBytes = Device.maxBufferLength;
        Result.MaxTextureDimension1D = 16384;
        Result.MaxTextureDimension2D = 16384;
        Result.MaxTextureDimension3D = 2048;
        Result.MaxTextureArrayLayers = 2048;
        Result.MaxPerStageBufferBindings = 31;
        Result.MaxPerStageTextureBindings = 128;
        Result.MaxPerStageSamplerBindings = 16;
        Result.MaxConstantRangeBytes = 4096;
        Result.MaxConstantDataBytesPerStage = 4096;
        const MTLSize Threads = Device.maxThreadsPerThreadgroup;
        Result.MaxComputeThreadgroupSizeX = static_cast<Core::uint32>(Threads.width);
        Result.MaxComputeThreadgroupSizeY = static_cast<Core::uint32>(Threads.height);
        Result.MaxComputeThreadgroupSizeZ = static_cast<Core::uint32>(Threads.depth);
        Result.MaxComputeThreadsPerThreadgroup = static_cast<Core::uint32>(
            std::min<NSUInteger>(Device.maxThreadsPerThreadgroup.width,
                std::numeric_limits<Core::uint32>::max()));
        Result.MaxComputeDispatchGroupsX = 65535;
        Result.MaxComputeDispatchGroupsY = 65535;
        Result.MaxComputeDispatchGroupsZ = 65535;
        for (RHI::ERHISampleCount Count : {
                 RHI::ERHISampleCount::One,
                 RHI::ERHISampleCount::Two,
                 RHI::ERHISampleCount::Four,
                 RHI::ERHISampleCount::Eight})
        {
            if ([Device supportsTextureSampleCount:
                    static_cast<NSUInteger>(Count)])
                Result.SupportedSampleCounts |=
                    static_cast<Core::uint32>(Count);
        }
        for (int Value = static_cast<int>(RHI::ERHIFormat::Unknown) + 1;
             Value < static_cast<int>(RHI::ERHIFormat::Count); ++Value)
        {
            const auto Format = static_cast<RHI::ERHIFormat>(Value);
            if (IsMetalFormatSupported(NativeDevice, Format))
                Result.Formats.push_back(RHI::MakeRHIFormatCapabilities(Format));
        }
    }
    return Result;
}

} // namespace Stoner::Backend::Metal::Private
