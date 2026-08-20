#include "FMetalBlitCommandEncoder.h"

#include "FMetalBuffer.h"
#include "FMetalTexture.h"

#import <Metal/Metal.h>

#include <limits>

namespace Stoner::Backend::Metal::Private
{
namespace
{

bool AlignUp(Core::uint64 Value, Core::uint64 Alignment, Core::uint64& Out) noexcept
{
    if (Alignment == 0 ||
        Value > std::numeric_limits<Core::uint64>::max() - (Alignment - 1))
        return false;
    Out = (Value + Alignment - 1) / Alignment * Alignment;
    return true;
}

} // namespace

RHI::ERHIResult EncodeMetalBlitCommand(
    void* NativeCommandBuffer,
    const FMetalCommandRecord& Record) noexcept
{
    if (Record.Type == RHI::ERHISymbolicCommandType::Barrier ||
        Record.Type == RHI::ERHISymbolicCommandType::LayoutTransition)
        return RHI::ERHIResult::Success;
    id<MTLCommandBuffer> CommandBuffer =
        (__bridge id<MTLCommandBuffer>)NativeCommandBuffer;
    id<MTLBlitCommandEncoder> Encoder = [CommandBuffer blitCommandEncoder];
    if (!Encoder) return RHI::ERHIResult::Failed;
    const auto Fail = [Encoder](RHI::ERHIResult Result) {
        [Encoder endEncoding];
        return Result;
    };
    if (Record.Type == RHI::ERHISymbolicCommandType::BufferCopy)
    {
        const auto Source = std::dynamic_pointer_cast<FMetalBuffer>(Record.BufferA);
        const auto Destination =
            std::dynamic_pointer_cast<FMetalBuffer>(Record.BufferB);
        if (!Source || !Destination) return Fail(RHI::ERHIResult::InvalidState);
        [Encoder copyFromBuffer:Source->GetNativeBuffer()
                   sourceOffset:Record.BufferCopy.SourceOffsetBytes
                       toBuffer:Destination->GetNativeBuffer()
              destinationOffset:Record.BufferCopy.DestinationOffsetBytes
                           size:Record.BufferCopy.SizeBytes];
    }
    else if (Record.Type == RHI::ERHISymbolicCommandType::TextureCopy)
    {
        const auto Source = std::dynamic_pointer_cast<FMetalTexture>(Record.TextureA);
        const auto Destination =
            std::dynamic_pointer_cast<FMetalTexture>(Record.TextureB);
        if (!Source || !Destination) return Fail(RHI::ERHIResult::InvalidState);
        const auto& Region = Record.TextureCopy;
        [Encoder copyFromTexture:Source->GetNativeTexture()
                     sourceSlice:Region.SourceArrayLayer
                     sourceLevel:Region.SourceMipLevel
                    sourceOrigin:MTLOriginMake(
                        Region.SourceX, Region.SourceY, Region.SourceZ)
                      sourceSize:MTLSizeMake(
                        Region.Width, Region.Height, Region.Depth)
                       toTexture:Destination->GetNativeTexture()
                destinationSlice:Region.DestinationArrayLayer
                destinationLevel:Region.DestinationMipLevel
               destinationOrigin:MTLOriginMake(
                    Region.DestinationX, Region.DestinationY,
                    Region.DestinationZ)];
    }
    else if (Record.Type == RHI::ERHISymbolicCommandType::TextureToBufferCopy)
    {
        const auto Source = std::dynamic_pointer_cast<FMetalTexture>(Record.TextureA);
        const auto Destination =
            std::dynamic_pointer_cast<FMetalBuffer>(Record.BufferA);
        if (!Source || !Destination) return Fail(RHI::ERHIResult::InvalidState);
        const auto& Region = Record.TextureToBufferCopy;
        RHI::FRHITextureFootprint Footprint;
        const Core::uint32 RowTexels = Region.DestinationRowLengthTexels == 0
            ? Region.Width : Region.DestinationRowLengthTexels;
        const Core::uint32 ImageRows = Region.DestinationImageHeightTexels == 0
            ? Region.Height : Region.DestinationImageHeightTexels;
        RHI::FRHITextureFootprint RegionFootprint;
        if (!RHI::TryGetRHITextureFootprint(
                Source->GetFormat(), RowTexels, ImageRows, 1, Footprint) ||
            !RHI::TryGetRHITextureFootprint(
                Source->GetFormat(), Region.Width, Region.Height,
                Region.Depth, RegionFootprint))
            return Fail(RHI::ERHIResult::Unsupported);
        const Core::uint64 DestinationRowBytes = Footprint.TightRowBytes;
        const Core::uint64 DestinationImageBytes =
            DestinationRowBytes * Footprint.BlockCountY;
        Core::uint64 MetalRowBytes = 0;
        if (!AlignUp(RegionFootprint.TightRowBytes, 256, MetalRowBytes) ||
            RegionFootprint.BlockCountY >
                std::numeric_limits<Core::uint64>::max() / MetalRowBytes)
            return Fail(RHI::ERHIResult::InvalidState);
        const Core::uint64 MetalImageBytes =
            MetalRowBytes * RegionFootprint.BlockCountY;
        if (RegionFootprint.BlockCountZ >
                std::numeric_limits<Core::uint64>::max() / MetalImageBytes)
            return Fail(RHI::ERHIResult::InvalidState);
        const Core::uint64 MetalTotalBytes =
            MetalImageBytes * RegionFootprint.BlockCountZ;

        id<MTLBuffer> CopyTarget = Destination->GetNativeBuffer();
        Core::uint64 CopyOffset = Region.DestinationOffsetBytes;
        id<MTLBuffer> Staging = nil;
        if (DestinationRowBytes != MetalRowBytes)
        {
            if (MetalTotalBytes > std::numeric_limits<NSUInteger>::max())
                return Fail(RHI::ERHIResult::InvalidState);
            Staging = [CopyTarget.device
                newBufferWithLength:static_cast<NSUInteger>(MetalTotalBytes)
                            options:MTLResourceStorageModePrivate];
            if (!Staging) return Fail(RHI::ERHIResult::Failed);
            CopyTarget = Staging;
            CopyOffset = 0;
        }
        [Encoder copyFromTexture:Source->GetNativeTexture()
                     sourceSlice:Region.SourceArrayLayer
                     sourceLevel:Region.SourceMipLevel
                    sourceOrigin:MTLOriginMake(
                        Region.SourceX, Region.SourceY, Region.SourceZ)
                      sourceSize:MTLSizeMake(
                        Region.Width, Region.Height, Region.Depth)
                        toBuffer:CopyTarget
               destinationOffset:CopyOffset
          destinationBytesPerRow:static_cast<NSUInteger>(MetalRowBytes)
        destinationBytesPerImage:static_cast<NSUInteger>(MetalImageBytes)];
        if (Staging)
        {
            for (Core::uint64 Z = 0; Z < RegionFootprint.BlockCountZ; ++Z)
            {
                for (Core::uint64 Y = 0; Y < RegionFootprint.BlockCountY; ++Y)
                {
                    [Encoder copyFromBuffer:Staging
                               sourceOffset:Z * MetalImageBytes + Y * MetalRowBytes
                                   toBuffer:Destination->GetNativeBuffer()
                          destinationOffset:Region.DestinationOffsetBytes +
                              Z * DestinationImageBytes + Y * DestinationRowBytes
                                       size:RegionFootprint.TightRowBytes];
                }
            }
        }
    }
    else return Fail(RHI::ERHIResult::InvalidState);
    [Encoder endEncoding];
    return RHI::ERHIResult::Success;
}

} // namespace Stoner::Backend::Metal::Private
