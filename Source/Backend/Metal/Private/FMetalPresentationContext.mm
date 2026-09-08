#include "FMetalPresentationContext.h"

#include "FMetalCapabilities.h"
#include "FMetalFormat.h"
#include "RHI/FRHIFormatInfo.h"
#include "FMetalSynchronization.h"
#include "FMetalTexture.h"

#if defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
    #define GLFW_INCLUDE_NONE
    #define GLFW_EXPOSE_NATIVE_COCOA
    #include <GLFW/glfw3.h>
    #include <GLFW/glfw3native.h>
#else
struct GLFWwindow;
#endif

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <atomic>
#include <array>
#include <mutex>
#include <new>
#include <vector>

namespace Stoner::Backend::Metal::Private
{

bool FMetalPresentationTracker::TryReserve(
    Core::uint32& OutImageIndex) noexcept
{
    std::lock_guard Lock(Mutex);
    for (Core::uint32 Index = 0;
         Index < RHI::MaxRHIPresentationImageLeases; ++Index)
    {
        if (!ActiveImageLeases[Index])
        {
            ActiveImageLeases[Index] = true;
            ++PendingCount;
            OutImageIndex = Index;
            return true;
        }
    }
    OutImageIndex = 0;
    return false;
}

void FMetalPresentationTracker::Reset() noexcept
{
    std::lock_guard Lock(Mutex);
    if (PendingCount != 0) return;
    ActiveImageLeases.fill(false);
    LastPresentedFrameToken = 0;
    bHasPresentedFrame = false;
}

void FMetalPresentationTracker::Release(Core::uint32 ImageIndex) noexcept
{
    std::lock_guard Lock(Mutex);
    if (ImageIndex < RHI::MaxRHIPresentationImageLeases &&
        ActiveImageLeases[ImageIndex])
    {
        ActiveImageLeases[ImageIndex] = false;
        if (PendingCount > 0) --PendingCount;
    }
    Condition.notify_all();
}

void FMetalPresentationTracker::Complete(
    Core::uint32 ImageIndex,
    Core::uint64 FrameToken,
    bool bActuallyPresented) noexcept
{
    std::lock_guard Lock(Mutex);
    if (ImageIndex < RHI::MaxRHIPresentationImageLeases &&
        ActiveImageLeases[ImageIndex])
    {
        ActiveImageLeases[ImageIndex] = false;
        if (PendingCount > 0) --PendingCount;
    }
    if (bActuallyPresented && FrameToken != 0)
    {
        LastPresentedFrameToken = FrameToken;
        bHasPresentedFrame = true;
    }
    Condition.notify_all();
}

bool FMetalPresentationTracker::IsEmpty() const noexcept
{
    std::lock_guard Lock(Mutex);
    return PendingCount == 0;
}

bool FMetalPresentationTracker::HasOtherLeases() const noexcept
{
    std::lock_guard Lock(Mutex);
    return PendingCount > 1;
}

bool FMetalPresentationTracker::WaitForZero(
    std::chrono::milliseconds Timeout) noexcept
{
    std::unique_lock Lock(Mutex);
    return Condition.wait_for(
        Lock, Timeout, [this] { return PendingCount == 0; });
}

Core::uint64 FMetalPresentationTracker::GetLastPresentedFrameToken()
    const noexcept
{
    std::lock_guard Lock(Mutex);
    return LastPresentedFrameToken;
}

bool FMetalPresentationTracker::HasPresentedFrame() const noexcept
{
    std::lock_guard Lock(Mutex);
    return bHasPresentedFrame;
}

Core::uint32 FMetalPresentationTracker::GetPendingCount() const noexcept
{
    std::lock_guard Lock(Mutex);
    return PendingCount;
}

struct FMetalPresentationCompletionState
{
    Core::TSharedPtr<FMetalDeviceOwnerState> Owner;
    Core::TSharedPtr<FMetalTexture> Texture;
    Core::TSharedPtr<FMetalSemaphore> RenderFinishedSemaphore;
    Core::TSharedPtr<FMetalFence> PresentationFence;
    Core::TSharedPtr<FMetalPresentationTracker> Tracker;
    Core::uint64 FrameToken = 0;
    Core::uint32 ImageIndex = 0;
    Core::uint64 PresentationEpoch = 0;
    std::atomic<bool> bCompleted{false};

    void Complete(
        bool bReleaseSucceeded,
        bool bActuallyPresented = false) noexcept
    {
        if (bCompleted.exchange(true, std::memory_order_acq_rel)) return;
        if (bReleaseSucceeded && Owner) Owner->RecordNativeOperation(EMetalNativeOperation::PresentationRelease);
        // Invalidate the external borrowed wrapper before publishing the
        // independent presentation completion proof.
        if (Texture) (void)Texture->Invalidate();
        if (PresentationFence && PresentationEpoch != 0)
            PresentationFence->CompleteSubmissionSignal(
                PresentationEpoch, bReleaseSucceeded);
        if (Tracker)
            Tracker->Complete(ImageIndex, FrameToken, bActuallyPresented);
        // The command buffer/frame owns the drawable until native command
        // completion.  The callback state retains only the independent
        // presentation proof and sampled resources, avoiding a drawable <-
        // handler <- completion <- drawable cycle.
        Texture.reset();
        RenderFinishedSemaphore.reset();
        PresentationFence.reset();
        Tracker.reset();
    }
};

// CAMetalLayer::nextDrawable may wait for the native layer pool.  Keep that
// potentially blocking operation off the application thread and retain only
// the layer/job state until the caller polls the acquired drawable.
struct FMetalDrawableAcquireState
{
    mutable std::mutex Mutex;
    std::condition_variable Condition;
    __strong CAMetalLayer* Layer = nil;
    __strong id<CAMetalDrawable> Drawable = nil;
    bool bComplete = false;
    bool bCancelled = false;
    bool bTaken = false;

    [[nodiscard]] bool TryTake(
        __strong id<CAMetalDrawable>& OutDrawable,
        bool& OutCancelled) noexcept
    {
        std::lock_guard Lock(Mutex);
        if (!bComplete || bTaken) return false;
        bTaken = true;
        OutDrawable = Drawable;
        Drawable = nil;
        OutCancelled = bCancelled;
        return true;
    }

    // A canceled result is retired by the context itself once the worker has
    // returned.  Do not consume a live, non-canceled result here: the caller
    // still needs to poll and publish that drawable as its borrowed target.
    [[nodiscard]] bool TryTakeCancelled(
        __strong id<CAMetalDrawable>& OutDrawable) noexcept
    {
        std::lock_guard Lock(Mutex);
        if (!bComplete || !bCancelled || bTaken) return false;
        bTaken = true;
        OutDrawable = Drawable;
        Drawable = nil;
        return true;
    }

    [[nodiscard]] bool IsCancelled() const noexcept
    {
        std::lock_guard Lock(Mutex);
        return bCancelled;
    }

    void Cancel() noexcept
    {
        std::lock_guard Lock(Mutex);
        bCancelled = true;
    }

    [[nodiscard]] bool WaitFor(
        std::chrono::milliseconds Timeout) noexcept
    {
        std::unique_lock Lock(Mutex);
        return Condition.wait_for(
            Lock, Timeout, [this] { return bComplete; });
    }

    void Complete(__strong id<CAMetalDrawable> InDrawable) noexcept
    {
        std::lock_guard Lock(Mutex);
        Drawable = bCancelled ? nil : InDrawable;
        bComplete = true;
        Condition.notify_all();
    }
};

struct FMetalPresentationContext::FImpl
{
    struct FFrame
    {
        __strong id<CAMetalDrawable> Drawable;
        Core::TSharedPtr<FMetalTexture> Texture;
        Core::uint64 Generation = 0;
        Core::uint64 FrameToken = 0;
        Core::uint32 ImageIndex = 0;
        Core::TSharedPtr<FMetalDrawableAcquireState> PendingDrawableAcquire;
        RHI::FRHIPresentationFrame BorrowedFrame;
        bool bBorrowedLeaseActive = false;
        bool bPresentationSubmitted = false;
        bool bInFlight = false;
    };

