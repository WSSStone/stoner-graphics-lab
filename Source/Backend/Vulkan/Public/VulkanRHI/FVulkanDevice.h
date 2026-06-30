#pragma once

#include "Core/CoreMinimal.h"
#include "VulkanRHI/FVulkanDiagnostics.h"
#include "VulkanRHI/FVulkanInstance.h"
#include "VulkanRHI/FVulkanPhysicalDevice.h"
#include "VulkanRHI/FVulkanSurface.h"
#include "RHI/RHIMinimal.h"

namespace Stoner::Backend::Vulkan
{

class FVulkanFence;
class FVulkanQueue;
class FVulkanSemaphore;
class FVulkanSwapchain;

class FVulkanDevice final : public Stoner::RHI::IRHIDevice
{
public:
    [[nodiscard]] Stoner::RHI::ERHIDeviceState GetState() const noexcept override;
    [[nodiscard]] const Stoner::RHI::FRHIDeviceCapabilities& GetCapabilities() const noexcept override;
    [[nodiscard]] bool IsActive() const noexcept override;
    [[nodiscard]] const FVulkanDiagnostics& GetDiagnostics() const noexcept;
    [[nodiscard]] const FVulkanAdapterCandidate& GetSelectedAdapter() const noexcept;

    Stoner::RHI::ERHIResult Initialize(const FVulkanInstanceDesc& Desc = {});
    Stoner::RHI::ERHIResult Shutdown() override;

    Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHICommandQueue> CreateCommandQueue(Stoner::RHI::ERHIQueueType QueueType) override;
    Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHICommandBuffer> CreateCommandBuffer(Stoner::RHI::ERHIQueueType CompatibleQueueType) override;
    Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIFence> CreateFence(bool bInitiallySignaled = false) override;
    Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHISemaphore> CreateSemaphore() override;
    Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHISwapchain> CreateSwapchain(Stoner::Core::uint32 FrameCount) override;
    Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIBuffer> CreateBuffer(const Stoner::RHI::FRHIBufferDesc& Desc) override;
    Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHITexture> CreateTexture(const Stoner::RHI::FRHITextureDesc& Desc) override;
    Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHISampler> CreateSampler(const Stoner::RHI::FRHISamplerDesc& Desc) override;
    Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIShaderModule> CreateShaderModule(const Stoner::RHI::FRHIShaderModuleDesc& Desc) override;
    Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIPipelineLayout> CreatePipelineLayout(const Stoner::RHI::FRHIPipelineLayoutDesc& Desc) override;
    Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIDescriptorSet> CreateDescriptorSet(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIPipelineLayout>& Layout, Stoner::Core::uint32 SetIndex) override;
    Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIGraphicsPipeline> CreateGraphicsPipeline(const Stoner::RHI::FRHIGraphicsPipelineDesc& Desc) override;
    Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIComputePipeline> CreateComputePipeline(const Stoner::RHI::FRHIComputePipelineDesc& Desc) override;
    Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIRenderPass> CreateRenderPass(const Stoner::RHI::FRHIRenderPassDesc& Desc) override;
    Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIFramebuffer> CreateFramebuffer(const Stoner::RHI::FRHIFramebufferDesc& Desc) override;

    [[nodiscard]] Stoner::RHI::ERHIResult CreateSurface(const Stoner::Core::FPlatformWindow& Window, FVulkanSurface& OutSurface);
    Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHISwapchain> CreateSwapchainForSurface(const FVulkanSurface& Surface, Stoner::Core::uint32 FrameCount);

private:
    template <typename T>
    [[nodiscard]] Stoner::RHI::TRHIObjectResult<T> UnsupportedObjectResult() const noexcept
    {
        return {IsActive() ? Stoner::RHI::ERHIResult::Unsupported : Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }

    void InvalidateOwnedObjects() noexcept;
    void MapCapabilities(const FVulkanAdapterCandidate& Adapter);

    Stoner::RHI::ERHIDeviceState State = Stoner::RHI::ERHIDeviceState::Uninitialized;
    Stoner::RHI::FRHIDeviceCapabilities Capabilities;
    FVulkanInstance Instance;
    FVulkanAdapterCandidate SelectedAdapter;
    FVulkanDiagnostics Diagnostics;
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<FVulkanQueue>> Queues;
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<FVulkanFence>> Fences;
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<FVulkanSemaphore>> Semaphores;
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<FVulkanSwapchain>> Swapchains;
};

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIDevice> CreateVulkanDevice(const FVulkanInstanceDesc& Desc = {});

} // namespace Stoner::Backend::Vulkan
