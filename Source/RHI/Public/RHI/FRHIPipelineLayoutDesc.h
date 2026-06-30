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
    if (Desc.Bindings.empty() || HasDuplicateRHIPipelineLayoutBinding(Desc))
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

} // namespace Stoner::RHI
