#include "VulkanRHI/FVulkanDescriptorSet.h"

#include "RHI/IRHIBuffer.h"
#include "RHI/IRHIPipelineLayout.h"
#include "RHI/IRHISampler.h"
#include "RHI/IRHITexture.h"

namespace Stoner::Backend::Vulkan
{

namespace
{

[[nodiscard]] bool IsResourceValid(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer>& Buffer) noexcept
{
    return Buffer && Buffer->GetLifecycleState() == Stoner::RHI::ERHIResourceLifecycleState::Valid;
}

[[nodiscard]] bool IsResourceValid(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& Texture) noexcept
{
    return Texture && Texture->GetLifecycleState() == Stoner::RHI::ERHIResourceLifecycleState::Valid;
}

[[nodiscard]] bool IsResourceValid(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHISampler>& Sampler) noexcept
{
    return Sampler && Sampler->GetLifecycleState() == Stoner::RHI::ERHIResourceLifecycleState::Valid;
}

} // namespace

bool FVulkanDescriptorBindingKey::operator<(const FVulkanDescriptorBindingKey& Other) const noexcept
{
    if (BindingSlot != Other.BindingSlot)
    {
        return BindingSlot < Other.BindingSlot;
    }
    return ArrayIndex < Other.ArrayIndex;
}

FVulkanDescriptorSet::FVulkanDescriptorSet(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIPipelineLayout>& InLayout, Stoner::Core::uint32 InSetIndex, std::shared_ptr<FVulkanDescriptorPool> InPool)
    : Layout(InLayout)
    , SetIndex(InSetIndex)
    , Pool(std::move(InPool))
{
}

FVulkanDescriptorSet::~FVulkanDescriptorSet()
{
    if (!bPoolReleased && Pool)
    {
        (void)Pool->Release();
        bPoolReleased = true;
    }
}

Stoner::Core::uint32 FVulkanDescriptorSet::GetSetIndex() const noexcept { return SetIndex; }
Stoner::Core::TSharedPtr<Stoner::RHI::IRHIPipelineLayout> FVulkanDescriptorSet::GetPipelineLayout() const noexcept { return Layout; }
Stoner::Core::uint32 FVulkanDescriptorSet::GetBoundResourceCount() const noexcept { return static_cast<Stoner::Core::uint32>(Records.size()); }
Stoner::RHI::ERHIResourceLifecycleState FVulkanDescriptorSet::GetLifecycleState() const noexcept { return LifecycleState; }

Stoner::RHI::ERHIDescriptorResourceKind FVulkanDescriptorSet::GetBoundResourceKind(Stoner::Core::uint32 BindingSlot, Stoner::Core::uint32 ArrayIndex) const noexcept
{
    const auto It = Records.find({BindingSlot, ArrayIndex});
    return It == Records.end() ? Stoner::RHI::ERHIDescriptorResourceKind::None : It->second.Kind;
}

bool FVulkanDescriptorSet::IsBoundResourceValid(Stoner::Core::uint32 BindingSlot, Stoner::Core::uint32 ArrayIndex) const noexcept
{
    const auto It = Records.find({BindingSlot, ArrayIndex});
    if (It == Records.end())
    {
        return false;
    }

    const FVulkanBoundResourceRecord& Record = It->second;
    switch (Record.Kind)
    {
    case Stoner::RHI::ERHIDescriptorResourceKind::Buffer:
        return IsResourceValid(Record.Buffer.lock());
    case Stoner::RHI::ERHIDescriptorResourceKind::Texture:
        return IsResourceValid(Record.Texture.lock());
    case Stoner::RHI::ERHIDescriptorResourceKind::Sampler:
        return IsResourceValid(Record.Sampler.lock());
    case Stoner::RHI::ERHIDescriptorResourceKind::CombinedTextureSampler:
        return IsResourceValid(Record.Texture.lock()) && IsResourceValid(Record.Sampler.lock());
    default:
        return false;
    }
}

Stoner::RHI::ERHIResult FVulkanDescriptorSet::UpdateBuffer(Stoner::Core::uint32 BindingSlot, Stoner::Core::uint32 ArrayIndex, const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer>& Buffer)
{
    Stoner::RHI::ERHIResult Result = ValidateBinding(BindingSlot, ArrayIndex, Stoner::RHI::ERHIDescriptorType::UniformBuffer);
    if (Result != Stoner::RHI::ERHIResult::Success)
    {
        Result = ValidateBinding(BindingSlot, ArrayIndex, Stoner::RHI::ERHIDescriptorType::StorageBuffer);
    }
    if (Result != Stoner::RHI::ERHIResult::Success || !IsResourceValid(Buffer))
    {
        return Result == Stoner::RHI::ERHIResult::Success ? Stoner::RHI::ERHIResult::InvalidState : Result;
    }
    Records[{BindingSlot, ArrayIndex}] = {Stoner::RHI::ERHIDescriptorResourceKind::Buffer, Buffer, {}, {}};
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanDescriptorSet::UpdateTexture(Stoner::Core::uint32 BindingSlot, Stoner::Core::uint32 ArrayIndex, const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& Texture)
{
    Stoner::RHI::ERHIResult Result = ValidateBinding(BindingSlot, ArrayIndex, Stoner::RHI::ERHIDescriptorType::SampledTexture);
    if (Result != Stoner::RHI::ERHIResult::Success)
    {
        Result = ValidateBinding(BindingSlot, ArrayIndex, Stoner::RHI::ERHIDescriptorType::StorageTexture);
    }
    if (Result != Stoner::RHI::ERHIResult::Success || !IsResourceValid(Texture))
    {
        return Result == Stoner::RHI::ERHIResult::Success ? Stoner::RHI::ERHIResult::InvalidState : Result;
    }
    Records[{BindingSlot, ArrayIndex}] = {Stoner::RHI::ERHIDescriptorResourceKind::Texture, {}, Texture, {}};
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanDescriptorSet::UpdateSampler(Stoner::Core::uint32 BindingSlot, Stoner::Core::uint32 ArrayIndex, const Stoner::Core::TSharedPtr<Stoner::RHI::IRHISampler>& Sampler)
{
    const Stoner::RHI::ERHIResult Result = ValidateBinding(BindingSlot, ArrayIndex, Stoner::RHI::ERHIDescriptorType::Sampler);
    if (Result != Stoner::RHI::ERHIResult::Success || !IsResourceValid(Sampler))
    {
        return Result == Stoner::RHI::ERHIResult::Success ? Stoner::RHI::ERHIResult::InvalidState : Result;
    }
    Records[{BindingSlot, ArrayIndex}] = {Stoner::RHI::ERHIDescriptorResourceKind::Sampler, {}, {}, Sampler};
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanDescriptorSet::UpdateCombinedTextureSampler(Stoner::Core::uint32 BindingSlot, Stoner::Core::uint32 ArrayIndex, const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& Texture, const Stoner::Core::TSharedPtr<Stoner::RHI::IRHISampler>& Sampler)
{
    const Stoner::RHI::ERHIResult Result = ValidateBinding(BindingSlot, ArrayIndex, Stoner::RHI::ERHIDescriptorType::CombinedTextureSampler);
    if (Result != Stoner::RHI::ERHIResult::Success || !IsResourceValid(Texture) || !IsResourceValid(Sampler))
    {
        return Result == Stoner::RHI::ERHIResult::Success ? Stoner::RHI::ERHIResult::InvalidState : Result;
    }
    Records[{BindingSlot, ArrayIndex}] = {Stoner::RHI::ERHIDescriptorResourceKind::CombinedTextureSampler, {}, Texture, Sampler};
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanDescriptorSet::Invalidate()
{
    if (LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Invalidated)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Invalidated;
    if (!bPoolReleased && Pool)
    {
        (void)Pool->Release();
        bPoolReleased = true;
    }
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanDescriptorSet::ValidateBinding(Stoner::Core::uint32 BindingSlot, Stoner::Core::uint32 ArrayIndex, Stoner::RHI::ERHIDescriptorType ExpectedType) const noexcept
{
    if (LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Invalidated)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (!Layout || Layout->GetLifecycleState() != Stoner::RHI::ERHIResourceLifecycleState::Valid)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    const Stoner::RHI::FRHIDescriptorBinding* Binding = Layout->FindBinding(SetIndex, BindingSlot);
    if (!Binding)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (ArrayIndex >= Binding->ArrayCount)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (Binding->DescriptorType != ExpectedType)
    {
        return Stoner::RHI::ERHIResult::Unsupported;
    }
    return Stoner::RHI::ERHIResult::Success;
}

} // namespace Stoner::Backend::Vulkan
