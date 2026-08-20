#include "FMetalSampler.h"

namespace Stoner::Backend::Metal::Private
{

FMetalSampler::FMetalSampler(
    Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
    const RHI::FRHISamplerDesc& Desc,
    id<MTLSamplerState> Sampler) noexcept
    : FMetalNativeObject(
          std::move(Owner), EMetalOwnershipCategory::Resource),
      Desc_(Desc), Sampler_(Sampler)
{
}

FMetalSampler::~FMetalSampler()
{
    (void)Invalidate();
}

const RHI::FRHISamplerDesc& FMetalSampler::GetDesc() const noexcept
{
    return Desc_;
}

RHI::ERHIResourceLifecycleState FMetalSampler::GetLifecycleState()
    const noexcept
{
    return GetLifecycle();
}

RHI::ERHIResult FMetalSampler::Invalidate()
{
    return InvalidateObject();
}

id<MTLSamplerState> FMetalSampler::GetNativeSampler() const noexcept
{
    return Sampler_;
}

} // namespace Stoner::Backend::Metal::Private
