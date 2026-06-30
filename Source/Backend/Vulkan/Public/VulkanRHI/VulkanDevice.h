#pragma once

#include "RHI/RHIMinimal.h"
#include "VulkanRHI/FVulkanDiagnostics.h"
#include "VulkanRHI/FVulkanDevice.h"
#include "VulkanRHI/FVulkanFence.h"
#include "VulkanRHI/FVulkanInstance.h"
#include "VulkanRHI/FVulkanPhysicalDevice.h"
#include "VulkanRHI/FVulkanQueue.h"
#include "VulkanRHI/FVulkanSemaphore.h"
#include "VulkanRHI/FVulkanSurface.h"
#include "VulkanRHI/FVulkanSwapchain.h"

// Vulkan backend — RHI implementation using Vulkan API
namespace Stoner::Backend::Vulkan
{
void VulkanDeviceInit();
} // namespace Stoner::Backend::Vulkan
