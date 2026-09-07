#include "MetalPresentationTests.h"

#include "Core/SGPlatform.h"
#include "MetalRHI/FMetalDeviceFactory.h"
#if SG_PLATFORM_MAC
#include "FMetalDeviceOwnerState.h"
#include "FMetalPresentationContext.h"
#include "FMetalPresentationSurface.h"
#include "FMetalSwapchain.h"
#endif
#include "RHI/RHIMinimal.h"

#include <chrono>
#include <iostream>

namespace
{

using namespace Stoner;
using namespace Stoner::Backend::Metal;
using namespace Stoner::Core;
using namespace Stoner::RHI;

void Record(FMetalPresentationTestResult& Result, bool bPassed,
    const char* Name)
{
    (bPassed ? ++Result.Passed : ++Result.Failed);
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

class FDefaultBorrowedSwapchain final : public IRHISwapchain
{
public:
    [[nodiscard]] ERHISwapchainState GetState() const noexcept override
    {
        return ERHISwapchainState::Unavailable;
    }

    [[nodiscard]] uint32 GetFrameCount() const noexcept override { return 0; }
    [[nodiscard]] uint32 GetCurrentFrameIndex() const noexcept override
    {
        return 0;
    }

    ERHIResult AcquireNextFrame(uint32&) override
    {
        return ERHIResult::Unsupported;
    }

    ERHIResult Present(uint32) override
    {
        return ERHIResult::Unsupported;
    }
};

void TestBorrowedSwapchainDefaults(FMetalPresentationTestResult& Result)
{
    FDefaultBorrowedSwapchain Swapchain;
    FRHIBorrowedAcquiredTarget Target;
    Target.Frame.FrameToken = 77;
    FRHIPresentationLease Lease;
    Lease.Frame.FrameToken = 77;
    FRHIRenderLease RenderLease;
    RenderLease.Frame.FrameToken = 77;
    Record(Result,
        Swapchain.AcquireBorrowedTarget(77, 0, Target) ==
                ERHIResult::Unsupported && Target.Frame.FrameToken == 0 &&
            Swapchain.PresentBorrowedTarget(Target, nullptr, Lease) ==
                ERHIResult::Unsupported && Lease.Frame.FrameToken == 0 &&
            Swapchain.PresentBorrowedTarget(Target, RenderLease, Lease) ==
                ERHIResult::Unsupported && Lease.Frame.FrameToken == 0 &&
            Swapchain.ReleaseBorrowedTarget(Target, nullptr) ==
                ERHIResult::Unsupported,
        "legacy swapchains keep borrowed ownership seams unsupported");
}

#if SG_PLATFORM_MAC
void TestPausedBorrowedAcquire(FMetalPresentationTestResult& Result)
{
    auto Owner = MakeShared<Private::FMetalDeviceOwnerState>(902);
    auto Context = MakeShared<Private::FMetalPresentationContext>(
        Owner, nullptr, nullptr);
    FRHIPresentationSurfaceDesc SurfaceDesc;
    auto Surface = MakeShared<Private::FMetalPresentationSurface>(
        Owner, SurfaceDesc, Context);
    FRHISwapchainDesc InitialDesc;
    InitialDesc.Width = 640;
    InitialDesc.Height = 360;
    InitialDesc.FramesInFlight = 2;
    auto Swapchain = MakeShared<Private::FMetalSwapchain>(
        Owner, Surface, InitialDesc);
    FRHISwapchainDesc ZeroDesc = InitialDesc;
    ZeroDesc.Width = 0;
    ZeroDesc.Height = 0;
    const ERHIResult ReconfigureResult = Swapchain->Reconfigure(ZeroDesc);
    FRHIBorrowedAcquiredTarget Target;
    const ERHIResult AcquireResult = Swapchain->AcquireBorrowedTarget(
        1, 0, Target);
    Record(Result,
        ReconfigureResult == ERHIResult::NotReady &&
            Swapchain->GetState() == ERHISwapchainState::Paused &&
            AcquireResult == ERHIResult::NotReady && !Target.IsValid() &&
            Context->GetPendingDrawableAcquireCount() == 0 &&
            Context->GetPendingPresentationLeaseCount() == 0,
        "paused Metal swapchains reject borrowed acquisition before native work");
}

void TestPresentationTrackerBounds(FMetalPresentationTestResult& Result)
{
    auto Tracker = MakeShared<Private::FMetalPresentationTracker>();
    uint32 ImageIndices[MaxRHIPresentationImageLeases]{};
    bool bReservedAll = true;
    for (uint32 Index = 0; Index < MaxRHIPresentationImageLeases; ++Index)
        bReservedAll = bReservedAll && Tracker->TryReserve(ImageIndices[Index]);
    uint32 RejectedIndex = 0;
    const bool bRejectedNinth = !Tracker->TryReserve(RejectedIndex);
    Tracker->Complete(ImageIndices[0], 10, false);
    const bool bFailureDidNotPresent =
        Tracker->GetPendingCount() == MaxRHIPresentationImageLeases - 1 &&
        !Tracker->HasPresentedFrame() &&
        Tracker->GetLastPresentedFrameToken() == 0;
    uint32 ReusedIndex = 0;
    const bool bReusedAfterFailure = Tracker->TryReserve(ReusedIndex) &&
        ReusedIndex == ImageIndices[0];
    Tracker->Complete(ImageIndices[1], 11, true);
    const bool bSuccessRecorded =
        Tracker->GetLastPresentedFrameToken() == 11;
    Tracker->Complete(ImageIndices[1], 12, false);
    const bool bDuplicateFailureDidNotAdvance =
        Tracker->GetLastPresentedFrameToken() == 11;
    for (uint32 Index = 2; Index < MaxRHIPresentationImageLeases; ++Index)
        Tracker->Release(ImageIndices[Index]);
    Tracker->Release(ReusedIndex);
    Record(Result,
        bReservedAll && bRejectedNinth && bFailureDidNotPresent &&
            bReusedAfterFailure && bSuccessRecorded &&
            bDuplicateFailureDidNotAdvance && Tracker->IsEmpty() &&
            Tracker->WaitForZero(std::chrono::milliseconds(0)),
        "Metal presentation tracker enforces eight leases and failure ownership");
}
#endif

} // namespace

FMetalPresentationTestResult RunMetalPresentationTests()
{
    FMetalPresentationTestResult Result;
    TestBorrowedSwapchainDefaults(Result);
#if SG_PLATFORM_MAC
    TestPausedBorrowedAcquire(Result);
    TestPresentationTrackerBounds(Result);
    auto Owner = MakeShared<Private::FMetalDeviceOwnerState>(901);
    auto Context = MakeShared<Private::FMetalPresentationContext>(
        Owner, nullptr, nullptr);
    FRHIPresentationSurfaceDesc Desc;
    Desc.Window = FPlatformWindow(reinterpret_cast<void*>(0x1));
    auto Surface = MakeShared<Private::FMetalPresentationSurface>(
        Owner, Desc, Context);
#if defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
    Record(Result,
        Context->Attach(Desc.Window, ERHIFormat::B8G8R8A8_UNorm, 2, true) ==
                ERHIResult::InvalidState &&
            !Context->IsAttached(),
        "presentation attach rejects missing native Metal ownership");
#else
    Record(Result,
        Context->Attach(Desc.Window, ERHIFormat::B8G8R8A8_UNorm, 2, true) ==
                ERHIResult::Unsupported &&
            !Context->IsAttached(),
        "presentation attach reports unsupported without GLFW");
#endif
    Record(Result,
        Surface->IsValid() && Surface->Invalidate() == ERHIResult::Success &&
            !Surface->IsValid() &&
            Surface->Invalidate() == ERHIResult::InvalidState,
        "presentation surface invalidation is generation-safe and idempotence-aware");

    FRHIPresentationLease InvalidLease;
    InvalidLease.Frame.FrameToken = 9;
#if defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
    const ERHIResult BorrowedResult = Context->PresentBorrowed(
        FRHIBorrowedAcquiredTarget{}, nullptr, InvalidLease);
    Record(Result,
        BorrowedResult == ERHIResult::InvalidState &&
            InvalidLease.Frame.FrameToken == 0,
        "Metal borrowed presentation rejects invalid ownership before native submission");
#else
    const ERHIResult BorrowedResult = Context->PresentBorrowed(
        FRHIBorrowedAcquiredTarget{}, nullptr, InvalidLease);
    Record(Result,
        BorrowedResult == ERHIResult::Unsupported &&
            InvalidLease.Frame.FrameToken == 0,
        "Metal borrowed presentation stays unsupported without the native window path");
#endif

    const auto Created = CreateMetalDevice();
    if (!Created.Succeeded())
    {
        Record(Result, Created.Result == ERHIResult::Unavailable,
            "presentation factory reports controlled unavailable without a Metal device");
    }
    else
    {
        FRHIPresentationSurfaceDesc Invalid;
        Record(Result,
            Created.Device->CreatePresentationSurface(Invalid).Result ==
                ERHIResult::InvalidState,
            "presentation factory rejects an invalid borrowed window");
        (void)Created.Device->Shutdown();
    }
#else
    Record(Result, true,
        "Metal presentation contracts remain isolated from non-macOS builds");
#endif
    return Result;
}
