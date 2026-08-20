#include "FMetalDescriptorSet.h"

#include "FMetalBuffer.h"
#include "FMetalPipelineLayout.h"
#include "FMetalSampler.h"
#include "FMetalTexture.h"

namespace Stoner::Backend::Metal::Private
{

FMetalDescriptorSet::FMetalDescriptorSet(
    Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
    Core::TSharedPtr<RHI::IRHIPipelineLayout> Layout,
    Core::uint32 SetIndex) noexcept
    : FMetalNativeObject(
          std::move(Owner), EMetalOwnershipCategory::Resource),
      Layout_(std::move(Layout)), SetIndex_(SetIndex)
{
}

FMetalDescriptorSet::~FMetalDescriptorSet() { (void)Invalidate(); }
Core::uint32 FMetalDescriptorSet::GetSetIndex() const noexcept { return SetIndex_; }
Core::TSharedPtr<RHI::IRHIPipelineLayout>
FMetalDescriptorSet::GetPipelineLayout() const noexcept { return Layout_; }

RHI::ERHIDescriptorResourceKind FMetalDescriptorSet::GetBoundResourceKind(
    Core::uint32 BindingSlot, Core::uint32 ArrayIndex) const noexcept
{
    std::lock_guard Lock(Mutex_);
    const auto It = Resources_.find({BindingSlot, ArrayIndex});
    return It == Resources_.end()
        ? RHI::ERHIDescriptorResourceKind::None : It->second.Kind;
}

Core::uint32 FMetalDescriptorSet::GetBoundResourceCount() const noexcept
{
    std::lock_guard Lock(Mutex_);
    return static_cast<Core::uint32>(Resources_.size());
}

RHI::ERHIResourceLifecycleState FMetalDescriptorSet::GetLifecycleState()
    const noexcept { return GetLifecycle(); }

const RHI::FRHIDescriptorBinding* FMetalDescriptorSet::ValidateSlot(
    Core::uint32 BindingSlot, Core::uint32 ArrayIndex,
    RHI::ERHIDescriptorType Expected) const noexcept
{
    if (GetLifecycle() != RHI::ERHIResourceLifecycleState::Valid || !Layout_ ||
        Layout_->GetLifecycleState() != RHI::ERHIResourceLifecycleState::Valid)
        return nullptr;
    const auto* Binding = Layout_->FindBinding(SetIndex_, BindingSlot);
    return Binding && Binding->DescriptorType == Expected &&
        ArrayIndex < Binding->ArrayCount ? Binding : nullptr;
}

RHI::ERHIResult FMetalDescriptorSet::UpdateBuffer(
    Core::uint32 Slot, Core::uint32 Array,
    const Core::TSharedPtr<RHI::IRHIBuffer>& Buffer)
{
    const auto Native = std::dynamic_pointer_cast<FMetalBuffer>(Buffer);
    const bool bUniform = ValidateSlot(
        Slot, Array, RHI::ERHIDescriptorType::UniformBuffer) != nullptr;
    const bool bStorage = ValidateSlot(
        Slot, Array, RHI::ERHIDescriptorType::StorageBuffer) != nullptr;
    if (!Native || !Native->IsCompatible(GetOwner()) ||
        (!bUniform && !bStorage))
        return RHI::ERHIResult::InvalidState;
    std::lock_guard Lock(Mutex_);
    Resources_[{Slot, Array}] = {
        RHI::ERHIDescriptorResourceKind::Buffer, Buffer, nullptr, nullptr};
    return RHI::ERHIResult::Success;
}

RHI::ERHIResult FMetalDescriptorSet::UpdateTexture(
    Core::uint32 Slot, Core::uint32 Array,
    const Core::TSharedPtr<RHI::IRHITexture>& Texture)
{
    const auto Native = std::dynamic_pointer_cast<FMetalTexture>(Texture);
    const bool bSampled = ValidateSlot(
        Slot, Array, RHI::ERHIDescriptorType::SampledTexture) != nullptr;
    const bool bStorage = ValidateSlot(
        Slot, Array, RHI::ERHIDescriptorType::StorageTexture) != nullptr;
    if (!Native || !Native->IsCompatible(GetOwner()) ||
        (!bSampled && !bStorage))
        return RHI::ERHIResult::InvalidState;
    std::lock_guard Lock(Mutex_);
    Resources_[{Slot, Array}] = {
        RHI::ERHIDescriptorResourceKind::Texture, nullptr, Texture, nullptr};
    return RHI::ERHIResult::Success;
}

RHI::ERHIResult FMetalDescriptorSet::UpdateSampler(
    Core::uint32 Slot, Core::uint32 Array,
    const Core::TSharedPtr<RHI::IRHISampler>& Sampler)
{
    const auto Native = std::dynamic_pointer_cast<FMetalSampler>(Sampler);
    if (!Native || !Native->IsCompatible(GetOwner()) ||
        !ValidateSlot(Slot, Array, RHI::ERHIDescriptorType::Sampler))
        return RHI::ERHIResult::InvalidState;
    std::lock_guard Lock(Mutex_);
    Resources_[{Slot, Array}] = {
        RHI::ERHIDescriptorResourceKind::Sampler, nullptr, nullptr, Sampler};
    return RHI::ERHIResult::Success;
}

RHI::ERHIResult FMetalDescriptorSet::UpdateCombinedTextureSampler(
    Core::uint32 Slot, Core::uint32 Array,
    const Core::TSharedPtr<RHI::IRHITexture>& Texture,
    const Core::TSharedPtr<RHI::IRHISampler>& Sampler)
{
    const auto NativeTexture = std::dynamic_pointer_cast<FMetalTexture>(Texture);
    const auto NativeSampler = std::dynamic_pointer_cast<FMetalSampler>(Sampler);
    if (!NativeTexture || !NativeSampler ||
        !NativeTexture->IsCompatible(GetOwner()) ||
        !NativeSampler->IsCompatible(GetOwner()) ||
        !ValidateSlot(Slot, Array, RHI::ERHIDescriptorType::CombinedTextureSampler))
        return RHI::ERHIResult::InvalidState;
    std::lock_guard Lock(Mutex_);
    Resources_[{Slot, Array}] = {
        RHI::ERHIDescriptorResourceKind::CombinedTextureSampler,
        nullptr, Texture, Sampler};
    return RHI::ERHIResult::Success;
}

RHI::ERHIResult FMetalDescriptorSet::Invalidate()
{
    return InvalidateObject();
}

FMetalDescriptorSnapshot FMetalDescriptorSet::Snapshot() const
{
    std::lock_guard Lock(Mutex_);
    return Resources_;
}

} // namespace Stoner::Backend::Metal::Private
