#include "RHIPresentationOutputTests.h"

#include "RHI/RHIMinimal.h"

#include <iostream>

namespace
{

using namespace Stoner::RHI;
using Stoner::Core::uint32;
using Stoner::Core::uint64;

void Record(FRHIPresentationOutputTestResult& Result, bool bPassed, const char* Name)
{
    if (bPassed)
    {
        ++Result.Passed;
        std::cout << "[PASS] " << Name << '\n';
    }
    else
    {
        ++Result.Failed;
        std::cout << "[FAIL] " << Name << '\n';
    }
}

class FSDROnlyPresentationSurface final : public IRHIPresentationSurface
{
public:
    FSDROnlyPresentationSurface()
    {
        Desc.SurfaceId = 29;
        Capabilities.SurfaceId = Desc.SurfaceId;
        Capabilities.CapabilityGeneration = 1;
        Capabilities.SupportedPairs.push_back({
            ERHIFormat::B8G8R8A8_UNorm,
            ERHIPresentationColorSpace::SrgbNonlinear});
        Capabilities.CapabilityDigest = "test-sdr-capability-digest";
    }

    [[nodiscard]] const FRHIPresentationSurfaceDesc& GetDesc() const noexcept override { return Desc; }
    [[nodiscard]] bool IsValid() const noexcept override { return bValid; }
    ERHIResult Invalidate() override
    {
        if (!bValid)
        {
            return ERHIResult::InvalidState;
        }
        bValid = false;
        ++Capabilities.CapabilityGeneration;
        return ERHIResult::Success;
    }
    ERHIResult QueryCapabilities(FRHIPresentationCapabilities& OutCapabilities) const override
    {
        if (!bValid)
        {
            return ERHIResult::InvalidState;
        }
        OutCapabilities = Capabilities;
        return ERHIResult::Success;
    }
    [[nodiscard]] uint64 GetCapabilityGeneration() const noexcept override
    {
        return Capabilities.CapabilityGeneration;
    }
    ERHIResult NotifyPresentationEnvironmentChanged() override
    {
        if (!bValid)
        {
            return ERHIResult::InvalidState;
        }
        ++Capabilities.CapabilityGeneration;
        Capabilities.CapabilityDigest = "test-sdr-capability-digest-v2";
        return ERHIResult::Success;
    }

private:
    FRHIPresentationSurfaceDesc Desc;
    FRHIPresentationCapabilities Capabilities;
    bool bValid = true;
};

class FSDROnlySwapchain final : public IRHISwapchain
{
public:
    using IRHISwapchain::AcquireNextFrame;
    using IRHISwapchain::Present;

    explicit FSDROnlySwapchain(FRHIPresentationCapabilities InCapabilities)
        : Capabilities(std::move(InCapabilities))
    {
    }

    ERHIResult Reconfigure(const FRHISwapchainDesc& Request) override
    {
        if (State == ERHISwapchainState::Acquired)
        {
            return ERHIResult::NotReady;
        }
        if (Request.IsZeroDrawable())
        {
            State = ERHISwapchainState::Paused;
            Resolved = {};
            return ERHIResult::NotReady;
        }
        if (!Request.IsValid() ||
            Request.SurfaceCapabilityGeneration != Capabilities.CapabilityGeneration ||
            !Capabilities.SupportsPair(Request.PreferredFormat, Request.PreferredColorSpace) ||
            Request.NativeEncoding != ERHIPresentationNativeEncoding::SdrExplicit ||
            Request.DisplayAdaptation !=
                ERHIPresentationDisplayAdaptation::None ||
            Request.bHasHDRMetadata)
        {
            return ERHIResult::Unsupported;
        }

        ++Generation;
        State = ERHISwapchainState::Ready;
        Resolved.ModeGeneration = Generation;
        Resolved.Width = Request.Width;
        Resolved.Height = Request.Height;
        Resolved.Format = Request.PreferredFormat;
        Resolved.ColorSpace = Request.PreferredColorSpace;
        Resolved.NativeEncoding = Request.NativeEncoding;
        Resolved.DisplayAdaptation = Request.DisplayAdaptation;
        Resolved.ReferenceWhiteNits = Request.ReferenceWhiteNits;
        Resolved.TargetPeakNits = Request.TargetPeakNits;
        Resolved.SwapchainImageGeneration = Generation;
        return ERHIResult::Success;
    }

