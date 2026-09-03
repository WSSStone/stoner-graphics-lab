#include "FMetalDevice.h"

#include "FMetalAdapter.h"
#include "FMetalBuffer.h"
#include "FMetalCapabilities.h"
#include "FMetalComputePipeline.h"
#include "FMetalCommandBuffer.h"
#include "FMetalDescriptorSet.h"
#include "FMetalFramebuffer.h"
#include "FMetalFailureInjector.h"
#include "FMetalFormat.h"
#include "FMetalGraphicsPipeline.h"
#include "FMetalPipelineLayout.h"
#include "FMetalPresentationContext.h"
#include "FMetalPresentationSurface.h"
#include "FMetalQueue.h"
#include "FMetalRenderPass.h"
#include "FMetalSampler.h"
#include "FMetalShaderLibrary.h"
#include "FMetalSynchronization.h"
#include "FMetalSwapchain.h"
#include "FMetalTexture.h"
#include "FMetalUploadReadback.h"
#include "MetalRHI/FMetalDeviceFactory.h"
#include "RHI/FRHIBufferDesc.h"
#include "RHI/FRHIBufferUploadDesc.h"
#include "RHI/FRHIComputePipelineDesc.h"
#include "RHI/FRHIFramebufferDesc.h"
#include "RHI/FRHIGraphicsPipelineDesc.h"
#include "RHI/FRHIPipelineLayoutDesc.h"
#include "RHI/FRHIRenderPassDesc.h"
#include "RHI/FRHISamplerDesc.h"
#include "RHI/FRHIShaderModuleDesc.h"
#include "RHI/FRHITextureDesc.h"

#import <Metal/Metal.h>

#include <algorithm>
#include <atomic>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <string>

namespace Stoner::Backend::Metal::Private
{
namespace
{

std::atomic<Core::uint64> GNextOwnerIdentity{1};

MTLTextureType ToTextureType(RHI::ERHITextureDimension Dimension) noexcept
{
    switch (Dimension)
    {
    case RHI::ERHITextureDimension::Texture1D: return MTLTextureType1D;
    case RHI::ERHITextureDimension::Texture2D: return MTLTextureType2D;
    case RHI::ERHITextureDimension::Texture3D: return MTLTextureType3D;
    case RHI::ERHITextureDimension::TextureCube: return MTLTextureTypeCube;
    case RHI::ERHITextureDimension::Texture1DArray: return MTLTextureType1DArray;
    case RHI::ERHITextureDimension::Texture2DArray: return MTLTextureType2DArray;
    case RHI::ERHITextureDimension::TextureCubeArray:
        return MTLTextureTypeCubeArray;
    }
    return MTLTextureType2D;
}

MTLTextureUsage ToTextureUsage(RHI::ERHITextureUsage Usage) noexcept
{
    MTLTextureUsage Result = MTLTextureUsageUnknown;
    if (RHI::HasRHIFlag(Usage, RHI::ERHITextureUsage::Sampled))
        Result |= MTLTextureUsageShaderRead;
    if (RHI::HasRHIFlag(Usage, RHI::ERHITextureUsage::Storage))
        Result |= MTLTextureUsageShaderWrite;
    if (RHI::HasRHIFlag(Usage, RHI::ERHITextureUsage::ColorAttachment) ||
        RHI::HasRHIFlag(Usage, RHI::ERHITextureUsage::DepthStencilAttachment) ||
        RHI::HasRHIFlag(Usage, RHI::ERHITextureUsage::Present))
        Result |= MTLTextureUsageRenderTarget;
    return Result;
}

MTLSamplerMinMagFilter ToFilter(RHI::ERHISamplerFilter Filter) noexcept
{
    return Filter == RHI::ERHISamplerFilter::Nearest
        ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear;
}

MTLSamplerMipFilter ToMipFilter(RHI::ERHISamplerMipFilter Filter) noexcept
{
    switch (Filter)
    {
    case RHI::ERHISamplerMipFilter::None: return MTLSamplerMipFilterNotMipmapped;
    case RHI::ERHISamplerMipFilter::Nearest: return MTLSamplerMipFilterNearest;
    case RHI::ERHISamplerMipFilter::Linear: return MTLSamplerMipFilterLinear;
    }
    return MTLSamplerMipFilterNotMipmapped;
}

MTLSamplerAddressMode ToAddress(RHI::ERHISamplerAddressMode Mode) noexcept
{
    switch (Mode)
    {
    case RHI::ERHISamplerAddressMode::Repeat: return MTLSamplerAddressModeRepeat;
    case RHI::ERHISamplerAddressMode::MirroredRepeat:
        return MTLSamplerAddressModeMirrorRepeat;
    case RHI::ERHISamplerAddressMode::ClampToEdge:
        return MTLSamplerAddressModeClampToEdge;
    case RHI::ERHISamplerAddressMode::ClampToBorder:
        return MTLSamplerAddressModeClampToBorderColor;
    }
    return MTLSamplerAddressModeClampToEdge;
}

MTLCompareFunction ToCompare(RHI::ERHISamplerCompareMode Mode) noexcept
{
    switch (Mode)
    {
    case RHI::ERHISamplerCompareMode::None: return MTLCompareFunctionNever;
    case RHI::ERHISamplerCompareMode::Less: return MTLCompareFunctionLess;
    case RHI::ERHISamplerCompareMode::LessEqual:
        return MTLCompareFunctionLessEqual;
    case RHI::ERHISamplerCompareMode::Greater: return MTLCompareFunctionGreater;
    case RHI::ERHISamplerCompareMode::GreaterEqual:
        return MTLCompareFunctionGreaterEqual;
    case RHI::ERHISamplerCompareMode::Equal: return MTLCompareFunctionEqual;
    case RHI::ERHISamplerCompareMode::NotEqual:
        return MTLCompareFunctionNotEqual;
    case RHI::ERHISamplerCompareMode::Always: return MTLCompareFunctionAlways;
    case RHI::ERHISamplerCompareMode::Never: return MTLCompareFunctionNever;
    }
    return MTLCompareFunctionNever;
}

template <typename T>
RHI::TRHIObjectResult<T> Unsupported() noexcept
{
    return {RHI::ERHIResult::Unsupported, nullptr};
}

} // namespace

struct FMetalDevice::FImpl
{
    explicit FImpl(void* RetainedDevice) noexcept
        : Device((__bridge_transfer id<MTLDevice>)RetainedDevice)
    {
    }

