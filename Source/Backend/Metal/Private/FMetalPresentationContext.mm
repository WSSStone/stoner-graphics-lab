#include "FMetalPresentationContext.h"

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
    std::vector<FFrame> Frames;
};

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
#if !defined(STONER_GLFW_AVAILABLE) || !STONER_GLFW_AVAILABLE
    (void)PlatformWindow;
    (void)Format;
    (void)MaximumDrawableCount;
    (void)bVSync;
    (void)NativeDevice_;
    (void)NativeQueue_;
    return RHI::ERHIResult::Unsupported;
#else
    if (!Impl_ || !Owner_ || !PlatformWindow.IsValid() ||
        NativeDevice_ == nullptr || NativeQueue_ == nullptr ||
        MaximumDrawableCount < 2 || MaximumDrawableCount > 3 ||
        ToMetalPixelFormat(Format) == MTLPixelFormatInvalid)
        return RHI::ERHIResult::InvalidState;

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
        Layer.pixelFormat = static_cast<MTLPixelFormat>(ToMetalPixelFormat(Format));
        CGColorSpaceRef ColorSpace =
            CGColorSpaceCreateWithName(kCGColorSpaceLinearSRGB);
        if (!ColorSpace) return;
        Layer.colorspace = ColorSpace;
        CGColorSpaceRelease(ColorSpace);
        Layer.framebufferOnly = NO;
        Layer.maximumDrawableCount = MaximumDrawableCount;
        Layer.displaySyncEnabled = bVSync;
        Layer.drawableSize = CGSizeMake(Width, Height);
        Layer.contentsScale = DisplayScale;
        Layer.opaque = YES;

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
        Impl_->Format = Format;
        Impl_->bAttached = true;
        Impl_->bAcceptingFrames = true;
    }
    try
    {
        std::lock_guard Lock(Impl_->Mutex);
        Impl_->Frames.resize(MaximumDrawableCount);
    }
    catch (const std::bad_alloc&)
    {
        (void)Shutdown();
        return RHI::ERHIResult::Failed;
    }
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
    Core::TSharedPtr<RHI::IRHITexture>& OutTexture,
    Core::uint64& OutGeneration) noexcept
{
    OutTexture.reset();
    OutGeneration = 0;
#if !defined(STONER_GLFW_AVAILABLE) || !STONER_GLFW_AVAILABLE
    (void)FrameSlot;
    return RHI::ERHIResult::Unsupported;
#else
    if (!Impl_) return RHI::ERHIResult::InvalidState;
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
            RHI::ERHITextureUsage::CopySource;
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
        OutTexture = Frame.Texture;
        OutGeneration = Frame.Generation;
        return RHI::ERHIResult::Success;
    }
#endif
}

RHI::ERHIResult FMetalPresentationContext::Present(
    Core::uint32 FrameSlot,
    Core::uint64 Generation,
    const Core::TSharedPtr<FMetalSemaphore>& WaitSemaphore) noexcept
{
#if !defined(STONER_GLFW_AVAILABLE) || !STONER_GLFW_AVAILABLE
    (void)FrameSlot;
    (void)Generation;
    (void)WaitSemaphore;
    return RHI::ERHIResult::Unsupported;
#else
    if (!Impl_ || !Owner_) return RHI::ERHIResult::InvalidState;
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
        }
        return Width == 0 || Height == 0 || bClosing || bPaused
            ? RHI::ERHIResult::Unavailable
            : RHI::ERHIResult::ResizeRequired;
    }
    if (!Frame.Drawable || Frame.bInFlight ||
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

} // namespace Stoner::Backend::Metal::Private
