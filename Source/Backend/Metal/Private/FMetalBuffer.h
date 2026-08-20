#pragma once

#include "FMetalNativeObject.h"
#include "RHI/IRHIBuffer.h"

#import <Metal/Metal.h>

namespace Stoner::Backend::Metal::Private
{

class FMetalBuffer final : public RHI::IRHIBuffer, public FMetalNativeObject
{
public:
    FMetalBuffer(
        Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
        const RHI::FRHIBufferDesc& Desc,
        id<MTLBuffer> Buffer) noexcept;
    ~FMetalBuffer() override;

    [[nodiscard]] const RHI::FRHIBufferDesc& GetDesc() const noexcept override;
    [[nodiscard]] Core::uint64 GetSizeInBytes() const noexcept override;
    [[nodiscard]] RHI::ERHIBufferUsage GetUsage() const noexcept override;
    [[nodiscard]] RHI::ERHIResourceLifecycleState GetLifecycleState()
        const noexcept override;
    RHI::ERHIResult Invalidate() override;
    RHI::ERHIResult Upload(
        const void* Data,
        Core::uint64 Size,
        Core::uint64 Offset = 0) override;

    [[nodiscard]] id<MTLBuffer> GetNativeBuffer() const noexcept;

private:
    RHI::FRHIBufferDesc Desc_;
    __strong id<MTLBuffer> Buffer_;
};

} // namespace Stoner::Backend::Metal::Private
