#pragma once

#include "RHI/ERHIPipelineState.h"
#include "RHI/ERHIResult.h"
#include "RHI/FRHITextureDesc.h"

namespace Stoner::RHI
{

class IRHITexture
{
public:
    virtual ~IRHITexture() = default;

    [[nodiscard]] virtual const FRHITextureDesc& GetDesc() const noexcept = 0;
    [[nodiscard]] virtual ERHITextureDimension GetDimension() const noexcept = 0;
    [[nodiscard]] virtual ERHIFormat GetFormat() const noexcept = 0;
    [[nodiscard]] virtual ERHITextureUsage GetUsage() const noexcept = 0;
    [[nodiscard]] virtual ERHIResourceLifecycleState GetLifecycleState() const noexcept = 0;

    virtual ERHIResult Invalidate() = 0;
};

} // namespace Stoner::RHI
