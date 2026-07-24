#pragma once

#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
#include <vulkan/vulkan.h>
#endif

namespace Stoner::Backend::Vulkan
{

struct FVulkanNativeDeviceAccess
{
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
    VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
    VkDevice Device = VK_NULL_HANDLE;
    VkQueue GraphicsQueue = VK_NULL_HANDLE;
    unsigned int GraphicsQueueFamily = 0;
#endif
};

} // namespace Stoner::Backend::Vulkan
