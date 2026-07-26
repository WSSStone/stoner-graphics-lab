#include "VulkanRHI/FVulkanDevice.h"

#include "VulkanRHI/FVulkanBuffer.h"
#include "VulkanRHI/FVulkanCommandBuffer.h"
#include "VulkanRHI/FVulkanCommandPool.h"
#include "VulkanRHI/FVulkanComputePipeline.h"
#include "VulkanRHI/FVulkanDescriptorSet.h"
#include "VulkanRHI/FVulkanFence.h"
#include "VulkanRHI/FVulkanFramebuffer.h"
#include "VulkanRHI/FVulkanGraphicsPipeline.h"
#include "VulkanRHI/FVulkanPipelineCache.h"
#include "VulkanRHI/FVulkanPipelineLayout.h"
#include "VulkanRHI/FVulkanQueue.h"
#include "VulkanRHI/FVulkanRenderPass.h"
#include "VulkanRHI/FVulkanSampler.h"
#include "VulkanRHI/FVulkanSemaphore.h"
#include "VulkanRHI/FVulkanShaderModule.h"
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

[[nodiscard]] bool IsRuntimeFallback(const FVulkanDiagnostics& Diagnostics) noexcept
{
    return Diagnostics.bUsedRuntimeFallback || Diagnostics.Availability != EVulkanBackendAvailability::Available;
}

[[nodiscard]] Stoner::RHI::ERHIRuntimeObjectMode RuntimeModeForDiagnostics(const FVulkanDiagnostics& Diagnostics) noexcept
{
    return IsRuntimeFallback(Diagnostics)
        ? Stoner::RHI::ERHIRuntimeObjectMode::DeterministicFallback
        : Stoner::RHI::ERHIRuntimeObjectMode::RealRuntime;
}

[[nodiscard]] bool IsShaderLayoutCompatible(
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIShaderModule>& Shader,
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIPipelineLayout>& Layout) noexcept
{
    if (!Shader || !Layout || Shader->GetLifecycleState() != Stoner::RHI::ERHIResourceLifecycleState::Valid ||
        Layout->GetLifecycleState() != Stoner::RHI::ERHIResourceLifecycleState::Valid)
    {
        return false;
    }
    auto VulkanLayout = std::dynamic_pointer_cast<FVulkanPipelineLayout>(Layout);
    return VulkanLayout && VulkanLayout->IsCompatibleWithShaderInterface(Shader->GetDesc().InterfaceMetadata);
}

