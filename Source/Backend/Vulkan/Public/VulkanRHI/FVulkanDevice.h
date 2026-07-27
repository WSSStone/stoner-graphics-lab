#pragma once

#include "Core/CoreMinimal.h"
#include "VulkanRHI/FVulkanBuffer.h"
#include "VulkanRHI/FVulkanCommandPool.h"
#include "VulkanRHI/FVulkanCommandSubmission.h"
#include "VulkanRHI/FVulkanDescriptorPool.h"
#include "VulkanRHI/FVulkanDescriptorSet.h"
#include "VulkanRHI/FVulkanDiagnostics.h"
#include "VulkanRHI/FVulkanDeviceOwnerState.h"
#include "VulkanRHI/FVulkanFramebuffer.h"
#include "VulkanRHI/FVulkanComputePipeline.h"
#include "VulkanRHI/FVulkanGraphicsPipeline.h"
#include "VulkanRHI/FVulkanInstance.h"
#include "VulkanRHI/FVulkanMemoryAllocator.h"
#include "VulkanRHI/FVulkanPhysicalDevice.h"
#include "VulkanRHI/FVulkanPipelineCache.h"
#include "VulkanRHI/FVulkanPipelineLayout.h"
#include "VulkanRHI/FVulkanRenderPass.h"
#include "VulkanRHI/FVulkanSampler.h"
#include "VulkanRHI/FVulkanShaderModule.h"
#include "VulkanRHI/FVulkanSurface.h"
#include "VulkanRHI/FVulkanTexture.h"
#include "VulkanRHI/FVulkanUploadStaging.h"
#include "RHI/RHIMinimal.h"

#include <memory>

namespace Stoner::Backend::Vulkan
{

class FVulkanFence;
class FVulkanQueue;
class FVulkanSemaphore;
class FVulkanSwapchain;
class FVulkanNativeContext;

class FVulkanDevice final : public Stoner::RHI::IRHIDevice
{
public:
    FVulkanDevice() = default;
    ~FVulkanDevice() override;
    FVulkanDevice(const FVulkanDevice&) = delete;
    FVulkanDevice& operator=(const FVulkanDevice&) = delete;
    FVulkanDevice(FVulkanDevice&&) = delete;
    FVulkanDevice& operator=(FVulkanDevice&&) = delete;

    [[nodiscard]] Stoner::RHI::ERHIDeviceState GetState() const noexcept override;
    [[nodiscard]] const Stoner::RHI::FRHIDeviceCapabilities& GetCapabilities() const noexcept override;
    [[nodiscard]] bool IsActive() const noexcept override;
    [[nodiscard]] const FVulkanDiagnostics& GetDiagnostics() const noexcept;
    [[nodiscard]] const FVulkanAdapterCandidate& GetSelectedAdapter() const noexcept;
    [[nodiscard]] FVulkanAllocationSnapshot GetAllocationSnapshot() const noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult EnableNativeShaderRuntime();
    [[nodiscard]] bool HasNativeShaderRuntime() const noexcept;
    [[nodiscard]] Stoner::Core::uint32 GetDescriptorPoolCapacity() const noexcept;
    [[nodiscard]] Stoner::Core::uint32 GetDescriptorPoolAllocatedCount() const noexcept;
    [[nodiscard]] Stoner::Core::uint32 GetCommandBufferCapacity() const noexcept;

