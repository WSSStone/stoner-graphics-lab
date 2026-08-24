#include "VulkanRHI/FVulkanDevice.h"

#include "VulkanRHI/FVulkanBuffer.h"
#include "VulkanRHI/FVulkanCommandBuffer.h"
#include "VulkanRHI/FVulkanCommandPool.h"
#include "VulkanRHI/FVulkanComputePipeline.h"
#include "VulkanRHI/FVulkanDescriptorSet.h"
#include "VulkanRHI/FVulkanFence.h"
#include "VulkanRHI/FVulkanFramebuffer.h"
#include "VulkanRHI/FVulkanGraphicsPipeline.h"
#include "VulkanRHI/FVulkanNativeContext.h"
#include "VulkanRHI/FVulkanPipelineCache.h"
#include "VulkanRHI/FVulkanPipelineLayout.h"
#include "VulkanRHI/FVulkanQueue.h"
#include "VulkanRHI/FVulkanRenderPass.h"
#include "VulkanRHI/FVulkanSampler.h"
#include "VulkanRHI/FVulkanSemaphore.h"
#include "VulkanRHI/FVulkanShaderModule.h"
#include "VulkanRHI/FVulkanSwapchain.h"
#include "VulkanRHI/FVulkanTexture.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

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

[[nodiscard]] bool IsShaderLayoutCompatible(
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIShaderModule>& Shader,
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIPipelineLayout>& Layout,
    const Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState>& Owner) noexcept
{
    if (!Shader || !Layout || Shader->GetLifecycleState() != Stoner::RHI::ERHIResourceLifecycleState::Valid ||
        Layout->GetLifecycleState() != Stoner::RHI::ERHIResourceLifecycleState::Valid)
    {
        return false;
    }
    auto VulkanShader = std::dynamic_pointer_cast<FVulkanShaderModule>(Shader);
    auto VulkanLayout = std::dynamic_pointer_cast<FVulkanPipelineLayout>(Layout);
    return VulkanShader && VulkanLayout &&
        VulkanShader->BelongsTo(Owner) && VulkanLayout->BelongsTo(Owner) &&
        VulkanLayout->IsCompatibleWithShaderInterface(
            Shader->GetDesc().InterfaceMetadata);
}

template <typename TObject>
[[nodiscard]] bool IsTrackedObject(
    const Stoner::Core::TArray<Stoner::Core::TWeakPtr<TObject>>& Objects,
    const Stoner::Core::TSharedPtr<TObject>& Object) noexcept
{
    return Object && std::any_of(
        Objects.begin(), Objects.end(),
        [&Object](const auto& WeakObject)
        {
            return WeakObject.lock() == Object;
        });
}

template <typename TObject>
void TrackObject(
    Stoner::Core::TArray<Stoner::Core::TWeakPtr<TObject>>& Objects,
    const Stoner::Core::TSharedPtr<TObject>& Object)
{
    std::erase_if(
        Objects,
        [](const auto& WeakObject) { return WeakObject.expired(); });
    Objects.push_back(Object);
}

[[nodiscard]] bool HasValidGraphicsShaderStages(
    const Stoner::RHI::FRHIGraphicsPipelineDesc& Desc,
    const Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState>& Owner) noexcept
{
    bool bHasVertex = false;
    bool bHasFragment = false;
    for (const auto& Shader : Desc.ShaderModules)
    {
        if (!Shader || Shader->GetLifecycleState() != Stoner::RHI::ERHIResourceLifecycleState::Valid)
        {
            return false;
        }
        if (!IsShaderLayoutCompatible(Shader, Desc.PipelineLayout, Owner))
        {
            return false;
        }
        if (Shader->GetStage() == Stoner::RHI::ERHIShaderStage::Vertex)
        {
            if (bHasVertex)
            {
                return false;
            }
            bHasVertex = true;
        }
        else if (Shader->GetStage() == Stoner::RHI::ERHIShaderStage::Fragment)
        {
            if (bHasFragment)
            {
                return false;
            }
            bHasFragment = true;
        }
        else
        {
            return false;
        }
    }
    return bHasVertex && bHasFragment;
}

[[nodiscard]] bool HasValidComputeShaderStage(
    const Stoner::RHI::FRHIComputePipelineDesc& Desc,
    const Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState>& Owner) noexcept
{
    return Desc.ShaderModules.size() == 1 &&
        Desc.ShaderModules[0] &&
        Desc.ShaderModules[0]->GetLifecycleState() == Stoner::RHI::ERHIResourceLifecycleState::Valid &&
        Desc.ShaderModules[0]->GetStage() == Stoner::RHI::ERHIShaderStage::Compute &&
        IsShaderLayoutCompatible(
            Desc.ShaderModules[0], Desc.PipelineLayout, Owner);
}

[[nodiscard]] Stoner::RHI::ERHIFormat SelectPresentationFormat(
    const Stoner::RHI::FRHIDeviceCapabilities& Capabilities) noexcept
{
    if (Capabilities.SupportsFormat(Stoner::RHI::ERHIFormat::B8G8R8A8_UNorm))
    {
        return Stoner::RHI::ERHIFormat::B8G8R8A8_UNorm;
    }
    if (Capabilities.SupportsFormat(Stoner::RHI::ERHIFormat::R8G8B8A8_UNorm))
    {
        return Stoner::RHI::ERHIFormat::R8G8B8A8_UNorm;
    }
    return Stoner::RHI::ERHIFormat::Unknown;
}

} // namespace

FVulkanDevice::~FVulkanDevice()
{
    if (State != Stoner::RHI::ERHIDeviceState::Shutdown)
    {
        (void)Shutdown();
    }
}

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

Stoner::RHI::ERHIRuntimeMode FVulkanDevice::GetRuntimeMode() const noexcept
{
    return NativeShaderContext && NativeShaderContext->IsAvailable()
        ? Stoner::RHI::ERHIRuntimeMode::NativeHeadless
        : Stoner::RHI::ERHIRuntimeMode::Deterministic;
}

