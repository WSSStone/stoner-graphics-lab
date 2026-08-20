#pragma once

#include "FMetalNativeObject.h"
#include "RHI/ERHIDescriptorType.h"
#include "RHI/FRHIDescriptorBinding.h"
#include "RHI/IRHIDescriptorSet.h"

#include <map>
#include <mutex>
#include <tuple>

namespace Stoner::Backend::Metal::Private
{

struct FMetalDescriptorResource
{
    RHI::ERHIDescriptorResourceKind Kind =
        RHI::ERHIDescriptorResourceKind::None;
    Core::TSharedPtr<RHI::IRHIBuffer> Buffer;
    Core::TSharedPtr<RHI::IRHITexture> Texture;
    Core::TSharedPtr<RHI::IRHISampler> Sampler;
};

using FMetalDescriptorSnapshot =
    std::map<std::pair<Core::uint32, Core::uint32>, FMetalDescriptorResource>;

class FMetalDescriptorSet final
    : public RHI::IRHIDescriptorSet,
      public FMetalNativeObject
{
public:
    FMetalDescriptorSet(
        Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
        Core::TSharedPtr<RHI::IRHIPipelineLayout> Layout,
        Core::uint32 SetIndex) noexcept;
    ~FMetalDescriptorSet() override;

    [[nodiscard]] Core::uint32 GetSetIndex() const noexcept override;
    [[nodiscard]] Core::TSharedPtr<RHI::IRHIPipelineLayout>
    GetPipelineLayout() const noexcept override;
    [[nodiscard]] RHI::ERHIDescriptorResourceKind GetBoundResourceKind(
        Core::uint32 BindingSlot,
        Core::uint32 ArrayIndex = 0) const noexcept override;
    [[nodiscard]] Core::uint32 GetBoundResourceCount() const noexcept override;
    [[nodiscard]] RHI::ERHIResourceLifecycleState GetLifecycleState()
        const noexcept override;
    RHI::ERHIResult UpdateBuffer(
        Core::uint32 BindingSlot, Core::uint32 ArrayIndex,
        const Core::TSharedPtr<RHI::IRHIBuffer>& Buffer) override;
    RHI::ERHIResult UpdateTexture(
        Core::uint32 BindingSlot, Core::uint32 ArrayIndex,
        const Core::TSharedPtr<RHI::IRHITexture>& Texture) override;
    RHI::ERHIResult UpdateSampler(
        Core::uint32 BindingSlot, Core::uint32 ArrayIndex,
        const Core::TSharedPtr<RHI::IRHISampler>& Sampler) override;
    RHI::ERHIResult UpdateCombinedTextureSampler(
        Core::uint32 BindingSlot, Core::uint32 ArrayIndex,
        const Core::TSharedPtr<RHI::IRHITexture>& Texture,
        const Core::TSharedPtr<RHI::IRHISampler>& Sampler) override;
    RHI::ERHIResult Invalidate() override;

    [[nodiscard]] FMetalDescriptorSnapshot Snapshot() const;

private:
    [[nodiscard]] const RHI::FRHIDescriptorBinding* ValidateSlot(
        Core::uint32 BindingSlot,
        Core::uint32 ArrayIndex,
        RHI::ERHIDescriptorType Expected) const noexcept;

    Core::TSharedPtr<RHI::IRHIPipelineLayout> Layout_;
    Core::uint32 SetIndex_ = 0;
    mutable std::mutex Mutex_;
    FMetalDescriptorSnapshot Resources_;
};

} // namespace Stoner::Backend::Metal::Private
