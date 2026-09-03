#include "MetalOutputTransformNativeTests.h"

#include "Core/SGPlatform.h"
#include "RHI/RHIMinimal.h"

#if SG_PLATFORM_MAC
#include "Application/FWindow.h"
#include "Application/FWindowDesc.h"
#include "FMetalDevice.h"
#include "FMetalFormat.h"
#include "FMetalPresentationContext.h"
#include "FMetalPresentationSurface.h"
#include "MetalRHI/FMetalDeviceFactory.h"
#endif

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace
{

void Record(FMetalOutputTransformNativeTestResult& Result, bool bPassed,
    const char* Name)
{
    (bPassed ? ++Result.Passed : ++Result.Failed);
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

#if SG_PLATFORM_MAC
// MTLPixelFormatBGR10A2Unorm is the stable native enum value 94. Keep this
// test independent of Objective-C Foundation headers because this suite is C++.
constexpr Stoner::Core::uint64 kMetalPixelFormatBGR10A2Unorm = 94;

bool NativePresentationRequested()
{
    const char* Value = std::getenv(
        "STONER_REQUIRE_METAL_OUTPUT_PRESENTATION");
    return Value && std::string_view(Value) == "1";
}

enum class ENativeProbeOutcome
{
    Passed,
    Unsupported,
    Failed
};

Stoner::RHI::FRHISwapchainDesc MakeSdrRequest()
{
    Stoner::RHI::FRHISwapchainDesc Request;
    Request.Width = 512;
    Request.Height = 512;
    Request.FramesInFlight = 2;
    Request.SurfaceCapabilityGeneration = 7;
    return Request;
}

Stoner::RHI::FRHIHDRMetadata MakeHdr10Metadata(float PeakNits)
{
    Stoner::RHI::FRHIHDRMetadata Metadata;
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
    Metadata.MasteringDisplayMaxLuminanceNits = PeakNits;
    Metadata.MaxContentLightLevelNits = PeakNits;
    Metadata.MaxFrameAverageLightLevelNits = PeakNits * 0.4f;
    Metadata.CanonicalDigest = "valid-hdr10-static-metadata";
    return Metadata;
}

ENativeProbeOutcome ProbeNativePresentationMode(
    Stoner::RHI::FRHISwapchainDesc Request,
    const Stoner::Backend::Metal::Private::FMetalPresentationLayerPolicy&
        ExpectedPolicy)
{
    using namespace Stoner;
    using namespace Stoner::Application;
    using namespace Stoner::Backend::Metal;
    using namespace Stoner::Backend::Metal::Private;
    using namespace Stoner::RHI;

    FWindow Window;
    FWindowDesc WindowDesc;
    WindowDesc.Title = "Stoner Metal Output Non-Visual Probe";
    WindowDesc.ClientWidth = 96;
    WindowDesc.ClientHeight = 64;
    WindowDesc.bVisible = false;
    if (Window.CreateRealWindow(WindowDesc) != EApplicationResult::Success)
        return ENativeProbeOutcome::Failed;

    auto Created = CreateMetalDevice();
    if (!Created.Succeeded())
    {
        (void)Window.Destroy();
        return ENativeProbeOutcome::Failed;
    }
    auto NativeDevice =
        std::dynamic_pointer_cast<FMetalDevice>(Created.Device);
    FRHIPresentationSurfaceDesc SurfaceDesc;
    SurfaceDesc.Window = Window.GetPlatformWindow();
    auto Surface = Created.Device->CreatePresentationSurface(SurfaceDesc);
    FRHIPresentationCapabilities Capabilities;
    const ERHIResult CapabilityResult = Surface.Succeeded()
        ? Surface.Object->QueryCapabilities(Capabilities)
        : Surface.Result;
    if (CapabilityResult != ERHIResult::Success || !NativeDevice)
    {
        if (Surface.Succeeded()) (void)Surface.Object->Invalidate();
        (void)Created.Device->Shutdown();
        (void)Window.Destroy();
        return ENativeProbeOutcome::Failed;
    }
    if (!Capabilities.SupportsPair(
            Request.PreferredFormat, Request.PreferredColorSpace))
    {
        (void)Surface.Object->Invalidate();
        (void)Created.Device->Shutdown();
        (void)Window.Destroy();
        return ENativeProbeOutcome::Unsupported;
    }

    Request.Width = Window.GetDrawableWidth();
    Request.Height = Window.GetDrawableHeight();
    Request.SurfaceCapabilityGeneration =
        Capabilities.CapabilityGeneration;
    auto Swapchain = Created.Device->CreateSwapchain(Surface.Object, Request);
    auto NativeSurface =
        std::dynamic_pointer_cast<FMetalPresentationSurface>(Surface.Object);
    if (!Swapchain.Succeeded() || !NativeSurface ||
        Request.Width == 0 || Request.Height == 0)
    {
        if (Surface.Succeeded()) (void)Surface.Object->Invalidate();
        (void)Created.Device->Shutdown();
        (void)Window.Destroy();
        return ENativeProbeOutcome::Failed;
    }

    const auto Resolved = Swapchain.Object->GetResolvedPresentationState();
    const auto Before = NativeSurface->GetContext()->GetLayerSnapshot();
    const bool bExactState = Resolved.IsValid() &&
        Resolved.Width == Request.Width &&
        Resolved.Height == Request.Height &&
        Resolved.Format == Request.PreferredFormat &&
        Resolved.ColorSpace == Request.PreferredColorSpace &&
        Resolved.NativeEncoding == Request.NativeEncoding &&
        Resolved.DisplayAdaptation == Request.DisplayAdaptation &&
        Before.Policy.PixelFormat == ExpectedPolicy.PixelFormat &&
        Before.Policy.ColorSpace == ExpectedPolicy.ColorSpace &&
        Before.Policy.DisplayAdaptation == ExpectedPolicy.DisplayAdaptation &&
        Before.Policy.bWantsExtendedDynamicRangeContent ==
            ExpectedPolicy.bWantsExtendedDynamicRangeContent &&
        !Before.Policy.bHasEDRMetadata && Before.MetadataDigest.IsEmpty();

    constexpr Core::uint64 FrameToken = 0x02900001u;
    FRHIPresentationFrame Frame;
    const ERHIResult AcquireResult =
        Swapchain.Object->AcquireNextFrame(FrameToken, Frame);
    const auto Acquired =
        NativeSurface->GetContext()->GetLayerSnapshot();
    auto Image = AcquireResult == ERHIResult::Success
        ? Swapchain.Object->GetImage(Frame.ImageIndex) : nullptr;

    FRHIRenderPassDesc PassDesc;
    PassDesc.Attachments.push_back({
        ERHIAttachmentRole::Color, Request.PreferredFormat,
        ERHISampleCount::One, ERHIAttachmentLoadOp::Clear,
        ERHIAttachmentStoreOp::Store});
    auto Pass = Image
        ? Created.Device->CreateRenderPass(PassDesc)
        : TRHIObjectResult<IRHIRenderPass>{};
    FRHIFramebufferDesc FramebufferDesc;
    FramebufferDesc.RenderPass = Pass.Object;
    FramebufferDesc.Attachments.push_back({Image, 0, 0});
    FramebufferDesc.Width = Frame.Width;
    FramebufferDesc.Height = Frame.Height;
    auto Framebuffer = Pass.Succeeded()
        ? Created.Device->CreateFramebuffer(FramebufferDesc)
        : TRHIObjectResult<IRHIFramebuffer>{};
    auto Queue = Created.Device->CreateCommandQueue(ERHIQueueType::Graphics);
    auto Commands = Created.Device->CreateCommandBuffer(ERHIQueueType::Graphics);
    FRHIRenderPassClearValues ClearValues;
    ClearValues.Colors.push_back({0.125f, 0.25f, 0.5f, 1.0f});
    bool bSubmitted = Framebuffer.Succeeded() && Queue.Succeeded() &&
        Commands.Succeeded() &&
        Commands.Object->Begin() == ERHIResult::Success &&
        Commands.Object->BeginRenderPass(
            Pass.Object, Framebuffer.Object, ClearValues) ==
            ERHIResult::Success &&
        Commands.Object->EndRenderPass() == ERHIResult::Success &&
        Commands.Object->End() == ERHIResult::Success &&
        Queue.Object->Submit(Commands.Object) == ERHIResult::Success &&
        Queue.Object->WaitIdle() == ERHIResult::Success;

    FRHITextureFootprint Footprint;
    const bool bFootprint = Image && TryGetRHITextureFootprint(
        Image->GetFormat(), Frame.Width, Frame.Height, 1, Footprint);
    Core::TArray<Core::uint8> Readback;
    const bool bReadback = bSubmitted && bFootprint &&
        NativeDevice->ReadbackTextureForTesting(Image, Readback) ==
            ERHIResult::Success &&
        Readback.size() == Footprint.TotalBytes;
    const ERHIResult PresentResult = bReadback
        ? Swapchain.Object->Present(Frame)
        : ERHIResult::InvalidState;
    const auto After = NativeSurface->GetContext()->GetLayerSnapshot();
    const bool bProvenance = Frame.Matches(Resolved) &&
        Acquired.LastAcquiredFrameToken == FrameToken &&
        PresentResult == ERHIResult::Success &&
        After.LastSubmittedFrameToken == FrameToken &&
        After.LastPresentedFrameToken == FrameToken &&
        Swapchain.Object->Present(Frame) == ERHIResult::InvalidState;

    const ERHIResult SurfaceResult = Surface.Object->Invalidate();
    const ERHIResult ShutdownResult = Created.Device->Shutdown();
    const EApplicationResult DestroyResult = Window.Destroy();
    const bool bPassed = bExactState && bReadback && bProvenance &&
            SurfaceResult == ERHIResult::Success &&
            ShutdownResult == ERHIResult::Success &&
            DestroyResult == EApplicationResult::Success;
    if (!bPassed)
    {
        std::cout << "[INFO] metal-output-native"
                  << " encoding=" << static_cast<int>(Request.NativeEncoding)
                  << " exact=" << bExactState
                  << " acquire=" << static_cast<int>(AcquireResult)
                  << " submitted=" << bSubmitted
                  << " footprint=" << bFootprint
                  << " readback=" << bReadback
                  << " bytes=" << Readback.size()
                  << " present=" << static_cast<int>(PresentResult)
                  << " provenance=" << bProvenance
                  << " surface=" << static_cast<int>(SurfaceResult)
                  << " shutdown=" << static_cast<int>(ShutdownResult)
                  << " destroy=" << static_cast<int>(DestroyResult)
                  << '\n';
    }
    return bPassed
        ? ENativeProbeOutcome::Passed
        : ENativeProbeOutcome::Failed;
}

#endif

} // namespace

FMetalOutputTransformNativeTestResult
RunMetalOutputTransformNativeTests()
{
    FMetalOutputTransformNativeTestResult Result;
#if SG_PLATFORM_MAC
    using namespace Stoner::Backend::Metal::Private;
    using namespace Stoner::RHI;

    const FRHISwapchainDesc Sdr = MakeSdrRequest();
    const FMetalPresentationLayerPolicy SdrPolicy =
        ResolveMetalPresentationLayerPolicy(Sdr);
    Record(Result,
        SdrPolicy.IsValid() &&
            SdrPolicy.PixelFormat ==
                ToMetalPixelFormat(ERHIFormat::B8G8R8A8_UNorm) &&
            SdrPolicy.ColorSpace ==
                ERHIPresentationColorSpace::SrgbNonlinear &&
            SdrPolicy.DisplayAdaptation ==
                ERHIPresentationDisplayAdaptation::None &&
            !SdrPolicy.bWantsExtendedDynamicRangeContent &&
            !SdrPolicy.bHasEDRMetadata,
        "Metal SDR uses explicit sRGB layer state without EDR adaptation");

    FRHISwapchainDesc Pq = MakeSdrRequest();
    Pq.PreferredFormat = ERHIFormat::R10G10B10A2_UNorm;
    Pq.PreferredColorSpace = ERHIPresentationColorSpace::Hdr10St2084;
    Pq.NativeEncoding = ERHIPresentationNativeEncoding::Pq;
    Pq.DisplayAdaptation =
        ERHIPresentationDisplayAdaptation::SystemColorManagement;
    Pq.ReferenceWhiteNits = 100.0f;
    Pq.TargetPeakNits = 1000.0f;
    const FMetalPresentationLayerPolicy PqPolicy =
        ResolveMetalPresentationLayerPolicy(Pq);
    Record(Result,
        PqPolicy.IsValid() &&
            PqPolicy.PixelFormat ==
                kMetalPixelFormatBGR10A2Unorm &&
            PqPolicy.ColorSpace ==
                ERHIPresentationColorSpace::Hdr10St2084 &&
            PqPolicy.DisplayAdaptation ==
                ERHIPresentationDisplayAdaptation::SystemColorManagement &&
            PqPolicy.bWantsExtendedDynamicRangeContent &&
            !PqPolicy.bHasEDRMetadata,
        "Metal PQ uses BGR10A2/PQ with ColorSync and EDRMetadata nil");

    FRHISwapchainDesc PqWithNativeMetadata = Pq;
    PqWithNativeMetadata.bHasHDRMetadata = true;
    PqWithNativeMetadata.HDRMetadata = MakeHdr10Metadata(1000.0f);
    Record(Result,
        !ResolveMetalPresentationLayerPolicy(PqWithNativeMetadata).IsValid(),
        "Metal PQ rejects native CAEDRMetadata system tone mapping");

    FRHISwapchainDesc PqWithoutAdaptation = Pq;
    PqWithoutAdaptation.DisplayAdaptation =
        ERHIPresentationDisplayAdaptation::None;
    Record(Result,
        !ResolveMetalPresentationLayerPolicy(PqWithoutAdaptation).IsValid(),
        "Metal PQ requires declared Core Animation color management");

    FRHISwapchainDesc Edr = MakeSdrRequest();
    Edr.PreferredFormat = ERHIFormat::R16G16B16A16_Float;
    Edr.PreferredColorSpace =
        ERHIPresentationColorSpace::ExtendedSrgbLinear;
    Edr.NativeEncoding = ERHIPresentationNativeEncoding::MetalEdr;
    Edr.ReferenceWhiteNits = 100.0f;
    Edr.TargetPeakNits = 1000.0f;
    const FMetalPresentationLayerPolicy EdrPolicy =
        ResolveMetalPresentationLayerPolicy(Edr);
    Record(Result,
        EdrPolicy.IsValid() &&
            EdrPolicy.PixelFormat ==
                ToMetalPixelFormat(ERHIFormat::R16G16B16A16_Float) &&
            EdrPolicy.ColorSpace ==
                ERHIPresentationColorSpace::ExtendedSrgbLinear &&
            EdrPolicy.DisplayAdaptation ==
                ERHIPresentationDisplayAdaptation::None &&
            EdrPolicy.bWantsExtendedDynamicRangeContent &&
            !EdrPolicy.bHasEDRMetadata,
        "Metal EDR uses FP16 extended-linear output with EDRMetadata nil");

    FRHISwapchainDesc EdrWithMetadata = Edr;
    EdrWithMetadata.bHasHDRMetadata = true;
    EdrWithMetadata.HDRMetadata = MakeHdr10Metadata(1000.0f);
    Record(Result,
        !ResolveMetalPresentationLayerPolicy(EdrWithMetadata).IsValid(),
        "Metal EDR rejects metadata that would enable system tone mapping");

    FRHISwapchainDesc EdrWithAdaptation = Edr;
    EdrWithAdaptation.DisplayAdaptation =
        ERHIPresentationDisplayAdaptation::SystemColorManagement;
    Record(Result,
        !ResolveMetalPresentationLayerPolicy(EdrWithAdaptation).IsValid(),
        "Metal EDR rejects system display adaptation");

    if (!NativePresentationRequested())
    {
        Record(Result, true,
            "Metal native output readback/present probe is explicit opt-in");
    }
    else
    {
        FRHISwapchainDesc NativeSdr = Sdr;
        NativeSdr.SurfaceCapabilityGeneration = 1;
        const auto NativeSdrOutcome = ProbeNativePresentationMode(
            NativeSdr, ResolveMetalPresentationLayerPolicy(NativeSdr));
        Record(Result, NativeSdrOutcome == ENativeProbeOutcome::Passed,
            "Metal SDR native layer, exact readback, and same-frame present pass");

        FRHISwapchainDesc NativePq = Pq;
        NativePq.SurfaceCapabilityGeneration = 1;
        const auto NativePqOutcome = ProbeNativePresentationMode(
            NativePq, ResolveMetalPresentationLayerPolicy(NativePq));
        Record(Result, NativePqOutcome != ENativeProbeOutcome::Failed,
            NativePqOutcome == ENativeProbeOutcome::Unsupported
                ? "Metal PQ native probe reports unsupported display without fallback"
                : "Metal PQ native non-visual state/readback/present pass; visual review remains manual");

        FRHISwapchainDesc NativeEdr = Edr;
        NativeEdr.SurfaceCapabilityGeneration = 1;
        const auto NativeEdrOutcome = ProbeNativePresentationMode(
            NativeEdr, ResolveMetalPresentationLayerPolicy(NativeEdr));
        Record(Result, NativeEdrOutcome != ENativeProbeOutcome::Failed,
            NativeEdrOutcome == ENativeProbeOutcome::Unsupported
                ? "Metal EDR native probe reports unsupported display without fallback"
                : "Metal EDR native non-visual state/readback/present pass; visual review remains manual");
    }
#else
    Record(Result, true,
        "Metal output-transform native policy is isolated from non-macOS builds");
#endif
    return Result;
}
