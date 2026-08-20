#pragma once

#include "FMetalNativeObject.h"
#include "RHI/IRHISampler.h"

#import <Metal/Metal.h>

namespace Stoner::Backend::Metal::Private
{

class FMetalSampler final : public RHI::IRHISampler, public FMetalNativeObject
{
public:
    FMetalSampler(
        Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
        const RHI::FRHISamplerDesc& Desc,
        id<MTLSamplerState> Sampler) noexcept;
    ~FMetalSampler() override;

    [[nodiscard]] const RHI::FRHISamplerDesc& GetDesc() const noexcept override;
    [[nodiscard]] RHI::ERHIResourceLifecycleState GetLifecycleState()
        const noexcept override;
    RHI::ERHIResult Invalidate() override;
    [[nodiscard]] id<MTLSamplerState> GetNativeSampler() const noexcept;

private:
    RHI::FRHISamplerDesc Desc_;
    __strong id<MTLSamplerState> Sampler_;
};

} // namespace Stoner::Backend::Metal::Private