    __strong id<MTLDevice> Device;
    __strong id<MTLCommandQueue> Queue;
    std::mutex PipelineCacheMutex;
    std::map<std::string, Core::TWeakPtr<FMetalGraphicsPipeline>>
        GraphicsPipelineCache;
    std::map<std::string, Core::TWeakPtr<FMetalComputePipeline>>
        ComputePipelineCache;
    std::mutex QueueMutex;
    Core::TArray<Core::TWeakPtr<FMetalQueue>> Queues;
    std::mutex SurfaceMutex;
    Core::TArray<Core::TWeakPtr<FMetalPresentationSurface>> Surfaces;
};

FMetalDevice::FMetalDevice(
    void* RetainedNativeDevice,
    Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
    FMetalAdapterSummary Adapter,
    RHI::FRHIDeviceCapabilities Capabilities)
    : Impl_(Core::MakeUnique<FImpl>(RetainedNativeDevice)),
      Owner_(std::move(Owner)),
      Adapter_(std::move(Adapter)),
      Capabilities_(std::move(Capabilities))
{
}

FMetalDevice::~FMetalDevice()
{
    (void)Shutdown();
}

bool FMetalDevice::Initialize(
    EMetalInitializationFailurePoint FailurePoint) noexcept
{
    if (!Impl_ || Impl_->Device == nil || !Owner_ ||
        State_ != RHI::ERHIDeviceState::Uninitialized ||
        FailurePoint == EMetalInitializationFailurePoint::AfterAdapterSelection)
        return false;
    if (!RHI::IsValidRHIDeviceCapabilities(Capabilities_) ||
        FailurePoint == EMetalInitializationFailurePoint::BeforeQueueCreation)
        return false;
    Impl_->Queue = [Impl_->Device newCommandQueue];
    if (Impl_->Queue == nil) return false;
    State_ = RHI::ERHIDeviceState::Active;
    return true;
}

RHI::ERHIDeviceState FMetalDevice::GetState() const noexcept { return State_; }

const RHI::FRHIDeviceCapabilities& FMetalDevice::GetCapabilities()
    const noexcept
{
    return Capabilities_;
}

bool FMetalDevice::IsActive() const noexcept
{
    return State_ == RHI::ERHIDeviceState::Active;
}

RHI::ERHIRuntimeMode FMetalDevice::GetRuntimeMode() const noexcept
{
    return RHI::ERHIRuntimeMode::NativeHeadless;
}

RHI::FRHIRuntimeSnapshot FMetalDevice::GetRuntimeSnapshot() const noexcept
{
    RHI::FRHIRuntimeSnapshot Result;
    Result.RequestedMode = RHI::ERHIRuntimeMode::NativeHeadless;
    Result.ObjectMode = IsActive()
        ? RHI::ERHIRuntimeObjectMode::RealRuntime
        : RHI::ERHIRuntimeObjectMode::Unknown;
    Result.AdapterName = Adapter_.Name;
    Result.bSoftwareDevice = false;
    Result.LiveInstances = IsActive() ? 1 : 0;
    Result.LiveDevices = IsActive() ? 1 : 0;
    const FMetalBackendInspection Inspection = Inspect();
    Result.LiveBuffers = static_cast<Core::uint32>(std::min<Core::uint64>(
        Inspection.ResourceOwnershipCount,
        std::numeric_limits<Core::uint32>::max()));
    Result.LivePipelines = static_cast<Core::uint32>(std::min<Core::uint64>(
        Inspection.PipelineOwnershipCount,
        std::numeric_limits<Core::uint32>::max()));
    Result.LiveCommandBuffers = static_cast<Core::uint32>(
        std::min<Core::uint64>(Inspection.CommandOwnershipCount,
            std::numeric_limits<Core::uint32>::max()));
    Result.LiveSynchronizationObjects = static_cast<Core::uint32>(
        std::min<Core::uint64>(
            Inspection.SynchronizationOwnershipCount +
                Inspection.SubmissionOwnershipCount,
            std::numeric_limits<Core::uint32>::max()));
    Result.LiveSurfaces = static_cast<Core::uint32>(std::min<Core::uint64>(
        Inspection.PresentationOwnershipCount,
        std::numeric_limits<Core::uint32>::max()));
    if (Impl_)
    {
        std::lock_guard Lock(Impl_->SurfaceMutex);
        for (const auto& WeakSurface : Impl_->Surfaces)
        {
            const auto Surface = WeakSurface.lock();
            if (!Surface || !Surface->IsValid() || !Surface->GetContext() ||
                !Surface->GetContext()->IsAttached())
                continue;
            const RHI::FRHIResolvedPresentationState Resolved =
                Surface->GetContext()->GetResolvedPresentationState();
            const FMetalPresentationLayerSnapshot Layer =
                Surface->GetContext()->GetLayerSnapshot();
            if (!Resolved.IsValid()) continue;
            Result.PresentationModeGeneration = Resolved.ModeGeneration;
            Result.PresentationWidth = Resolved.Width;
            Result.PresentationHeight = Resolved.Height;
            Result.PresentationFormat = Resolved.Format;
            Result.PresentationColorSpace = Resolved.ColorSpace;
            Result.PresentationNativeEncoding = Resolved.NativeEncoding;
            Result.PresentationDisplayAdaptation =
                Resolved.DisplayAdaptation;
            Result.PresentationMetadataDigest = Resolved.MetadataDigest;
            Result.LastAcquiredFrameToken = Layer.LastAcquiredFrameToken;
            Result.LastSubmittedFrameToken = Layer.LastSubmittedFrameToken;
            Result.LastPresentedFrameToken = Layer.LastPresentedFrameToken;
            Result.LiveSwapchains = 1;
            break;
        }
    }
    return Result;
}

RHI::ERHIResult FMetalDevice::Shutdown()
{
    if (State_ == RHI::ERHIDeviceState::Shutdown)
        return RHI::ERHIResult::InvalidState;
    const bool bInjectedShutdownFailure =
        FMetalFailureInjector::ShouldFail(EMetalFailurePoint::Shutdown);
    if (Owner_)
    {
        Owner_->StopAdmission();
    }
    RHI::ERHIResult DrainResult = RHI::ERHIResult::Success;
    if (Impl_)
    {
        Core::TArray<Core::TSharedPtr<FMetalQueue>> Queues;
        try
        {
            std::lock_guard Lock(Impl_->QueueMutex);
            for (const auto& WeakQueue : Impl_->Queues)
                if (auto Queue = WeakQueue.lock()) Queues.push_back(std::move(Queue));
        }
        catch (const std::bad_alloc&)
        {
            DrainResult = RHI::ERHIResult::Failed;
        }
        for (const auto& Queue : Queues)
        {
            const auto Result = Queue->WaitIdle();
            if (Result != RHI::ERHIResult::Success) DrainResult = Result;
        }
        Core::TArray<Core::TSharedPtr<FMetalPresentationSurface>> Surfaces;
        try
        {
            std::lock_guard Lock(Impl_->SurfaceMutex);
            for (const auto& WeakSurface : Impl_->Surfaces)
                if (auto Surface = WeakSurface.lock())
                    Surfaces.push_back(std::move(Surface));
        }
        catch (const std::bad_alloc&)
        {
            DrainResult = RHI::ERHIResult::Failed;
        }
        for (const auto& Surface : Surfaces)
        {
            const auto Result = Surface->Invalidate();
            if (Result != RHI::ERHIResult::Success &&
                Result != RHI::ERHIResult::InvalidState)
                DrainResult = Result;
        }
        if (Owner_ && !Owner_->IsShutdownReady())
        {
            Owner_->RecordTerminalFailure(
                Core::FString("metal-shutdown-in-flight-work-remains"));
            DrainResult = RHI::ERHIResult::Failed;
        }
        {
            std::lock_guard Lock(Impl_->PipelineCacheMutex);
            Impl_->GraphicsPipelineCache.clear();
            Impl_->ComputePipelineCache.clear();
        }
        {
            std::lock_guard Lock(Impl_->QueueMutex);
            Impl_->Queues.clear();
        }
        {
            std::lock_guard Lock(Impl_->SurfaceMutex);
            Impl_->Surfaces.clear();
        }
    }
    if (bInjectedShutdownFailure)
    {
        if (Owner_)
        {
            Owner_->RecordTerminalFailure(
                Core::FString(ToStableName(EMetalFailurePoint::Shutdown)));
            Owner_->RecordDiagnostic(
                Core::FString("Shutdown"), Core::FString("device"),
                RHI::ERHIResult::Failed,
                Core::FString(ToStableName(EMetalFailurePoint::Shutdown)),
                0, 0, {}, Core::FString("terminal-cleanup-complete"));
        }
        DrainResult = RHI::ERHIResult::Failed;
    }
    if (Owner_)
    {
        Owner_->AdvanceGeneration();
        Owner_->ReleaseDeviceOwnership();
    }
    Impl_.reset();
    State_ = RHI::ERHIDeviceState::Shutdown;
    return DrainResult;
}

RHI::TRHIObjectResult<RHI::IRHICommandQueue>
FMetalDevice::CreateCommandQueue(RHI::ERHIQueueType QueueType)
{
    if (!IsActive()) return {RHI::ERHIResult::InvalidState, nullptr};
    if (!Capabilities_.SupportsQueue(QueueType))
        return {RHI::ERHIResult::Unsupported, nullptr};
    try
    {
        void* RetainedQueue = (__bridge_retained void*)Impl_->Queue;
        Core::TSharedPtr<FMetalQueue> Queue;
        try
        {
            Queue = Core::MakeShared<FMetalQueue>(
                Owner_, QueueType, RetainedQueue, Capabilities_);
        }
        catch (...)
        {
            (void)(__bridge_transfer id<MTLCommandQueue>)RetainedQueue;
            throw;
        }
        if (!Queue->IsCompatible(Owner_))
            return {RHI::ERHIResult::InvalidState, nullptr};
        {
            std::lock_guard Lock(Impl_->QueueMutex);
            Impl_->Queues.push_back(Queue);
        }
        return {RHI::ERHIResult::Success, std::move(Queue)};
    }
    catch (const std::bad_alloc&)
    {
        return {RHI::ERHIResult::Failed, nullptr};
    }
    catch (const std::length_error&)
    {
        return {RHI::ERHIResult::Failed, nullptr};
    }
}
RHI::TRHIObjectResult<RHI::IRHICommandBuffer>
FMetalDevice::CreateCommandBuffer(RHI::ERHIQueueType QueueType)
{
    if (!IsActive()) return {RHI::ERHIResult::InvalidState, nullptr};
    if (!Capabilities_.SupportsQueue(QueueType))
        return {RHI::ERHIResult::Unsupported, nullptr};
    if (FMetalFailureInjector::ShouldFail(
            EMetalFailurePoint::CommandRecording))
    {
        Owner_->RecordDiagnostic(
            Core::FString("CreateCommandBuffer"), Core::FString("command"),
            RHI::ERHIResult::Failed,
            Core::FString(ToStableName(
                EMetalFailurePoint::CommandRecording)),
            0, 0, {}, Core::FString("recoverable"));
        return {RHI::ERHIResult::Failed, nullptr};
    }
    try
    {
        auto Commands = Core::MakeShared<FMetalCommandBuffer>(Owner_, QueueType);
        return Commands->IsCompatibleWith(Owner_)
            ? RHI::TRHIObjectResult<RHI::IRHICommandBuffer>{
                RHI::ERHIResult::Success, std::move(Commands)}
            : RHI::TRHIObjectResult<RHI::IRHICommandBuffer>{
                RHI::ERHIResult::InvalidState, nullptr};
    }
    catch (const std::bad_alloc&)
    {
        return {RHI::ERHIResult::Failed, nullptr};
    }
}
RHI::TRHIObjectResult<RHI::IRHIFence>
FMetalDevice::CreateFence(bool bInitiallySignaled)
{
    if (!IsActive() || !Capabilities_.bSupportsSynchronization)
        return {RHI::ERHIResult::InvalidState, nullptr};
    if (FMetalFailureInjector::ShouldFail(
            EMetalFailurePoint::Synchronization))
    {
        Owner_->RecordDiagnostic(
            Core::FString("CreateFence"), Core::FString("synchronization"),
            RHI::ERHIResult::Failed,
            Core::FString(ToStableName(
                EMetalFailurePoint::Synchronization)),
            0, 0, Core::FString("shared-events"),
            Core::FString("recoverable"));
        return {RHI::ERHIResult::Failed, nullptr};
    }
    @autoreleasepool
    {
        id<MTLSharedEvent> Event = [Impl_->Device newSharedEvent];
        if (Event == nil) return {RHI::ERHIResult::Unavailable, nullptr};
        void* RetainedEvent = (__bridge_retained void*)Event;
        try
        {
            auto Fence = Core::MakeShared<FMetalFence>(
                Owner_, RetainedEvent, bInitiallySignaled);
            return {RHI::ERHIResult::Success, std::move(Fence)};
        }
        catch (const std::bad_alloc&)
        {
            (void)(__bridge_transfer id<MTLSharedEvent>)RetainedEvent;
            return {RHI::ERHIResult::Failed, nullptr};
        }
    }
}
RHI::TRHIObjectResult<RHI::IRHISemaphore>
FMetalDevice::CreateSemaphore()
{
    if (!IsActive() || !Capabilities_.bSupportsSynchronization)
        return {RHI::ERHIResult::InvalidState, nullptr};
    if (FMetalFailureInjector::ShouldFail(
            EMetalFailurePoint::Synchronization))
    {
        Owner_->RecordDiagnostic(
            Core::FString("CreateSemaphore"),
            Core::FString("synchronization"), RHI::ERHIResult::Failed,
            Core::FString(ToStableName(
                EMetalFailurePoint::Synchronization)),
            0, 0, Core::FString("shared-events"),
            Core::FString("recoverable"));
        return {RHI::ERHIResult::Failed, nullptr};
    }
    @autoreleasepool
    {
        id<MTLSharedEvent> Event = [Impl_->Device newSharedEvent];
        if (Event == nil) return {RHI::ERHIResult::Unavailable, nullptr};
        void* RetainedEvent = (__bridge_retained void*)Event;
        try
        {
            auto Semaphore = Core::MakeShared<FMetalSemaphore>(
                Owner_, RetainedEvent);
            return {RHI::ERHIResult::Success, std::move(Semaphore)};
        }
        catch (const std::bad_alloc&)
        {
            (void)(__bridge_transfer id<MTLSharedEvent>)RetainedEvent;
            return {RHI::ERHIResult::Failed, nullptr};
        }
    }
}
RHI::TRHIObjectResult<RHI::IRHISwapchain>
FMetalDevice::CreateSwapchain(Core::uint32) { return Unsupported<RHI::IRHISwapchain>(); }

RHI::TRHIObjectResult<RHI::IRHIBuffer> FMetalDevice::CreateBuffer(
    const RHI::FRHIBufferDesc& Desc)
{
    if (!IsActive() || !RHI::IsValidRHIBufferDesc(Desc) ||
        Desc.SizeInBytes > Capabilities_.MaxBufferSizeBytes ||
        Desc.SizeInBytes > std::numeric_limits<NSUInteger>::max())
        return {RHI::ERHIResult::InvalidState, nullptr};
    if (FMetalFailureInjector::ShouldFail(
            EMetalFailurePoint::ResourceAllocation))
    {
        Owner_->RecordDiagnostic(
            Core::FString("CreateBuffer"), Core::FString("resource"),
            RHI::ERHIResult::Failed,
            Core::FString(ToStableName(
                EMetalFailurePoint::ResourceAllocation)),
            0, 0, {}, Core::FString("recoverable"));
        return {RHI::ERHIResult::Failed, nullptr};
    }
    @autoreleasepool
    {
        MTLResourceOptions Options = MTLResourceStorageModePrivate;
        if (Desc.MemoryAccess == RHI::ERHIMemoryAccess::HostVisible)
            Options = Impl_->Device.hasUnifiedMemory
                ? MTLResourceStorageModeShared
                : MTLResourceStorageModeManaged;
        id<MTLBuffer> Native = [Impl_->Device
            newBufferWithLength:static_cast<NSUInteger>(Desc.SizeInBytes)
                        options:Options];
        if (Native == nil) return {RHI::ERHIResult::Failed, nullptr};
        try
        {
            auto Object = Core::MakeShared<FMetalBuffer>(Owner_, Desc, Native);
            return Object->GetLifecycleState() ==
                    RHI::ERHIResourceLifecycleState::Valid
                ? RHI::TRHIObjectResult<RHI::IRHIBuffer>{
                    RHI::ERHIResult::Success, std::move(Object)}
                : RHI::TRHIObjectResult<RHI::IRHIBuffer>{
                    RHI::ERHIResult::InvalidState, nullptr};
        }
        catch (const std::bad_alloc&)
        {
            return {RHI::ERHIResult::Failed, nullptr};
        }
    }
}

RHI::ERHIResult FMetalDevice::UploadBuffer(
    const Core::TSharedPtr<RHI::IRHIBuffer>& Buffer,
    const RHI::FRHIBufferUploadDesc& Upload)
{
    auto Native = std::dynamic_pointer_cast<FMetalBuffer>(Buffer);
    if (!IsActive() || !Native || !Native->IsCompatible(Owner_) ||
        !RHI::IsValidRHIBufferUploadDesc(Native->GetDesc(), Upload))
        return RHI::ERHIResult::InvalidState;
    if (Native->GetNativeBuffer().storageMode != MTLStorageModePrivate)
        return Native->Upload(
            Upload.Data, Upload.DataSizeBytes, Upload.DestinationOffset);
    return UploadMetalPrivateBuffer(
        Impl_->Queue, Native->GetNativeBuffer(), Upload.Data,
        Upload.DataSizeBytes, Upload.DestinationOffset);
}

RHI::TRHIObjectResult<RHI::IRHITexture> FMetalDevice::CreateTexture(
    const RHI::FRHITextureDesc& Desc)
{
    if (!IsActive() || !RHI::IsValidRHITextureDesc(Desc) ||
        !Capabilities_.SupportsFormat(Desc.Format) ||
        !Capabilities_.SupportsSampleCount(Desc.SampleCount))
        return {RHI::ERHIResult::InvalidState, nullptr};
    if (FMetalFailureInjector::ShouldFail(
            EMetalFailurePoint::ResourceAllocation))
    {
        Owner_->RecordDiagnostic(
            Core::FString("CreateTexture"), Core::FString("resource"),
            RHI::ERHIResult::Failed,
            Core::FString(ToStableName(
                EMetalFailurePoint::ResourceAllocation)),
            0, 0, {}, Core::FString("recoverable"));
        return {RHI::ERHIResult::Failed, nullptr};
    }
    @autoreleasepool
    {
        MTLTextureDescriptor* NativeDesc = [[MTLTextureDescriptor alloc] init];
        NativeDesc.textureType = ToTextureType(Desc.Dimension);
        NativeDesc.pixelFormat = static_cast<MTLPixelFormat>(
            ToMetalPixelFormat(Desc.Format));
        NativeDesc.width = Desc.Width;
        NativeDesc.height = Desc.Height;
        NativeDesc.depth = Desc.Depth;
        NativeDesc.mipmapLevelCount = Desc.MipLevels;
        NativeDesc.sampleCount = static_cast<NSUInteger>(Desc.SampleCount);
        if (Desc.Dimension == RHI::ERHITextureDimension::TextureCubeArray)
            NativeDesc.arrayLength = Desc.ArrayLayers / 6;
        else if (Desc.Dimension == RHI::ERHITextureDimension::Texture1DArray ||
                 Desc.Dimension == RHI::ERHITextureDimension::Texture2DArray)
            NativeDesc.arrayLength = Desc.ArrayLayers;
        NativeDesc.storageMode = MTLStorageModePrivate;
        NativeDesc.usage = ToTextureUsage(Desc.Usage);
        id<MTLTexture> Native = [Impl_->Device newTextureWithDescriptor:NativeDesc];
        if (Native == nil) return {RHI::ERHIResult::Failed, nullptr};
        try
        {
            auto Object = Core::MakeShared<FMetalTexture>(Owner_, Desc, Native);
            return Object->GetLifecycleState() ==
                    RHI::ERHIResourceLifecycleState::Valid
                ? RHI::TRHIObjectResult<RHI::IRHITexture>{
                    RHI::ERHIResult::Success, std::move(Object)}
                : RHI::TRHIObjectResult<RHI::IRHITexture>{
                    RHI::ERHIResult::InvalidState, nullptr};
        }
        catch (const std::bad_alloc&)
        {
            return {RHI::ERHIResult::Failed, nullptr};
        }
    }
}

RHI::ERHIResult FMetalDevice::UploadTexture(
    const Core::TSharedPtr<RHI::IRHITexture>& Texture,
    const RHI::FRHITextureUploadDesc& Upload)
{
    auto Native = std::dynamic_pointer_cast<FMetalTexture>(Texture);
    if (!IsActive() || !Native || !Native->IsCompatible(Owner_))
        return RHI::ERHIResult::InvalidState;
    return UploadMetalTexture(
        Impl_->Queue, Native->GetNativeTexture(), Native->GetDesc(), Upload);
}

RHI::TRHIObjectResult<RHI::IRHISampler> FMetalDevice::CreateSampler(
    const RHI::FRHISamplerDesc& Desc)
{
    if (!IsActive() || !RHI::IsValidRHISamplerDesc(Desc))
        return {RHI::ERHIResult::InvalidState, nullptr};
    if (FMetalFailureInjector::ShouldFail(
            EMetalFailurePoint::ResourceAllocation))
    {
        Owner_->RecordDiagnostic(
            Core::FString("CreateSampler"), Core::FString("resource"),
            RHI::ERHIResult::Failed,
            Core::FString(ToStableName(
                EMetalFailurePoint::ResourceAllocation)),
            0, 0, {}, Core::FString("recoverable"));
        return {RHI::ERHIResult::Failed, nullptr};
    }
    @autoreleasepool
    {
        MTLSamplerDescriptor* NativeDesc = [[MTLSamplerDescriptor alloc] init];
        NativeDesc.minFilter = ToFilter(Desc.MinFilter);
        NativeDesc.magFilter = ToFilter(Desc.MagFilter);
        NativeDesc.mipFilter = ToMipFilter(Desc.MipFilter);
        NativeDesc.sAddressMode = ToAddress(Desc.AddressU);
        NativeDesc.tAddressMode = ToAddress(Desc.AddressV);
        NativeDesc.rAddressMode = ToAddress(Desc.AddressW);
        if (Desc.CompareMode != RHI::ERHISamplerCompareMode::None)
            NativeDesc.compareFunction = ToCompare(Desc.CompareMode);
        id<MTLSamplerState> Native =
            [Impl_->Device newSamplerStateWithDescriptor:NativeDesc];
        if (Native == nil) return {RHI::ERHIResult::Failed, nullptr};
        try
        {
            auto Object = Core::MakeShared<FMetalSampler>(Owner_, Desc, Native);
            return Object->GetLifecycleState() ==
                    RHI::ERHIResourceLifecycleState::Valid
                ? RHI::TRHIObjectResult<RHI::IRHISampler>{
                    RHI::ERHIResult::Success, std::move(Object)}
                : RHI::TRHIObjectResult<RHI::IRHISampler>{
                    RHI::ERHIResult::InvalidState, nullptr};
        }
        catch (const std::bad_alloc&)
        {
            return {RHI::ERHIResult::Failed, nullptr};
        }
    }
}

RHI::TRHIObjectResult<RHI::IRHIShaderModule>
FMetalDevice::CreateShaderModule(const RHI::FRHIShaderModuleDesc& Desc)
{
    if (!IsActive()) return {RHI::ERHIResult::InvalidState, nullptr};
    if (FMetalFailureInjector::ShouldFail(
            EMetalFailurePoint::PipelineCreation))
    {
        Owner_->RecordDiagnostic(
            Core::FString("CreateShaderModule"), Core::FString("pipeline"),
            RHI::ERHIResult::Failed,
            Core::FString(ToStableName(
                EMetalFailurePoint::PipelineCreation)),
            0, 0, {}, Core::FString("recoverable"));
        return {RHI::ERHIResult::Failed, nullptr};
    }
    return CreateMetalShaderLibrary(Owner_, GetNativeDevice(), Desc);
}
RHI::TRHIObjectResult<RHI::IRHIPipelineLayout>
FMetalDevice::CreatePipelineLayout(const RHI::FRHIPipelineLayoutDesc& Desc)
{
    if (!IsActive() || !RHI::IsValidRHIPipelineLayoutDesc(Desc))
        return {RHI::ERHIResult::InvalidState, nullptr};
    try
    {
        auto Object = Core::MakeShared<FMetalPipelineLayout>(Owner_, Desc);
        return {RHI::ERHIResult::Success, std::move(Object)};
    }
    catch (const std::bad_alloc&)
    {
        return {RHI::ERHIResult::Failed, nullptr};
    }
}
RHI::TRHIObjectResult<RHI::IRHIDescriptorSet>
FMetalDevice::CreateDescriptorSet(
    const Core::TSharedPtr<RHI::IRHIPipelineLayout>& Layout,
    Core::uint32 SetIndex)
{
    const auto Native = std::dynamic_pointer_cast<FMetalPipelineLayout>(Layout);
    if (!IsActive() || !Native || !Native->IsCompatible(Owner_) ||
        SetIndex >= Native->GetSetCount())
        return {RHI::ERHIResult::InvalidState, nullptr};
    try
    {
        auto Object = Core::MakeShared<FMetalDescriptorSet>(
            Owner_, Layout, SetIndex);
        return {RHI::ERHIResult::Success, std::move(Object)};
    }
    catch (const std::bad_alloc&)
    {
        return {RHI::ERHIResult::Failed, nullptr};
    }
}
RHI::TRHIObjectResult<RHI::IRHIGraphicsPipeline>
FMetalDevice::CreateGraphicsPipeline(const RHI::FRHIGraphicsPipelineDesc& Desc)
{
    if (!IsActive() || !RHI::IsValidRHIGraphicsPipelineState(Desc) ||
        Desc.ShaderModules.size() != 2 ||
        !Desc.ShaderModules[0] || !Desc.ShaderModules[1])
        return {RHI::ERHIResult::InvalidState, nullptr};
    if (FMetalFailureInjector::ShouldFail(
            EMetalFailurePoint::PipelineCreation))
    {
        Owner_->RecordDiagnostic(
            Core::FString("CreateGraphicsPipeline"),
            Core::FString("pipeline"), RHI::ERHIResult::Failed,
            Core::FString(ToStableName(
                EMetalFailurePoint::PipelineCreation)),
            0, 0, {}, Core::FString("recoverable"));
        return {RHI::ERHIResult::Failed, nullptr};
    }
    try
    {
        const std::string Key(BuildMetalGraphicsPipelineKey(Desc).View());
        {
            std::lock_guard Lock(Impl_->PipelineCacheMutex);
            const auto It = Impl_->GraphicsPipelineCache.find(Key);
            if (It != Impl_->GraphicsPipelineCache.end())
            {
                if (auto Existing = It->second.lock())
                {
                    if (Existing->IsCompatible(Owner_))
                    {
                        Existing->MarkReused();
                        return {RHI::ERHIResult::Success, std::move(Existing)};
                    }
                }
                Impl_->GraphicsPipelineCache.erase(It);
            }
        }
        auto Created = CreateMetalGraphicsPipeline(
            Owner_, GetNativeDevice(), Capabilities_, Desc);
        if (Created.Succeeded())
        {
            auto Native =
                std::dynamic_pointer_cast<FMetalGraphicsPipeline>(Created.Object);
            std::lock_guard Lock(Impl_->PipelineCacheMutex);
            Impl_->GraphicsPipelineCache.emplace(Key, Native);
        }
        return Created;
    }
    catch (const std::bad_alloc&)
    {
        return {RHI::ERHIResult::Failed, nullptr};
    }
    catch (const std::length_error&)
    {
        return {RHI::ERHIResult::Failed, nullptr};
    }
}
RHI::TRHIObjectResult<RHI::IRHIComputePipeline>
FMetalDevice::CreateComputePipeline(const RHI::FRHIComputePipelineDesc& Desc)
{
    if (!IsActive() || !Desc.PipelineLayout ||
        Desc.ShaderModules.size() != 1 || !Desc.ShaderModules[0])
        return {RHI::ERHIResult::InvalidState, nullptr};
    if (FMetalFailureInjector::ShouldFail(
            EMetalFailurePoint::PipelineCreation))
    {
        Owner_->RecordDiagnostic(
            Core::FString("CreateComputePipeline"),
            Core::FString("pipeline"), RHI::ERHIResult::Failed,
            Core::FString(ToStableName(
                EMetalFailurePoint::PipelineCreation)),
            0, 0, {}, Core::FString("recoverable"));
        return {RHI::ERHIResult::Failed, nullptr};
    }
    try
    {
        const std::string Key(BuildMetalComputePipelineKey(Desc).View());
        {
            std::lock_guard Lock(Impl_->PipelineCacheMutex);
            const auto It = Impl_->ComputePipelineCache.find(Key);
            if (It != Impl_->ComputePipelineCache.end())
            {
                if (auto Existing = It->second.lock())
                {
                    if (Existing->IsCompatible(Owner_))
                    {
                        Existing->MarkReused();
                        return {RHI::ERHIResult::Success, std::move(Existing)};
                    }
                }
                Impl_->ComputePipelineCache.erase(It);
            }
        }
        auto Created = CreateMetalComputePipeline(
            Owner_, GetNativeDevice(), Capabilities_, Desc);
        if (Created.Succeeded())
        {
            auto Native =
                std::dynamic_pointer_cast<FMetalComputePipeline>(Created.Object);
            std::lock_guard Lock(Impl_->PipelineCacheMutex);
            Impl_->ComputePipelineCache.emplace(Key, Native);
        }
        return Created;
    }
    catch (const std::bad_alloc&)
    {
        return {RHI::ERHIResult::Failed, nullptr};
    }
    catch (const std::length_error&)
    {
        return {RHI::ERHIResult::Failed, nullptr};
    }
}
RHI::TRHIObjectResult<RHI::IRHIRenderPass>
FMetalDevice::CreateRenderPass(const RHI::FRHIRenderPassDesc& Desc)
{
    if (!IsActive() || !RHI::IsValidRHIRenderPassDesc(Desc))
        return {RHI::ERHIResult::InvalidState, nullptr};
    for (const auto& Attachment : Desc.Attachments)
    {
        const auto Required = Attachment.Role == RHI::ERHIAttachmentRole::Color
            ? RHI::ERHIFormatCapability::ColorAttachment
            : RHI::ERHIFormatCapability::DepthStencilAttachment;
        if (!Capabilities_.SupportsFormatUsage(Attachment.Format, Required) ||
            !Capabilities_.SupportsSampleCount(Attachment.SampleCount))
            return {RHI::ERHIResult::Unsupported, nullptr};
    }
    try
    {
        auto Object = Core::MakeShared<FMetalRenderPass>(Owner_, Desc);
        return {RHI::ERHIResult::Success, std::move(Object)};
    }
    catch (const std::bad_alloc&)
    {
        return {RHI::ERHIResult::Failed, nullptr};
    }
}
RHI::TRHIObjectResult<RHI::IRHIFramebuffer>
FMetalDevice::CreateFramebuffer(const RHI::FRHIFramebufferDesc& Desc)
{
    if (!IsActive() || !FMetalFramebuffer::IsCompatibleDesc(Desc, Owner_))
        return {RHI::ERHIResult::InvalidState, nullptr};
    try
    {
        auto Object = Core::MakeShared<FMetalFramebuffer>(Owner_, Desc);
        return {RHI::ERHIResult::Success, std::move(Object)};
    }
    catch (const std::bad_alloc&)
    {
        return {RHI::ERHIResult::Failed, nullptr};
    }
}
RHI::TRHIObjectResult<RHI::IRHIPresentationSurface>
FMetalDevice::CreatePresentationSurface(
    const RHI::FRHIPresentationSurfaceDesc& Desc)
{
    if (!IsActive() || !Desc.IsValid())
        return {RHI::ERHIResult::InvalidState, nullptr};
    if (!Capabilities_.bSupportsPresentation)
        return {RHI::ERHIResult::Unsupported, nullptr};
    try
    {
        auto Context = Core::MakeShared<FMetalPresentationContext>(
            Owner_, GetNativeDevice(), GetNativeQueue());
        auto Surface = Core::MakeShared<FMetalPresentationSurface>(
            Owner_, Desc, std::move(Context));
        RHI::FRHIPresentationCapabilities PresentationCapabilities;
        const RHI::ERHIResult CapabilityResult =
            Surface->QueryCapabilities(PresentationCapabilities);
        if (CapabilityResult != RHI::ERHIResult::Success)
            return {CapabilityResult, nullptr};
        {
            std::lock_guard Lock(Impl_->SurfaceMutex);
            Impl_->Surfaces.push_back(Surface);
        }
        return {RHI::ERHIResult::Success, std::move(Surface)};
    }
    catch (const std::bad_alloc&)
    {
        return {RHI::ERHIResult::Failed, nullptr};
    }
    catch (const std::length_error&)
    {
        return {RHI::ERHIResult::Failed, nullptr};
    }
}
RHI::TRHIObjectResult<RHI::IRHISwapchain>
FMetalDevice::CreateSwapchain(
    const Core::TSharedPtr<RHI::IRHIPresentationSurface>& Surface,
    const RHI::FRHISwapchainDesc& Desc)
{
    const auto Native =
        std::dynamic_pointer_cast<FMetalPresentationSurface>(Surface);
    if (!IsActive() || !Native || !Native->IsCompatible(Owner_) ||
        !Native->IsValid() || !Desc.IsExactPresentationRequestValid())
        return {RHI::ERHIResult::InvalidState, nullptr};
    if (!Capabilities_.bSupportsPresentation ||
        !Capabilities_.bSupportsPresentQueue ||
        Desc.FramesInFlight > Capabilities_.MaxInFlightFrames ||
        !Capabilities_.SupportsFormatUsage(
            Desc.PreferredFormat, RHI::ERHIFormatCapability::ColorAttachment))
        return {RHI::ERHIResult::Unsupported, nullptr};
    if (Native->GetContext()->IsAttached())
        return {RHI::ERHIResult::InvalidState, nullptr};
    RHI::FRHIPresentationCapabilities PresentationCapabilities;
    const RHI::ERHIResult CapabilityResult =
        Native->QueryCapabilities(PresentationCapabilities);
    if (CapabilityResult != RHI::ERHIResult::Success)
        return {CapabilityResult, nullptr};
    if (Desc.SurfaceCapabilityGeneration !=
            PresentationCapabilities.CapabilityGeneration ||
        !PresentationCapabilities.SupportsPair(
            Desc.PreferredFormat, Desc.PreferredColorSpace))
        return {RHI::ERHIResult::Unsupported, nullptr};
    const auto AttachResult = Native->GetContext()->Attach(
        Native->GetDesc().Window, Desc);
    if (AttachResult != RHI::ERHIResult::Success)
        return {AttachResult, nullptr};
    try
    {
        auto Swapchain = Core::MakeShared<FMetalSwapchain>(
            Owner_, Native, Desc);
        if (Swapchain->GetState() != RHI::ERHISwapchainState::Ready)
        {
            (void)Native->Invalidate();
            return {RHI::ERHIResult::Failed, nullptr};
        }
        return {RHI::ERHIResult::Success, std::move(Swapchain)};
    }
    catch (const std::bad_alloc&)
    {
        (void)Native->Invalidate();
        return {RHI::ERHIResult::Failed, nullptr};
    }
}

FMetalBackendInspection FMetalDevice::Inspect() const noexcept
{
    return Owner_ ? Owner_->Inspect() : FMetalBackendInspection{};
}

FMetalBackendDiagnostics FMetalDevice::InspectDiagnostics() const
{
    return Owner_ ? Owner_->SnapshotDiagnostics()
        : FMetalBackendDiagnostics{};
}

const Core::TSharedPtr<FMetalDeviceOwnerState>&
FMetalDevice::GetOwner() const noexcept { return Owner_; }
void* FMetalDevice::GetNativeDevice() const noexcept
{
    return Impl_ ? (__bridge void*)Impl_->Device : nullptr;
}
void* FMetalDevice::GetNativeQueue() const noexcept
{
    return Impl_ ? (__bridge void*)Impl_->Queue : nullptr;
}

RHI::ERHIResult FMetalDevice::ReadbackTextureForTesting(
    const Core::TSharedPtr<RHI::IRHITexture>& Texture,
    Core::TArray<Core::uint8>& OutBytes) const noexcept
{
    OutBytes.clear();
    if (!IsActive() || !Texture)
        return RHI::ERHIResult::InvalidState;
    const auto Native = std::dynamic_pointer_cast<FMetalTexture>(Texture);
    if (!Native || !Native->IsCompatible(Owner_) ||
        Native->GetLifecycleState() !=
            RHI::ERHIResourceLifecycleState::Valid)
        return RHI::ERHIResult::InvalidState;
    const auto& Desc = Native->GetDesc();
    const RHI::FRHIFormatInfo FormatInfo =
        RHI::GetRHIFormatInfo(Desc.Format);
    if (!FormatInfo.IsValid() || FormatInfo.bCompressed ||
        FormatInfo.BlockWidth != 1 || FormatInfo.BlockHeight != 1 ||
        FormatInfo.BlockDepth != 1)
        return RHI::ERHIResult::Unsupported;
    const Core::uint32 BytesPerPixel = FormatInfo.BytesPerBlock;
    if (BytesPerPixel == 0 || Desc.Width >
            std::numeric_limits<Core::uint64>::max() / BytesPerPixel)
        return RHI::ERHIResult::Unsupported;
    return ReadbackMetalTexture(
        (__bridge id<MTLCommandQueue>)GetNativeQueue(),
        Native->GetNativeTexture(),
        static_cast<Core::uint64>(Desc.Width) * BytesPerPixel,
        Desc.Height, OutBytes);
}

} // namespace Stoner::Backend::Metal::Private

