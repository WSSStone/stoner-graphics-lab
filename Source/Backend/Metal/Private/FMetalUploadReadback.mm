#include "FMetalUploadReadback.h"

#include "RHI/FRHIFormatInfo.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>

namespace Stoner::Backend::Metal::Private
{
namespace
{

bool Complete(id<MTLCommandBuffer> CommandBuffer) noexcept
{
    [CommandBuffer commit];
    [CommandBuffer waitUntilCompleted];
    return CommandBuffer.status == MTLCommandBufferStatusCompleted;
}

bool AlignUp(Core::uint64 Value, Core::uint64 Alignment, Core::uint64& Out) noexcept
{
    if (Alignment == 0 || Value >
        std::numeric_limits<Core::uint64>::max() - (Alignment - 1))
        return false;
    Out = (Value + Alignment - 1) / Alignment * Alignment;
    return true;
}

} // namespace

RHI::ERHIResult UploadMetalPrivateBuffer(
    id<MTLCommandQueue> Queue,
    id<MTLBuffer> Destination,
    const void* Data,
    Core::uint64 Size,
    Core::uint64 Offset) noexcept
{
    if (Queue == nil || Destination == nil || Data == nullptr || Size == 0 ||
        Offset > Destination.length || Size > Destination.length - Offset ||
        Size > std::numeric_limits<NSUInteger>::max())
        return RHI::ERHIResult::InvalidState;
    @autoreleasepool
    {
        id<MTLBuffer> Staging = [Destination.device
            newBufferWithBytes:Data
                      length:static_cast<NSUInteger>(Size)
                     options:MTLResourceStorageModeShared];
        id<MTLCommandBuffer> Commands = [Queue commandBuffer];
        id<MTLBlitCommandEncoder> Blit = [Commands blitCommandEncoder];
        if (Staging == nil || Commands == nil || Blit == nil)
            return RHI::ERHIResult::Failed;
        [Blit copyFromBuffer:Staging sourceOffset:0
                    toBuffer:Destination destinationOffset:Offset size:Size];
        [Blit endEncoding];
        return Complete(Commands)
            ? RHI::ERHIResult::Success
            : RHI::ERHIResult::Failed;
    }
}

RHI::ERHIResult UploadMetalTexture(
    id<MTLCommandQueue> Queue,
    id<MTLTexture> Destination,
    const RHI::FRHITextureDesc& TextureDesc,
    const RHI::FRHITextureUploadDesc& Upload) noexcept
{
    if (Queue == nil || Destination == nil ||
        !RHI::IsValidRHITextureUploadDesc(TextureDesc, Upload))
        return RHI::ERHIResult::InvalidState;
    RHI::FRHITextureFootprint Footprint;
    if (!RHI::TryGetRHITextureFootprint(
            TextureDesc.Format, Upload.Width, Upload.Height,
            Upload.Depth, Footprint))
        return RHI::ERHIResult::InvalidState;
    Core::uint64 AlignedRow = 0;
    if (!AlignUp(Footprint.TightRowBytes, 256, AlignedRow) ||
        Footprint.BlockCountY >
            std::numeric_limits<Core::uint64>::max() / AlignedRow)
        return RHI::ERHIResult::InvalidState;
    const Core::uint64 BytesPerImage = AlignedRow * Footprint.BlockCountY;
    if (Footprint.BlockCountZ >
        std::numeric_limits<Core::uint64>::max() / BytesPerImage)
        return RHI::ERHIResult::InvalidState;
    const Core::uint64 Total = BytesPerImage * Footprint.BlockCountZ;
    if (Total > std::numeric_limits<NSUInteger>::max())
        return RHI::ERHIResult::InvalidState;
    try
    {
        Core::TArray<Core::uint8> Repacked(static_cast<Core::usize>(Total), 0);
        const auto* Source = static_cast<const Core::uint8*>(Upload.Data);
        for (Core::uint64 Z = 0; Z < Footprint.BlockCountZ; ++Z)
        {
            for (Core::uint64 Y = 0; Y < Footprint.BlockCountY; ++Y)
            {
                std::memcpy(
                    Repacked.data() + Z * BytesPerImage + Y * AlignedRow,
                    Source + (Z * Footprint.BlockCountY + Y) *
                        Upload.RowPitchBytes,
                    static_cast<std::size_t>(Footprint.TightRowBytes));
            }
        }
        @autoreleasepool
        {
            id<MTLBuffer> Staging = [Destination.device
                newBufferWithBytes:Repacked.data()
                          length:static_cast<NSUInteger>(Total)
                         options:MTLResourceStorageModeShared];
            id<MTLCommandBuffer> Commands = [Queue commandBuffer];
            id<MTLBlitCommandEncoder> Blit = [Commands blitCommandEncoder];
            if (Staging == nil || Commands == nil || Blit == nil)
                return RHI::ERHIResult::Failed;
            const MTLOrigin Origin = MTLOriginMake(Upload.X, Upload.Y, Upload.Z);
            const MTLSize Size = MTLSizeMake(
                Upload.Width, Upload.Height, Upload.Depth);
            [Blit copyFromBuffer:Staging
                    sourceOffset:0
               sourceBytesPerRow:static_cast<NSUInteger>(AlignedRow)
             sourceBytesPerImage:static_cast<NSUInteger>(BytesPerImage)
                      sourceSize:Size
                       toTexture:Destination
                destinationSlice:Upload.ArrayLayer
                destinationLevel:Upload.MipLevel
               destinationOrigin:Origin];
            [Blit endEncoding];
            return Complete(Commands)
                ? RHI::ERHIResult::Success
                : RHI::ERHIResult::Failed;
        }
    }
    catch (const std::bad_alloc&)
    {
        return RHI::ERHIResult::Failed;
    }
}

RHI::ERHIResult ReadbackMetalBuffer(
    id<MTLCommandQueue> Queue,
    id<MTLBuffer> Source,
    Core::uint64 Offset,
    Core::uint64 Size,
    Core::TArray<Core::uint8>& OutBytes) noexcept
{
    OutBytes.clear();
    if (Queue == nil || Source == nil || Size == 0 ||
        Offset > Source.length || Size > Source.length - Offset ||
        Size > std::numeric_limits<NSUInteger>::max())
        return RHI::ERHIResult::InvalidState;
    try
    {
        @autoreleasepool
        {
            const auto CopySourceBytes = [&]()
            {
                if (Source.contents == nullptr)
                    return RHI::ERHIResult::Failed;
                OutBytes.resize(static_cast<Core::usize>(Size));
                std::memcpy(
                    OutBytes.data(),
                    static_cast<const Core::uint8*>(Source.contents) +
                        static_cast<NSUInteger>(Offset),
                    static_cast<std::size_t>(Size));
                return RHI::ERHIResult::Success;
            };
            if (Source.storageMode == MTLStorageModeShared)
                return CopySourceBytes();
            if (Source.storageMode == MTLStorageModeManaged)
            {
                id<MTLCommandBuffer> Commands = [Queue commandBuffer];
                id<MTLBlitCommandEncoder> Blit = [Commands blitCommandEncoder];
                if (Commands == nil || Blit == nil)
                    return RHI::ERHIResult::Failed;
                [Blit synchronizeResource:Source];
                [Blit endEncoding];
                return Complete(Commands)
                    ? CopySourceBytes()
                    : RHI::ERHIResult::Failed;
            }
            if (Source.storageMode != MTLStorageModePrivate)
                return RHI::ERHIResult::Unsupported;
            id<MTLBuffer> Readback = [Source.device
                newBufferWithLength:static_cast<NSUInteger>(Size)
                            options:MTLResourceStorageModeShared];
            id<MTLCommandBuffer> Commands = [Queue commandBuffer];
            id<MTLBlitCommandEncoder> Blit = [Commands blitCommandEncoder];
            if (Readback == nil || Commands == nil || Blit == nil)
                return RHI::ERHIResult::Failed;
            [Blit copyFromBuffer:Source sourceOffset:Offset
                        toBuffer:Readback destinationOffset:0 size:Size];
            [Blit endEncoding];
            if (!Complete(Commands)) return RHI::ERHIResult::Failed;
            OutBytes.resize(static_cast<Core::usize>(Size));
            std::memcpy(
                OutBytes.data(), Readback.contents,
                static_cast<std::size_t>(Size));
            return RHI::ERHIResult::Success;
        }
    }
    catch (const std::bad_alloc&)
    {
        OutBytes.clear();
        return RHI::ERHIResult::Failed;
    }
}

RHI::ERHIResult ReadbackMetalTexture(
    id<MTLCommandQueue> Queue,
    id<MTLTexture> Source,
    Core::uint64 TightRowBytes,
    Core::uint32 Height,
    Core::TArray<Core::uint8>& OutBytes) noexcept
{
    OutBytes.clear();
    if (Queue == nil || Source == nil || TightRowBytes == 0 || Height == 0 ||
        Source.width == 0 || Source.height != Height || Source.depth != 1 ||
        Source.sampleCount != 1 ||
        TightRowBytes > std::numeric_limits<NSUInteger>::max())
        return RHI::ERHIResult::InvalidState;
    @autoreleasepool
    {
        id<MTLDevice> Device = Source.device;
        const NSUInteger Alignment = std::max<NSUInteger>(1,
            [Device minimumLinearTextureAlignmentForPixelFormat:
                Source.pixelFormat]);
        const NSUInteger Tight = static_cast<NSUInteger>(TightRowBytes);
        if (Tight > std::numeric_limits<NSUInteger>::max() -
                (Alignment - 1))
            return RHI::ERHIResult::Unavailable;
        const NSUInteger Padded =
            ((Tight + Alignment - 1) / Alignment) * Alignment;
        if (Padded == 0 || Height >
                std::numeric_limits<NSUInteger>::max() / Padded)
            return RHI::ERHIResult::Unavailable;
        const NSUInteger ByteCount = Padded * Height;
        id<MTLBuffer> Buffer = [Device newBufferWithLength:ByteCount
            options:MTLResourceStorageModeShared];
        id<MTLCommandBuffer> Commands = [Queue commandBuffer];
        id<MTLBlitCommandEncoder> Blit = [Commands blitCommandEncoder];
        if (Buffer == nil || Commands == nil || Blit == nil)
            return RHI::ERHIResult::Unavailable;
        [Blit copyFromTexture:Source sourceSlice:0 sourceLevel:0
            sourceOrigin:MTLOriginMake(0, 0, 0)
            sourceSize:MTLSizeMake(Source.width, Source.height, 1)
            toBuffer:Buffer destinationOffset:0
            destinationBytesPerRow:Padded
            destinationBytesPerImage:ByteCount];
        [Blit endEncoding];
        [Commands commit];
        [Commands waitUntilCompleted];
        if (Commands.status != MTLCommandBufferStatusCompleted)
            return RHI::ERHIResult::Failed;
        try
        {
            OutBytes.resize(static_cast<Core::usize>(Tight) * Height);
        }
        catch (...)
        {
            return RHI::ERHIResult::Unavailable;
        }
        const auto* SourceBytes = static_cast<const Core::uint8*>(
            Buffer.contents);
        for (Core::uint32 Row = 0; Row < Height; ++Row)
        {
            std::memcpy(
                OutBytes.data() + static_cast<Core::usize>(Row) * Tight,
                SourceBytes + static_cast<Core::usize>(Row) * Padded,
                Tight);
        }
        return RHI::ERHIResult::Success;
    }
}

} // namespace Stoner::Backend::Metal::Private
