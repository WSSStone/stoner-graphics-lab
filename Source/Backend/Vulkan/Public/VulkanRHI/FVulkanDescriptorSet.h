#pragma once

#include "VulkanRHI/FVulkanDescriptorPool.h"
#include "RHI/ERHIDescriptorType.h"
#include "RHI/IRHIDescriptorSet.h"

#include <map>
#include <memory>

namespace Stoner::Backend::Vulkan
{

struct FVulkanDescriptorBindingKey
{
    Stoner::Core::uint32 BindingSlot = 0;
    Stoner::Core::uint32 ArrayIndex = 0;

    [[nodiscard]] bool operator<(const FVulkanDescriptorBindingKey& Other) const noexcept;
};

struct FVulkanBoundResourceRecord
{
    Stoner::RHI::ERHIDescriptorResourceKind Kind = Stoner::RHI::ERHIDescriptorResourceKind::None;
    Stoner::Core::TWeakPtr<Stoner::RHI::IRHIBuffer> Buffer;
    Stoner::Core::TWeakPtr<Stoner::RHI::IRHITexture> Texture;
    Stoner::Core::TWeakPtr<Stoner::RHI::IRHISampler> Sampler;
};

class FVulkanDescriptorSet final : public Stoner::RHI::IRHIDescriptorSet
{
public:
    ~FVulkanDescriptorSet() override;
    FVulkanDescriptorSet(const FVulkanDescriptorSet&) = delete;
    FVulkanDescriptorSet& operator=(const FVulkanDescriptorSet&) = delete;

    [[nodiscard]] Stoner::Core::uint32 GetSetIndex() const noexcept override;
    [[nodiscard]] Stoner::Core::TSharedPtr<Stoner::RHI::IRHIPipelineLayout> GetPipelineLayout() const noexcept override;
    [[nodiscard]] Stoner::RHI::ERHIDescriptorResourceKind GetBoundResourceKind(Stoner::Core::uint32 BindingSlot, Stoner::Core::uint32 ArrayIndex = 0) const noexcept override;
    [[nodiscard]] Stoner::Core::uint32 GetBoundResourceCount() const noexcept override;
    [[nodiscard]] Stoner::RHI::ERHIResourceLifecycleState GetLifecycleState() const noexcept override;
    [[nodiscard]] bool IsBoundResourceValid(Stoner::Core::uint32 BindingSlot, Stoner::Core::uint32 ArrayIndex = 0) const noexcept;

    Stoner::RHI::ERHIResult UpdateBuffer(Stoner::Core::uint32 BindingSlot, Stoner::Core::uint32 ArrayIndex, const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer>& Buffer) override;
    Stoner::RHI::ERHIResult UpdateTexture(Stoner::Core::uint32 BindingSlot, Stoner::Core::uint32 ArrayIndex, const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& Texture) override;
    Stoner::RHI::ERHIResult UpdateSampler(Stoner::Core::uint32 BindingSlot, Stoner::Core::uint32 ArrayIndex, const Stoner::Core::TSharedPtr<Stoner::RHI::IRHISampler>& Sampler) override;
    Stoner::RHI::ERHIResult UpdateCombinedTextureSampler(Stoner::Core::uint32 BindingSlot, Stoner::Core::uint32 ArrayIndex, const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& Texture, const Stoner::Core::TSharedPtr<Stoner::RHI::IRHISampler>& Sampler) override;
    Stoner::RHI::ERHIResult Invalidate() override;

private:
    friend class FVulkanDevice;
    friend class FVulkanNativeContext;

    FVulkanDescriptorSet(
        const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIPipelineLayout>& InLayout,
        Stoner::Core::uint32 InSetIndex,
        FVulkanDescriptorReservation&& InReservation) noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult ValidateBinding(Stoner::Core::uint32 BindingSlot, Stoner::Core::uint32 ArrayIndex, Stoner::RHI::ERHIDescriptorType ExpectedType) const noexcept;

    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIPipelineLayout> Layout;
    Stoner::Core::uint32 SetIndex = 0;
    FVulkanDescriptorReservation Reservation;
    std::map<FVulkanDescriptorBindingKey, FVulkanBoundResourceRecord> Records;
    Stoner::RHI::ERHIResourceLifecycleState LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Valid;
};

} // namespace Stoner::Backend::Vulkan