namespace Stoner::Backend::Metal
{

FMetalDeviceCreateResult CreateMetalDevice(
    const FMetalBackendConfig& Config) noexcept
{
    FMetalDeviceCreateResult Result;
    try
    {
        if (Private::FMetalFailureInjector::ShouldFail(
                Private::EMetalFailurePoint::DeviceInitialization))
        {
            Result.Result = RHI::ERHIResult::Failed;
            Result.Diagnostics.Records.push_back({
                Core::FString("CreateDevice"), Core::FString("initialization"),
                Result.Result,
                Core::FString(Private::ToStableName(
                    Private::EMetalFailurePoint::DeviceInitialization))});
            return Result;
        }
        Private::FMetalAdapterSelection Selection =
            Private::SelectMetalAdapter(Config);
        Result.Diagnostics.Candidates = Selection.Candidates;
        Result.Diagnostics.SelectedAdapter = Selection.Selected;
        if (Selection.Result != RHI::ERHIResult::Success)
        {
            Result.Result = Selection.Result;
            Result.Diagnostics.Records.push_back({
                Core::FString("CreateDevice"), Core::FString("adapter"),
                Selection.Result, Selection.StableReason});
            return Result;
        }
        void* Native = Selection.ReleaseNativeDevice();
        const RHI::FRHIDeviceCapabilities Capabilities =
            Private::QueryMetalCapabilities(Native);
        auto Owner = Core::MakeShared<Private::FMetalDeviceOwnerState>(
            Private::GNextOwnerIdentity.fetch_add(1, std::memory_order_relaxed));
        auto Device = Core::MakeShared<Private::FMetalDevice>(
            Native, Owner, Selection.Selected, Capabilities);
        if (!Device->Initialize(Config.FailurePoint))
        {
            Result.Result = RHI::ERHIResult::Unavailable;
            Result.Diagnostics.Records.push_back({
                Core::FString("CreateDevice"), Core::FString("initialization"),
                Result.Result, Core::FString("metal-device-initialization-failed")});
            return Result;
        }
        Result.Result = RHI::ERHIResult::Success;
        Result.Device = std::move(Device);
        return Result;
    }
    catch (const std::bad_alloc&)
    {
        Result.Result = RHI::ERHIResult::Failed;
        return Result;
    }
}

bool InspectMetalDevice(
    const Core::TSharedPtr<RHI::IRHIDevice>& Device,
    FMetalBackendInspection& OutInspection) noexcept
{
    OutInspection = {};
    const auto Native = std::dynamic_pointer_cast<Private::FMetalDevice>(Device);
    if (!Native) return false;
    OutInspection = Native->Inspect();
    return true;
}

bool InspectMetalDiagnostics(
    const Core::TSharedPtr<RHI::IRHIDevice>& Device,
    FMetalBackendDiagnostics& OutDiagnostics) noexcept
{
    OutDiagnostics = {};
    const auto Native = std::dynamic_pointer_cast<Private::FMetalDevice>(Device);
    if (!Native) return false;
    try
    {
        OutDiagnostics = Native->InspectDiagnostics();
        return true;
    }
    catch (...)
    {
        OutDiagnostics = {};
        return false;
    }
}

RHI::ERHIResult ReadMetalBufferForValidation(
    const Core::TSharedPtr<RHI::IRHIDevice>& Device,
    const Core::TSharedPtr<RHI::IRHIBuffer>& Buffer,
    Core::uint64 Offset,
    Core::uint64 Size,
    Core::TArray<Core::uint8>& OutBytes) noexcept
{
    OutBytes.clear();
    const auto NativeDevice =
        std::dynamic_pointer_cast<Private::FMetalDevice>(Device);
    const auto NativeBuffer =
        std::dynamic_pointer_cast<Private::FMetalBuffer>(Buffer);
    if (!NativeDevice || !NativeBuffer ||
        !NativeBuffer->IsCompatible(NativeDevice->GetOwner()))
        return RHI::ERHIResult::InvalidState;
    return Private::ReadbackMetalBuffer(
        (__bridge id<MTLCommandQueue>)NativeDevice->GetNativeQueue(),
        NativeBuffer->GetNativeBuffer(), Offset, Size, OutBytes);
}

} // namespace Stoner::Backend::Metal
