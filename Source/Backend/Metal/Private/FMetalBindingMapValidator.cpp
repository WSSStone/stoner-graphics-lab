#include "FMetalBindingMapValidator.h"

#include <algorithm>
#include <tuple>

namespace Stoner::Backend::Metal::Private
{
namespace
{

Core::uint32 LimitFor(
    RHI::ERHINativeResourceClass Class,
    const RHI::FRHIDeviceCapabilities& Capabilities) noexcept
{
    switch (Class)
    {
    case RHI::ERHINativeResourceClass::Buffer:
        return Capabilities.MaxPerStageBufferBindings;
    case RHI::ERHINativeResourceClass::Texture:
        return Capabilities.MaxPerStageTextureBindings;
    case RHI::ERHINativeResourceClass::Sampler:
        return Capabilities.MaxPerStageSamplerBindings;
    }
    return 0;
}

} // namespace

RHI::ERHIResult ValidateMetalBindingMap(
    const RHI::FRHINativeBindingMap& Map,
    const RHI::FRHIShaderInterfaceMetadata& Interface,
    const RHI::FRHIPipelineLayoutDesc& Layout,
    const RHI::FRHIDeviceCapabilities& Capabilities) noexcept
{
    if (Map.PolicyVersion != Core::FString("metal-direct-binding-v1") ||
        !RHI::IsCanonicalRHINativeBindingMap(Map) ||
        !RHI::IsRHIShaderInterfaceCompatibleWithPipelineLayout(
            Interface, Layout) ||
        !RHI::IsValidRHIDeviceCapabilities(Capabilities))
        return RHI::ERHIResult::InvalidState;
    if (Map.LimitSnapshot.size() != 3) return RHI::ERHIResult::InvalidState;
    const RHI::ERHIShaderStage Stage = Map.LimitSnapshot.front().Stage;
    for (const auto& Limit : Map.LimitSnapshot)
    {
        if (Limit.Stage != Stage ||
            Limit.MaxCount > LimitFor(Limit.NativeClass, Capabilities))
            return RHI::ERHIResult::Unsupported;
    }
    for (const auto& Range : Map.ReservedRanges)
    {
        if (Range.Stage != Stage ||
            static_cast<Core::uint64>(Range.FirstIndex) + Range.Count >
                LimitFor(Range.NativeClass, Capabilities))
            return RHI::ERHIResult::Unsupported;
    }
    for (const auto& Entry : Map.Entries)
    {
        const auto Binding = std::find_if(
            Interface.Bindings.begin(), Interface.Bindings.end(),
            [&Entry](const auto& Candidate) {
                return Candidate.SetIndex == Entry.SetIndex &&
                    Candidate.BindingSlot == Entry.BindingSlot &&
                    Candidate.DescriptorType == Entry.DescriptorType &&
                    Entry.ArrayElement < Candidate.ArrayCount &&
                    (Candidate.Visibility &
                        RHI::ToShaderStageFlag(Entry.Stage)) !=
                        RHI::ERHIShaderStageFlags::None;
            });
        if (Binding == Interface.Bindings.end() ||
            Entry.Stage != Stage ||
            Entry.NativeIndex >= LimitFor(Entry.NativeClass, Capabilities))
            return RHI::ERHIResult::InvalidState;
    }
    for (const auto& Binding : Interface.Bindings)
    {
        if ((Binding.Visibility & RHI::ToShaderStageFlag(Stage)) ==
            RHI::ERHIShaderStageFlags::None)
            continue;
        for (Core::uint32 Element = 0; Element < Binding.ArrayCount; ++Element)
        {
            const auto* Found = FindMetalNativeBinding(
                    Map, Stage, Binding.SetIndex, Binding.BindingSlot,
                    Binding.DescriptorType, Element);
            if (!Found)
                return RHI::ERHIResult::InvalidState;
            if (Binding.DescriptorType ==
                RHI::ERHIDescriptorType::CombinedTextureSampler)
            {
                bool bTexture = false;
                bool bSampler = false;
                for (const auto& Entry : Map.Entries)
                {
                    if (Entry.Stage != Stage ||
                        Entry.SetIndex != Binding.SetIndex ||
                        Entry.BindingSlot != Binding.BindingSlot ||
                        Entry.DescriptorType != Binding.DescriptorType ||
                        Entry.ArrayElement != Element)
                        continue;
                    bTexture = bTexture || Entry.NativeClass ==
                        RHI::ERHINativeResourceClass::Texture;
                    bSampler = bSampler || Entry.NativeClass ==
                        RHI::ERHINativeResourceClass::Sampler;
                }
                if (!bTexture || !bSampler)
                    return RHI::ERHIResult::InvalidState;
            }
        }
    }
    return RHI::ERHIResult::Success;
}

const RHI::FRHINativeBindingEntry* FindMetalNativeBinding(
    const RHI::FRHINativeBindingMap& Map,
    RHI::ERHIShaderStage Stage,
    Core::uint32 SetIndex,
    Core::uint32 BindingSlot,
    RHI::ERHIDescriptorType DescriptorType,
    Core::uint32 ArrayElement) noexcept
{
    const auto It = std::lower_bound(
        Map.Entries.begin(), Map.Entries.end(),
        std::tuple(Stage, SetIndex, BindingSlot, DescriptorType, ArrayElement),
        [](const RHI::FRHINativeBindingEntry& Entry, const auto& Key)
        {
            return std::tuple(
                Entry.Stage, Entry.SetIndex, Entry.BindingSlot,
                Entry.DescriptorType, Entry.ArrayElement) < Key;
        });
    if (It == Map.Entries.end() || It->Stage != Stage ||
        It->SetIndex != SetIndex || It->BindingSlot != BindingSlot ||
        It->DescriptorType != DescriptorType ||
        It->ArrayElement != ArrayElement)
        return nullptr;
    return &*It;
}

} // namespace Stoner::Backend::Metal::Private
