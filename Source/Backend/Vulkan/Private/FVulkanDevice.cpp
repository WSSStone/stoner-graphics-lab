#include "VulkanRHI/FVulkanDevice.h"

#include "VulkanRHI/FVulkanBuffer.h"
#include "VulkanRHI/FVulkanDescriptorSet.h"
#include "VulkanRHI/FVulkanFence.h"
#include "VulkanRHI/FVulkanPipelineLayout.h"
#include "VulkanRHI/FVulkanQueue.h"
#include "VulkanRHI/FVulkanSampler.h"
#include "VulkanRHI/FVulkanSemaphore.h"
#include "VulkanRHI/FVulkanSwapchain.h"
#include "VulkanRHI/FVulkanTexture.h"

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

FVulkanAllocationSnapshot FVulkanDevice::GetAllocationSnapshot() const noexcept
{
    return Allocator ? Allocator->GetSnapshot() : FVulkanAllocationSnapshot{};
}

Stoner::Core::uint32 FVulkanDevice::GetDescriptorPoolCapacity() const noexcept
{
    return DescriptorPool ? DescriptorPool->GetCapacity() : DescriptorPoolCapacity;
}

Stoner::Core::uint32 FVulkanDevice::GetDescriptorPoolAllocatedCount() const noexcept
{
    return DescriptorPool ? DescriptorPool->GetAllocatedCount() : 0;
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
    if (!Allocator)
    {
        Allocator = std::make_shared<FVulkanMemoryAllocator>();
    }
    Allocator->SetRuntimeAvailable(!Diagnostics.bUsedRuntimeFallback);
    Allocator->Reset();
    DescriptorPool.reset();
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

void FVulkanDevice::ConfigureAllocationBudget(Stoner::Core::uint64 MaxBytes) noexcept
{
    if (Allocator)
    {
        Allocator->ConfigureBudgetLimit(MaxBytes);
    }
}

void FVulkanDevice::ConfigureAllocationCountLimit(Stoner::Core::uint32 MaxAllocations) noexcept
{
    if (Allocator)
    {
        Allocator->ConfigureAllocationCountLimit(MaxAllocations);
    }
}

void FVulkanDevice::ConfigureDescriptorPoolCapacity(Stoner::Core::uint32 Capacity) noexcept
{
    DescriptorPoolCapacity = Capacity;
    if (!DescriptorPool || DescriptorPool->GetAllocatedCount() == 0)
    {
        DescriptorPool = std::make_shared<FVulkanDescriptorPool>(DescriptorPoolCapacity);
    }
}

void FVulkanDevice::ResetResourceConfiguration() noexcept
{
    if (Allocator)
    {
        Allocator->ClearLimits();
    }
    DescriptorPoolCapacity = 16;
    DescriptorPool.reset();
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

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIBuffer> FVulkanDevice::CreateBuffer(const Stoner::RHI::FRHIBufferDesc& Desc)
{
    if (!IsActive())
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    if (!SupportsBufferDesc(Desc))
    {
        return {Stoner::RHI::ERHIResult::Unsupported, nullptr};
    }

    FVulkanResourceAllocation Allocation = Allocator->AllocateBuffer(Desc, IsActive());
    if (!Allocation.IsSuccessful())
    {
        MarkAllocationFailure(Diagnostics, Allocation.Reason);
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }

    auto Buffer = Stoner::Core::MakeShared<FVulkanBuffer>(Desc, Allocation, Allocator);
    Buffers.push_back(Buffer);
    MarkResourceAllocation(Diagnostics, Allocation.Reason);
    return {Stoner::RHI::ERHIResult::Success, Buffer};
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHITexture> FVulkanDevice::CreateTexture(const Stoner::RHI::FRHITextureDesc& Desc)
{
    if (!IsActive())
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    if (!SupportsTextureDesc(Desc))
    {
        return {Stoner::RHI::ERHIResult::Unsupported, nullptr};
    }

    FVulkanResourceAllocation Allocation = Allocator->AllocateTexture(Desc, IsActive());
    if (!Allocation.IsSuccessful())
    {
        MarkAllocationFailure(Diagnostics, Allocation.Reason);
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }

    auto Texture = Stoner::Core::MakeShared<FVulkanTexture>(Desc, Allocation, Allocator);
    Textures.push_back(Texture);
    MarkResourceAllocation(Diagnostics, Allocation.Reason);
    return {Stoner::RHI::ERHIResult::Success, Texture};
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHISampler> FVulkanDevice::CreateSampler(const Stoner::RHI::FRHISamplerDesc& Desc)
{
    if (!IsActive())
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    if (!SupportsSamplerDesc(Desc))
    {
        return {Stoner::RHI::ERHIResult::Unsupported, nullptr};
    }

    auto Sampler = Stoner::Core::MakeShared<FVulkanSampler>(Desc);
    Samplers.push_back(Sampler);
    return {Stoner::RHI::ERHIResult::Success, Sampler};
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIShaderModule> FVulkanDevice::CreateShaderModule(const Stoner::RHI::FRHIShaderModuleDesc&)
{
    return UnsupportedObjectResult<Stoner::RHI::IRHIShaderModule>();
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIPipelineLayout> FVulkanDevice::CreatePipelineLayout(const Stoner::RHI::FRHIPipelineLayoutDesc& Desc)
{
    if (!IsActive())
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    for (const Stoner::RHI::FRHIDescriptorBinding& Binding : Desc.Bindings)
    {
        if (!Stoner::RHI::IsValidRHIDescriptorBinding(Binding))
        {
            MarkDescriptorUpdate(Diagnostics, "invalid descriptor binding in pipeline layout");
            return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
        }
    }
    auto Layout = Stoner::Core::MakeShared<FVulkanPipelineLayout>(Desc);
    PipelineLayouts.push_back(Layout);
    return {Stoner::RHI::ERHIResult::Success, Layout};
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIDescriptorSet> FVulkanDevice::CreateDescriptorSet(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIPipelineLayout>& Layout, Stoner::Core::uint32 SetIndex)
{
    if (!IsActive())
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    if (!Layout || Layout->GetLifecycleState() != Stoner::RHI::ERHIResourceLifecycleState::Valid || SetIndex >= Layout->GetSetCount())
    {
        MarkDescriptorUpdate(Diagnostics, "missing or invalid descriptor set layout");
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }

    EnsureDescriptorPool();
    const Stoner::RHI::ERHIResult PoolResult = DescriptorPool->Allocate();
    if (PoolResult != Stoner::RHI::ERHIResult::Success)
    {
        MarkDescriptorPool(Diagnostics, "descriptor pool capacity exhausted");
        return {PoolResult, nullptr};
    }

    auto DescriptorSet = Stoner::Core::MakeShared<FVulkanDescriptorSet>(Layout, SetIndex, DescriptorPool);
    DescriptorSets.push_back(DescriptorSet);
    return {Stoner::RHI::ERHIResult::Success, DescriptorSet};
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

Stoner::RHI::TRHIObjectResult<FVulkanUploadRequest> FVulkanDevice::StageBufferUpload(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer>& Buffer, const void* Data, Stoner::Core::uint64 SizeBytes, FVulkanBufferUploadRange Range)
{
    if (!IsActive())
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    auto Result = FVulkanUploadRequest::CreateBufferUpload(Buffer, Data, SizeBytes, Range);
    if (Result.Succeeded())
    {
        UploadRequests.push_back(Result.Object);
    }
    else
    {
        MarkUploadRejection(Diagnostics, "buffer upload staging request rejected");
    }
    return Result;
}

Stoner::RHI::TRHIObjectResult<FVulkanUploadRequest> FVulkanDevice::StageTextureUpload(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& Texture, const void* Data, Stoner::Core::uint64 SizeBytes, FVulkanTextureUploadRegion Region)
{
    if (!IsActive())
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    auto Result = FVulkanUploadRequest::CreateTextureUpload(Texture, Data, SizeBytes, Region);
    if (Result.Succeeded())
    {
        UploadRequests.push_back(Result.Object);
    }
    else
    {
        MarkUploadRejection(Diagnostics, "texture upload staging request rejected");
    }
    return Result;
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
    for (const auto& Request : UploadRequests)
    {
        if (Request)
        {
            (void)Request->Invalidate();
        }
    }
    for (const auto& DescriptorSet : DescriptorSets)
    {
        if (DescriptorSet)
        {
            (void)DescriptorSet->Invalidate();
        }
    }
    if (DescriptorPool)
    {
        (void)DescriptorPool->Invalidate();
    }
    for (const auto& Layout : PipelineLayouts)
    {
        if (Layout)
        {
            (void)Layout->Invalidate();
        }
    }
    for (const auto& Sampler : Samplers)
    {
        if (Sampler)
        {
            (void)Sampler->Invalidate();
        }
    }
    for (const auto& Texture : Textures)
    {
        if (Texture)
        {
            (void)Texture->Invalidate();
        }
    }
    for (const auto& Buffer : Buffers)
    {
        if (Buffer)
        {
            (void)Buffer->Invalidate();
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

bool FVulkanDevice::SupportsBufferDesc(const Stoner::RHI::FRHIBufferDesc& Desc) noexcept
{
    if (!Stoner::RHI::IsValidRHIBufferDesc(Desc))
    {
        MarkResourceAllocation(Diagnostics, "invalid or unsupported buffer description");
        return false;
    }
    return true;
}

bool FVulkanDevice::SupportsTextureDesc(const Stoner::RHI::FRHITextureDesc& Desc) const noexcept
{
    return Stoner::RHI::IsValidRHITextureDesc(Desc) && Capabilities.SupportsFormat(Desc.Format);
}

bool FVulkanDevice::SupportsSamplerDesc(const Stoner::RHI::FRHISamplerDesc& Desc) noexcept
{
    if (!Stoner::RHI::IsValidRHISamplerDesc(Desc))
    {
        MarkResourceAllocation(Diagnostics, "invalid or unsupported sampler description");
        return false;
    }
    return true;
}

void FVulkanDevice::EnsureDescriptorPool() noexcept
{
    if (!DescriptorPool)
    {
        DescriptorPool = std::make_shared<FVulkanDescriptorPool>(DescriptorPoolCapacity);
    }
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
