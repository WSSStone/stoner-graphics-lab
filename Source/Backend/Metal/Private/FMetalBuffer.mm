#include "FMetalBuffer.h"

#include <cstring>

namespace Stoner::Backend::Metal::Private
{

FMetalBuffer::FMetalBuffer(
    Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
    const RHI::FRHIBufferDesc& Desc,
    id<MTLBuffer> Buffer) noexcept
    : FMetalNativeObject(
          std::move(Owner), EMetalOwnershipCategory::Resource),
      Desc_(Desc), Buffer_(Buffer)
{
}

FMetalBuffer::~FMetalBuffer()
{
    (void)Invalidate();
}

const RHI::FRHIBufferDesc& FMetalBuffer::GetDesc() const noexcept
{
    return Desc_;
}

Core::uint64 FMetalBuffer::GetSizeInBytes() const noexcept
{
    return Desc_.SizeInBytes;
}

RHI::ERHIBufferUsage FMetalBuffer::GetUsage() const noexcept
{
    return Desc_.Usage;
}

RHI::ERHIResourceLifecycleState FMetalBuffer::GetLifecycleState()
    const noexcept
{
    return GetLifecycle();
}

RHI::ERHIResult FMetalBuffer::Invalidate()
{
    return InvalidateObject();
}

RHI::ERHIResult FMetalBuffer::Upload(
    const void* Data,
    Core::uint64 Size,
    Core::uint64 Offset)
{
    if (GetLifecycle() != RHI::ERHIResourceLifecycleState::Valid ||
        Buffer_ == nil || Data == nullptr || Size == 0 ||
        Offset > Desc_.SizeInBytes || Size > Desc_.SizeInBytes - Offset)
        return RHI::ERHIResult::InvalidState;
    if (Buffer_.storageMode == MTLStorageModePrivate)
        return RHI::ERHIResult::Unsupported;
    std::memcpy(
        static_cast<unsigned char*>(Buffer_.contents) + Offset,
        Data, static_cast<std::size_t>(Size));
    if (Buffer_.storageMode == MTLStorageModeManaged)
        [Buffer_ didModifyRange:NSMakeRange(
            static_cast<NSUInteger>(Offset), static_cast<NSUInteger>(Size))];
    return RHI::ERHIResult::Success;
}

id<MTLBuffer> FMetalBuffer::GetNativeBuffer() const noexcept
{
    return Buffer_;
}

} // namespace Stoner::Backend::Metal::Private