    mutable std::mutex Mutex;
    std::condition_variable Condition;
    GLFWwindow* Window = nullptr;
    __strong NSView* View;
    __strong CALayer* PreviousLayer;
    __strong CAMetalLayer* Layer;
    bool bPreviousWantsLayer = false;
    bool bAttached = false;
    bool bAcceptingFrames = false;
    Core::uint32 LogicalWidth = 0;
    Core::uint32 LogicalHeight = 0;
    Core::uint32 Width = 0;
    Core::uint32 Height = 0;
    CGFloat DisplayScale = 1.0;
    Core::uint64 Generation = 1;
    Core::uint32 InFlightCount = 0;
    // A pending job includes a native layer owner and its image admission
    // until the worker has returned and the job has been consumed.  Keeping
    // this count separate makes the two-job bound explicit even when a
    // completed job has not yet been polled by its caller.
    Core::uint32 PendingDrawableAcquireCount = 0;
    RHI::ERHIFormat Format = RHI::ERHIFormat::Unknown;
    RHI::FRHIResolvedPresentationState ResolvedState;
    FMetalPresentationLayerSnapshot LayerSnapshot;
    [[nodiscard]] Core::uint64 EstimatedColorBytes() const noexcept
    {
        return static_cast<Core::uint64>(Width) * Height *
            RHI::GetRHIFormatInfo(Format).BytesPerBlock *
            static_cast<Core::uint64>(Layer ? Layer.maximumDrawableCount : 0);
    }
    std::vector<FFrame> Frames;
    Core::TSharedPtr<FMetalPresentationTracker> PresentationTracker =
        Core::MakeShared<FMetalPresentationTracker>();
};

#if defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
namespace
{

CGColorSpaceRef CreateMetalPresentationColorSpace(
    RHI::ERHIPresentationColorSpace ColorSpace) noexcept
{
    CFStringRef Name = nullptr;
    switch (ColorSpace)
    {
    case RHI::ERHIPresentationColorSpace::SrgbNonlinear:
        Name = kCGColorSpaceSRGB;
        break;
    case RHI::ERHIPresentationColorSpace::Bt709Nonlinear:
        Name = kCGColorSpaceITUR_709;
        break;
    case RHI::ERHIPresentationColorSpace::Hdr10St2084:
        Name = kCGColorSpaceITUR_2100_PQ;
        break;
    case RHI::ERHIPresentationColorSpace::ExtendedSrgbLinear:
        Name = kCGColorSpaceExtendedLinearSRGB;
        break;
    case RHI::ERHIPresentationColorSpace::SdrPassThrough:
    case RHI::ERHIPresentationColorSpace::Unknown:
        break;
    }
    return Name ? CGColorSpaceCreateWithName(Name) : nullptr;
}

bool ApplyMetalPresentationLayerPolicy(
    CAMetalLayer* Layer,
    const RHI::FRHISwapchainDesc& Request,
    const FMetalPresentationLayerPolicy& Policy,
    Core::uint32 Width,
    Core::uint32 Height,
    CGFloat DisplayScale) noexcept
{
    if (!Layer || !Policy.IsValid()) return false;
    CGColorSpaceRef ColorSpace = CreateMetalPresentationColorSpace(
        Policy.ColorSpace);
    if (Policy.ColorSpace !=
            RHI::ERHIPresentationColorSpace::SdrPassThrough &&
        !ColorSpace)
        return false;
    Layer.pixelFormat = static_cast<MTLPixelFormat>(Policy.PixelFormat);
    Layer.colorspace = ColorSpace;
    if (ColorSpace) CGColorSpaceRelease(ColorSpace);
    Layer.wantsExtendedDynamicRangeContent =
        Policy.bWantsExtendedDynamicRangeContent;
    // Both Feature 029 Metal HDR paths keep this nil. PQ relies only on the
    // ITU-R 2100 PQ colorspace for Core Animation color management; EDR uses
    // Renderer-owned extended-linear packing. Neither requests Apple's system
    // tone mapper, whose documented input contract is extended-linear FP16.
    Layer.EDRMetadata = nil;
    Layer.framebufferOnly = NO;
    Layer.maximumDrawableCount = Request.FramesInFlight;
    Layer.displaySyncEnabled = Request.bVSync;
    Layer.drawableSize = CGSizeMake(Width, Height);
    Layer.contentsScale = DisplayScale;
    Layer.opaque = YES;
    return true;
}

} // namespace
#endif

FMetalPresentationLayerPolicy ResolveMetalPresentationLayerPolicy(
    const RHI::FRHISwapchainDesc& Request) noexcept
{
    FMetalPresentationLayerPolicy Result;
    if (!Request.IsExactPresentationRequestValid()) return Result;
    Result.PixelFormat = ToMetalPixelFormat(Request.PreferredFormat);
    Result.ColorSpace = Request.PreferredColorSpace;
    Result.DisplayAdaptation = Request.DisplayAdaptation;
    using RHI::ERHIFormat;
    using RHI::ERHIPresentationColorSpace;
    using RHI::ERHIPresentationDisplayAdaptation;
    using RHI::ERHIPresentationNativeEncoding;
    switch (Request.NativeEncoding)
    {
    case ERHIPresentationNativeEncoding::SdrExplicit:
        if ((Request.PreferredFormat != ERHIFormat::B8G8R8A8_UNorm &&
             Request.PreferredFormat != ERHIFormat::R8G8B8A8_UNorm) ||
            (Request.PreferredColorSpace !=
                 ERHIPresentationColorSpace::SrgbNonlinear &&
             Request.PreferredColorSpace !=
                 ERHIPresentationColorSpace::Bt709Nonlinear &&
             Request.PreferredColorSpace !=
                 ERHIPresentationColorSpace::SdrPassThrough) ||
            Request.bHasHDRMetadata ||
            Request.DisplayAdaptation !=
                ERHIPresentationDisplayAdaptation::None)
            return {};
        break;
    case ERHIPresentationNativeEncoding::Pq:
        if (Request.PreferredFormat != ERHIFormat::R10G10B10A2_UNorm ||
            Request.PreferredColorSpace !=
                ERHIPresentationColorSpace::Hdr10St2084 ||
            Request.bHasHDRMetadata ||
            Request.DisplayAdaptation !=
                ERHIPresentationDisplayAdaptation::SystemColorManagement)
            return {};
        Result.bWantsExtendedDynamicRangeContent = true;
        Result.bHasEDRMetadata = false;
        break;
    case ERHIPresentationNativeEncoding::MetalEdr:
        if (Request.PreferredFormat != ERHIFormat::R16G16B16A16_Float ||
            Request.PreferredColorSpace !=
                ERHIPresentationColorSpace::ExtendedSrgbLinear ||
            Request.bHasHDRMetadata ||
            Request.DisplayAdaptation !=
                ERHIPresentationDisplayAdaptation::None)
            return {};
        Result.bWantsExtendedDynamicRangeContent = true;
        Result.bHasEDRMetadata = false;
        break;
    case ERHIPresentationNativeEncoding::ScRgb80:
    case ERHIPresentationNativeEncoding::Unknown:
        return {};
    }
    return Result;
}

FMetalPresentationContext::FMetalPresentationContext(
    Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
    void* NativeDevice,
    void* NativeQueue)
    : Impl_(Core::MakeUnique<FImpl>()), Owner_(std::move(Owner)),
      NativeDevice_(NativeDevice), NativeQueue_(NativeQueue)
{
}

FMetalPresentationContext::~FMetalPresentationContext()
{
    (void)Shutdown();
}

RHI::ERHIResult FMetalPresentationContext::Attach(
    const Core::FPlatformWindow& PlatformWindow,
    RHI::ERHIFormat Format,
    Core::uint32 MaximumDrawableCount,
    bool bVSync) noexcept
{
    RHI::FRHISwapchainDesc Request;
    Request.Width = 1;
    Request.Height = 1;
    Request.FramesInFlight = MaximumDrawableCount;
    Request.PreferredFormat = Format;
    Request.PreferredColorSpace =
        RHI::ERHIPresentationColorSpace::SrgbNonlinear;
    Request.NativeEncoding =
        RHI::ERHIPresentationNativeEncoding::SdrExplicit;
    Request.SurfaceCapabilityGeneration = 1;
    Request.bVSync = bVSync;
    return Attach(PlatformWindow, Request);
}

RHI::ERHIResult FMetalPresentationContext::QueryCapabilities(
    const RHI::FRHIPresentationSurfaceDesc& Surface,
    Core::uint64 CapabilityGeneration,
    RHI::FRHIPresentationCapabilities& OutCapabilities) const noexcept
{
    if (!Impl_ || !Owner_ || !Surface.IsValid())
    {
        OutCapabilities = {};
        return RHI::ERHIResult::InvalidState;
    }
    Core::uint64 SurfaceId = Surface.SurfaceId;
    if (SurfaceId == 0)
    {
        SurfaceId = static_cast<Core::uint64>(
            reinterpret_cast<std::uintptr_t>(
                Surface.Window.GetNativeHandle()));
    }
    return QueryMetalPresentationCapabilities(
        NativeDevice_, Surface.Window, SurfaceId, CapabilityGeneration,
        OutCapabilities);
}

RHI::ERHIResult FMetalPresentationContext::Attach(
    const Core::FPlatformWindow& PlatformWindow,
    const RHI::FRHISwapchainDesc& Request) noexcept
{
#if !defined(STONER_GLFW_AVAILABLE) || !STONER_GLFW_AVAILABLE
    (void)PlatformWindow;
    (void)Request;
    (void)NativeDevice_;
    (void)NativeQueue_;
    return RHI::ERHIResult::Unsupported;
#else
    if (!Impl_ || !Owner_ || !PlatformWindow.IsValid() ||
        NativeDevice_ == nullptr || NativeQueue_ == nullptr ||
        Request.FramesInFlight < 2 || Request.FramesInFlight > 3)
        return RHI::ERHIResult::InvalidState;
    {
        std::lock_guard Lock(Impl_->Mutex);
        if (Impl_->bAttached) return RHI::ERHIResult::InvalidState;
    }
    const FMetalPresentationLayerPolicy LayerPolicy =
        ResolveMetalPresentationLayerPolicy(Request);
    if (!LayerPolicy.IsValid()) return RHI::ERHIResult::Unsupported;

    RHI::FRHIPresentationCapabilities Capabilities;
    Core::uint64 SurfaceId = static_cast<Core::uint64>(
        reinterpret_cast<std::uintptr_t>(
            PlatformWindow.GetNativeHandle()));
    if (SurfaceId == 0) SurfaceId = 1;
    const RHI::ERHIResult CapabilityResult =
        QueryMetalPresentationCapabilities(
            NativeDevice_, PlatformWindow, SurfaceId,
            Request.SurfaceCapabilityGeneration, Capabilities);
    if (CapabilityResult != RHI::ERHIResult::Success)
        return CapabilityResult;
    if (!Capabilities.SupportsPair(
            Request.PreferredFormat, Request.PreferredColorSpace) ||
        (Request.bHasHDRMetadata && !Capabilities.bSupportsHDRMetadata) ||
        (Request.NativeEncoding ==
             RHI::ERHIPresentationNativeEncoding::MetalEdr &&
         !Capabilities.bSupportsExtendedRange))
        return RHI::ERHIResult::Unsupported;

    __block bool bSuccess = false;
    __block Core::uint32 LogicalWidth = 0;
    __block Core::uint32 LogicalHeight = 0;
    __block Core::uint32 Width = 0;
    __block Core::uint32 Height = 0;
    __block CGFloat DisplayScale = 1.0;
    const auto AttachOnMain = ^{
        auto* Window = static_cast<GLFWwindow*>(PlatformWindow.GetNativeHandle());
        if (!Window || glfwWindowShouldClose(Window) == GLFW_TRUE) return;
        NSWindow* CocoaWindow = glfwGetCocoaWindow(Window);
        NSView* View = CocoaWindow.contentView;
        if (!CocoaWindow || !View) return;
        int PixelWidth = 0;
        int PixelHeight = 0;
        int WindowWidth = 0;
        int WindowHeight = 0;
        glfwGetWindowSize(Window, &WindowWidth, &WindowHeight);
        glfwGetFramebufferSize(Window, &PixelWidth, &PixelHeight);
        LogicalWidth = WindowWidth > 0
            ? static_cast<Core::uint32>(WindowWidth) : 0;
        LogicalHeight = WindowHeight > 0
            ? static_cast<Core::uint32>(WindowHeight) : 0;
        Width = PixelWidth > 0 ? static_cast<Core::uint32>(PixelWidth) : 0;
        Height = PixelHeight > 0 ? static_cast<Core::uint32>(PixelHeight) : 0;
        DisplayScale = CocoaWindow.backingScaleFactor;

        CAMetalLayer* Layer = [CAMetalLayer layer];
        Layer.device = (__bridge id<MTLDevice>)NativeDevice_;
        if (!ApplyMetalPresentationLayerPolicy(
                Layer, Request, LayerPolicy, Width, Height, DisplayScale))
            return;

        Impl_->Window = Window;
        Impl_->View = View;
        Impl_->PreviousLayer = View.layer;
        Impl_->bPreviousWantsLayer = View.wantsLayer;
        View.wantsLayer = YES;
        View.layer = Layer;
        Impl_->Layer = Layer;
        bSuccess = true;
    };
    if ([NSThread isMainThread]) AttachOnMain();
    else dispatch_sync(dispatch_get_main_queue(), AttachOnMain);
    if (!bSuccess) return RHI::ERHIResult::Unavailable;

    {
        std::lock_guard Lock(Impl_->Mutex);
        if (Impl_->bAttached) return RHI::ERHIResult::InvalidState;
        Impl_->LogicalWidth = LogicalWidth;
        Impl_->LogicalHeight = LogicalHeight;
        Impl_->Width = Width;
        Impl_->Height = Height;
        Impl_->DisplayScale = DisplayScale;
        Impl_->Format = Request.PreferredFormat;
        Impl_->ResolvedState.ModeGeneration = Impl_->Generation;
        Impl_->ResolvedState.Width = Width;
        Impl_->ResolvedState.Height = Height;
        Impl_->ResolvedState.Format = Request.PreferredFormat;
        Impl_->ResolvedState.ColorSpace = Request.PreferredColorSpace;
        Impl_->ResolvedState.NativeEncoding = Request.NativeEncoding;
        Impl_->ResolvedState.DisplayAdaptation = Request.DisplayAdaptation;
        Impl_->ResolvedState.bHasHDRMetadata = Request.bHasHDRMetadata;
        Impl_->ResolvedState.MetadataDigest = Request.bHasHDRMetadata
            ? Request.HDRMetadata.CanonicalDigest : Core::FString{};
        Impl_->ResolvedState.ReferenceWhiteNits =
            Request.NativeEncoding ==
                RHI::ERHIPresentationNativeEncoding::MetalEdr
            ? Capabilities.NativeReferenceWhiteNits
            : Request.ReferenceWhiteNits;
        Impl_->ResolvedState.TargetPeakNits = Request.TargetPeakNits;
        Impl_->ResolvedState.SwapchainImageGeneration = Impl_->Generation;
        Impl_->LayerSnapshot.Policy = LayerPolicy;
        Impl_->LayerSnapshot.ModeGeneration = Impl_->Generation;
        Impl_->LayerSnapshot.Width = Width;
        Impl_->LayerSnapshot.Height = Height;
        if (Owner_) Owner_->RecordPresentationBytes(Impl_->EstimatedColorBytes());
        Impl_->LayerSnapshot.NativeReferenceWhiteNits =
            Capabilities.NativeReferenceWhiteNits;
        Impl_->LayerSnapshot.CurrentHeadroom =
            Capabilities.CurrentHeadroom;
        Impl_->LayerSnapshot.PotentialHeadroom =
            Capabilities.PotentialHeadroom;
        Impl_->LayerSnapshot.MetadataDigest =
            Impl_->ResolvedState.MetadataDigest;
        if (Impl_->PresentationTracker)
            Impl_->PresentationTracker->Reset();
        Impl_->PendingDrawableAcquireCount = 0;
        Impl_->bAttached = true;
        Impl_->bAcceptingFrames = true;
    }
    try
    {
        std::lock_guard Lock(Impl_->Mutex);
        Impl_->Frames.resize(Request.FramesInFlight);
    }
    catch (const std::bad_alloc&)
    {
        (void)Shutdown();
        return RHI::ERHIResult::Failed;
    }
    return RHI::ERHIResult::Success;
#endif
}

RHI::ERHIResult FMetalPresentationContext::Reconfigure(
    const Core::FPlatformWindow& PlatformWindow,
    const RHI::FRHISwapchainDesc& Request) noexcept
{
#if !defined(STONER_GLFW_AVAILABLE) || !STONER_GLFW_AVAILABLE
    (void)PlatformWindow;
    (void)Request;
    return RHI::ERHIResult::Unsupported;
#else
    if (!Impl_ || !Owner_ || !PlatformWindow.IsValid() ||
        NativeDevice_ == nullptr || NativeQueue_ == nullptr)
        return RHI::ERHIResult::InvalidState;
    if (!IsAttached()) return Attach(PlatformWindow, Request);
    const FMetalPresentationLayerPolicy LayerPolicy =
        ResolveMetalPresentationLayerPolicy(Request);
    if (!LayerPolicy.IsValid()) return RHI::ERHIResult::Unsupported;

    RHI::FRHIPresentationCapabilities Capabilities;
    Core::uint64 SurfaceId = static_cast<Core::uint64>(
        reinterpret_cast<std::uintptr_t>(
            PlatformWindow.GetNativeHandle()));
    const RHI::ERHIResult CapabilityResult =
        QueryMetalPresentationCapabilities(
            NativeDevice_, PlatformWindow, SurfaceId,
            Request.SurfaceCapabilityGeneration, Capabilities);
    if (CapabilityResult != RHI::ERHIResult::Success)
        return CapabilityResult;
    if (!Capabilities.SupportsPair(
            Request.PreferredFormat, Request.PreferredColorSpace) ||
        (Request.bHasHDRMetadata && !Capabilities.bSupportsHDRMetadata) ||
        (Request.NativeEncoding ==
             RHI::ERHIPresentationNativeEncoding::MetalEdr &&
         !Capabilities.bSupportsExtendedRange))
        return RHI::ERHIResult::Unsupported;

    std::vector<FImpl::FFrame> NewFrames;
    try
    {
        NewFrames.resize(Request.FramesInFlight);
    }
    catch (const std::bad_alloc&)
    {
        return RHI::ERHIResult::Failed;
    }

    std::unique_lock Lock(Impl_->Mutex);
    if (!Impl_->bAttached || !Impl_->bAcceptingFrames)
        return RHI::ERHIResult::InvalidState;
    bool bPendingAcquire = false;
    for (auto& Frame : Impl_->Frames)
    {
        if (!Frame.PendingDrawableAcquire) continue;
        const auto Pending = Frame.PendingDrawableAcquire;
        Pending->Cancel();
        __strong id<CAMetalDrawable> Drawable = nil;
        bool bCancelled = false;
        if (!Pending->TryTake(Drawable, bCancelled))
        {
            // The worker still owns a potentially blocking nextDrawable
            // call.  Poll it on a later reconfigure attempt; never detach
            // the layer or release its image lease while it is running.
            bPendingAcquire = true;
            continue;
        }
        Frame.PendingDrawableAcquire.reset();
        if (Impl_->PendingDrawableAcquireCount > 0)
            --Impl_->PendingDrawableAcquireCount;
        const Core::uint32 ImageIndex = Frame.ImageIndex;
        Frame.FrameToken = 0;
        Frame.Generation = 0;
        Frame.ImageIndex = 0;
        Frame.BorrowedFrame = {};
        if (Impl_->PresentationTracker)
            Impl_->PresentationTracker->Release(ImageIndex);
        (void)bCancelled;
    }
    if (bPendingAcquire) return RHI::ERHIResult::NotReady;
    if (Impl_->InFlightCount != 0 ||
        (Impl_->PresentationTracker &&
         !Impl_->PresentationTracker->IsEmpty()))
        return RHI::ERHIResult::NotReady;
    for (const FImpl::FFrame& Frame : Impl_->Frames)
    {
        if (Frame.Drawable || Frame.bInFlight)
            return RHI::ERHIResult::NotReady;
    }

    __block Core::uint32 LogicalWidth = 0;
    __block Core::uint32 LogicalHeight = 0;
    __block Core::uint32 Width = 0;
    __block Core::uint32 Height = 0;
    __block CGFloat DisplayScale = 1.0;
    __block bool bSuccess = false;
    const auto ReconfigureOnMain = ^{
        auto* Window = static_cast<GLFWwindow*>(
            PlatformWindow.GetNativeHandle());
        if (!Window || glfwWindowShouldClose(Window) == GLFW_TRUE ||
            Window != Impl_->Window || !Impl_->Layer)
            return;
        int WindowWidth = 0;
        int WindowHeight = 0;
        int PixelWidth = 0;
        int PixelHeight = 0;
        glfwGetWindowSize(Window, &WindowWidth, &WindowHeight);
        glfwGetFramebufferSize(Window, &PixelWidth, &PixelHeight);
        LogicalWidth = WindowWidth > 0
            ? static_cast<Core::uint32>(WindowWidth) : 0;
        LogicalHeight = WindowHeight > 0
            ? static_cast<Core::uint32>(WindowHeight) : 0;
        Width = PixelWidth > 0
            ? static_cast<Core::uint32>(PixelWidth) : 0;
        Height = PixelHeight > 0
            ? static_cast<Core::uint32>(PixelHeight) : 0;
        NSWindow* CocoaWindow = glfwGetCocoaWindow(Window);
        if (!CocoaWindow || Width == 0 || Height == 0) return;
        DisplayScale = CocoaWindow.backingScaleFactor;
        bSuccess = ApplyMetalPresentationLayerPolicy(
            Impl_->Layer, Request, LayerPolicy, Width, Height, DisplayScale);
    };
    if ([NSThread isMainThread]) ReconfigureOnMain();
    else dispatch_sync(dispatch_get_main_queue(), ReconfigureOnMain);
    if (!bSuccess) return RHI::ERHIResult::Unavailable;

    ++Impl_->Generation;
    Impl_->LogicalWidth = LogicalWidth;
    Impl_->LogicalHeight = LogicalHeight;
    Impl_->Width = Width;
    Impl_->Height = Height;
    Impl_->DisplayScale = DisplayScale;
    Impl_->Format = Request.PreferredFormat;
    Impl_->Frames = std::move(NewFrames);
    Impl_->ResolvedState.ModeGeneration = Impl_->Generation;
    Impl_->ResolvedState.Width = Width;
    Impl_->ResolvedState.Height = Height;
    Impl_->ResolvedState.Format = Request.PreferredFormat;
    Impl_->ResolvedState.ColorSpace = Request.PreferredColorSpace;
    Impl_->ResolvedState.NativeEncoding = Request.NativeEncoding;
    Impl_->ResolvedState.DisplayAdaptation = Request.DisplayAdaptation;
    Impl_->ResolvedState.bHasHDRMetadata = Request.bHasHDRMetadata;
    Impl_->ResolvedState.MetadataDigest = Request.bHasHDRMetadata
        ? Request.HDRMetadata.CanonicalDigest : Core::FString{};
    Impl_->ResolvedState.ReferenceWhiteNits =
        Request.NativeEncoding ==
            RHI::ERHIPresentationNativeEncoding::MetalEdr
        ? Capabilities.NativeReferenceWhiteNits
        : Request.ReferenceWhiteNits;
    Impl_->ResolvedState.TargetPeakNits = Request.TargetPeakNits;
    Impl_->ResolvedState.SwapchainImageGeneration = Impl_->Generation;
    Impl_->LayerSnapshot.Policy = LayerPolicy;
    Impl_->LayerSnapshot.ModeGeneration = Impl_->Generation;
    Impl_->LayerSnapshot.Width = Width;
    Impl_->LayerSnapshot.Height = Height;
    if (Owner_) Owner_->RecordPresentationBytes(Impl_->EstimatedColorBytes());
    Impl_->LayerSnapshot.NativeReferenceWhiteNits =
        Capabilities.NativeReferenceWhiteNits;
    Impl_->LayerSnapshot.CurrentHeadroom = Capabilities.CurrentHeadroom;
    Impl_->LayerSnapshot.PotentialHeadroom = Capabilities.PotentialHeadroom;
    Impl_->LayerSnapshot.MetadataDigest =
        Impl_->ResolvedState.MetadataDigest;
    return RHI::ERHIResult::Success;
#endif
}

bool FMetalPresentationContext::IsAttached() const noexcept
{
    if (!Impl_) return false;
    std::lock_guard Lock(Impl_->Mutex);
    return Impl_->bAttached && Owner_ &&
        Owner_->IsCompatible(Owner_->GetOwnerIdentity(), Owner_->GetGeneration());
}

RHI::ERHIResult FMetalPresentationContext::Acquire(
    Core::uint32 FrameSlot,
    Core::uint64 FrameToken,
    Core::TSharedPtr<RHI::IRHITexture>& OutTexture,
    Core::uint64& OutGeneration) noexcept
{
    Core::uint32 UnusedImageIndex = 0;
    return AcquireInternal(
        FrameSlot, FrameToken, OutTexture, OutGeneration,
        UnusedImageIndex, false);
}

RHI::ERHIResult FMetalPresentationContext::AcquireBorrowed(
    Core::uint32 FrameSlot,
    Core::uint64 FrameToken,
    Core::TSharedPtr<RHI::IRHITexture>& OutTexture,
    Core::uint64& OutGeneration,
    Core::uint32& OutImageIndex) noexcept
{
    return AcquireInternal(
        FrameSlot, FrameToken, OutTexture, OutGeneration,
        OutImageIndex, true);
}

RHI::ERHIResult FMetalPresentationContext::AcquireInternal(
    Core::uint32 FrameSlot,
    Core::uint64 FrameToken,
    Core::TSharedPtr<RHI::IRHITexture>& OutTexture,
    Core::uint64& OutGeneration,
    Core::uint32& OutImageIndex,
    bool bBorrowed) noexcept
{
    OutTexture.reset();
    OutGeneration = 0;
    OutImageIndex = 0;
#if !defined(STONER_GLFW_AVAILABLE) || !STONER_GLFW_AVAILABLE
    (void)FrameSlot;
    (void)FrameToken;
    (void)bBorrowed;
    return RHI::ERHIResult::Unsupported;
#else
    if (!Impl_ || FrameToken == 0) return RHI::ERHIResult::InvalidState;
    std::lock_guard Lock(Impl_->Mutex);
    if (!Impl_->bAttached || !Impl_->bAcceptingFrames ||
        FrameSlot >= Impl_->Frames.size())
        return RHI::ERHIResult::InvalidState;
    auto& Frame = Impl_->Frames[FrameSlot];
    if (Frame.Drawable || Frame.bInFlight ||
        (!bBorrowed && Frame.PendingDrawableAcquire))
        return RHI::ERHIResult::NotReady;

    Core::uint32 ReservedImageIndex = 0;
    bool bReservedPresentationLease = false;
    __strong id<CAMetalDrawable> Drawable = nil;
    if (bBorrowed && Frame.PendingDrawableAcquire)
    {
        const auto Pending = Frame.PendingDrawableAcquire;
        bool bPendingCancelled = Pending->IsCancelled();
        if (Frame.FrameToken != FrameToken && !bPendingCancelled)
        {
            // A newer token supersedes an unpublished acquisition.  Keep the
            // reservation until the worker has actually returned so callers
            // cannot create unbounded native nextDrawable jobs.
            Pending->Cancel();
            bPendingCancelled = true;
        }
        if (!Pending->TryTake(Drawable, bPendingCancelled))
            return RHI::ERHIResult::NotReady;
        ReservedImageIndex = Frame.ImageIndex;
        Frame.PendingDrawableAcquire.reset();
        if (Impl_->PendingDrawableAcquireCount > 0)
            --Impl_->PendingDrawableAcquireCount;
        Frame.FrameToken = 0;
        Frame.Generation = 0;
        Frame.ImageIndex = 0;
        Frame.BorrowedFrame = {};
        // Cancellation wins even if it raced a worker that had already
        // returned a drawable.  That drawable was never published and must
        // not be turned into a fresh borrowed lease by this poll.
        if (bPendingCancelled)
            Drawable = nil;
        if (!bPendingCancelled && Drawable)
        {
            // The image admission belongs to the published borrowed target
            // now.  Keep it held until presentation completion or a proven
            // pre-submit release; releasing here would let the tracker report
            // fewer leases than the native layer actually owns.
            bReservedPresentationLease = true;
        }
        else if (Impl_->PresentationTracker)
        {
            // A cancelled or empty job never published a drawable.  Its
            // admission can be released only after the worker has completed
            // and TryTake has consumed the result.
            Impl_->PresentationTracker->Release(ReservedImageIndex);
        }
    }
    if (bBorrowed && !bReservedPresentationLease && !Drawable)
    {
        bReservedPresentationLease = Impl_->PresentationTracker &&
            Impl_->PresentationTracker->TryReserve(ReservedImageIndex);
        if (!bReservedPresentationLease)
            return RHI::ERHIResult::NotReady;
    }
    else if (Frame.PendingDrawableAcquire)
    {
        return RHI::ERHIResult::NotReady;
    }
    const auto ReleasePresentationReservation = [&]() noexcept {
        if (bReservedPresentationLease)
        {
            Impl_->PresentationTracker->Release(ReservedImageIndex);
            bReservedPresentationLease = false;
        }
    };
    const auto ClearPendingFrame = [&]() noexcept {
        if (bBorrowed && !Frame.bBorrowedLeaseActive &&
            Frame.FrameToken == FrameToken)
        {
            if (Frame.PendingDrawableAcquire)
            {
                // Keep the pending slot and image lease until the worker
                // returns from nextDrawable; only the worker owns that
                // potentially blocking native call.
                const auto Pending = Frame.PendingDrawableAcquire;
                Pending->Cancel();
                __strong id<CAMetalDrawable> Discarded = nil;
                bool bCancelled = false;
                if (Pending->TryTake(Discarded, bCancelled))
                {
                    Frame.PendingDrawableAcquire.reset();
                    if (Impl_->PendingDrawableAcquireCount > 0)
                        --Impl_->PendingDrawableAcquireCount;
                    const Core::uint32 ImageIndex = Frame.ImageIndex;
                    Frame.FrameToken = 0;
                    Frame.Generation = 0;
                    Frame.ImageIndex = 0;
                    Frame.BorrowedFrame = {};
                    if (Impl_->PresentationTracker)
                        Impl_->PresentationTracker->Release(ImageIndex);
                }
                bReservedPresentationLease = false;
                return;
            }
            Frame.FrameToken = 0;
            Frame.Generation = 0;
            Frame.ImageIndex = 0;
            Frame.BorrowedFrame = {};
        }
    };

    __block Core::uint32 LogicalWidth = 0;
    __block Core::uint32 LogicalHeight = 0;
    __block Core::uint32 Width = 0;
    __block Core::uint32 Height = 0;
    __block CGFloat DisplayScale = 1.0;
    __block bool bClosing = false;
    __block bool bPaused = false;
    const auto RefreshOnMain = ^{
        if (!Impl_->Window)
        {
            bClosing = true;
            return;
        }
        bClosing = glfwWindowShouldClose(Impl_->Window) == GLFW_TRUE;
        bPaused = glfwGetWindowAttrib(
            Impl_->Window, GLFW_ICONIFIED) == GLFW_TRUE;
        int WindowWidth = 0;
        int WindowHeight = 0;
        int PixelWidth = 0;
        int PixelHeight = 0;
        glfwGetWindowSize(Impl_->Window, &WindowWidth, &WindowHeight);
        glfwGetFramebufferSize(Impl_->Window, &PixelWidth, &PixelHeight);
        LogicalWidth = WindowWidth > 0
            ? static_cast<Core::uint32>(WindowWidth) : 0;
        LogicalHeight = WindowHeight > 0
            ? static_cast<Core::uint32>(WindowHeight) : 0;
        Width = PixelWidth > 0 ? static_cast<Core::uint32>(PixelWidth) : 0;
        Height = PixelHeight > 0 ? static_cast<Core::uint32>(PixelHeight) : 0;
        NSWindow* Window = glfwGetCocoaWindow(Impl_->Window);
        if (Window) DisplayScale = Window.backingScaleFactor;
    };
    if ([NSThread isMainThread]) RefreshOnMain();
    else dispatch_sync(dispatch_get_main_queue(), RefreshOnMain);
    if (bClosing || bPaused || Width == 0 || Height == 0)
    {
        ClearPendingFrame();
        ReleasePresentationReservation();
        return RHI::ERHIResult::Unavailable;
    }
    if (LogicalWidth != Impl_->LogicalWidth ||
        LogicalHeight != Impl_->LogicalHeight || Width != Impl_->Width ||
        Height != Impl_->Height || DisplayScale != Impl_->DisplayScale)
    {
        if (Impl_->PresentationTracker &&
            (bBorrowed ? Impl_->PresentationTracker->HasOtherLeases()
                       : !Impl_->PresentationTracker->IsEmpty()))
        {
            ClearPendingFrame();
            ReleasePresentationReservation();
            return RHI::ERHIResult::NotReady;
        }
        Impl_->LogicalWidth = LogicalWidth;
        Impl_->LogicalHeight = LogicalHeight;
        Impl_->Width = Width;
        Impl_->Height = Height;
        Impl_->DisplayScale = DisplayScale;
        ++Impl_->Generation;
        Impl_->Layer.contentsScale = DisplayScale;
        Impl_->Layer.drawableSize = CGSizeMake(Width, Height);
        Impl_->ResolvedState.ModeGeneration = Impl_->Generation;
        Impl_->ResolvedState.Width = Width;
        Impl_->ResolvedState.Height = Height;
        Impl_->ResolvedState.SwapchainImageGeneration = Impl_->Generation;
        Impl_->LayerSnapshot.ModeGeneration = Impl_->Generation;
        Impl_->LayerSnapshot.Width = Width;
        Impl_->LayerSnapshot.Height = Height;
        if (Owner_) Owner_->RecordPresentationBytes(Impl_->EstimatedColorBytes());
        ClearPendingFrame();
        ReleasePresentationReservation();
        return RHI::ERHIResult::ResizeRequired;
    }

    if (bBorrowed && !Drawable)
    {
        if (Impl_->PendingDrawableAcquireCount >= RHI::MaxRHIFrameSlots)
        {
            ReleasePresentationReservation();
            return RHI::ERHIResult::NotReady;
        }
        Core::TSharedPtr<FMetalDrawableAcquireState> Pending;
        try
        {
            Pending = Core::MakeShared<FMetalDrawableAcquireState>();
        }
        catch (const std::bad_alloc&)
        {
            ReleasePresentationReservation();
            return RHI::ERHIResult::Failed;
        }
        Pending->Layer = Impl_->Layer;
        Frame.Generation = Impl_->Generation;
        Frame.FrameToken = FrameToken;
        Frame.ImageIndex = ReservedImageIndex;
        Frame.PendingDrawableAcquire = Pending;
        ++Impl_->PendingDrawableAcquireCount;
        const auto WeakContext = weak_from_this();
        dispatch_async(
            dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
            ^{
                @autoreleasepool
                {
                    id<CAMetalDrawable> Acquired = Pending->Layer
                        ? [Pending->Layer nextDrawable] : nil;
                    Pending->Complete(Acquired);
                    // A canceled worker may finish after its owning
                    // swapchain has been destroyed.  Retire that completed
                    // unpublished frame through the still-live context so
                    // its bounded image admission cannot remain stranded.
                    if (const auto Context = WeakContext.lock())
                        Context->PollCompletedUnpublishedBorrowedAcquires();
                }
            });
        return RHI::ERHIResult::NotReady;
    }

    @autoreleasepool
    {
        if (!bBorrowed)
            Drawable = [Impl_->Layer nextDrawable];
        if (!Drawable)
        {
            ReleasePresentationReservation();
            return RHI::ERHIResult::Unavailable;
        }
        RHI::FRHITextureDesc Desc;
        Desc.Width = Width;
        Desc.Height = Height;
        Desc.Format = Impl_->Format;
        Desc.Usage = RHI::ERHITextureUsage::ColorAttachment |
            RHI::ERHITextureUsage::Present |
            RHI::ERHITextureUsage::CopySource |
            RHI::ERHITextureUsage::CopyDestination;
        try
        {
            Frame.Texture = Core::MakeShared<FMetalTexture>(
                Owner_, Desc, Drawable.texture);
        }
        catch (const std::bad_alloc&)
        {
            ReleasePresentationReservation();
            return RHI::ERHIResult::Failed;
        }
        Frame.Drawable = Drawable;
        Frame.Generation = Impl_->Generation;
        Frame.FrameToken = FrameToken;
        if (bBorrowed)
        {
            Frame.ImageIndex = ReservedImageIndex;
            Frame.BorrowedFrame.FrameToken = FrameToken;
            Frame.BorrowedFrame.ModeGeneration =
                Impl_->ResolvedState.ModeGeneration;
            Frame.BorrowedFrame.SwapchainImageGeneration =
                Impl_->ResolvedState.SwapchainImageGeneration;
            Frame.BorrowedFrame.ImageIndex = ReservedImageIndex;
            Frame.BorrowedFrame.Width = Impl_->ResolvedState.Width;
            Frame.BorrowedFrame.Height = Impl_->ResolvedState.Height;
            Frame.BorrowedFrame.Format = Impl_->ResolvedState.Format;
            Frame.BorrowedFrame.ColorSpace =
                Impl_->ResolvedState.ColorSpace;
            Frame.BorrowedFrame.DisplayAdaptation =
                Impl_->ResolvedState.DisplayAdaptation;
            Frame.BorrowedFrame.MetadataDigest =
                Impl_->ResolvedState.MetadataDigest;
            Frame.bBorrowedLeaseActive = true;
            Frame.bPresentationSubmitted = false;
            OutImageIndex = ReservedImageIndex;
        }
        Impl_->LayerSnapshot.LastAcquiredFrameToken = FrameToken;
        OutTexture = Frame.Texture;
        OutGeneration = Frame.Generation;
        return RHI::ERHIResult::Success;
    }
#endif
}

RHI::ERHIResult FMetalPresentationContext::Present(
    Core::uint32 FrameSlot,
    Core::uint64 Generation,
    Core::uint64 FrameToken,
    const Core::TSharedPtr<FMetalSemaphore>& WaitSemaphore) noexcept
{
#if !defined(STONER_GLFW_AVAILABLE) || !STONER_GLFW_AVAILABLE
    (void)FrameSlot;
    (void)Generation;
    (void)FrameToken;
    (void)WaitSemaphore;
    return RHI::ERHIResult::Unsupported;
#else
    if (!Impl_ || !Owner_ || FrameToken == 0)
        return RHI::ERHIResult::InvalidState;
    std::unique_lock Lock(Impl_->Mutex);
    if (!Impl_->bAttached || !Impl_->bAcceptingFrames ||
        FrameSlot >= Impl_->Frames.size())
        return RHI::ERHIResult::InvalidState;
    auto& Frame = Impl_->Frames[FrameSlot];
    __block Core::uint32 LogicalWidth = 0;
    __block Core::uint32 LogicalHeight = 0;
    __block Core::uint32 Width = 0;
    __block Core::uint32 Height = 0;
    __block CGFloat DisplayScale = 1.0;
    __block bool bClosing = false;
    __block bool bPaused = false;
    const auto RefreshOnMain = ^{
        if (!Impl_->Window)
        {
            bClosing = true;
            return;
        }
        bClosing = glfwWindowShouldClose(Impl_->Window) == GLFW_TRUE;
        bPaused = glfwGetWindowAttrib(
            Impl_->Window, GLFW_ICONIFIED) == GLFW_TRUE;
        int WindowWidth = 0;
        int WindowHeight = 0;
        int PixelWidth = 0;
        int PixelHeight = 0;
        glfwGetWindowSize(Impl_->Window, &WindowWidth, &WindowHeight);
        glfwGetFramebufferSize(Impl_->Window, &PixelWidth, &PixelHeight);
        LogicalWidth = WindowWidth > 0
            ? static_cast<Core::uint32>(WindowWidth) : 0;
        LogicalHeight = WindowHeight > 0
            ? static_cast<Core::uint32>(WindowHeight) : 0;
        Width = PixelWidth > 0 ? static_cast<Core::uint32>(PixelWidth) : 0;
        Height = PixelHeight > 0 ? static_cast<Core::uint32>(PixelHeight) : 0;
        NSWindow* Window = glfwGetCocoaWindow(Impl_->Window);
        if (Window) DisplayScale = Window.backingScaleFactor;
    };
    if ([NSThread isMainThread]) RefreshOnMain();
    else dispatch_sync(dispatch_get_main_queue(), RefreshOnMain);
    if (bClosing || bPaused || Width == 0 || Height == 0 ||
        LogicalWidth != Impl_->LogicalWidth ||
        LogicalHeight != Impl_->LogicalHeight || Width != Impl_->Width ||
        Height != Impl_->Height || DisplayScale != Impl_->DisplayScale)
    {
        if ((LogicalWidth != Impl_->LogicalWidth ||
             LogicalHeight != Impl_->LogicalHeight ||
             Width != Impl_->Width || Height != Impl_->Height ||
             DisplayScale != Impl_->DisplayScale) &&
            Impl_->PresentationTracker &&
            !Impl_->PresentationTracker->IsEmpty())
            return RHI::ERHIResult::NotReady;
        Frame.Drawable = nil;
        Frame.Texture.reset();
        if (LogicalWidth != Impl_->LogicalWidth ||
            LogicalHeight != Impl_->LogicalHeight ||
            Width != Impl_->Width || Height != Impl_->Height ||
            DisplayScale != Impl_->DisplayScale)
        {
            Impl_->LogicalWidth = LogicalWidth;
            Impl_->LogicalHeight = LogicalHeight;
            Impl_->Width = Width;
            Impl_->Height = Height;
            Impl_->DisplayScale = DisplayScale;
            ++Impl_->Generation;
            Impl_->Layer.contentsScale = DisplayScale;
            Impl_->Layer.drawableSize = CGSizeMake(Width, Height);
            Impl_->ResolvedState.ModeGeneration = Impl_->Generation;
            Impl_->ResolvedState.Width = Width;
            Impl_->ResolvedState.Height = Height;
            Impl_->ResolvedState.SwapchainImageGeneration = Impl_->Generation;
            Impl_->LayerSnapshot.ModeGeneration = Impl_->Generation;
            Impl_->LayerSnapshot.Width = Width;
            Impl_->LayerSnapshot.Height = Height;
            if (Owner_) Owner_->RecordPresentationBytes(Impl_->EstimatedColorBytes());
        }
        return Width == 0 || Height == 0 || bClosing || bPaused
            ? RHI::ERHIResult::Unavailable
            : RHI::ERHIResult::ResizeRequired;
    }
    if (!Frame.Drawable || Frame.bInFlight ||
        Frame.FrameToken != FrameToken ||
        Frame.Generation != Generation || Generation != Impl_->Generation)
        return Generation != Impl_->Generation
            ? RHI::ERHIResult::ResizeRequired
            : RHI::ERHIResult::InvalidState;
    Core::uint64 WaitEpoch = 0;
    if (WaitSemaphore)
    {
        WaitEpoch = WaitSemaphore->ReserveSubmissionWait();
        if (WaitEpoch == 0) return RHI::ERHIResult::NotReady;
    }
    if (!Owner_->TryBeginSubmission())
    {
        if (WaitSemaphore) WaitSemaphore->CancelSubmissionWait(WaitEpoch);
        return RHI::ERHIResult::InvalidState;
    }

    @autoreleasepool
    {
        id<MTLCommandQueue> Queue =
            (__bridge id<MTLCommandQueue>)NativeQueue_;
        id<MTLCommandBuffer> Commands = [Queue commandBuffer];
        if (!Commands)
        {
            if (WaitSemaphore)
                WaitSemaphore->CancelSubmissionWait(WaitEpoch);
            Owner_->EndSubmission();
            return RHI::ERHIResult::Failed;
        }
        if (WaitSemaphore)
            WaitSemaphore->EncodeSubmissionWait(
                (__bridge void*)Commands, WaitEpoch);
        [Commands presentDrawable:Frame.Drawable];
        Frame.bInFlight = true;
        ++Impl_->InFlightCount;
        auto Self = weak_from_this().lock();
        if (!Self)
        {
            Frame.bInFlight = false;
            --Impl_->InFlightCount;
            if (WaitSemaphore)
                WaitSemaphore->CancelSubmissionWait(WaitEpoch);
            Owner_->EndSubmission();
            return RHI::ERHIResult::InvalidState;
        }
        [Commands addCompletedHandler:^(id<MTLCommandBuffer> Buffer) {
            std::lock_guard CompletionLock(Self->Impl_->Mutex);
            auto& Completed = Self->Impl_->Frames[FrameSlot];
            if (Completed.Generation == Generation)
            {
                Completed.Drawable = nil;
                Completed.Texture.reset();
                Completed.FrameToken = 0;
                Completed.bInFlight = false;
            }
            if (Self->Impl_->InFlightCount > 0)
                --Self->Impl_->InFlightCount;
            if (Buffer.status != MTLCommandBufferStatusCompleted ||
                Buffer.error)
                Self->Owner_->RecordTerminalFailure(
                    Core::FString("metal-presentation-command-failed"));
            Self->Owner_->EndSubmission();
            Self->Impl_->Condition.notify_all();
        }];
        if (WaitSemaphore)
            WaitSemaphore->CommitSubmissionWait(WaitEpoch);
        [Commands commit];
        Impl_->LayerSnapshot.LastSubmittedFrameToken = FrameToken;
        Impl_->LayerSnapshot.LastPresentedFrameToken = FrameToken;
        return RHI::ERHIResult::Success;
    }
#endif
}

RHI::ERHIResult FMetalPresentationContext::PresentBorrowed(
    const RHI::FRHIBorrowedAcquiredTarget& Target,
    const Core::TSharedPtr<FMetalSemaphore>& RenderFinishedSemaphore,
    RHI::FRHIPresentationLease& OutPresentationLease) noexcept
{
    return PresentBorrowedInternal(
        Target, RenderFinishedSemaphore, nullptr, OutPresentationLease);
}

RHI::ERHIResult FMetalPresentationContext::PresentBorrowedAfterRender(
    const RHI::FRHIBorrowedAcquiredTarget& Target,
    const Core::TSharedPtr<FMetalFence>& RenderCompletionFence,
    RHI::FRHIPresentationLease& OutPresentationLease) noexcept
{
    return PresentBorrowedInternal(
        Target, nullptr, RenderCompletionFence, OutPresentationLease);
}

RHI::ERHIResult FMetalPresentationContext::PresentBorrowedInternal(
    const RHI::FRHIBorrowedAcquiredTarget& Target,
    const Core::TSharedPtr<FMetalSemaphore>& RenderFinishedSemaphore,
    const Core::TSharedPtr<FMetalFence>& RenderCompletionFence,
    RHI::FRHIPresentationLease& OutPresentationLease) noexcept
{
    OutPresentationLease = {};
#if !defined(STONER_GLFW_AVAILABLE) || !STONER_GLFW_AVAILABLE
    (void)Target;
    (void)RenderFinishedSemaphore;
    (void)RenderCompletionFence;
    return RHI::ERHIResult::Unsupported;
#else
    if (!Impl_ || !Owner_ || !Target.IsValid() ||
        (!RenderFinishedSemaphore && !RenderCompletionFence))
        return RHI::ERHIResult::InvalidState;
    const auto NativeTexture =
        std::dynamic_pointer_cast<FMetalTexture>(Target.Texture);
    if (!NativeTexture || !NativeTexture->IsCompatible(Owner_))
        return RHI::ERHIResult::InvalidState;
    if (RenderFinishedSemaphore &&
        !RenderFinishedSemaphore->IsCompatible(Owner_))
        return RHI::ERHIResult::InvalidState;
    if (RenderCompletionFence)
    {
        if (!RenderCompletionFence->IsCompatible(Owner_))
            return RHI::ERHIResult::InvalidState;
        // A null render-finished semaphore is only valid with an explicit
        // caller-owned render completion proof.  Poll it without waiting so
        // presentation never races a separate render queue.
        const auto RenderCompletion = RenderCompletionFence->Wait(0);
        if (RenderCompletion == RHI::ERHIResult::NotReady)
            return RHI::ERHIResult::NotReady;
        if (RenderCompletion != RHI::ERHIResult::Success)
            return RHI::ERHIResult::Failed;
    }

    std::unique_lock Lock(Impl_->Mutex);
    if (!Impl_->bAttached || !Impl_->bAcceptingFrames ||
        Target.FrameSlotIndex >= Impl_->Frames.size())
        return RHI::ERHIResult::InvalidState;
    auto& Frame = Impl_->Frames[Target.FrameSlotIndex];
    if (!Frame.Drawable || Frame.bInFlight ||
        Frame.Texture != NativeTexture ||
        !Frame.BorrowedFrame.IsValid() ||
        Frame.BorrowedFrame != Target.Frame ||
        Frame.Generation != Impl_->Generation)
        return Frame.Generation != Impl_->Generation
            ? RHI::ERHIResult::ResizeRequired
            : RHI::ERHIResult::InvalidState;

    __block Core::uint32 LogicalWidth = 0;
    __block Core::uint32 LogicalHeight = 0;
    __block Core::uint32 Width = 0;
    __block Core::uint32 Height = 0;
    __block CGFloat DisplayScale = 1.0;
    __block bool bClosing = false;
    __block bool bPaused = false;
    const auto RefreshOnMain = ^{
        if (!Impl_->Window)
        {
            bClosing = true;
            return;
        }
        bClosing = glfwWindowShouldClose(Impl_->Window) == GLFW_TRUE;
        bPaused = glfwGetWindowAttrib(
            Impl_->Window, GLFW_ICONIFIED) == GLFW_TRUE;
        int WindowWidth = 0;
        int WindowHeight = 0;
        int PixelWidth = 0;
        int PixelHeight = 0;
        glfwGetWindowSize(Impl_->Window, &WindowWidth, &WindowHeight);
        glfwGetFramebufferSize(Impl_->Window, &PixelWidth, &PixelHeight);
        LogicalWidth = WindowWidth > 0
            ? static_cast<Core::uint32>(WindowWidth) : 0;
        LogicalHeight = WindowHeight > 0
            ? static_cast<Core::uint32>(WindowHeight) : 0;
        Width = PixelWidth > 0 ? static_cast<Core::uint32>(PixelWidth) : 0;
        Height = PixelHeight > 0 ? static_cast<Core::uint32>(PixelHeight) : 0;
        NSWindow* Window = glfwGetCocoaWindow(Impl_->Window);
        if (Window) DisplayScale = Window.backingScaleFactor;
    };
    if ([NSThread isMainThread]) RefreshOnMain();
    else dispatch_sync(dispatch_get_main_queue(), RefreshOnMain);
    if (bClosing || bPaused || Width == 0 || Height == 0)
        // Window events can arrive between the session's event snapshot and
        // this native presentation check. Keep the unsubmitted borrowed
        // drawable for resume or lifecycle cancellation, rather than turning
        // an ordinary close/minimize into a terminal device failure.
        return RHI::ERHIResult::NotReady;
    if (LogicalWidth != Impl_->LogicalWidth ||
        LogicalHeight != Impl_->LogicalHeight || Width != Impl_->Width ||
        Height != Impl_->Height || DisplayScale != Impl_->DisplayScale)
    {
        if (Impl_->PresentationTracker &&
            !Impl_->PresentationTracker->IsEmpty())
            return RHI::ERHIResult::NotReady;
        // Keep the borrowed drawable alive. It may still be referenced by
        // work recorded by the caller; a failed present never recycles it.
        Impl_->LogicalWidth = LogicalWidth;
        Impl_->LogicalHeight = LogicalHeight;
        Impl_->Width = Width;
        Impl_->Height = Height;
        Impl_->DisplayScale = DisplayScale;
        ++Impl_->Generation;
        Impl_->Layer.contentsScale = DisplayScale;
        Impl_->Layer.drawableSize = CGSizeMake(Width, Height);
        Impl_->ResolvedState.ModeGeneration = Impl_->Generation;
        Impl_->ResolvedState.Width = Width;
        Impl_->ResolvedState.Height = Height;
        Impl_->ResolvedState.SwapchainImageGeneration = Impl_->Generation;
        Impl_->LayerSnapshot.ModeGeneration = Impl_->Generation;
        Impl_->LayerSnapshot.Width = Width;
        Impl_->LayerSnapshot.Height = Height;
        if (Owner_) Owner_->RecordPresentationBytes(Impl_->EstimatedColorBytes());
        return RHI::ERHIResult::ResizeRequired;
    }

    const Core::uint64 WaitEpoch = RenderFinishedSemaphore
        ? RenderFinishedSemaphore->ReserveSubmissionWait() : 0;
    if (RenderFinishedSemaphore && WaitEpoch == 0)
        return RHI::ERHIResult::NotReady;
    if (!Owner_->TryBeginSubmission())
    {
        if (RenderFinishedSemaphore)
            RenderFinishedSemaphore->CancelSubmissionWait(WaitEpoch);
        return RHI::ERHIResult::InvalidState;
    }

    @autoreleasepool
    {
        id<MTLCommandQueue> Queue =
            (__bridge id<MTLCommandQueue>)NativeQueue_;
        id<MTLCommandBuffer> Commands = Queue ? [Queue commandBuffer] : nil;
        if (!Commands)
        {
            if (RenderFinishedSemaphore)
                RenderFinishedSemaphore->CancelSubmissionWait(WaitEpoch);
            Owner_->EndSubmission();
            return RHI::ERHIResult::Failed;
        }

        Core::TSharedPtr<FMetalFence> PresentationFence;
        Core::TSharedPtr<FMetalPresentationCompletionState> Completion;
        try
        {
            PresentationFence = Core::MakeShared<FMetalFence>(
                Owner_, nullptr, false);
            Completion = Core::MakeShared<
                FMetalPresentationCompletionState>();
        }
        catch (const std::bad_alloc&)
        {
            if (RenderFinishedSemaphore)
                RenderFinishedSemaphore->CancelSubmissionWait(WaitEpoch);
            Owner_->EndSubmission();
            return RHI::ERHIResult::Failed;
        }
        const Core::uint64 PresentationEpoch =
            PresentationFence->ReserveSubmissionSignal();
        if (PresentationEpoch == 0)
        {
            if (RenderFinishedSemaphore)
                RenderFinishedSemaphore->CancelSubmissionWait(WaitEpoch);
            Owner_->EndSubmission();
            return RHI::ERHIResult::InvalidState;
        }
        Completion->Owner = Owner_;
        Completion->Texture = NativeTexture;
        Completion->RenderFinishedSemaphore = RenderFinishedSemaphore;
        Completion->PresentationFence = PresentationFence;
        Completion->Tracker = Impl_->PresentationTracker;
        Completion->FrameToken = Target.Frame.FrameToken;
        Completion->ImageIndex = Target.Frame.ImageIndex;
        Completion->PresentationEpoch = PresentationEpoch;

        if (RenderFinishedSemaphore)
            RenderFinishedSemaphore->EncodeSubmissionWait(
                (__bridge void*)Commands, WaitEpoch);
        [Frame.Drawable addPresentedHandler:
            ^(id<MTLDrawable> PresentedDrawable) {
                // A drawable can be skipped while a window is being
                // minimized or resized.  presentedTime==0 is not display
                // proof, but the native release itself is still a successful
                // terminal presentation completion.  Keep those outcomes
                // separate so a dropped frame cannot advance presented state.
                Completion->Complete(
                    true, PresentedDrawable.presentedTime > 0.0);
        }];
        [Commands presentDrawable:Frame.Drawable];

        Frame.bInFlight = true;
        Frame.bPresentationSubmitted = true;
        ++Impl_->InFlightCount;
        const auto WeakContext = weak_from_this();
        const auto SubmissionOwner = Owner_;
        const Core::uint32 FrameSlot = Target.FrameSlotIndex;
        const Core::uint64 Generation = Frame.Generation;
        const Core::uint64 FrameToken = Target.Frame.FrameToken;
        [Commands addCompletedHandler:^(id<MTLCommandBuffer> Buffer) {
            // Dropping the frame's drawable can invoke its release handler.
            // Publish a native failure first so that callback cannot turn a
            // failed presentation command into a successful lease result.
            if (Buffer.status != MTLCommandBufferStatusCompleted ||
                Buffer.error)
            {
                Completion->Complete(false, false);
                SubmissionOwner->RecordTerminalFailure(
                    Core::FString("metal-presentation-command-failed"));
            }
            if (auto Context = WeakContext.lock())
            {
                std::lock_guard CompletionLock(Context->Impl_->Mutex);
                auto& Completed = Context->Impl_->Frames[FrameSlot];
                if (Completed.Generation == Generation &&
                    Completed.FrameToken == FrameToken)
                {
                    Completed.Drawable = nil;
                    Completed.Texture.reset();
                    Completed.FrameToken = 0;
                    Completed.Generation = 0;
                    Completed.ImageIndex = 0;
                    Completed.BorrowedFrame = {};
                    Completed.bBorrowedLeaseActive = false;
                    Completed.bPresentationSubmitted = false;
                    Completed.bInFlight = false;
                }
                if (Context->Impl_->InFlightCount > 0)
                    --Context->Impl_->InFlightCount;
                Context->Impl_->Condition.notify_all();
            }
            SubmissionOwner->EndSubmission();
        }];

        // Publish the lease before commit. The presentation callback only
        // touches Completion, so it cannot race context destruction or retain
        // sampled render resources.
        OutPresentationLease.Frame = Target.Frame;
        OutPresentationLease.RenderFinishedSemaphore =
            RenderFinishedSemaphore;
        OutPresentationLease.PresentationCompletionFence = PresentationFence;
        if (RenderFinishedSemaphore)
            RenderFinishedSemaphore->CommitSubmissionWait(WaitEpoch);
        Impl_->LayerSnapshot.LastSubmittedFrameToken =
            Target.Frame.FrameToken;
        [Commands commit];
        return RHI::ERHIResult::Success;
    }
#endif
}

RHI::ERHIResult FMetalPresentationContext::ReleaseBorrowed(
    const RHI::FRHIBorrowedAcquiredTarget& Target,
    const Core::TSharedPtr<RHI::IRHIFence>& RenderCompletionFence) noexcept
{
#if !defined(STONER_GLFW_AVAILABLE) || !STONER_GLFW_AVAILABLE
    (void)Target;
    (void)RenderCompletionFence;
    return RHI::ERHIResult::Unsupported;
#else
    if (!Impl_ || !Owner_ || !Target.IsValid())
        return RHI::ERHIResult::InvalidState;
    const auto NativeTexture =
        std::dynamic_pointer_cast<FMetalTexture>(Target.Texture);
    if (!NativeTexture || !NativeTexture->IsCompatible(Owner_))
        return RHI::ERHIResult::InvalidState;
    if (RenderCompletionFence)
    {
        const auto NativeFence =
            std::dynamic_pointer_cast<FMetalFence>(RenderCompletionFence);
        if (!NativeFence || !NativeFence->IsCompatible(Owner_))
            return RHI::ERHIResult::InvalidState;
        const auto RenderCompletion = RenderCompletionFence->Wait(0);
        if (RenderCompletion == RHI::ERHIResult::NotReady)
            return RHI::ERHIResult::NotReady;
        if (RenderCompletion != RHI::ERHIResult::Success &&
            RenderCompletion != RHI::ERHIResult::Failed)
            return RenderCompletion;
    }

    std::lock_guard Lock(Impl_->Mutex);
    if (Target.FrameSlotIndex >= Impl_->Frames.size())
        return RHI::ERHIResult::InvalidState;
    auto& Frame = Impl_->Frames[Target.FrameSlotIndex];
    if (!Frame.bBorrowedLeaseActive || Frame.bInFlight ||
        Frame.Texture != NativeTexture ||
        Frame.BorrowedFrame != Target.Frame)
        return Frame.bInFlight
            ? RHI::ERHIResult::NotReady : RHI::ERHIResult::InvalidState;
    (void)NativeTexture->Invalidate();
    Frame.Drawable = nil;
    Frame.Texture.reset();
    Frame.FrameToken = 0;
    Frame.Generation = 0;
    Frame.ImageIndex = 0;
    Frame.BorrowedFrame = {};
    Frame.bBorrowedLeaseActive = false;
    Frame.bPresentationSubmitted = false;
    if (Impl_->PresentationTracker)
        Impl_->PresentationTracker->Release(Target.Frame.ImageIndex);
    Impl_->Condition.notify_all();
    return RHI::ERHIResult::Success;
#endif
}

void FMetalPresentationContext::ReleaseBorrowedAcquire(
    Core::uint32 FrameSlot,
    Core::uint64 Generation,
    Core::uint64 FrameToken) noexcept
{
    if (!Impl_) return;
    std::lock_guard Lock(Impl_->Mutex);
    if (FrameSlot >= Impl_->Frames.size()) return;
    auto& Frame = Impl_->Frames[FrameSlot];
    if (!Frame.bBorrowedLeaseActive || Frame.bInFlight ||
        Frame.bPresentationSubmitted || Frame.Generation != Generation ||
        Frame.FrameToken != FrameToken)
        return;
    if (Frame.Texture) (void)Frame.Texture->Invalidate();
    const Core::uint32 ImageIndex = Frame.ImageIndex;
    Frame.Drawable = nil;
    Frame.Texture.reset();
    Frame.FrameToken = 0;
    Frame.Generation = 0;
    Frame.ImageIndex = 0;
    Frame.BorrowedFrame = {};
    Frame.bBorrowedLeaseActive = false;
    if (Impl_->PresentationTracker)
        Impl_->PresentationTracker->Release(ImageIndex);
    Impl_->Condition.notify_all();
}

bool FMetalPresentationContext::HasPendingBorrowedAcquire(
    Core::uint32 FrameSlot, Core::uint64 FrameToken) const noexcept
{
    if (!Impl_ || FrameToken == 0) return false;
    std::lock_guard Lock(Impl_->Mutex);
    return FrameSlot < Impl_->Frames.size() &&
        Impl_->Frames[FrameSlot].FrameToken == FrameToken &&
        Impl_->Frames[FrameSlot].PendingDrawableAcquire != nullptr;
}

RHI::ERHIResult FMetalPresentationContext::CancelPendingBorrowedAcquire(
    Core::uint32 FrameSlot, Core::uint64 FrameToken) noexcept
{
    if (!Impl_ || FrameToken == 0) return RHI::ERHIResult::InvalidState;
    {
        std::lock_guard Lock(Impl_->Mutex);
        if (FrameSlot >= Impl_->Frames.size()) return RHI::ERHIResult::InvalidState;
        const auto& Frame = Impl_->Frames[FrameSlot];
        // A completed canceled job may already have been reaped by its callback.
        if (Frame.FrameToken == 0) return RHI::ERHIResult::Success;
        if (Frame.FrameToken != FrameToken || Frame.bInFlight || Frame.bBorrowedLeaseActive)
            return RHI::ERHIResult::InvalidState;
    }
    CancelAcquire(FrameSlot, FrameToken);
    std::lock_guard Lock(Impl_->Mutex);
    return Impl_->Frames[FrameSlot].FrameToken == 0
        ? RHI::ERHIResult::Success : RHI::ERHIResult::NotReady;
}

void FMetalPresentationContext::CancelAcquire(
    Core::uint32 FrameSlot,
    Core::uint64 FrameToken) noexcept
{
    CancelAcquire(FrameSlot, 0, FrameToken);
}

void FMetalPresentationContext::PollCompletedUnpublishedBorrowedAcquires()
    noexcept
{
    if (!Impl_) return;
    std::lock_guard Lock(Impl_->Mutex);
    for (auto& Frame : Impl_->Frames)
    {
        if (!Frame.PendingDrawableAcquire || Frame.bBorrowedLeaseActive ||
            Frame.bInFlight)
            continue;
        __strong id<CAMetalDrawable> Discarded = nil;
        if (!Frame.PendingDrawableAcquire->TryTakeCancelled(Discarded))
            continue;

        Frame.PendingDrawableAcquire.reset();
        if (Impl_->PendingDrawableAcquireCount > 0)
            --Impl_->PendingDrawableAcquireCount;
        const Core::uint32 ImageIndex = Frame.ImageIndex;
        Frame.Drawable = nil;
        Frame.Texture.reset();
        Frame.FrameToken = 0;
        Frame.Generation = 0;
        Frame.ImageIndex = 0;
        Frame.BorrowedFrame = {};
        if (Impl_->PresentationTracker)
            Impl_->PresentationTracker->Release(ImageIndex);
    }
    Impl_->Condition.notify_all();
}

void FMetalPresentationContext::CancelAllUnpublishedBorrowedAcquires()
    noexcept
{
    if (!Impl_) return;
    {
        std::lock_guard Lock(Impl_->Mutex);
        for (auto& Frame : Impl_->Frames)
        {
            if (Frame.PendingDrawableAcquire &&
                !Frame.bBorrowedLeaseActive && !Frame.bInFlight)
                Frame.PendingDrawableAcquire->Cancel();
        }
    }
    // Consume any jobs that completed before or during the cancellation
    // pass.  Jobs still inside nextDrawable remain in their frame records;
    // the worker completion callback will call the same poll once it returns.
    PollCompletedUnpublishedBorrowedAcquires();
}

void FMetalPresentationContext::CancelAcquire(
    Core::uint32 FrameSlot,
    Core::uint64 Generation,
    Core::uint64 FrameToken) noexcept
{
    if (!Impl_ || FrameToken == 0) return;
    std::lock_guard Lock(Impl_->Mutex);
    if (FrameSlot >= Impl_->Frames.size()) return;
    auto& Frame = Impl_->Frames[FrameSlot];
    const bool bGenerationMatches = Generation == 0 ||
        Frame.Generation == Generation;
    if (Frame.bInFlight || Frame.bBorrowedLeaseActive ||
        !bGenerationMatches || Frame.FrameToken != FrameToken)
        return;

    if (Frame.PendingDrawableAcquire)
    {
        // A cancellation request cannot revoke nextDrawable.  Keep both the
        // worker's layer owner and the image admission in the bounded frame
        // slot until the worker returns.  If it already returned, consume the
        // completed unpublished result here so a caller does not need a
        // second lifecycle operation to release the admission.
        const auto Pending = Frame.PendingDrawableAcquire;
        Pending->Cancel();
        __strong id<CAMetalDrawable> Discarded = nil;
        bool bCancelled = false;
        if (Pending->TryTake(Discarded, bCancelled))
        {
            (void)bCancelled;
            Frame.PendingDrawableAcquire.reset();
            if (Impl_->PendingDrawableAcquireCount > 0)
                --Impl_->PendingDrawableAcquireCount;
            const Core::uint32 ImageIndex = Frame.ImageIndex;
            Frame.Drawable = nil;
            Frame.Texture.reset();
            Frame.FrameToken = 0;
            Frame.Generation = 0;
            Frame.ImageIndex = 0;
            Frame.BorrowedFrame = {};
            if (Impl_->PresentationTracker)
                Impl_->PresentationTracker->Release(ImageIndex);
        }
        Impl_->Condition.notify_all();
        return;
    }

    Frame.Drawable = nil;
    Frame.Texture.reset();
    Frame.FrameToken = 0;
    Frame.Generation = 0;
    Frame.ImageIndex = 0;
    Frame.BorrowedFrame = {};
    Impl_->Condition.notify_all();
}

RHI::ERHIResult FMetalPresentationContext::Shutdown() noexcept
{
    if (!Impl_) return RHI::ERHIResult::InvalidState;
    const auto Deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    std::array<Core::TSharedPtr<FMetalDrawableAcquireState>,
        RHI::MaxRHIFrameSlots> PendingJobs{};
    Core::uint32 PendingJobCount = 0;
    {
        std::unique_lock Lock(Impl_->Mutex);
        if (!Impl_->bAttached) return RHI::ERHIResult::InvalidState;
        Impl_->bAcceptingFrames = false;
        for (auto& Frame : Impl_->Frames)
        {
            if (Frame.PendingDrawableAcquire)
            {
                const auto Pending = Frame.PendingDrawableAcquire;
                Pending->Cancel();
                __strong id<CAMetalDrawable> Discarded = nil;
                bool bCancelled = false;
                if (Pending->TryTake(Discarded, bCancelled))
                {
                    (void)bCancelled;
                    Frame.PendingDrawableAcquire.reset();
                    if (Impl_->PendingDrawableAcquireCount > 0)
                        --Impl_->PendingDrawableAcquireCount;
                    const Core::uint32 ImageIndex = Frame.ImageIndex;
                    Frame.FrameToken = 0;
                    Frame.Generation = 0;
                    Frame.ImageIndex = 0;
                    Frame.BorrowedFrame = {};
                    if (Impl_->PresentationTracker)
                        Impl_->PresentationTracker->Release(ImageIndex);
                }
                else if (PendingJobCount < PendingJobs.size())
                {
                    // The worker still owns the layer and may be blocked in
                    // nextDrawable.  Keep the shared job until its actual
                    // completion, even though shutdown has stopped new
                    // submissions.
                    PendingJobs[PendingJobCount++] = Pending;
                }
            }
            if (!Frame.bInFlight)
            {
                // An acquired borrowed drawable may still be referenced by
                // render work submitted by the caller.  Shutdown has no
                // completion proof for that work, so keep the target and its
                // image lease until ReleaseBorrowed supplies one.
                if (Frame.bBorrowedLeaseActive ||
                    Frame.PendingDrawableAcquire)
                    continue;
                Frame.Drawable = nil;
                Frame.Texture.reset();
                Frame.BorrowedFrame = {};
            }
        }
        if (!Impl_->Condition.wait_for(
                Lock, std::chrono::duration_cast<std::chrono::milliseconds>(
                    Deadline - std::chrono::steady_clock::now()),
                [this] { return Impl_->InFlightCount == 0; }))
            return RHI::ERHIResult::Timeout;
    }
    for (Core::uint32 Index = 0; Index < PendingJobCount; ++Index)
    {
        const auto Remaining = Deadline - std::chrono::steady_clock::now();
        if (Remaining <= std::chrono::steady_clock::duration::zero() ||
            !PendingJobs[Index]->WaitFor(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    Remaining)))
            return RHI::ERHIResult::Timeout;
    }
    if (PendingJobCount != 0)
    {
        std::lock_guard Lock(Impl_->Mutex);
        for (Core::uint32 Index = 0; Index < PendingJobCount; ++Index)
        {
            for (auto& Frame : Impl_->Frames)
            {
                if (Frame.PendingDrawableAcquire != PendingJobs[Index])
                    continue;
                __strong id<CAMetalDrawable> Drawable = nil;
                bool bCancelled = false;
                if (!PendingJobs[Index]->TryTake(Drawable, bCancelled))
                    return RHI::ERHIResult::Timeout;
                (void)bCancelled;
                Frame.PendingDrawableAcquire.reset();
                if (Impl_->PendingDrawableAcquireCount > 0)
                    --Impl_->PendingDrawableAcquireCount;
                const Core::uint32 ImageIndex = Frame.ImageIndex;
                Frame.FrameToken = 0;
                Frame.Generation = 0;
                Frame.ImageIndex = 0;
                Frame.BorrowedFrame = {};
                if (Impl_->PresentationTracker)
                    Impl_->PresentationTracker->Release(ImageIndex);
                break;
            }
        }
    }
    if (Impl_->PresentationTracker)
    {
        const auto Remaining = Deadline - std::chrono::steady_clock::now();
        if (Remaining <= std::chrono::steady_clock::duration::zero() ||
            !Impl_->PresentationTracker->WaitForZero(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    Remaining)))
            return RHI::ERHIResult::Timeout;
    }
    const auto DetachOnMain = ^{
        if (Impl_->View && Impl_->View.layer == Impl_->Layer)
        {
            Impl_->Layer.device = nil;
            Impl_->View.layer = Impl_->PreviousLayer;
            Impl_->View.wantsLayer = Impl_->bPreviousWantsLayer;
        }
        Impl_->Layer = nil;
        Impl_->PreviousLayer = nil;
        Impl_->View = nil;
        Impl_->Window = nullptr;
    };
    if ([NSThread isMainThread]) DetachOnMain();
    else dispatch_sync(dispatch_get_main_queue(), DetachOnMain);
    std::lock_guard Lock(Impl_->Mutex);
    Impl_->Frames.clear();
    Impl_->bAttached = false;
    ++Impl_->Generation;
    return RHI::ERHIResult::Success;
}

