#pragma once

#include "RHI/RHIMinimal.h"
#include "VulkanRHI/FVulkanBuffer.h"
#include "VulkanRHI/FVulkanCommandBuffer.h"
#include "VulkanRHI/FVulkanCommandPool.h"
#include "VulkanRHI/FVulkanCommandSubmission.h"
#include "VulkanRHI/FVulkanComputePipeline.h"
#include "VulkanRHI/FVulkanDescriptorPool.h"
#include "VulkanRHI/FVulkanDescriptorSet.h"
#include "VulkanRHI/FVulkanDiagnostics.h"
#include "VulkanRHI/FVulkanDevice.h"
#include "VulkanRHI/FVulkanFence.h"
#include "VulkanRHI/FVulkanFramebuffer.h"
#include "VulkanRHI/FVulkanGraphicsPipeline.h"
#include "VulkanRHI/FVulkanInstance.h"
#include "VulkanRHI/FVulkanMemoryAllocator.h"
#include "VulkanRHI/FVulkanPhysicalDevice.h"
#include "VulkanRHI/FVulkanPipelineCache.h"
#include "VulkanRHI/FVulkanPipelineLayout.h"
#include "VulkanRHI/FVulkanQueue.h"
#include "VulkanRHI/FVulkanRenderPass.h"
#include "VulkanRHI/FVulkanResourceAllocation.h"
#include "VulkanRHI/FVulkanSampler.h"
#include "VulkanRHI/FVulkanSemaphore.h"
#include "VulkanRHI/FVulkanShaderModule.h"
#include "VulkanRHI/FVulkanSurface.h"
#include "VulkanRHI/FVulkanSwapchain.h"
#include "VulkanRHI/FVulkanTexture.h"
#include "VulkanRHI/FVulkanUploadStaging.h"

// Vulkan backend — RHI implementation using Vulkan API
namespace Stoner::Backend::Vulkan
{
void VulkanDeviceInit();
} // namespace Stoner::Backend::Vulkan