Stoner::RHI::FRHIRuntimeSnapshot FVulkanDevice::GetRuntimeSnapshot() const noexcept
{
    return NativeShaderContext && NativeShaderContext->IsAvailable()
        ? NativeShaderContext->GetSnapshot()
        : Stoner::RHI::FRHIRuntimeSnapshot{};
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

Stoner::RHI::ERHIResult FVulkanDevice::EnableNativeShaderRuntime()
{
    if (!IsActive())
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (HasNativeShaderRuntime())
    {
        return Stoner::RHI::ERHIResult::Success;
    }

    Stoner::Core::TSharedPtr<FVulkanNativeContext> Context;
    try
    {
        Context = Stoner::Core::MakeShared<FVulkanNativeContext>();
        const Stoner::RHI::ERHIResult InitializeResult =
            Context->Initialize(Stoner::RHI::ERHIRuntimeMode::NativeHeadless);
        if (InitializeResult != Stoner::RHI::ERHIResult::Success)
        {
            return InitializeResult;
        }
        auto NativeFormats =
            Context->QueryTextureFormatCapabilities();
        if (NativeFormats.empty())
        {
            (void)Context->Shutdown();
            return Stoner::RHI::ERHIResult::Unsupported;
        }
        Capabilities.Formats = std::move(NativeFormats);
        NativeShaderContext = std::move(Context);
    }
    catch (const std::bad_alloc&)
    {
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    catch (const std::length_error&)
    {
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    MarkRuntimeMode(
        Diagnostics,
        "native shader runtime enabled; other FVulkanDevice objects remain explicit deterministic fallback");
    return Stoner::RHI::ERHIResult::Success;
}

bool FVulkanDevice::HasNativeShaderRuntime() const noexcept
{
    return NativeShaderContext && NativeShaderContext->IsAvailable();
}

Stoner::Core::uint32 FVulkanDevice::GetDescriptorPoolCapacity() const noexcept
{
    return DescriptorPool ? DescriptorPool->GetCapacity() : DescriptorPoolCapacity;
}

Stoner::Core::uint32 FVulkanDevice::GetDescriptorPoolAllocatedCount() const noexcept
{
    return DescriptorPool ? DescriptorPool->GetAllocatedCount() : 0;
}

Stoner::Core::uint32 FVulkanDevice::GetCommandBufferCapacity() const noexcept
{
    return CommandBufferCapacity;
}

Stoner::Core::uint32 FVulkanDevice::GetTrackedUploadRequestCount() const noexcept
{
    return static_cast<Stoner::Core::uint32>(std::count_if(
        UploadRequests.begin(), UploadRequests.end(),
        [](const auto& Request) { return !Request.expired(); }));
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
    if (!Stoner::RHI::IsValidRHIDeviceCapabilities(Capabilities))
    {
        State = Stoner::RHI::ERHIDeviceState::Shutdown;
        MarkUnsupportedRuntime(
            Diagnostics, "selected Vulkan adapter produced an invalid capability snapshot");
        return Stoner::RHI::ERHIResult::Unsupported;
    }
    if (!Allocator)
    {
        Allocator = std::make_shared<FVulkanMemoryAllocator>();
    }
    Allocator->SetRuntimeAvailable(!Diagnostics.bUsedRuntimeFallback);
    Allocator->Reset();
    DescriptorPool.reset();
    CommandPools.clear();
    ShaderModules.clear();
    GraphicsPipelines.clear();
    ComputePipelines.clear();
    PipelineCache.Invalidate();
    SuccessfulPipelineCreations = 0;
    PresentationOwner = std::make_shared<FVulkanPresentationOwnerState>();
    PresentationOwner->bActive = true;
    DeviceOwner->bActive = true;
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
    if (NativeShaderContext)
    {
        (void)NativeShaderContext->Shutdown();
        NativeShaderContext.reset();
    }
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
    if (DescriptorPool && DescriptorPool->GetAllocatedCount() == 0)
    {
        DescriptorPool.reset();
    }
}

void FVulkanDevice::ConfigureCommandBufferCapacity(Stoner::Core::uint32 Capacity) noexcept
{
    CommandBufferCapacity = Capacity;
}

void FVulkanDevice::ConfigurePipelineCreationLimit(Stoner::Core::uint32 MaxSuccessfulCreations) noexcept
{
    PipelineCreationLimit = MaxSuccessfulCreations;
    SuccessfulPipelineCreations = 0;
}

void FVulkanDevice::ConfigureFallbackCompletionInjection(FVulkanCompletionInjectionConfig Injection) noexcept
{
    CompletionInjection = Injection;
    for (const auto& WeakQueue : Queues)
    {
        if (const auto Queue = WeakQueue.lock())
        {
            Queue->ConfigureCompletionInjection(CompletionInjection);
        }
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
    CommandBufferCapacity = 64;
    PipelineCreationLimit = 0;
    SuccessfulPipelineCreations = 0;
    PipelineCache.Invalidate();
    CompletionInjection = {};
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

    Stoner::Core::TSharedPtr<FVulkanQueue> Queue;
    try
    {
        Queue.reset(new FVulkanQueue(
            QueueType, DeviceOwner, &Diagnostics, CompletionInjection,
            NativeShaderContext));
        Queues.erase(
            std::remove_if(
                Queues.begin(), Queues.end(),
                [](const auto& Candidate) { return Candidate.expired(); }),
            Queues.end());
        Queues.push_back(Queue);
    }
    catch (const std::bad_alloc&)
    {
        if (Queue)
        {
            Queue->Invalidate();
        }
        MarkQueueCapability(Diagnostics, "command queue allocation failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    catch (const std::length_error&)
    {
        if (Queue)
        {
            Queue->Invalidate();
        }
        MarkQueueCapability(Diagnostics, "command queue tracking capacity exceeded");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    return {Stoner::RHI::ERHIResult::Success, Queue};
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHICommandBuffer> FVulkanDevice::CreateCommandBuffer(Stoner::RHI::ERHIQueueType CompatibleQueueType)
{
    if (!IsActive())
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    if (!Capabilities.SupportsQueue(CompatibleQueueType))
    {
        MarkCommandAllocation(Diagnostics, "requested command buffer queue type is unsupported");
        return {Stoner::RHI::ERHIResult::Unsupported, nullptr};
    }

    Stoner::Core::TSharedPtr<FVulkanCommandPool> Pool;
    for (const auto& ExistingPool : CommandPools)
    {
        if (ExistingPool && ExistingPool->IsValid() && ExistingPool->GetQueueType() == CompatibleQueueType)
        {
            Pool = ExistingPool;
            break;
        }
    }
    if (!Pool)
    {
        try
        {
            Pool.reset(new FVulkanCommandPool(
                CompatibleQueueType, CommandBufferCapacity, DeviceOwner));
            CommandPools.push_back(Pool);
        }
        catch (const std::bad_alloc&)
        {
            MarkCommandAllocation(Diagnostics, "command pool allocation failed");
            return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
        }
        catch (const std::length_error&)
        {
            MarkCommandAllocation(Diagnostics, "command pool tracking capacity exceeded");
            return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
        }
    }

    auto Result = Pool->Allocate(Diagnostics);
    return {Result.Result, Result.Object};
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIFence> FVulkanDevice::CreateFence(bool bInitiallySignaled)
{
    if (!IsActive())
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }

    Stoner::Core::TSharedPtr<FVulkanFence> Fence;
    try
    {
        Fence.reset(new FVulkanFence(bInitiallySignaled, DeviceOwner));
        TrackObject(Fences, Fence);
    }
    catch (const std::bad_alloc&)
    {
        if (Fence)
        {
            Fence->Invalidate();
        }
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    catch (const std::length_error&)
    {
        if (Fence)
        {
            Fence->Invalidate();
        }
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    return {Stoner::RHI::ERHIResult::Success, Fence};
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHISemaphore> FVulkanDevice::CreateSemaphore()
{
    if (!IsActive())
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }

    Stoner::Core::TSharedPtr<FVulkanSemaphore> Semaphore;
    try
    {
        Semaphore.reset(new FVulkanSemaphore(DeviceOwner));
        TrackObject(Semaphores, Semaphore);
    }
    catch (const std::bad_alloc&)
    {
        if (Semaphore)
        {
            Semaphore->Invalidate();
        }
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    catch (const std::length_error&)
    {
        if (Semaphore)
        {
            Semaphore->Invalidate();
        }
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    return {Stoner::RHI::ERHIResult::Success, Semaphore};
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHISwapchain> FVulkanDevice::CreateSwapchain(Stoner::Core::uint32 FrameCount)
{
    if (!IsActive())
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    if (FrameCount == 0)
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    if (!Capabilities.bSupportsPresentation || !Capabilities.bSupportsPresentQueue ||
        FrameCount > Capabilities.MaxInFlightFrames)
    {
        return {Stoner::RHI::ERHIResult::Unsupported, nullptr};
    }

    auto Swapchain = Stoner::Core::MakeShared<FVulkanSwapchain>(
        FrameCount, Capabilities.MaxInFlightFrames);
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
        MarkAllocationFailure(Diagnostics, Allocation.GetReason());
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }

    Stoner::Core::TSharedPtr<FVulkanBuffer> Buffer;
    try
    {
        Buffer.reset(new FVulkanBuffer(
            Desc, std::move(Allocation), Allocator));
    }
    catch (const std::bad_alloc&)
    {
        (void)Allocator->Release(Allocation);
        MarkAllocationFailure(Diagnostics, "buffer wrapper allocation failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    if (Buffer->GetLifecycleState() !=
        Stoner::RHI::ERHIResourceLifecycleState::Valid)
    {
        (void)Allocator->Release(Allocation);
        MarkAllocationFailure(Diagnostics, "buffer allocation ownership rejected");
        return {Stoner::RHI::ERHIResult::Failed, nullptr};
    }
    try
    {
        TrackObject(Buffers, Buffer);
    }
    catch (const std::bad_alloc&)
    {
        (void)Buffer->Invalidate();
        MarkAllocationFailure(Diagnostics, "buffer resource tracking allocation failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    catch (const std::length_error&)
    {
        (void)Buffer->Invalidate();
        MarkAllocationFailure(Diagnostics, "buffer resource tracking capacity exceeded");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    MarkResourceAllocation(
        Diagnostics, Buffer->GetAllocation().GetReason());
    return {Stoner::RHI::ERHIResult::Success, Buffer};
}

Stoner::RHI::ERHIResult FVulkanDevice::UploadBuffer(
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer>& Buffer,
    const Stoner::RHI::FRHIBufferUploadDesc& Upload)
{
    using namespace Stoner::RHI;
    if (!IsActive() || !Buffer ||
        Buffer->GetLifecycleState() != ERHIResourceLifecycleState::Valid)
    {
        return ERHIResult::InvalidState;
    }

    const auto VulkanBuffer = std::dynamic_pointer_cast<FVulkanBuffer>(Buffer);
    const bool bOwned = IsTrackedObject(Buffers, VulkanBuffer);
    if (!bOwned || !IsValidRHIBufferUploadDesc(Buffer->GetDesc(), Upload))
    {
        return ERHIResult::InvalidState;
    }

    if (Buffer->GetDesc().MemoryAccess == ERHIMemoryAccess::HostVisible)
    {
        return VulkanBuffer->Upload(
            Upload.Data, Upload.DataSizeBytes, Upload.DestinationOffset);
    }
    if (!HasRHIFlag(Buffer->GetUsage(), ERHIBufferUsage::CopyDestination))
    {
        return ERHIResult::Unsupported;
    }

    const auto Staged = StageBufferUpload(
        Buffer, Upload.Data, Upload.DataSizeBytes,
        {Upload.DestinationOffset, Upload.DataSizeBytes});
    if (!Staged.Succeeded() || !HasNativeShaderRuntime())
        return Staged.Result;
    return VulkanBuffer->RecordNativeUpload(
        Upload.Data, Upload.DataSizeBytes, Upload.DestinationOffset);
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
        MarkAllocationFailure(Diagnostics, Allocation.GetReason());
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }

    Stoner::Core::TSharedPtr<FVulkanNativeContext> TextureNativeContext;
    Stoner::Core::uint64 NativeToken = 0;
    if (HasNativeShaderRuntime())
    {
        const Stoner::RHI::ERHIResult NativeResult =
            NativeShaderContext->CreateOwnedTexture(
                Desc, NativeToken);
        if (NativeResult != Stoner::RHI::ERHIResult::Success)
        {
            (void)Allocator->Release(Allocation);
            return {NativeResult, nullptr};
        }
        TextureNativeContext = NativeShaderContext;
    }

    Stoner::Core::TSharedPtr<FVulkanTexture> Texture;
    try
    {
        Texture.reset(new FVulkanTexture(
            Desc,
            std::move(Allocation),
            Allocator,
            TextureNativeContext,
            NativeToken));
    }
    catch (const std::bad_alloc&)
    {
        if (TextureNativeContext && NativeToken != 0)
        {
            TextureNativeContext->DestroyOwnedTexture(NativeToken);
        }
        (void)Allocator->Release(Allocation);
        MarkAllocationFailure(Diagnostics, "texture wrapper allocation failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    if (Texture->GetLifecycleState() !=
        Stoner::RHI::ERHIResourceLifecycleState::Valid)
    {
        (void)Allocator->Release(Allocation);
        MarkAllocationFailure(Diagnostics, "texture allocation ownership rejected");
        return {Stoner::RHI::ERHIResult::Failed, nullptr};
    }
    try
    {
        TrackObject(Textures, Texture);
    }
    catch (const std::bad_alloc&)
    {
        (void)Texture->Invalidate();
        MarkAllocationFailure(Diagnostics, "texture resource tracking allocation failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    catch (const std::length_error&)
    {
        (void)Texture->Invalidate();
        MarkAllocationFailure(Diagnostics, "texture resource tracking capacity exceeded");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    MarkResourceAllocation(
        Diagnostics, Texture->GetAllocation().GetReason());
    return {Stoner::RHI::ERHIResult::Success, Texture};
}

Stoner::RHI::ERHIResult FVulkanDevice::UploadTexture(
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& Texture,
    const Stoner::RHI::FRHITextureUploadDesc& Upload)
{
    using namespace Stoner::RHI;
    if (!IsActive() || !Texture ||
        Texture->GetLifecycleState() !=
            ERHIResourceLifecycleState::Valid)
    {
        return ERHIResult::InvalidState;
    }

    const auto VulkanTexture =
        std::dynamic_pointer_cast<FVulkanTexture>(Texture);
    const bool bOwned = IsTrackedObject(Textures, VulkanTexture);
    if (!bOwned)
    {
        return ERHIResult::InvalidState;
    }

    const FRHITextureDesc& TextureDesc = Texture->GetDesc();
    if (!Capabilities.SupportsFormat(TextureDesc.Format) ||
        !HasRHIFlag(
            TextureDesc.Usage, ERHITextureUsage::CopyDestination) ||
        TextureDesc.SampleCount != ERHISampleCount::One)
    {
        return ERHIResult::Unsupported;
    }
    if (!IsValidRHITextureUploadDesc(TextureDesc, Upload))
    {
        return ERHIResult::InvalidState;
    }

    const Stoner::Core::uint32 MipWidth =
        GetRHIMipExtent(TextureDesc.Width, Upload.MipLevel);
    const Stoner::Core::uint32 MipHeight =
        GetRHIMipExtent(TextureDesc.Height, Upload.MipLevel);
    const Stoner::Core::uint32 MipDepth =
        GetRHIMipExtent(TextureDesc.Depth, Upload.MipLevel);
    if (Upload.X != 0 || Upload.Y != 0 || Upload.Z != 0 ||
        Upload.Width != MipWidth || Upload.Height != MipHeight ||
        Upload.Depth != MipDepth)
    {
        return ERHIResult::InvalidState;
    }

    FRHITextureFootprint Footprint;
    if (!TryGetRHITextureFootprint(
            TextureDesc.Format,
            Upload.Width,
            Upload.Height,
            Upload.Depth,
            Footprint) ||
        Footprint.TotalBytes >
            static_cast<Stoner::Core::uint64>(
                std::numeric_limits<Stoner::Core::usize>::max()))
    {
        return ERHIResult::Unavailable;
    }
    const Stoner::Core::uint64 TightRowBytes =
        Footprint.TightRowBytes;
    const Stoner::Core::uint64 RowCount =
        Footprint.BlockCountY * Footprint.BlockCountZ;

    Stoner::Core::TArray<Stoner::Core::uint8> TightBytes;
    try
    {
        TightBytes.resize(static_cast<Stoner::Core::usize>(
            TightRowBytes * RowCount));
        const auto* Source =
            static_cast<const Stoner::Core::uint8*>(Upload.Data);
        for (Stoner::Core::uint64 Row = 0; Row < RowCount; ++Row)
        {
            std::memcpy(
                TightBytes.data() +
                    static_cast<Stoner::Core::usize>(
                        Row * TightRowBytes),
                Source +
                    static_cast<Stoner::Core::usize>(
                        Row * Upload.RowPitchBytes),
                static_cast<Stoner::Core::usize>(TightRowBytes));
        }
    }
    catch (const std::bad_alloc&)
    {
        return ERHIResult::Unavailable;
    }
    catch (const std::length_error&)
    {
        return ERHIResult::Unavailable;
    }

    const FVulkanTextureUploadRegion Region{
        Upload.MipLevel,
        Upload.ArrayLayer,
        Upload.X,
        Upload.Y,
        Upload.Z,
        Upload.Width,
        Upload.Height,
        Upload.Depth};
    const auto Staging = StageTextureUpload(
        Texture,
        TightBytes.data(),
        TightBytes.size(),
        Region);
    if (!Staging.Succeeded())
    {
        return Staging.Result;
    }
    if (VulkanTexture->NativeContext &&
        VulkanTexture->NativeToken != 0)
    {
        const ERHIResult NativeResult =
            VulkanTexture->NativeContext->UploadOwnedTexture(
                VulkanTexture->NativeToken,
                FRHITextureUploadDesc{
                    Upload.MipLevel,
                    Upload.ArrayLayer,
                    Upload.X,
                    Upload.Y,
                    Upload.Z,
                    Upload.Width,
                    Upload.Height,
                    Upload.Depth,
                    TightRowBytes,
                    TightBytes.data(),
                    TightBytes.size()});
        if (NativeResult != ERHIResult::Success)
        {
            (void)Staging.Object->Invalidate();
            return NativeResult;
        }
    }
    if (Staging.Object->MarkScheduled() != ERHIResult::Success)
    {
        return ERHIResult::Failed;
    }
    return VulkanTexture->RecordUploadedMip(
        Upload.MipLevel, std::move(TightBytes));
}

Stoner::RHI::ERHIResult FVulkanDevice::ReadbackTextureForTesting(
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& Texture,
    Stoner::Core::uint32 MipLevel,
    Stoner::Core::TArray<Stoner::Core::uint8>& OutBytes)
{
    OutBytes.clear();
    if (!IsActive() || !Texture)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    const auto VulkanTexture =
        std::dynamic_pointer_cast<FVulkanTexture>(Texture);
    if (!IsTrackedObject(Textures, VulkanTexture) ||
        MipLevel >= VulkanTexture->GetDesc().MipLevels)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (VulkanTexture->NativeContext &&
        VulkanTexture->NativeToken != 0)
    {
        return VulkanTexture->NativeContext->ReadbackOwnedTexture(
            VulkanTexture->NativeToken, MipLevel, OutBytes);
    }
    const auto Uploaded =
        VulkanTexture->GetUploadedMipData(MipLevel);
    if (Uploaded.empty())
    {
        return Stoner::RHI::ERHIResult::NotReady;
    }
    try
    {
        OutBytes.assign(Uploaded.begin(), Uploaded.end());
    }
    catch (const std::bad_alloc&)
    {
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    catch (const std::length_error&)
    {
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanDevice::ReadbackBufferForTesting(
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer>& Buffer,
    Stoner::Core::uint64 Offset,
    Stoner::Core::uint64 Size,
    Stoner::Core::TArray<Stoner::Core::uint8>& OutBytes)
{
    OutBytes.clear();
    if (!IsActive() || !Buffer || Size == 0 ||
        Offset > Buffer->GetSizeInBytes() ||
        Size > Buffer->GetSizeInBytes() - Offset)
        return Stoner::RHI::ERHIResult::InvalidState;
    const auto VulkanBuffer = std::dynamic_pointer_cast<FVulkanBuffer>(Buffer);
    if (!VulkanBuffer || VulkanBuffer->GetLifecycleState() !=
            Stoner::RHI::ERHIResourceLifecycleState::Valid)
        return Stoner::RHI::ERHIResult::InvalidState;
    const auto& Bytes = VulkanBuffer->GetUploadedBytes();
    if (Offset > Bytes.size() || Size > Bytes.size() - Offset)
        return Stoner::RHI::ERHIResult::NotReady;
    try
    {
        OutBytes.assign(
            Bytes.begin() + static_cast<Stoner::Core::usize>(Offset),
            Bytes.begin() + static_cast<Stoner::Core::usize>(Offset + Size));
    }
    catch (const std::bad_alloc&)
    {
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    catch (const std::length_error&)
    {
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    return Stoner::RHI::ERHIResult::Success;
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

    Stoner::Core::TSharedPtr<FVulkanSampler> Sampler;
    try
    {
        Sampler.reset(new FVulkanSampler(Desc));
    }
    catch (const std::bad_alloc&)
    {
        MarkResourceAllocation(
            Diagnostics, "sampler wrapper allocation failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    try
    {
        TrackObject(Samplers, Sampler);
    }
    catch (const std::bad_alloc&)
    {
        (void)Sampler->Invalidate();
        MarkResourceAllocation(
            Diagnostics, "sampler resource tracking allocation failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    catch (const std::length_error&)
    {
        (void)Sampler->Invalidate();
        MarkResourceAllocation(
            Diagnostics, "sampler resource tracking capacity exceeded");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    return {Stoner::RHI::ERHIResult::Success, Sampler};
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIShaderModule> FVulkanDevice::CreateShaderModule(const Stoner::RHI::FRHIShaderModuleDesc& Desc)
{
    if (!IsActive())
    {
        MarkShaderModule(Diagnostics, "shader module creation rejected after device shutdown");
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    if (!Stoner::RHI::IsSupportedRHIShaderStage(Desc.Stage))
    {
        MarkShaderModule(Diagnostics, "shader module creation rejected unsupported stage");
        return {Stoner::RHI::ERHIResult::Unsupported, nullptr};
    }
    if (!Stoner::RHI::IsValidRHIShaderModuleDesc(Desc))
    {
        MarkShaderModule(Diagnostics, "shader module creation rejected invalid bytecode or interface metadata");
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    if (Desc.Payload.Format != Stoner::RHI::ERHIShaderPayloadFormat::SPIRV)
    {
        MarkShaderModule(Diagnostics, "Vulkan shader module creation requires SPIR-V payload bytes");
        return {Stoner::RHI::ERHIResult::Unsupported, nullptr};
    }

    Stoner::RHI::FRHIShaderModuleDesc RuntimeDesc;
    try
    {
        RuntimeDesc = Desc;
    }
    catch (const std::bad_alloc&)
    {
        MarkShaderModule(Diagnostics, "shader module description allocation failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    catch (const std::length_error&)
    {
        MarkShaderModule(Diagnostics, "shader module description capacity exceeded");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    Stoner::Core::uint64 NativeToken = 0;
    const bool bUseNativeRuntime = HasNativeShaderRuntime();
    if (bUseNativeRuntime)
    {
        Stoner::Core::TArray<Stoner::Core::uint32> SpirvWords;
        if (!Stoner::RHI::TryGetRHIShaderSpirvWords(
                Desc.Payload, SpirvWords))
        {
            MarkShaderModule(
                Diagnostics, "native shader module creation could not align SPIR-V bytes");
            return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
        }
        const Stoner::RHI::ERHIResult NativeResult =
            NativeShaderContext->CreateOwnedShaderModule(
                SpirvWords, NativeToken);
        if (NativeResult != Stoner::RHI::ERHIResult::Success)
        {
            MarkShaderModule(
                Diagnostics, "native shader module creation rejected by Vulkan runtime");
            return {NativeResult, nullptr};
        }
    }
    RuntimeDesc.RuntimeMode = bUseNativeRuntime
        ? Stoner::RHI::ERHIRuntimeObjectMode::RealRuntime
        : Stoner::RHI::ERHIRuntimeObjectMode::DeterministicFallback;
    RuntimeDesc.ValidationMode = bUseNativeRuntime
        ? Stoner::RHI::ERHIShaderBytecodeValidationMode::Runtime
        : Stoner::RHI::ERHIShaderBytecodeValidationMode::StructuralFallback;
    const char* Reason = bUseNativeRuntime
        ? "shader module created and retained by native Vulkan runtime"
        : "shader module created as deterministic fallback; no real runtime execution occurred";

    Stoner::Core::TSharedPtr<FVulkanShaderModule> ShaderModule;
    try
    {
        ShaderModule.reset(new FVulkanShaderModule(
            std::move(RuntimeDesc),
            DeviceOwner,
            bUseNativeRuntime ? NativeShaderContext : nullptr,
            NativeToken,
            Reason));
    }
    catch (const std::bad_alloc&)
    {
        if (bUseNativeRuntime)
        {
            NativeShaderContext->DestroyOwnedShaderModule(NativeToken);
        }
        MarkShaderModule(Diagnostics, "shader module wrapper allocation failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    try
    {
        TrackObject(ShaderModules, ShaderModule);
    }
    catch (const std::bad_alloc&)
    {
        (void)ShaderModule->Invalidate();
        MarkShaderModule(Diagnostics, "shader module tracking allocation failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    catch (const std::length_error&)
    {
        (void)ShaderModule->Invalidate();
        MarkShaderModule(Diagnostics, "shader module tracking capacity exceeded");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    MarkShaderModule(Diagnostics, Reason);
    MarkRuntimeMode(Diagnostics, Reason);
    return {Stoner::RHI::ERHIResult::Success, ShaderModule};
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIPipelineLayout> FVulkanDevice::CreatePipelineLayout(const Stoner::RHI::FRHIPipelineLayoutDesc& Desc)
{
    if (!IsActive())
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    if (!Stoner::RHI::IsValidRHIPipelineLayoutDesc(Desc))
    {
        MarkDescriptorUpdate(Diagnostics, "invalid descriptor binding or constant range in pipeline layout");
        MarkPipelineLayout(Diagnostics, "invalid descriptor binding or constant range in pipeline layout");
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    Stoner::Core::TSharedPtr<FVulkanPipelineLayout> Layout;
    try
    {
        Layout.reset(new FVulkanPipelineLayout(Desc, DeviceOwner));
        TrackObject(PipelineLayouts, Layout);
    }
    catch (const std::bad_alloc&)
    {
        if (Layout)
        {
            (void)Layout->Invalidate();
        }
        MarkPipelineLayout(Diagnostics, "pipeline layout allocation failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    catch (const std::length_error&)
    {
        if (Layout)
        {
            (void)Layout->Invalidate();
        }
        MarkPipelineLayout(Diagnostics, "pipeline layout tracking capacity exceeded");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    MarkPipelineLayout(Diagnostics, "pipeline layout created with descriptor bindings and constant ranges");
    return {Stoner::RHI::ERHIResult::Success, Layout};
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIDescriptorSet> FVulkanDevice::CreateDescriptorSet(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIPipelineLayout>& Layout, Stoner::Core::uint32 SetIndex)
{
    if (!IsActive())
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    auto VulkanLayout = std::dynamic_pointer_cast<FVulkanPipelineLayout>(Layout);
    if (!VulkanLayout || !VulkanLayout->BelongsTo(DeviceOwner) ||
        SetIndex >= Layout->GetSetCount())
    {
        MarkDescriptorUpdate(Diagnostics, "missing or invalid descriptor set layout");
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }

    const Stoner::RHI::ERHIResult PoolReady = EnsureDescriptorPool();
    if (PoolReady != Stoner::RHI::ERHIResult::Success)
    {
        MarkDescriptorPool(
            Diagnostics, "descriptor pool allocation failed");
        return {PoolReady, nullptr};
    }

    FVulkanDescriptorReservation Reservation;
    const Stoner::RHI::ERHIResult PoolResult =
        DescriptorPool->Acquire(DescriptorPool, Reservation);
    if (PoolResult != Stoner::RHI::ERHIResult::Success)
    {
        MarkDescriptorPool(Diagnostics, "descriptor pool capacity exhausted");
        return {PoolResult, nullptr};
    }

    Stoner::Core::TSharedPtr<FVulkanDescriptorSet> DescriptorSet;
    try
    {
        DescriptorSet.reset(new FVulkanDescriptorSet(
            Layout, SetIndex, std::move(Reservation)));
    }
    catch (const std::bad_alloc&)
    {
        MarkDescriptorPool(
            Diagnostics, "descriptor set wrapper allocation failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    try
    {
        DescriptorSets.erase(
            std::remove_if(
                DescriptorSets.begin(), DescriptorSets.end(),
                [](const auto& Candidate) { return Candidate.expired(); }),
            DescriptorSets.end());
        DescriptorSets.push_back(DescriptorSet);
    }
    catch (const std::bad_alloc&)
    {
        (void)DescriptorSet->Invalidate();
        MarkDescriptorPool(
            Diagnostics, "descriptor set tracking allocation failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    catch (const std::length_error&)
    {
        (void)DescriptorSet->Invalidate();
        MarkDescriptorPool(
            Diagnostics, "descriptor set tracking capacity exceeded");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    return {Stoner::RHI::ERHIResult::Success, DescriptorSet};
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIGraphicsPipeline> FVulkanDevice::CreateGraphicsPipeline(const Stoner::RHI::FRHIGraphicsPipelineDesc& Desc)
{
    if (!IsActive())
    {
        MarkGraphicsPipeline(Diagnostics, "graphics pipeline creation rejected after device shutdown");
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    if (!Stoner::RHI::IsValidRHIGraphicsPipelineState(Desc) ||
        !HasValidGraphicsShaderStages(Desc, DeviceOwner))
    {
        MarkGraphicsPipeline(Diagnostics, "graphics pipeline creation rejected by shader layout or triangle-ready state validation");
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    if (!SupportsGraphicsPipelineDesc(Desc))
    {
        MarkGraphicsPipeline(
            Diagnostics,
            "graphics pipeline creation rejected by device format capabilities");
        return {Stoner::RHI::ERHIResult::Unsupported, nullptr};
    }

    Stoner::RHI::FRHIGraphicsPipelineDesc RuntimeDesc;
    try
    {
        RuntimeDesc = Desc;
        RuntimeDesc.ReuseState =
            Stoner::RHI::ERHIPipelineReuseState::Created;
        RuntimeDesc.CompatibilitySummary =
            "graphics pipeline compatible";
    }
    catch (const std::bad_alloc&)
    {
        MarkGraphicsPipeline(
            Diagnostics, "graphics pipeline description allocation failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    catch (const std::length_error&)
    {
        MarkGraphicsPipeline(
            Diagnostics, "graphics pipeline description capacity exceeded");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }

    Stoner::Core::TSharedPtr<FVulkanShaderModule> VertexShader;
    Stoner::Core::TSharedPtr<FVulkanShaderModule> FragmentShader;
    for (const auto& Shader : RuntimeDesc.ShaderModules)
    {
        auto VulkanShader =
            std::dynamic_pointer_cast<FVulkanShaderModule>(Shader);
        if (VulkanShader &&
            VulkanShader->GetStage() ==
                Stoner::RHI::ERHIShaderStage::Vertex)
        {
            VertexShader = std::move(VulkanShader);
        }
        else if (VulkanShader &&
            VulkanShader->GetStage() ==
                Stoner::RHI::ERHIShaderStage::Fragment)
        {
            FragmentShader = std::move(VulkanShader);
        }
    }
    const bool bUseNativeRuntime =
        HasNativeShaderRuntime() &&
        VertexShader && FragmentShader &&
        VertexShader->HasNativeObject() &&
        FragmentShader->HasNativeObject() &&
        VertexShader->NativeContext == NativeShaderContext &&
        FragmentShader->NativeContext == NativeShaderContext;
    RuntimeDesc.RuntimeMode = bUseNativeRuntime
        ? Stoner::RHI::ERHIRuntimeObjectMode::RealRuntime
        : Stoner::RHI::ERHIRuntimeObjectMode::DeterministicFallback;

    Stoner::Core::FString Key;
    try
    {
        Key = PipelineCache.BuildGraphicsKey(RuntimeDesc);
    }
    catch (const std::bad_alloc&)
    {
        MarkPipelineCache(
            Diagnostics, "graphics pipeline cache key allocation failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    catch (const std::length_error&)
    {
        MarkPipelineCache(
            Diagnostics, "graphics pipeline cache key capacity exceeded");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    if (auto Existing = PipelineCache.FindGraphics(Key))
    {
        Existing->SetReuseState(Stoner::RHI::ERHIPipelineReuseState::Reused);
        MarkPipelineCache(Diagnostics, "graphics pipeline process-local cache hit");
        MarkGraphicsPipeline(Diagnostics, "graphics pipeline reused from process-local cache");
        return {Stoner::RHI::ERHIResult::Success, Existing};
    }
    if (!CanCreatePipeline())
    {
        MarkGraphicsPipeline(Diagnostics, "graphics pipeline creation rejected by configured creation limit");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }

    Stoner::Core::uint64 NativeToken = 0;
    if (bUseNativeRuntime)
    {
        const Stoner::RHI::ERHIResult NativeResult =
            NativeShaderContext->CreateOwnedGraphicsPipeline(
                RuntimeDesc,
                VertexShader->NativeToken,
                FragmentShader->NativeToken,
                NativeToken);
        if (NativeResult != Stoner::RHI::ERHIResult::Success)
        {
            MarkGraphicsPipeline(
                Diagnostics,
                "native graphics pipeline creation rejected by Vulkan runtime");
            return {NativeResult, nullptr};
        }
    }
    const char* Reason = bUseNativeRuntime
        ? "graphics pipeline created and retained by native Vulkan runtime"
        : "graphics pipeline created as deterministic fallback; no real runtime execution occurred";
    Stoner::Core::TSharedPtr<FVulkanGraphicsPipeline> Pipeline;
    try
    {
        Pipeline.reset(new FVulkanGraphicsPipeline(
            std::move(RuntimeDesc),
            DeviceOwner,
            bUseNativeRuntime ? NativeShaderContext : nullptr,
            NativeToken,
            Reason));
    }
    catch (const std::bad_alloc&)
    {
        if (bUseNativeRuntime)
        {
            NativeShaderContext->DestroyOwnedPipeline(NativeToken);
        }
        MarkGraphicsPipeline(
            Diagnostics, "graphics pipeline wrapper allocation failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    try
    {
        TrackObject(GraphicsPipelines, Pipeline);
    }
    catch (const std::bad_alloc&)
    {
        (void)Pipeline->Invalidate();
        MarkGraphicsPipeline(
            Diagnostics, "graphics pipeline tracking allocation failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    catch (const std::length_error&)
    {
        (void)Pipeline->Invalidate();
        MarkGraphicsPipeline(
            Diagnostics, "graphics pipeline tracking capacity exceeded");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    try
    {
        PipelineCache.InsertGraphics(Key, Pipeline);
    }
    catch (const std::bad_alloc&)
    {
        GraphicsPipelines.pop_back();
        (void)Pipeline->Invalidate();
        MarkPipelineCache(
            Diagnostics, "graphics pipeline cache insertion failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    catch (const std::length_error&)
    {
        GraphicsPipelines.pop_back();
        (void)Pipeline->Invalidate();
        MarkPipelineCache(
            Diagnostics, "graphics pipeline cache capacity exceeded");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    ++SuccessfulPipelineCreations;
    MarkGraphicsPipeline(Diagnostics, Reason);
    MarkRuntimeMode(Diagnostics, Reason);
    MarkPipelineCache(Diagnostics, "graphics pipeline inserted into process-local cache");
    return {Stoner::RHI::ERHIResult::Success, Pipeline};
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIComputePipeline> FVulkanDevice::CreateComputePipeline(const Stoner::RHI::FRHIComputePipelineDesc& Desc)
{
    if (!IsActive())
    {
        MarkComputePipeline(Diagnostics, "compute pipeline creation rejected after device shutdown");
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    if (!Capabilities.SupportsQueue(Stoner::RHI::ERHIQueueType::Compute))
    {
        MarkComputePipeline(Diagnostics, "compute pipeline creation rejected because compute queue is unsupported");
        return {Stoner::RHI::ERHIResult::Unsupported, nullptr};
    }
    if (!Desc.PipelineLayout || Desc.PipelineLayout->GetLifecycleState() != Stoner::RHI::ERHIResourceLifecycleState::Valid ||
        Desc.ShaderModules.size() != 1 || !Desc.ShaderModules[0] || Desc.ShaderModules[0]->GetLifecycleState() != Stoner::RHI::ERHIResourceLifecycleState::Valid)
    {
        MarkComputePipeline(Diagnostics, "compute pipeline creation rejected by missing or invalidated dependency");
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    if (Desc.ShaderModules[0]->GetStage() != Stoner::RHI::ERHIShaderStage::Compute)
    {
        MarkComputePipeline(Diagnostics, "compute pipeline creation rejected non-compute shader");
        return {Stoner::RHI::ERHIResult::Unsupported, nullptr};
    }
    if (!HasValidComputeShaderStage(Desc, DeviceOwner))
    {
        MarkComputePipeline(Diagnostics, "compute pipeline creation rejected by shader interface and layout mismatch");
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }

    Stoner::RHI::FRHIComputePipelineDesc RuntimeDesc;
    try
    {
        RuntimeDesc = Desc;
        RuntimeDesc.RuntimeMode =
            Stoner::RHI::ERHIRuntimeObjectMode::Unknown;
        RuntimeDesc.ReuseState =
            Stoner::RHI::ERHIPipelineReuseState::Created;
        RuntimeDesc.CompatibilitySummary =
            "compute pipeline compatible";
    }
    catch (const std::bad_alloc&)
    {
        MarkComputePipeline(
            Diagnostics, "compute pipeline description allocation failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    catch (const std::length_error&)
    {
        MarkComputePipeline(
            Diagnostics, "compute pipeline description capacity exceeded");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }

    auto ComputeShader =
        std::dynamic_pointer_cast<FVulkanShaderModule>(
            RuntimeDesc.ShaderModules[0]);
    const bool bUseNativeRuntime =
        HasNativeShaderRuntime() &&
        ComputeShader && ComputeShader->HasNativeObject() &&
        ComputeShader->NativeContext == NativeShaderContext;
    RuntimeDesc.RuntimeMode = bUseNativeRuntime
        ? Stoner::RHI::ERHIRuntimeObjectMode::RealRuntime
        : Stoner::RHI::ERHIRuntimeObjectMode::DeterministicFallback;

    Stoner::Core::FString Key;
    try
    {
        Key = PipelineCache.BuildComputeKey(RuntimeDesc);
    }
    catch (const std::bad_alloc&)
    {
        MarkPipelineCache(
            Diagnostics, "compute pipeline cache key allocation failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    catch (const std::length_error&)
    {
        MarkPipelineCache(
            Diagnostics, "compute pipeline cache key capacity exceeded");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    if (auto Existing = PipelineCache.FindCompute(Key))
    {
        Existing->SetReuseState(Stoner::RHI::ERHIPipelineReuseState::Reused);
        MarkPipelineCache(Diagnostics, "compute pipeline process-local cache hit");
        MarkComputePipeline(Diagnostics, "compute pipeline reused from process-local cache");
        return {Stoner::RHI::ERHIResult::Success, Existing};
    }
    if (!CanCreatePipeline())
    {
        MarkComputePipeline(Diagnostics, "compute pipeline creation rejected by configured creation limit");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }

    Stoner::Core::uint64 NativeToken = 0;
    if (bUseNativeRuntime)
    {
        const Stoner::RHI::ERHIResult NativeResult =
            NativeShaderContext->CreateOwnedComputePipeline(
                RuntimeDesc,
                ComputeShader->NativeToken,
                NativeToken);
        if (NativeResult != Stoner::RHI::ERHIResult::Success)
        {
            MarkComputePipeline(
                Diagnostics,
                "native compute pipeline creation rejected by Vulkan runtime");
            return {NativeResult, nullptr};
        }
    }
    const char* Reason = bUseNativeRuntime
        ? "compute pipeline created and retained by native Vulkan runtime"
        : "compute pipeline created as deterministic fallback; no real runtime execution occurred";
    Stoner::Core::TSharedPtr<FVulkanComputePipeline> Pipeline;
    try
    {
        Pipeline.reset(new FVulkanComputePipeline(
            std::move(RuntimeDesc),
            DeviceOwner,
            bUseNativeRuntime ? NativeShaderContext : nullptr,
            NativeToken,
            Reason));
    }
    catch (const std::bad_alloc&)
    {
        if (bUseNativeRuntime)
        {
            NativeShaderContext->DestroyOwnedPipeline(NativeToken);
        }
        MarkComputePipeline(
            Diagnostics, "compute pipeline wrapper allocation failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    try
    {
        TrackObject(ComputePipelines, Pipeline);
    }
    catch (const std::bad_alloc&)
    {
        (void)Pipeline->Invalidate();
        MarkComputePipeline(
            Diagnostics, "compute pipeline tracking allocation failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    catch (const std::length_error&)
    {
        (void)Pipeline->Invalidate();
        MarkComputePipeline(
            Diagnostics, "compute pipeline tracking capacity exceeded");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    try
    {
        PipelineCache.InsertCompute(Key, Pipeline);
    }
    catch (const std::bad_alloc&)
    {
        ComputePipelines.pop_back();
        (void)Pipeline->Invalidate();
        MarkPipelineCache(
            Diagnostics, "compute pipeline cache insertion failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    catch (const std::length_error&)
    {
        ComputePipelines.pop_back();
        (void)Pipeline->Invalidate();
        MarkPipelineCache(
            Diagnostics, "compute pipeline cache capacity exceeded");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    ++SuccessfulPipelineCreations;
    MarkComputePipeline(Diagnostics, Reason);
    MarkRuntimeMode(Diagnostics, Reason);
    MarkPipelineCache(Diagnostics, "compute pipeline inserted into process-local cache");
    return {Stoner::RHI::ERHIResult::Success, Pipeline};
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIRenderPass> FVulkanDevice::CreateRenderPass(const Stoner::RHI::FRHIRenderPassDesc& Desc)
{
    if (!IsActive())
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    if (!FVulkanRenderPass::IsSupportedDesc(Desc))
    {
        MarkRenderPass(Diagnostics, "invalid or unsupported minimal render pass description");
        return {Stoner::RHI::ERHIResult::Unsupported, nullptr};
    }

    Stoner::Core::TSharedPtr<FVulkanRenderPass> RenderPass;
    try
    {
        RenderPass.reset(new FVulkanRenderPass(Desc));
    }
    catch (const std::bad_alloc&)
    {
        MarkRenderPass(Diagnostics, "render pass wrapper allocation failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    try
    {
        TrackObject(RenderPasses, RenderPass);
    }
    catch (const std::bad_alloc&)
    {
        (void)RenderPass->Invalidate();
        MarkRenderPass(Diagnostics, "render pass tracking allocation failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    catch (const std::length_error&)
    {
        (void)RenderPass->Invalidate();
        MarkRenderPass(Diagnostics, "render pass tracking capacity exceeded");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    MarkRenderPass(Diagnostics, "minimal single-subpass render pass created");
    return {Stoner::RHI::ERHIResult::Success, RenderPass};
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIFramebuffer> FVulkanDevice::CreateFramebuffer(const Stoner::RHI::FRHIFramebufferDesc& Desc)
{
    if (!IsActive())
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    if (!FVulkanFramebuffer::IsSupportedDesc(Desc))
    {
        MarkFramebuffer(Diagnostics, "invalid or incompatible minimal framebuffer description");
        return {Stoner::RHI::ERHIResult::Unsupported, nullptr};
    }

    Stoner::Core::TSharedPtr<FVulkanFramebuffer> Framebuffer;
    try
    {
        Framebuffer.reset(new FVulkanFramebuffer(Desc));
    }
    catch (const std::bad_alloc&)
    {
        MarkFramebuffer(Diagnostics, "framebuffer wrapper allocation failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    try
    {
        TrackObject(Framebuffers, Framebuffer);
    }
    catch (const std::bad_alloc&)
    {
        (void)Framebuffer->Invalidate();
        MarkFramebuffer(Diagnostics, "framebuffer tracking allocation failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    catch (const std::length_error&)
    {
        (void)Framebuffer->Invalidate();
        MarkFramebuffer(Diagnostics, "framebuffer tracking capacity exceeded");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    MarkFramebuffer(Diagnostics, "minimal framebuffer created");
    return {Stoner::RHI::ERHIResult::Success, Framebuffer};
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
        try
        {
            UploadRequests.erase(
                std::remove_if(
                    UploadRequests.begin(), UploadRequests.end(),
                    [](const auto& Request) { return Request.expired(); }),
                UploadRequests.end());
            UploadRequests.push_back(Result.Object);
        }
        catch (const std::bad_alloc&)
        {
            (void)Result.Object->Invalidate();
            MarkUploadRejection(
                Diagnostics, "buffer upload tracking allocation failed");
            return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
        }
        catch (const std::length_error&)
        {
            (void)Result.Object->Invalidate();
            MarkUploadRejection(
                Diagnostics, "buffer upload tracking capacity exceeded");
            return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
        }
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
        try
        {
            UploadRequests.erase(
                std::remove_if(
                    UploadRequests.begin(), UploadRequests.end(),
                    [](const auto& Request) { return Request.expired(); }),
                UploadRequests.end());
            UploadRequests.push_back(Result.Object);
        }
        catch (const std::bad_alloc&)
        {
            (void)Result.Object->Invalidate();
            MarkUploadRejection(
                Diagnostics, "texture upload tracking allocation failed");
            return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
        }
        catch (const std::length_error&)
        {
            (void)Result.Object->Invalidate();
            MarkUploadRejection(
                Diagnostics, "texture upload tracking capacity exceeded");
            return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
        }
    }
    else
    {
        MarkUploadRejection(Diagnostics, "texture upload staging request rejected");
    }
    return Result;
}

Stoner::RHI::ERHIResult FVulkanDevice::CreateSurface(const Stoner::Core::FPlatformWindow& Window, FVulkanSurface& OutSurface)
{
    OutSurface = FVulkanSurface{};
    if (!IsActive())
    {
        MarkPresentationSkipped(Diagnostics, "inactive Vulkan device");
        return Stoner::RHI::ERHIResult::InvalidState;
    }

    Stoner::RHI::FRHIPresentationSurfaceDesc Desc;
    Desc.Window = Window;
    Desc.DebugName = "LegacyVulkanSurface";
    const Stoner::RHI::ERHIResult Result =
        FVulkanSurface::Create(Desc, PresentationOwner, OutSurface);
    if (Result != Stoner::RHI::ERHIResult::Success)
    {
        MarkPresentationSkipped(Diagnostics, OutSurface.GetDiagnosticReason());
        return Result;
    }

    Surfaces.push_back(Stoner::Core::MakeShared<FVulkanSurface>(OutSurface));
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIPresentationSurface>
FVulkanDevice::CreatePresentationSurface(
    const Stoner::RHI::FRHIPresentationSurfaceDesc& Desc)
{
    if (!IsActive())
    {
        MarkPresentationSkipped(Diagnostics, "inactive Vulkan device");
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }

    auto Surface = Stoner::Core::MakeShared<FVulkanSurface>();
    const Stoner::RHI::ERHIResult Result =
        FVulkanSurface::Create(Desc, PresentationOwner, *Surface);
    if (Result != Stoner::RHI::ERHIResult::Success)
    {
        MarkPresentationSkipped(Diagnostics, Surface->GetDiagnosticReason());
        return {Result, nullptr};
    }

    Surfaces.push_back(Surface);
    return {Stoner::RHI::ERHIResult::Success, Surface};
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHISwapchain>
FVulkanDevice::CreateSurfaceBackedSwapchain(
    const Stoner::Core::TSharedPtr<FVulkanSurface>& Surface,
    const Stoner::RHI::FRHISwapchainDesc& Desc)
{
    if (!IsActive())
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    if (!Surface || !Surface->IsValid() ||
        !Surface->BelongsTo(PresentationOwner))
    {
        MarkPresentationSkipped(
            Diagnostics, "missing, stale, or foreign presentation surface");
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    if (!Desc.IsValid() ||
        !Stoner::RHI::IsValidRHIFormat(Desc.PreferredFormat) ||
        Stoner::RHI::IsDepthStencilFormat(Desc.PreferredFormat))
    {
        MarkPresentationSkipped(Diagnostics, "invalid swapchain description");
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    if (!Capabilities.bSupportsPresentation ||
        !Capabilities.bSupportsPresentQueue ||
        Desc.FramesInFlight > Capabilities.MaxInFlightFrames ||
        !Capabilities.SupportsFormat(Desc.PreferredFormat))
    {
        MarkPresentationSkipped(
            Diagnostics, "unsupported swapchain capability request");
        return {Stoner::RHI::ERHIResult::Unsupported, nullptr};
    }

    auto Swapchain = Stoner::Core::MakeShared<FVulkanSwapchain>(
        Surface, Desc, Capabilities.MaxInFlightFrames);
    if (Swapchain->GetState() != Stoner::RHI::ERHISwapchainState::Ready ||
        !Swapchain->GetImage(0))
    {
        return {Stoner::RHI::ERHIResult::Failed, nullptr};
    }

    Swapchains.push_back(Swapchain);
    return {Stoner::RHI::ERHIResult::Success, Swapchain};
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHISwapchain>
FVulkanDevice::CreateSwapchain(
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIPresentationSurface>& Surface,
    const Stoner::RHI::FRHISwapchainDesc& Desc)
{
    return CreateSurfaceBackedSwapchain(
        std::dynamic_pointer_cast<FVulkanSurface>(Surface), Desc);
}

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHISwapchain> FVulkanDevice::CreateSwapchainForSurface(const FVulkanSurface& Surface, Stoner::Core::uint32 FrameCount)
{
    if (!IsActive())
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    if (FrameCount == 0)
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    if (!Surface.IsValid() || !Surface.BelongsTo(PresentationOwner))
    {
        MarkPresentationSkipped(
            Diagnostics, "missing, stale, or foreign presentation surface");
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }

    Stoner::RHI::FRHISwapchainDesc Desc;
    Desc.Width = 1;
    Desc.Height = 1;
    Desc.FramesInFlight = FrameCount;
    Desc.PreferredFormat = SelectPresentationFormat(Capabilities);
    return CreateSurfaceBackedSwapchain(
        Stoner::Core::MakeShared<FVulkanSurface>(Surface), Desc);
}

void FVulkanDevice::InvalidateOwnedObjects() noexcept
{
    if (DeviceOwner)
    {
        DeviceOwner->bActive = false;
    }
    for (const auto& WeakQueue : Queues)
    {
        if (const auto Queue = WeakQueue.lock())
        {
            Queue->Invalidate();
        }
    }
    for (const auto& Pool : CommandPools)
    {
        if (Pool)
        {
            Pool->Invalidate();
        }
    }
    for (const auto& WeakFence : Fences)
    {
        if (const auto Fence = WeakFence.lock())
        {
            Fence->Invalidate();
        }
    }
    for (const auto& WeakSemaphore : Semaphores)
    {
        if (const auto Semaphore = WeakSemaphore.lock())
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
    for (const auto& Surface : Surfaces)
    {
        if (Surface)
        {
            (void)Surface->Invalidate();
        }
    }
    if (PresentationOwner)
    {
        PresentationOwner->bActive = false;
    }
    for (const auto& WeakRequest : UploadRequests)
    {
        if (const auto Request = WeakRequest.lock())
        {
            (void)Request->Invalidate();
        }
    }
    for (const auto& WeakDescriptorSet : DescriptorSets)
    {
        if (const auto DescriptorSet = WeakDescriptorSet.lock())
        {
            (void)DescriptorSet->Invalidate();
        }
    }
    if (DescriptorPool)
    {
        (void)DescriptorPool->Invalidate();
    }
    PipelineCache.Invalidate();
    for (const auto& WeakGraphicsPipeline : GraphicsPipelines)
    {
        if (const auto GraphicsPipeline = WeakGraphicsPipeline.lock())
        {
            (void)GraphicsPipeline->Invalidate();
        }
    }
    for (const auto& WeakComputePipeline : ComputePipelines)
    {
        if (const auto ComputePipeline = WeakComputePipeline.lock())
        {
            (void)ComputePipeline->Invalidate();
        }
    }
    for (const auto& WeakShaderModule : ShaderModules)
    {
        if (const auto ShaderModule = WeakShaderModule.lock())
        {
            (void)ShaderModule->Invalidate();
        }
    }
    for (const auto& WeakLayout : PipelineLayouts)
    {
        if (const auto Layout = WeakLayout.lock())
        {
            (void)Layout->Invalidate();
        }
    }
    for (const auto& WeakSampler : Samplers)
    {
        if (const auto Sampler = WeakSampler.lock())
        {
            (void)Sampler->Invalidate();
        }
    }
    for (const auto& WeakFramebuffer : Framebuffers)
    {
        if (const auto Framebuffer = WeakFramebuffer.lock())
        {
            (void)Framebuffer->Invalidate();
        }
    }
    for (const auto& WeakRenderPass : RenderPasses)
    {
        if (const auto RenderPass = WeakRenderPass.lock())
        {
            (void)RenderPass->Invalidate();
        }
    }
    for (const auto& WeakTexture : Textures)
    {
        if (const auto Texture = WeakTexture.lock())
        {
            (void)Texture->Invalidate();
        }
    }
    for (const auto& WeakBuffer : Buffers)
    {
        if (const auto Buffer = WeakBuffer.lock())
        {
            (void)Buffer->Invalidate();
        }
    }
}

void FVulkanDevice::MapCapabilities(const FVulkanAdapterCandidate& Adapter)
{
    Capabilities = MakeVulkanBaselineDeviceCapabilities();
    Capabilities.bSupportsGraphicsQueue = Adapter.Queues.bGraphics;
    Capabilities.bSupportsComputeQueue = Adapter.Queues.bCompute;
    Capabilities.bSupportsTransferQueue = Adapter.Queues.bTransfer;
    Capabilities.bSupportsPresentQueue = Adapter.Queues.bPresent;
    Capabilities.bSupportsPresentation = Adapter.bPresentationSupported;
    Capabilities.Formats =
        Adapter.Formats.GetFormatCapabilities();
}

bool FVulkanDevice::SupportsBufferDesc(const Stoner::RHI::FRHIBufferDesc& Desc) noexcept
{
    if (!Stoner::RHI::IsValidRHIBufferDesc(Desc))
    {
        MarkResourceAllocation(Diagnostics, "invalid or unsupported buffer description");
        return false;
    }
    if (Desc.SizeInBytes > Capabilities.MaxBufferSizeBytes)
    {
        MarkResourceAllocation(Diagnostics, "buffer size exceeds published device capability");
        return false;
    }
    return true;
}

bool FVulkanDevice::SupportsTextureDesc(const Stoner::RHI::FRHITextureDesc& Desc) const noexcept
{
    using namespace Stoner::RHI;
    if (!IsValidRHITextureDesc(Desc))
    {
        return false;
    }
    const bool bDimensionSupported =
        (Desc.Dimension == ERHITextureDimension::Texture1D &&
         Desc.Width <= Capabilities.MaxTextureDimension1D) ||
        (Desc.Dimension == ERHITextureDimension::Texture2D &&
         Desc.Width <= Capabilities.MaxTextureDimension2D &&
         Desc.Height <= Capabilities.MaxTextureDimension2D) ||
        (Desc.Dimension == ERHITextureDimension::Texture3D &&
         Desc.Width <= Capabilities.MaxTextureDimension3D &&
         Desc.Height <= Capabilities.MaxTextureDimension3D &&
         Desc.Depth <= Capabilities.MaxTextureDimension3D) ||
        (Desc.Dimension == ERHITextureDimension::TextureCube &&
         Desc.Width <= Capabilities.MaxTextureDimension2D &&
         Desc.Height <= Capabilities.MaxTextureDimension2D) ||
        (Desc.Dimension == ERHITextureDimension::Texture1DArray &&
         Desc.Width <= Capabilities.MaxTextureDimension1D &&
         Desc.ArrayLayers <= Capabilities.MaxTextureArrayLayers) ||
        (Desc.Dimension == ERHITextureDimension::Texture2DArray &&
         Desc.Width <= Capabilities.MaxTextureDimension2D &&
         Desc.Height <= Capabilities.MaxTextureDimension2D &&
         Desc.ArrayLayers <= Capabilities.MaxTextureArrayLayers) ||
        (Desc.Dimension == ERHITextureDimension::TextureCubeArray &&
         Desc.Width <= Capabilities.MaxTextureDimension2D &&
         Desc.Height <= Capabilities.MaxTextureDimension2D &&
         Desc.ArrayLayers <= Capabilities.MaxTextureArrayLayers);
    if (!bDimensionSupported || !Capabilities.SupportsSampleCount(Desc.SampleCount))
    {
        return false;
    }
    ERHIFormatCapability Required =
        ERHIFormatCapability::None;
    if (HasRHIFlag(Desc.Usage, ERHITextureUsage::Sampled))
        Required |= ERHIFormatCapability::SampledImage;
    if (HasRHIFlag(Desc.Usage, ERHITextureUsage::CopySource))
        Required |= ERHIFormatCapability::CopySource;
    if (HasRHIFlag(
            Desc.Usage, ERHITextureUsage::CopyDestination))
        Required |= ERHIFormatCapability::CopyDestination;
    if (HasRHIFlag(
            Desc.Usage, ERHITextureUsage::ColorAttachment))
        Required |= ERHIFormatCapability::ColorAttachment;
    if (HasRHIFlag(
            Desc.Usage,
            ERHITextureUsage::DepthStencilAttachment))
        Required |=
            ERHIFormatCapability::DepthStencilAttachment;
    return Required == ERHIFormatCapability::None
        ? Capabilities.SupportsFormat(Desc.Format)
        : Capabilities.SupportsFormatUsage(
              Desc.Format, Required);
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

bool FVulkanDevice::SupportsGraphicsPipelineDesc(
    const Stoner::RHI::FRHIGraphicsPipelineDesc& Desc) const noexcept
{
    for (Stoner::RHI::ERHIFormat Format :
         Desc.RenderTargets.ColorFormats)
    {
        if (!Capabilities.SupportsFormat(Format))
        {
            return false;
        }
    }
    return Desc.RenderTargets.DepthStencilFormat ==
            Stoner::RHI::ERHIFormat::Unknown ||
        Capabilities.SupportsFormat(
            Desc.RenderTargets.DepthStencilFormat);
}

bool FVulkanDevice::CanCreatePipeline() noexcept
{
    return PipelineCreationLimit == 0 || SuccessfulPipelineCreations < PipelineCreationLimit;
}

Stoner::RHI::ERHIResult FVulkanDevice::EnsureDescriptorPool() noexcept
{
    if (DescriptorPool)
    {
        return DescriptorPool->GetLifecycleState() ==
                Stoner::RHI::ERHIResourceLifecycleState::Valid
            ? Stoner::RHI::ERHIResult::Success
            : Stoner::RHI::ERHIResult::InvalidState;
    }
    try
    {
        DescriptorPool.reset(
            new FVulkanDescriptorPool(DescriptorPoolCapacity));
    }
    catch (const std::bad_alloc&)
    {
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    return Stoner::RHI::ERHIResult::Success;
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