    Stoner::RHI::ERHIResult Initialize(const FVulkanInstanceDesc& Desc = {});
    Stoner::RHI::ERHIResult Shutdown() override;
    void ConfigureAllocationBudget(Stoner::Core::uint64 MaxBytes) noexcept;
    void ConfigureAllocationCountLimit(Stoner::Core::uint32 MaxAllocations) noexcept;
    void ConfigureDescriptorPoolCapacity(Stoner::Core::uint32 Capacity) noexcept;
    void ConfigureCommandBufferCapacity(Stoner::Core::uint32 Capacity) noexcept;
    void ConfigurePipelineCreationLimit(Stoner::Core::uint32 MaxSuccessfulCreations) noexcept;
    void ConfigureFallbackCompletionInjection(FVulkanCompletionInjectionConfig Injection) noexcept;
    void ResetResourceConfiguration() noexcept;

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
    Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIPresentationSurface> CreatePresentationSurface(
        const Stoner::RHI::FRHIPresentationSurfaceDesc& Desc) override;
    Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHISwapchain> CreateSwapchain(
        const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIPresentationSurface>& Surface,
        const Stoner::RHI::FRHISwapchainDesc& Desc) override;
    Stoner::RHI::TRHIObjectResult<FVulkanUploadRequest> StageBufferUpload(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer>& Buffer, const void* Data, Stoner::Core::uint64 SizeBytes, FVulkanBufferUploadRange Range);
    Stoner::RHI::TRHIObjectResult<FVulkanUploadRequest> StageTextureUpload(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& Texture, const void* Data, Stoner::Core::uint64 SizeBytes, FVulkanTextureUploadRegion Region);

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
    [[nodiscard]] bool SupportsBufferDesc(const Stoner::RHI::FRHIBufferDesc& Desc) noexcept;
    [[nodiscard]] bool SupportsTextureDesc(const Stoner::RHI::FRHITextureDesc& Desc) const noexcept;
    [[nodiscard]] bool SupportsSamplerDesc(const Stoner::RHI::FRHISamplerDesc& Desc) noexcept;
    [[nodiscard]] bool CanCreatePipeline() noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult EnsureDescriptorPool() noexcept;
    Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHISwapchain> CreateSurfaceBackedSwapchain(
        const Stoner::Core::TSharedPtr<FVulkanSurface>& Surface,
        const Stoner::RHI::FRHISwapchainDesc& Desc);

    Stoner::RHI::ERHIDeviceState State = Stoner::RHI::ERHIDeviceState::Uninitialized;
    Stoner::RHI::FRHIDeviceCapabilities Capabilities;
    FVulkanInstance Instance;
    FVulkanAdapterCandidate SelectedAdapter;
    FVulkanDiagnostics Diagnostics;
    Stoner::Core::TSharedPtr<FVulkanNativeContext> NativeShaderContext;
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<FVulkanQueue>> Queues;
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<FVulkanCommandPool>> CommandPools;
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<FVulkanFence>> Fences;
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<FVulkanSemaphore>> Semaphores;
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<FVulkanSurface>> Surfaces;
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<FVulkanSwapchain>> Swapchains;
    std::shared_ptr<FVulkanDeviceOwnerState> DeviceOwner =
        std::make_shared<FVulkanDeviceOwnerState>();
    std::shared_ptr<FVulkanPresentationOwnerState> PresentationOwner =
        std::make_shared<FVulkanPresentationOwnerState>();
    std::shared_ptr<FVulkanMemoryAllocator> Allocator = std::make_shared<FVulkanMemoryAllocator>();
    Stoner::Core::uint32 DescriptorPoolCapacity = 16;
    std::shared_ptr<FVulkanDescriptorPool> DescriptorPool;
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<FVulkanBuffer>> Buffers;
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<FVulkanTexture>> Textures;
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<FVulkanSampler>> Samplers;
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<FVulkanRenderPass>> RenderPasses;
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<FVulkanFramebuffer>> Framebuffers;
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<FVulkanPipelineLayout>> PipelineLayouts;
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<FVulkanShaderModule>> ShaderModules;
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<FVulkanGraphicsPipeline>> GraphicsPipelines;
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<FVulkanComputePipeline>> ComputePipelines;
    FVulkanPipelineCache PipelineCache;
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<FVulkanDescriptorSet>> DescriptorSets;
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<FVulkanUploadRequest>> UploadRequests;
    Stoner::Core::uint32 CommandBufferCapacity = 64;
    Stoner::Core::uint32 PipelineCreationLimit = 0;
    Stoner::Core::uint32 SuccessfulPipelineCreations = 0;
    FVulkanCompletionInjectionConfig CompletionInjection;
};

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIDevice> CreateVulkanDevice(const FVulkanInstanceDesc& Desc = {});

} // namespace Stoner::Backend::Vulkan
