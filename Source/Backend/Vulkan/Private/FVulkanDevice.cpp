#include "VulkanRHI/FVulkanDevice.h"

#include "VulkanRHI/FVulkanFence.h"
#include "VulkanRHI/FVulkanQueue.h"
#include "VulkanRHI/FVulkanSemaphore.h"
#include "VulkanRHI/FVulkanSwapchain.h"

namespace Stoner::Backend::Vulkan
{

namespace
{

[[nodiscard]] Stoner::RHI::ERHIResult InactiveResult(Stoner::RHI::ERHIDeviceState State) noexcept
{
    return State == Stoner::RHI::ERHIDeviceState::Active
        ? Stoner::RHI::ERHIResult::Success
        : Stoner::RHI::ERHIResult::InvalidState;
}

} // namespace

Stoner::RHI::ERHIDeviceState FVulkanDevice::GetState() const noexcept
{
    return State;
}

const Stoner::RHI::FRHIDeviceCapabilities& FVulkanDevice::GetCapabilities() const noexcept
{
    return Capabilities;
}

bool FVulkanDevice::IsActive() const noexcept
{
    return State == Stoner::RHI::ERHIDeviceState::Active;
}

const FVulkanDiagnostics& FVulkanDevice::GetDiagnostics() const noexcept
{
    return Diagnostics;
}

const FVulkanAdapterCandidate& FVulkanDevice::GetSelectedAdapter() const noexcept
{
    return SelectedAdapter;
}

Stoner::RHI::ERHIResult FVulkanDevice::Initialize(const FVulkanInstanceDesc& Desc)
{
    if (State != Stoner::RHI::ERHIDeviceState::Uninitialized)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }

    const Stoner::RHI::ERHIResult InstanceResult = Instance.Initialize(Desc);
    Diagnostics = Instance.GetDiagnostics();
    if (InstanceResult != Stoner::RHI::ERHIResult::Success)
    {
        State = Stoner::RHI::ERHIDeviceState::Shutdown;
        return InstanceResult;
    }

    const FVulkanAdapterSelection& Selection = Instance.GetAdapterSelection();
    if (!Selection.bSucceeded)
    {
        State = Stoner::RHI::ERHIDeviceState::Shutdown;
        return Stoner::RHI::ERHIResult::Unsupported;
    }

    SelectedAdapter = Selection.Selected;
    MapCapabilities(SelectedAdapter);
    State = Stoner::RHI::ERHIDeviceState::Active;
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanDevice::Shutdown()
{
    if (State == Stoner::RHI::ERHIDeviceState::Shutdown)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }

