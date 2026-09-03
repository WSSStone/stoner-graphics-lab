#include "OutputPresentationLifecycleTests.h"

#include "RHI/RHIMinimal.h"
#include "VulkanRHI/FVulkanSurface.h"
#include "VulkanRHI/FVulkanSwapchain.h"

#include <iostream>

namespace
{

using namespace Stoner;
using namespace Stoner::Backend::Vulkan;
using namespace Stoner::Core;
using namespace Stoner::RHI;

void Record(FOutputPresentationLifecycleTestResult& Result, bool bPassed,
    const char* Name)
{
    (bPassed ? ++Result.Passed : ++Result.Failed);
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

FRHIHDRMetadata MakeMetadata()
{
    FRHIHDRMetadata Metadata;
    Metadata.bPresent = true;
    Metadata.DisplayPrimaryRedX = 0.708f;
    Metadata.DisplayPrimaryRedY = 0.292f;
    Metadata.DisplayPrimaryGreenX = 0.170f;
    Metadata.DisplayPrimaryGreenY = 0.797f;
    Metadata.DisplayPrimaryBlueX = 0.131f;
    Metadata.DisplayPrimaryBlueY = 0.046f;
    Metadata.WhitePointX = 0.3127f;
    Metadata.WhitePointY = 0.3290f;
    Metadata.MasteringDisplayMinLuminanceNits = 0.005f;
    Metadata.MasteringDisplayMaxLuminanceNits = 1000.0f;
    Metadata.MaxContentLightLevelNits = 1000.0f;
    Metadata.MaxFrameAverageLightLevelNits = 400.0f;
    Metadata.CanonicalDigest = "lifecycle-hdr10-metadata-v1";
    return Metadata;
}

FRHISwapchainDesc MakeSdr(Core::uint64 Generation,
    Core::uint32 Width = 64, Core::uint32 Height = 64)
{
    FRHISwapchainDesc Request;
    Request.Width = Width;
    Request.Height = Height;
    Request.FramesInFlight = 2;
    Request.SurfaceCapabilityGeneration = Generation;
    return Request;
}

FRHISwapchainDesc MakePq(Core::uint64 Generation,
    Core::uint32 Width, Core::uint32 Height)
{
    FRHISwapchainDesc Request = MakeSdr(Generation, Width, Height);
    Request.PreferredFormat = ERHIFormat::R10G10B10A2_UNorm;
    Request.PreferredColorSpace =
        ERHIPresentationColorSpace::Hdr10St2084;
    Request.NativeEncoding = ERHIPresentationNativeEncoding::Pq;
    Request.ReferenceWhiteNits = 100.0f;
    Request.TargetPeakNits = 1000.0f;
    Request.bHasHDRMetadata = true;
    Request.HDRMetadata = MakeMetadata();
    return Request;
}

FRHISwapchainDesc MakeLinear(Core::uint64 Generation,
    Core::uint32 Width, Core::uint32 Height)
{
    FRHISwapchainDesc Request = MakeSdr(Generation, Width, Height);
    Request.PreferredFormat = ERHIFormat::R16G16B16A16_Float;
    Request.PreferredColorSpace =
        ERHIPresentationColorSpace::ExtendedSrgbLinear;
    Request.NativeEncoding = ERHIPresentationNativeEncoding::ScRgb80;
    Request.ReferenceWhiteNits = 80.0f;
    Request.TargetPeakNits = 1000.0f;
    return Request;
}

} // namespace

FOutputPresentationLifecycleTestResult
RunOutputPresentationLifecycleTests()
{
    FOutputPresentationLifecycleTestResult Result;
    auto Owner = std::make_shared<FVulkanPresentationOwnerState>();
    Owner->bActive = true;
    FRHIPresentationSurfaceDesc SurfaceDesc;
    SurfaceDesc.SurfaceId = 29;
    SurfaceDesc.Window = FPlatformWindow(reinterpret_cast<void*>(0x1));
    FVulkanSurface SurfaceValue;
    const ERHIResult SurfaceResult = FVulkanSurface::Create(
        SurfaceDesc, Owner, SurfaceValue);
    auto Surface = MakeShared<FVulkanSurface>(SurfaceValue);
    FRHIPresentationCapabilities Capabilities;
    Capabilities.SurfaceId = 29;
    Capabilities.CapabilityGeneration = 1;
    Capabilities.SupportedPairs = {
        {ERHIFormat::B8G8R8A8_UNorm,
            ERHIPresentationColorSpace::SrgbNonlinear},
        {ERHIFormat::R10G10B10A2_UNorm,
            ERHIPresentationColorSpace::Hdr10St2084},
        {ERHIFormat::R16G16B16A16_Float,
            ERHIPresentationColorSpace::ExtendedSrgbLinear}};
    Capabilities.bSupportsHDRMetadata = true;
    Capabilities.bSupportsExtendedRange = true;
    Capabilities.NativeReferenceWhiteNits = 80.0f;
    Capabilities.CurrentHeadroom = 12.5f;
    Capabilities.PotentialHeadroom = 25.0f;
    Capabilities.CapabilityDigest = "output-lifecycle-capabilities-v1";
    const ERHIResult CapabilityResult =
        Surface->UpdateCapabilities(Capabilities);
    auto Swapchain = MakeShared<FVulkanSwapchain>(
        Surface, MakeSdr(1), 3);
    Record(Result,
        SurfaceResult == ERHIResult::Success &&
            CapabilityResult == ERHIResult::Success &&
            Swapchain->GetState() == ERHISwapchainState::Ready,
        "presentation lifecycle fixture resolves exact initial state");

    bool bTransitions = true;
    Core::uint64 PreviousGeneration = Swapchain->GetGeneration();
    for (Core::uint32 Index = 0; Index < 100 && bTransitions; ++Index)
    {
        const Core::uint32 Width = 64 + Index;
        const Core::uint32 Height = 64 + (Index % 7);
        FRHISwapchainDesc Request;
        switch (Index % 3)
        {
        case 0: Request = MakeSdr(1, Width, Height); break;
        case 1: Request = MakePq(1, Width, Height); break;
        default: Request = MakeLinear(1, Width, Height); break;
        }
        FRHIPresentationFrame Frame;
        bTransitions = Swapchain->Reconfigure(Request) ==
                ERHIResult::Success &&
            Swapchain->GetGeneration() > PreviousGeneration &&
            Swapchain->AcquireNextFrame(1000 + Index, Frame) ==
                ERHIResult::Success &&
            Frame.FrameToken == 1000 + Index &&
            Frame.ModeGeneration == Swapchain->GetGeneration() &&
            Swapchain->Present(Frame) == ERHIResult::Success;
        PreviousGeneration = Swapchain->GetGeneration();
    }
    Record(Result, bTransitions,
        "presentation completes 100 exact profile and extent generations");

    FRHIPresentationFrame Stale;
    const bool bAcquiredStale =
        Swapchain->AcquireNextFrame(9001, Stale) == ERHIResult::Success;
    const ERHIResult BusyReconfigure =
        Swapchain->Reconfigure(MakeSdr(1, 90, 90));
    const bool bPresentedCurrent =
        Swapchain->Present(Stale) == ERHIResult::Success;
    const bool bReconfigured =
        Swapchain->Reconfigure(MakeSdr(1, 90, 90)) == ERHIResult::Success;
    Record(Result,
        bAcquiredStale && BusyReconfigure == ERHIResult::NotReady &&
            bPresentedCurrent && bReconfigured &&
            Swapchain->Present(Stale) == ERHIResult::InvalidState,
        "presentation rejects busy reconfigure and stale completed tokens");

    const FRHIResolvedPresentationState BeforeUnsupported =
        Swapchain->GetResolvedPresentationState();
    FRHISwapchainDesc Unsupported = MakeSdr(1, 91, 91);
    Unsupported.PreferredColorSpace =
        ERHIPresentationColorSpace::Hdr10St2084;
    Record(Result,
        Swapchain->Reconfigure(Unsupported) == ERHIResult::Unsupported &&
            Swapchain->GetResolvedPresentationState() == BeforeUnsupported,
        "unsupported mode transition fails without partial mutation");

    FRHISwapchainDesc Paused = MakeSdr(1);
    Paused.Width = 0;
    const ERHIResult PauseResult = Swapchain->Reconfigure(Paused);
    const bool bPaused = PauseResult == ERHIResult::NotReady &&
        Swapchain->GetState() == ERHISwapchainState::Paused;
    const bool bRestored =
        Swapchain->Reconfigure(MakeSdr(1, 96, 54)) ==
            ERHIResult::Success &&
        Swapchain->GetState() == ERHISwapchainState::Ready;
    Record(Result, bPaused && bRestored,
        "zero drawable pauses publication and restore creates fresh resources");

    bool bFailureRecovery = true;
    for (const EVulkanVisibleFrameFailurePoint FailurePoint : {
             EVulkanVisibleFrameFailurePoint::AcquireSuboptimal,
             EVulkanVisibleFrameFailurePoint::Record,
             EVulkanVisibleFrameFailurePoint::SubmitAfterFenceReset})
    {
        const FVulkanVisibleFrameFailureReport Failure =
            FVulkanNativeContext::RunVisibleFrameFailureLifecycleValidation(
                FailurePoint);
        bFailureRecovery = bFailureRecovery && Failure.bPassed &&
            Failure.InjectedFailure == FailurePoint &&
            Failure.FirstResult != ERHIResult::Success &&
            Failure.bAcquiredStateReleased &&
            Failure.bFenceReadyForReuse &&
            Failure.NextAcquireResult == ERHIResult::Success;
    }
    Record(Result, bFailureRecovery,
        "injected acquire record and submit failures preserve first result and recover ownership");

    const bool bInvalidated =
        Surface->Invalidate() == ERHIResult::Success &&
        Swapchain->GetState() == ERHISwapchainState::Unavailable;
    Owner->bActive = false;
    Record(Result, bInvalidated && !Surface->IsValid(),
        "presentation owner teardown leaves no valid surface generation");
    return Result;
}