Core::uint64 FMetalPresentationContext::GetGeneration() const noexcept
{
    if (!Impl_) return 0;
    std::lock_guard Lock(Impl_->Mutex);
    return Impl_->Generation;
}

RHI::FRHIResolvedPresentationState
FMetalPresentationContext::GetResolvedPresentationState() const noexcept
{
    if (!Impl_) return {};
    std::lock_guard Lock(Impl_->Mutex);
    return Impl_->ResolvedState;
}

FMetalPresentationLayerSnapshot
FMetalPresentationContext::GetLayerSnapshot() const noexcept
{
    if (!Impl_) return {};
    std::lock_guard Lock(Impl_->Mutex);
    FMetalPresentationLayerSnapshot Snapshot = Impl_->LayerSnapshot;
    auto& Native = Snapshot.NativeStatistics;
    Native.bAvailable = true;
    if (Impl_->bAttached)
    {
        Snapshot.Policy.bHasEDRMetadata = Impl_->Layer.EDRMetadata != nil;
        Native.ActiveGeneration = Impl_->Generation;
        Native.ActiveImageCount = static_cast<Core::uint32>(Impl_->Layer.maximumDrawableCount);
        Native.EstimatedColorBytes = Impl_->EstimatedColorBytes();
    }
    Native.PeakEstimatedColorBytes = Owner_ ? Owner_->GetPeakPresentationBytes() : 0;
    Native.PendingAcquireCount = Impl_->PendingDrawableAcquireCount;
    Native.AcquisitionRecordCount = Impl_->PresentationTracker ? Impl_->PresentationTracker->GetPendingCount() : 0;
    Native.PresentationOwnerCount = Native.AcquisitionRecordCount;
    Native.ResidualNativeOwners = Native.AcquisitionRecordCount + Native.PendingAcquireCount;
    if (Impl_->PresentationTracker &&
        Impl_->PresentationTracker->HasPresentedFrame())
        Snapshot.LastPresentedFrameToken =
            Impl_->PresentationTracker->GetLastPresentedFrameToken();
    return Snapshot;
}

Core::uint32 FMetalPresentationContext::GetPendingPresentationLeaseCount()
    const noexcept
{
    if (!Impl_ || !Impl_->PresentationTracker) return 0;
    return Impl_->PresentationTracker->GetPendingCount();
}

Core::uint32 FMetalPresentationContext::GetPendingDrawableAcquireCount()
    const noexcept
{
    if (!Impl_) return 0;
    std::lock_guard Lock(Impl_->Mutex);
    return Impl_->PendingDrawableAcquireCount;
}

} // namespace Stoner::Backend::Metal::Private