[[nodiscard]] bool HasValidGraphicsShaderStages(const Stoner::RHI::FRHIGraphicsPipelineDesc& Desc) noexcept
{
    bool bHasVertex = false;
    bool bHasFragment = false;
    for (const auto& Shader : Desc.ShaderModules)
    {
        if (!Shader || Shader->GetLifecycleState() != Stoner::RHI::ERHIResourceLifecycleState::Valid)
        {
            return false;
        }
        if (!IsShaderLayoutCompatible(Shader, Desc.PipelineLayout))
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

[[nodiscard]] bool HasValidComputeShaderStage(const Stoner::RHI::FRHIComputePipelineDesc& Desc) noexcept
{
    return Desc.ShaderModules.size() == 1 &&
        Desc.ShaderModules[0] &&
        Desc.ShaderModules[0]->GetLifecycleState() == Stoner::RHI::ERHIResourceLifecycleState::Valid &&
        Desc.ShaderModules[0]->GetStage() == Stoner::RHI::ERHIShaderStage::Compute &&
        IsShaderLayoutCompatible(Desc.ShaderModules[0], Desc.PipelineLayout);
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

Stoner::Core::uint32 FVulkanDevice::GetCommandBufferCapacity() const noexcept
{
    return CommandBufferCapacity;
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
    CommandPools.clear();
    CommandBuffers.clear();
    ShaderModules.clear();
    GraphicsPipelines.clear();
    ComputePipelines.clear();
    PipelineCache.Invalidate();
    SuccessfulPipelineCreations = 0;
    PresentationOwner = std::make_shared<FVulkanPresentationOwnerState>();
    PresentationOwner->bActive = true;
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
    for (const auto& Queue : Queues)
    {
        if (Queue)
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

    auto Queue = Stoner::Core::MakeShared<FVulkanQueue>(QueueType, &Diagnostics, CompletionInjection);
    Queues.push_back(Queue);
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
        Pool = Stoner::Core::MakeShared<FVulkanCommandPool>(CompatibleQueueType, CommandBufferCapacity);
        CommandPools.push_back(Pool);
    }

    auto Result = Pool->Allocate(Diagnostics);
    if (Result.Succeeded())
    {
        CommandBuffers.push_back(Result.Object);
    }
    return {Result.Result, Result.Object};
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

    Stoner::RHI::FRHIShaderModuleDesc RuntimeDesc = Desc;
    RuntimeDesc.RuntimeMode = RuntimeModeForDiagnostics(Diagnostics);
    RuntimeDesc.ValidationMode = IsRuntimeFallback(Diagnostics)
        ? Stoner::RHI::ERHIShaderBytecodeValidationMode::StructuralFallback
        : Stoner::RHI::ERHIShaderBytecodeValidationMode::Runtime;
    const char* Reason = IsRuntimeFallback(Diagnostics)
        ? "shader module created as deterministic fallback; no real runtime execution occurred"
        : "shader module created with runtime validation";
    auto ShaderModule = Stoner::Core::MakeShared<FVulkanShaderModule>(RuntimeDesc, Reason);
    ShaderModules.push_back(ShaderModule);
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
    auto Layout = Stoner::Core::MakeShared<FVulkanPipelineLayout>(Desc);
    PipelineLayouts.push_back(Layout);
    MarkPipelineLayout(Diagnostics, "pipeline layout created with descriptor bindings and constant ranges");
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

Stoner::RHI::TRHIObjectResult<Stoner::RHI::IRHIGraphicsPipeline> FVulkanDevice::CreateGraphicsPipeline(const Stoner::RHI::FRHIGraphicsPipelineDesc& Desc)
{
    if (!IsActive())
    {
        MarkGraphicsPipeline(Diagnostics, "graphics pipeline creation rejected after device shutdown");
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    if (!Stoner::RHI::IsValidRHIGraphicsPipelineState(Desc) || !HasValidGraphicsShaderStages(Desc))
    {
        MarkGraphicsPipeline(Diagnostics, "graphics pipeline creation rejected by shader layout or triangle-ready state validation");
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }

    const Stoner::Core::FString Key = PipelineCache.BuildGraphicsKey(Desc);
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

    Stoner::RHI::FRHIGraphicsPipelineDesc RuntimeDesc = Desc;
    RuntimeDesc.RuntimeMode = RuntimeModeForDiagnostics(Diagnostics);
    RuntimeDesc.ReuseState = Stoner::RHI::ERHIPipelineReuseState::Created;
    RuntimeDesc.CompatibilitySummary = "graphics pipeline compatible";
    const char* Reason = IsRuntimeFallback(Diagnostics)
        ? "graphics pipeline created as deterministic fallback; no real runtime execution occurred"
        : "graphics pipeline created with runtime object";
    auto Pipeline = Stoner::Core::MakeShared<FVulkanGraphicsPipeline>(RuntimeDesc, Reason);
    GraphicsPipelines.push_back(Pipeline);
    PipelineCache.InsertGraphics(Key, Pipeline);
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
    if (!HasValidComputeShaderStage(Desc))
    {
        MarkComputePipeline(Diagnostics, "compute pipeline creation rejected by shader interface and layout mismatch");
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }

    const Stoner::Core::FString Key = PipelineCache.BuildComputeKey(Desc);
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

    Stoner::RHI::FRHIComputePipelineDesc RuntimeDesc = Desc;
    RuntimeDesc.RuntimeMode = RuntimeModeForDiagnostics(Diagnostics);
    RuntimeDesc.ReuseState = Stoner::RHI::ERHIPipelineReuseState::Created;
    RuntimeDesc.CompatibilitySummary = "compute pipeline compatible";
    const char* Reason = IsRuntimeFallback(Diagnostics)
        ? "compute pipeline created as deterministic fallback; no real runtime execution occurred"
        : "compute pipeline created with runtime object";
    auto Pipeline = Stoner::Core::MakeShared<FVulkanComputePipeline>(RuntimeDesc, Reason);
    ComputePipelines.push_back(Pipeline);
    PipelineCache.InsertCompute(Key, Pipeline);
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

    auto RenderPass = Stoner::Core::MakeShared<FVulkanRenderPass>(Desc);
    RenderPasses.push_back(RenderPass);
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

    auto Framebuffer = Stoner::Core::MakeShared<FVulkanFramebuffer>(Desc);
    Framebuffers.push_back(Framebuffer);
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
    for (const auto& Queue : Queues)
    {
        if (Queue)
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
    for (const auto& CommandBuffer : CommandBuffers)
    {
        if (CommandBuffer)
        {
            CommandBuffer->Invalidate();
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
    PipelineCache.Invalidate();
    for (const auto& GraphicsPipeline : GraphicsPipelines)
    {
        if (GraphicsPipeline)
        {
            (void)GraphicsPipeline->Invalidate();
        }
    }
    for (const auto& ComputePipeline : ComputePipelines)
    {
        if (ComputePipeline)
        {
            (void)ComputePipeline->Invalidate();
        }
    }
    for (const auto& ShaderModule : ShaderModules)
    {
        if (ShaderModule)
        {
            (void)ShaderModule->Invalidate();
        }
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
    for (const auto& Framebuffer : Framebuffers)
    {
        if (Framebuffer)
        {
            (void)Framebuffer->Invalidate();
        }
    }
    for (const auto& RenderPass : RenderPasses)
    {
        if (RenderPass)
        {
            (void)RenderPass->Invalidate();
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
    Capabilities.SupportedFormats = Adapter.Formats.GetSupportedFormats();
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

bool FVulkanDevice::CanCreatePipeline() noexcept
{
    return PipelineCreationLimit == 0 || SuccessfulPipelineCreations < PipelineCreationLimit;
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
