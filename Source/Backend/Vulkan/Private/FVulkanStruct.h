#pragma once

#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE

#include <vulkan/vulkan.h>

namespace Stoner::Backend::Vulkan
{

template <typename T>
T MakeVulkanStruct(VkStructureType Type) noexcept
{
    T Value{};
    Value.sType = Type;
    return Value;
}

} // namespace Stoner::Backend::Vulkan

#endif