    InvalidateOwnedObjects();
    if (Instance.IsInitialized())
    {
        (void)Instance.Shutdown();
    }
    State = Stoner::RHI::ERHIDeviceState::Shutdown;
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHICommandQueue> FVulkanDevice::CreateCommandQueue(Stoner::RHI::ERHIQueueType QueueType)
{
    if (InactiveResult(State) != Stoner::RHI::ERHIResult::Success)
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    if (!Capabilities.SupportsQueue(QueueType))
    {
        MarkQueueCapability(Diagnostics, "requested queue type is not supported by selected adapter");
        return {Stoner::RHI::ERHIResult::Unsupported, nullptr};
    }

    auto Queue = Stoner::Core::MakeShared<FVulkanQueue>(QueueType);
    Queues.push_back(Queue);
    return {Stoner::RHI::ERHIResult::Success, Queue};
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHICommandBuffer> FVulkanDevice::CreateCommandBuffer(Stoner::RHI::ERHIQueueType)
{
    return UnsupportedObjectResult<Stoner::RHI::IRHICommandBuffer>();
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIFence> FVulkanDevice::CreateFence(bool bInitiallySignaled)
{
    if (!IsActive())
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }

    auto Fence = Stoner::Core::MakeShared<FVulkanFence>(bInitiallySignaled);
    Fences.push_back(Fence);
    return {Stoner::RHI::ERHIResult::Success, Fence};
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHISemaphore> FVulkanDevice::CreateSemaphore()
{
    if (!IsActive())
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }

    auto Semaphore = Stoner::Core::MakeShared<FVulkanSemaphore>();
    Semaphores.push_back(Semaphore);
    return {Stoner::RHI::ERHIResult::Success, Semaphore};
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHISwapchain> FVulkanDevice::CreateSwapchain(Stoner::Core::uint32 FrameCount)
{
    if (!IsActive())
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    if (!Capabilities.bSupportsPresentation || !Capabilities.bSupportsPresentQueue || FrameCount == 0 || FrameCount > Capabilities.MaxInFlightFrames)
    {
        return {Stoner::RHI::ERHIResult::Unsupported, nullptr};
    }

    auto Swapchain = Stoner::Core::MakeShared<FVulkanSwapchain>(FrameCount);
    Swapchains.push_back(Swapchain);
    return {Stoner::RHI::ERHIResult::Success, Swapchain};
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIBuffer> FVulkanDevice::CreateBuffer(const Stoner::RHI::FRHIBufferDesc&)
{
    return UnsupportedObjectResult<Stoner::RHI::IRHIBuffer>();
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHITexture> FVulkanDevice::CreateTexture(const Stoner::RHI::FRHITextureDesc&)
{
    return UnsupportedObjectResult<Stoner::RHI::IRHITexture>();
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHISampler> FVulkanDevice::CreateSampler(const Stoner::RHI::FRHISamplerDesc&)
{
    return UnsupportedObjectResult<Stoner::RHI::IRHISampler>();
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIShaderModule> FVulkanDevice::CreateShaderModule(const Stoner::RHI::FRHIShaderModuleDesc&)
{
    return UnsupportedObjectResult<Stoner::RHI::IRHIShaderModule>();
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIPipelineLayout> FVulkanDevice::CreatePipelineLayout(const Stoner::RHI::FRHIPipelineLayoutDesc&)
{
    return UnsupportedObjectResult<Stoner::RHI::IRHIPipelineLayout>();
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIDescriptorSet> FVulkanDevice::CreateDescriptorSet(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIPipelineLayout>&, Stoner::Core::uint32)
{
    return UnsupportedObjectResult<Stoner::RHI::IRHIDescriptorSet>();
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIGraphicsPipeline> FVulkanDevice::CreateGraphicsPipeline(const Stoner::RHI::FRHIGraphicsPipelineDesc&)
{
    return UnsupportedObjectResult<Stoner::RHI::IRHIGraphicsPipeline>();
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIComputePipeline> FVulkanDevice::CreateComputePipeline(const Stoner::RHI::FRHIComputePipelineDesc&)
{
    return UnsupportedObjectResult<Stoner::RHI::IRHIComputePipeline>();
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIRenderPass> FVulkanDevice::CreateRenderPass(const Stoner::RHI::FRHIRenderPassDesc&)
{
    return UnsupportedObjectResult<Stoner::RHI::IRHIRenderPass>();
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIFramebuffer> FVulkanDevice::CreateFramebuffer(const Stoner::RHI::FRHIFramebufferDesc&)
{
    return UnsupportedObjectResult<Stoner::RHI::IRHIFramebuffer>();
}

Stoner::RHI::ERHIResult FVulkanDevice::CreateSurface(const Stoner::Core::FPlatformWindow& Window, FVulkanSurface& OutSurface)
{
    if (!IsActive())
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }

    const Stoner::RHI::ERHIResult Result = FVulkanSurface::Create(Window, OutSurface);
    if (Result != Stoner::RHI::ERHIResult::Success)
    {
        MarkPresentationSkipped(Diagnostics, OutSurface.GetDiagnosticReason());
    }
    return Result;
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHISwapchain> FVulkanDevice::CreateSwapchainForSurface(const FVulkanSurface& Surface, Stoner::Core::uint32 FrameCount)
{
    if (!IsActive())
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    if (!Surface.IsValid())
    {
        MarkPresentationSkipped(Diagnostics, "missing or invalid presentation surface");
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    return CreateSwapchain(FrameCount);
}

void FVulkanDevice::InvalidateOwnedObjects() noexcept
{
    for (const auto& Queue : Queues)
    {
        if (Queue)
        {
            Queue->Invalidate();
        }
    }
    for (const auto& Fence : Fences)
    {
        if (Fence)
        {
            Fence->Invalidate();
        }
    }
    for (const auto& Semaphore : Semaphores)
    {
        if (Semaphore)
        {
            Semaphore->Invalidate();
        }
    }
    for (const auto& Swapchain : Swapchains)
    {
        if (Swapchain)
        {
            Swapchain->Invalidate();
        }
    }
}

void FVulkanDevice::MapCapabilities(const FVulkanAdapterCandidate& Adapter)
{
    Capabilities = {};
    Capabilities.bSupportsGraphicsQueue = Adapter.Queues.bGraphics;
    Capabilities.bSupportsComputeQueue = Adapter.Queues.bCompute;
    Capabilities.bSupportsTransferQueue = Adapter.Queues.bTransfer;
    Capabilities.bSupportsPresentQueue = Adapter.Queues.bPresent;
    Capabilities.bSupportsPresentation = Adapter.bPresentationSupported;
    Capabilities.bSupportsSynchronization = true;
    Capabilities.MaxInFlightFrames = 3;
    Capabilities.MaxCommandBuffersPerQueue = 0;
    Capabilities.MaxQueuesPerType = 1;
    Capabilities.SupportedFormats = GetDefaultVulkanSupportedFormats();
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIDevice> CreateVulkanDevice(const FVulkanInstanceDesc& Desc)
{
    auto Device = Stoner::Core::MakeShared<FVulkanDevice>();
    const Stoner::RHI::ERHIResult Result = Device->Initialize(Desc);
    if (Result != Stoner::RHI::ERHIResult::Success)
    {
        return {Result, nullptr};
    }
    return {Stoner::RHI::ERHIResult::Success, Device};
}

} // namespace Stoner::Backend::Vulkan