    [[nodiscard]] ERHISwapchainState GetState() const noexcept override { return State; }
    [[nodiscard]] uint32 GetFrameCount() const noexcept override { return 2; }
    [[nodiscard]] uint32 GetCurrentFrameIndex() const noexcept override { return CurrentFrameIndex; }
    ERHIResult AcquireNextFrame(uint32& OutFrameIndex) override
    {
        if (State == ERHISwapchainState::Paused)
        {
            return ERHIResult::NotReady;
        }
        if (State != ERHISwapchainState::Ready || !Resolved.IsValid())
        {
            return ERHIResult::InvalidState;
        }
        OutFrameIndex = CurrentFrameIndex;
        AcquiredGeneration = Generation;
        State = ERHISwapchainState::Acquired;
        return ERHIResult::Success;
    }
    ERHIResult Present(uint32 FrameIndex) override
    {
        if (State != ERHISwapchainState::Acquired ||
            FrameIndex != CurrentFrameIndex ||
            AcquiredGeneration != Generation)
        {
            return ERHIResult::InvalidState;
        }
        CurrentFrameIndex = (CurrentFrameIndex + 1) % GetFrameCount();
        AcquiredGeneration = 0;
        State = ERHISwapchainState::Ready;
        return ERHIResult::Success;
    }
    [[nodiscard]] Stoner::Core::TSharedPtr<IRHITexture> GetImage(uint32) const override
    {
        ++ImageLookups;
        return nullptr;
    }
    [[nodiscard]] uint64 GetGeneration() const noexcept override { return Generation; }
    [[nodiscard]] const FRHIResolvedPresentationState& GetResolvedPresentationState() const noexcept override
    {
        return Resolved;
    }
    [[nodiscard]] uint32 GetImageLookupCount() const noexcept { return ImageLookups; }

private:
    FRHIPresentationCapabilities Capabilities;
    FRHIResolvedPresentationState Resolved;
    ERHISwapchainState State = ERHISwapchainState::Unavailable;
    uint64 Generation = 0;
    uint64 AcquiredGeneration = 0;
    uint32 CurrentFrameIndex = 0;
    mutable uint32 ImageLookups = 0;
};

void TestCapabilityAndMetadataContracts(FRHIPresentationOutputTestResult& Result)
{
    FSDROnlyPresentationSurface Surface;
    FRHIPresentationCapabilities Capabilities;
    Record(Result,
        Surface.QueryCapabilities(Capabilities) == ERHIResult::Success &&
            Capabilities.IsValid() && Capabilities.HasUniqueSupportedPairs(),
        "RHI presentation capability snapshot is valid and exact-pair unique");
    Record(Result,
        Capabilities.SupportsPair(ERHIFormat::B8G8R8A8_UNorm, ERHIPresentationColorSpace::SrgbNonlinear) &&
            !Capabilities.SupportsPair(ERHIFormat::B8G8R8A8_UNorm, ERHIPresentationColorSpace::Hdr10St2084) &&
            !Capabilities.SupportsPair(ERHIFormat::R10G10B10A2_UNorm, ERHIPresentationColorSpace::SrgbNonlinear),
        "RHI presentation support is an exact format and color-space pair");

    FRHIPresentationCapabilities Duplicate = Capabilities;
    Duplicate.SupportedPairs.push_back(Duplicate.SupportedPairs.front());
    Record(Result, !Duplicate.IsValid() && !Duplicate.HasUniqueSupportedPairs(),
        "RHI presentation capabilities reject duplicate exact pairs");

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
    Metadata.CanonicalDigest = "test-hdr-metadata-digest";
    Record(Result, Metadata.IsValid(), "RHI HDR metadata accepts a versioned bounded payload");
    Metadata.Version = 0;
    Record(Result, !Metadata.IsValid(), "RHI HDR metadata rejects an unversioned payload");
}

void TestGenerationAndFailClosedResolution(FRHIPresentationOutputTestResult& Result)
{
    FSDROnlyPresentationSurface Surface;
    FRHIPresentationCapabilities Capabilities;
    (void)Surface.QueryCapabilities(Capabilities);
    FSDROnlySwapchain Swapchain(Capabilities);

    const FRHISwapchainDesc DefaultRequest;
    Record(Result,
        DefaultRequest.PreferredFormat == ERHIFormat::B8G8R8A8_UNorm &&
            DefaultRequest.PreferredColorSpace ==
                ERHIPresentationColorSpace::SrgbNonlinear &&
            DefaultRequest.NativeEncoding ==
                ERHIPresentationNativeEncoding::SdrExplicit &&
            DefaultRequest.DisplayAdaptation ==
                ERHIPresentationDisplayAdaptation::None &&
            DefaultRequest.ReferenceWhiteNits == 100.0f &&
            DefaultRequest.TargetPeakNits == 100.0f &&
            !DefaultRequest.bHasHDRMetadata &&
            DefaultRequest.HDRMetadata.IsValid(),
        "RHI swapchain request defaults are deterministic SDR-only policy");

    FRHISwapchainDesc Request;
    Request.Width = 640;
    Request.Height = 360;
    Request.PreferredFormat = ERHIFormat::B8G8R8A8_UNorm;
    Request.PreferredColorSpace = ERHIPresentationColorSpace::SrgbNonlinear;
    Request.NativeEncoding = ERHIPresentationNativeEncoding::SdrExplicit;
    Request.ReferenceWhiteNits = 100.0f;
    Request.TargetPeakNits = 100.0f;
    Request.SurfaceCapabilityGeneration = Capabilities.CapabilityGeneration;

    uint32 UnsupportedFrame = 0;
    Record(Result,
        Swapchain.AcquireNextFrame(UnsupportedFrame) == ERHIResult::InvalidState &&
            Swapchain.Present(0) == ERHIResult::InvalidState,
        "RHI presentation rejects acquire and present before exact resolution");

    Record(Result,
        Swapchain.Reconfigure(Request) == ERHIResult::Success &&
            Swapchain.GetResolvedPresentationState().IsValid(),
        "RHI swapchain resolves one exact supported presentation request");
    const uint64 FirstModeGeneration =
        Swapchain.GetResolvedPresentationState().ModeGeneration;
    Record(Result,
        Swapchain.GetResolvedPresentationState().Width == Request.Width &&
            Swapchain.GetResolvedPresentationState().Height == Request.Height &&
            Swapchain.GetResolvedPresentationState().Format ==
                Request.PreferredFormat &&
            Swapchain.GetResolvedPresentationState().ColorSpace ==
                Request.PreferredColorSpace &&
            Swapchain.GetResolvedPresentationState().DisplayAdaptation ==
                ERHIPresentationDisplayAdaptation::None,
        "RHI resolved presentation state preserves exact requested pixels and pair");
    const FRHIResolvedPresentationState ResolvedBeforeFailure =
        Swapchain.GetResolvedPresentationState();

    Request.PreferredColorSpace = ERHIPresentationColorSpace::Hdr10St2084;
    Request.PreferredFormat = ERHIFormat::R10G10B10A2_UNorm;
    Request.NativeEncoding = ERHIPresentationNativeEncoding::Pq;
    Request.bHasHDRMetadata = true;
    Record(Result,
        Swapchain.Reconfigure(Request) == ERHIResult::Unsupported &&
            Swapchain.GetResolvedPresentationState() == ResolvedBeforeFailure,
        "RHI swapchain rejects unsupported HDR without fallback or partial mutation");

    Request.PreferredFormat = ERHIFormat::B8G8R8A8_UNorm;
    Request.PreferredColorSpace = ERHIPresentationColorSpace::SrgbNonlinear;
    Request.NativeEncoding = ERHIPresentationNativeEncoding::SdrExplicit;
    Request.bHasHDRMetadata = false;
    FRHIPresentationFrame Acquired;
    Record(Result,
        Swapchain.AcquireNextFrame(73, Acquired) == ERHIResult::Success &&
            Acquired.IsValid() && Acquired.FrameToken == 73 &&
            Acquired.Matches(ResolvedBeforeFailure),
        "RHI acquire issues an immutable same-frame presentation ticket");
    const FRHIResolvedPresentationState WhileAcquired =
        Swapchain.GetResolvedPresentationState();
    Request.Width = 800;
    Record(Result,
        Swapchain.Reconfigure(Request) == ERHIResult::NotReady &&
            Swapchain.GetResolvedPresentationState() == WhileAcquired,
        "RHI reconfigure is transactional while an image remains acquired");
    FRHIPresentationFrame StaleTicket = Acquired;
    ++StaleTicket.ModeGeneration;
    Record(Result,
        Swapchain.Present(StaleTicket) == ERHIResult::InvalidState &&
            Swapchain.GetState() == ERHISwapchainState::Acquired,
        "RHI present rejects a stale mode-generation ticket before mutation");
    Record(Result,
        Swapchain.Present(Acquired) == ERHIResult::Success &&
            Swapchain.GetState() == ERHISwapchainState::Ready,
        "RHI present consumes the exact acquired frame ticket once");
    Record(Result,
        Swapchain.Present(Acquired) == ERHIResult::InvalidState,
        "RHI presentation ticket cannot complete twice");

    Request.Width = 0;
    Request.Height = 0;
    Record(Result,
        Swapchain.Reconfigure(Request) == ERHIResult::NotReady &&
            Swapchain.GetState() == ERHISwapchainState::Paused &&
            !Swapchain.GetResolvedPresentationState().IsValid(),
        "RHI zero drawable enters paused state without a formal image");

    Request.Width = 640;
    Request.Height = 360;
    Request.PreferredFormat = ERHIFormat::B8G8R8A8_UNorm;
    Request.PreferredColorSpace = ERHIPresentationColorSpace::SrgbNonlinear;
    Request.NativeEncoding = ERHIPresentationNativeEncoding::SdrExplicit;
    Request.bHasHDRMetadata = false;
    Request.SurfaceCapabilityGeneration = Capabilities.CapabilityGeneration;
    Record(Result, Swapchain.Reconfigure(Request) == ERHIResult::Success,
        "RHI swapchain restores a new nonzero mode generation");
    Record(Result,
        Swapchain.GetResolvedPresentationState().ModeGeneration >
            FirstModeGeneration &&
            Swapchain.GetResolvedPresentationState().Width == 640 &&
            Swapchain.GetResolvedPresentationState().Height == 360,
        "RHI restore uses a fresh exact nonzero generation");

    FRHIPresentationFrame Restored;
    Record(Result,
        Swapchain.AcquireNextFrame(74, Restored) == ERHIResult::Success &&
            Restored.FrameToken == 74 && Restored.ModeGeneration !=
                Acquired.ModeGeneration &&
            Swapchain.Present(Acquired) == ERHIResult::InvalidState &&
            Swapchain.Present(Restored) == ERHIResult::Success,
        "RHI stale acquire ticket cannot present after pause and restore");

    const uint64 CurrentGeneration = Swapchain.GetGeneration();
    const uint32 LookupsBefore = Swapchain.GetImageLookupCount();
    Record(Result,
        !Swapchain.GetImageForGeneration(0, CurrentGeneration - 1) &&
            Swapchain.GetImageLookupCount() == LookupsBefore,
        "RHI swapchain rejects stale image generation before lookup");
    (void)Swapchain.GetImageForGeneration(0, CurrentGeneration);
    Record(Result, Swapchain.GetImageLookupCount() == LookupsBefore + 1,
        "RHI swapchain accepts current generation for image lookup");

    const uint64 SurfaceGeneration = Surface.GetCapabilityGeneration();
    Record(Result,
        Surface.NotifyPresentationEnvironmentChanged() == ERHIResult::Success &&
            Surface.GetCapabilityGeneration() == SurfaceGeneration + 1,
        "RHI surface capability generation advances monotonically");
}

} // namespace

FRHIPresentationOutputTestResult RunRHIPresentationOutputTests()
{
    FRHIPresentationOutputTestResult Result;
    TestCapabilityAndMetadataContracts(Result);
    TestGenerationAndFailClosedResolution(Result);
    return Result;
}
