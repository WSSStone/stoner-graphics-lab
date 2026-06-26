#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIDescriptorType.h"
#include "RHI/ERHIShaderStage.h"

namespace Stoner::RHI
{

struct FRHIDescriptorBinding
{
    Stoner::Core::uint32 SetIndex = 0;
    Stoner::Core::uint32 BindingSlot = 0;
    ERHIDescriptorType DescriptorType = ERHIDescriptorType::UniformBuffer;
    Stoner::Core::uint32 ArrayCount = 1;
    ERHIShaderStageFlags Visibility = ERHIShaderStageFlags::None;
};

[[nodiscard]] constexpr bool IsValidRHIDescriptorBinding(const FRHIDescriptorBinding& Binding) noexcept
{
    return Binding.ArrayCount > 0 && Binding.Visibility != ERHIShaderStageFlags::None;
}

} // namespace Stoner::RHI
