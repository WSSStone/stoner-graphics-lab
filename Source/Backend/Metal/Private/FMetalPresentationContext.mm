#include "FMetalPresentationContext.h"

#include "FMetalCapabilities.h"
#include "FMetalFormat.h"
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
#include <mutex>
#include <new>
#include <vector>

namespace Stoner::Backend::Metal::Private
{

struct FMetalPresentationContext::FImpl
{
    struct FFrame
    {
        __strong id<CAMetalDrawable> Drawable;
        Core::TSharedPtr<FMetalTexture> Texture;
        Core::uint64 Generation = 0;
        Core::uint64 FrameToken = 0;
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
    RHI::ERHIFormat Format = RHI::ERHIFormat::Unknown;
    RHI::FRHIResolvedPresentationState ResolvedState;
    FMetalPresentationLayerSnapshot LayerSnapshot;
    std::vector<FFrame> Frames;
};

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
        Impl_->LayerSnapshot.NativeReferenceWhiteNits =
            Capabilities.NativeReferenceWhiteNits;
        Impl_->LayerSnapshot.CurrentHeadroom =
            Capabilities.CurrentHeadroom;
        Impl_->LayerSnapshot.PotentialHeadroom =
            Capabilities.PotentialHeadroom;
        Impl_->LayerSnapshot.MetadataDigest =
            Impl_->ResolvedState.MetadataDigest;
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
    if (Impl_->InFlightCount != 0)
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
    OutTexture.reset();
    OutGeneration = 0;
#if !defined(STONER_GLFW_AVAILABLE) || !STONER_GLFW_AVAILABLE
    (void)FrameSlot;
    (void)FrameToken;
    return RHI::ERHIResult::Unsupported;
#else
    if (!Impl_ || FrameToken == 0) return RHI::ERHIResult::InvalidState;
    std::lock_guard Lock(Impl_->Mutex);
    if (!Impl_->bAttached || !Impl_->bAcceptingFrames ||
        FrameSlot >= Impl_->Frames.size())
        return RHI::ERHIResult::InvalidState;
    auto& Frame = Impl_->Frames[FrameSlot];
    if (Frame.Drawable || Frame.bInFlight) return RHI::ERHIResult::NotReady;

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
        return RHI::ERHIResult::Unavailable;
    if (LogicalWidth != Impl_->LogicalWidth ||
        LogicalHeight != Impl_->LogicalHeight || Width != Impl_->Width ||
        Height != Impl_->Height || DisplayScale != Impl_->DisplayScale)
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
        return RHI::ERHIResult::ResizeRequired;
    }

    @autoreleasepool
    {
        id<CAMetalDrawable> Drawable = [Impl_->Layer nextDrawable];
        if (!Drawable) return RHI::ERHIResult::Unavailable;
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
            return RHI::ERHIResult::Failed;
        }
        Frame.Drawable = Drawable;
        Frame.Generation = Impl_->Generation;
        Frame.FrameToken = FrameToken;
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

void FMetalPresentationContext::CancelAcquire(
    Core::uint32 FrameSlot,
    Core::uint64 Generation) noexcept
{
    if (!Impl_) return;
    std::lock_guard Lock(Impl_->Mutex);
    if (FrameSlot >= Impl_->Frames.size()) return;
    auto& Frame = Impl_->Frames[FrameSlot];
    if (!Frame.bInFlight && Frame.Generation == Generation)
    {
        Frame.Drawable = nil;
        Frame.Texture.reset();
        Frame.FrameToken = 0;
    }
}

RHI::ERHIResult FMetalPresentationContext::Shutdown() noexcept
{
    if (!Impl_) return RHI::ERHIResult::InvalidState;
    {
        std::unique_lock Lock(Impl_->Mutex);
        if (!Impl_->bAttached) return RHI::ERHIResult::InvalidState;
        Impl_->bAcceptingFrames = false;
        for (auto& Frame : Impl_->Frames)
        {
            if (!Frame.bInFlight)
            {
                Frame.Drawable = nil;
                Frame.Texture.reset();
            }
        }
        if (!Impl_->Condition.wait_for(
                Lock, std::chrono::seconds(5),
                [this] { return Impl_->InFlightCount == 0; }))
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
    return Impl_->LayerSnapshot;
}

} // namespace Stoner::Backend::Metal::Private
