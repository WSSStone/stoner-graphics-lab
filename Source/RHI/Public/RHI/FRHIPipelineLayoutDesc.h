#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/FRHIDescriptorBinding.h"
#include "RHI/FRHIShaderModuleDesc.h"

#include <cstddef>

namespace Stoner::RHI
{

struct FRHIPipelineLayoutDesc
{
    Stoner::Core::TArray<FRHIDescriptorBinding> Bindings;
    Stoner::Core::TArray<FRHIShaderConstantRange> ConstantRanges;
};

[[nodiscard]] inline bool HasDuplicateRHIPipelineLayoutBinding(const FRHIPipelineLayoutDesc& Desc) noexcept
{
    for (std::size_t LeftIndex = 0; LeftIndex < Desc.Bindings.size(); ++LeftIndex)
    {
        for (std::size_t RightIndex = LeftIndex + 1; RightIndex < Desc.Bindings.size(); ++RightIndex)
        {
            if (Desc.Bindings[LeftIndex].SetIndex == Desc.Bindings[RightIndex].SetIndex &&
                Desc.Bindings[LeftIndex].BindingSlot == Desc.Bindings[RightIndex].BindingSlot)
            {
                return true;
            }
        }
    }
    return false;
}

[[nodiscard]] inline bool IsValidRHIPipelineLayoutDesc(const FRHIPipelineLayoutDesc& Desc) noexcept
{
    if (Desc.Bindings.empty() ||
        HasDuplicateRHIPipelineLayoutBinding(Desc) ||
        HasIncompatibleRHIShaderConstantRangeOverlap(Desc.ConstantRanges))
    {
        return false;
    }
    for (const FRHIDescriptorBinding& Binding : Desc.Bindings)
    {
        if (!IsValidRHIDescriptorBinding(Binding))
        {
            return false;
        }
    }
    for (const FRHIShaderConstantRange& Range : Desc.ConstantRanges)
    {
        if (!IsValidRHIShaderConstantRange(Range))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] inline const FRHIDescriptorBinding* FindRHIPipelineLayoutBinding(
    const FRHIPipelineLayoutDesc& Desc,
    Stoner::Core::uint32 SetIndex,
    Stoner::Core::uint32 BindingSlot) noexcept
{
    for (const FRHIDescriptorBinding& Binding : Desc.Bindings)
    {
        if (Binding.SetIndex == SetIndex && Binding.BindingSlot == BindingSlot)
        {
            return &Binding;
        }
    }
    return nullptr;
}

[[nodiscard]] inline bool DoesRHIShaderConstantRangeContain(
    const FRHIShaderConstantRange& Available,
    const FRHIShaderConstantRange& Required) noexcept
{
    if (!IsValidRHIShaderConstantRange(Available) || !IsValidRHIShaderConstantRange(Required))
    {
        return false;
    }
    const Stoner::Core::uint64 AvailableEnd =
        static_cast<Stoner::Core::uint64>(Available.OffsetBytes) + Available.SizeBytes;
    const Stoner::Core::uint64 RequiredEnd =
        static_cast<Stoner::Core::uint64>(Required.OffsetBytes) + Required.SizeBytes;
    return Required.OffsetBytes >= Available.OffsetBytes &&
        RequiredEnd <= AvailableEnd &&
        (Available.Visibility & Required.Visibility) == Required.Visibility;
}

[[nodiscard]] inline bool IsRHIShaderInterfaceCompatibleWithPipelineLayout(
    const FRHIShaderInterfaceMetadata& Metadata,
    const FRHIPipelineLayoutDesc& Layout) noexcept
{
    if (!IsValidRHIPipelineLayoutDesc(Layout) ||
        HasDuplicateRHIShaderInterfaceBinding(Metadata) ||
        HasIncompatibleRHIShaderConstantRangeOverlap(Metadata.ConstantRanges))
    {
        return false;
    }
    for (const FRHIShaderInterfaceBinding& Required : Metadata.Bindings)
    {
        if (!IsValidRHIShaderInterfaceBinding(Required))
        {
            return false;
        }
        const FRHIDescriptorBinding* Binding =
            FindRHIPipelineLayoutBinding(Layout, Required.SetIndex, Required.BindingSlot);
        if (!Binding ||
            Binding->DescriptorType != Required.DescriptorType ||
            Binding->ArrayCount < Required.ArrayCount ||
            (Binding->Visibility & Required.Visibility) != Required.Visibility)
        {
            return false;
        }
    }

    for (const FRHIShaderConstantRange& Required : Metadata.ConstantRanges)
    {
        if (!IsValidRHIShaderConstantRange(Required))
        {
            return false;
        }
        bool bMatched = false;
        for (const FRHIShaderConstantRange& Available : Layout.ConstantRanges)
        {
            if (DoesRHIShaderConstantRangeContain(Available, Required))
            {
                bMatched = true;
                break;
            }
        }
        if (!bMatched)
        {
            return false;
        }
    }
    return true;
}

} // namespace Stoner::RHI
