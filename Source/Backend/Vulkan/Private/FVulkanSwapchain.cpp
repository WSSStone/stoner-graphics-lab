#include "VulkanRHI/FVulkanSwapchain.h"

#include "VulkanRHI/FVulkanNativeContext.h"

#include <algorithm>

namespace Stoner::Backend::Vulkan
{

namespace
{

class FVulkanSwapchainImage final : public Stoner::RHI::IRHITexture
{
public:
    explicit FVulkanSwapchainImage(const Stoner::RHI::FRHITextureDesc& InDesc)
        : Desc(InDesc)
    {
    }

    [[nodiscard]] const Stoner::RHI::FRHITextureDesc& GetDesc() const noexcept override
    {
        return Desc;
    }

    [[nodiscard]] Stoner::RHI::ERHITextureDimension GetDimension() const noexcept override
    {
        return Desc.Dimension;
    }

    [[nodiscard]] Stoner::RHI::ERHIFormat GetFormat() const noexcept override
    {
        return Desc.Format;
    }

    [[nodiscard]] Stoner::RHI::ERHITextureUsage GetUsage() const noexcept override
    {
        return Desc.Usage;
    }

    [[nodiscard]] Stoner::RHI::ERHIResourceLifecycleState GetLifecycleState() const noexcept override
    {
        return LifecycleState;
    }

    Stoner::RHI::ERHIResult Invalidate() override
    {
        if (LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Invalidated)
        {
            return Stoner::RHI::ERHIResult::InvalidState;
        }
        LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Invalidated;
        return Stoner::RHI::ERHIResult::Success;
    }

private:
    Stoner::RHI::FRHITextureDesc Desc;
    Stoner::RHI::ERHIResourceLifecycleState LifecycleState =
        Stoner::RHI::ERHIResourceLifecycleState::Valid;
};

[[nodiscard]] Stoner::RHI::FRHITextureDesc MakeSwapchainImageDesc(
    const Stoner::RHI::FRHISwapchainDesc& Desc)
{
    Stoner::RHI::FRHITextureDesc ImageDesc;
    ImageDesc.Width = Desc.Width;
    ImageDesc.Height = Desc.Height;
    ImageDesc.Format = Desc.PreferredFormat;
    ImageDesc.Usage = Stoner::RHI::ERHITextureUsage::ColorAttachment |
        Stoner::RHI::ERHITextureUsage::Present |
        Stoner::RHI::ERHITextureUsage::CopySource |
        Stoner::RHI::ERHITextureUsage::CopyDestination;
    return ImageDesc;
}

[[nodiscard]] bool IsEncodingPairCompatible(
    const Stoner::RHI::FRHISwapchainDesc& Desc) noexcept
{
    using namespace Stoner::RHI;
    switch (Desc.NativeEncoding)
    {
    case ERHIPresentationNativeEncoding::SdrExplicit:
        return (Desc.PreferredFormat == ERHIFormat::B8G8R8A8_UNorm ||
                Desc.PreferredFormat == ERHIFormat::R8G8B8A8_UNorm) &&
            Desc.PreferredColorSpace !=
                ERHIPresentationColorSpace::Hdr10St2084 &&
            Desc.PreferredColorSpace !=
                ERHIPresentationColorSpace::ExtendedSrgbLinear &&
            !Desc.bHasHDRMetadata &&
            Desc.DisplayAdaptation ==
                ERHIPresentationDisplayAdaptation::None;
    case ERHIPresentationNativeEncoding::Pq:
        return Desc.PreferredFormat == ERHIFormat::R10G10B10A2_UNorm &&
            Desc.PreferredColorSpace ==
                ERHIPresentationColorSpace::Hdr10St2084 &&
            Desc.bHasHDRMetadata &&
            Desc.DisplayAdaptation ==
                ERHIPresentationDisplayAdaptation::None;
    case ERHIPresentationNativeEncoding::ScRgb80:
        return Desc.PreferredFormat == ERHIFormat::R16G16B16A16_Float &&
            Desc.PreferredColorSpace ==
                ERHIPresentationColorSpace::ExtendedSrgbLinear &&
            !Desc.bHasHDRMetadata &&
            Desc.DisplayAdaptation ==
                ERHIPresentationDisplayAdaptation::None;
    case ERHIPresentationNativeEncoding::MetalEdr:
    case ERHIPresentationNativeEncoding::Unknown:
        return false;
    }
    return false;
}

} // namespace

FVulkanSwapchain::FVulkanSwapchain(
    Stoner::Core::uint32 InFrameCount,
    Stoner::Core::uint32 InMaxFrameCount)
    : FrameCount(InFrameCount)
    , MaxFrameCount(InMaxFrameCount)
    , bValid(InFrameCount > 0 && InMaxFrameCount > 0 && InFrameCount <= InMaxFrameCount)
{
    if (!bValid)
    {
        State = Stoner::RHI::ERHISwapchainState::Unavailable;
    }
}

FVulkanSwapchain::FVulkanSwapchain(
    Stoner::Core::TSharedPtr<FVulkanSurface> InSurface,
    const Stoner::RHI::FRHISwapchainDesc& InDesc,
    Stoner::Core::uint32 InMaxFrameCount)
    : FrameCount(InDesc.FramesInFlight)
    , MaxFrameCount(InMaxFrameCount)
    , State(Stoner::RHI::ERHISwapchainState::Unavailable)
    , Generation(0)
    , Surface(std::move(InSurface))
    , Desc(InDesc)
    , bValid(Surface && Surface->IsValid() && InMaxFrameCount > 0)
{
    if (!bValid || Reconfigure(InDesc) != Stoner::RHI::ERHIResult::Success)
    {
        bValid = false;
        State = Stoner::RHI::ERHISwapchainState::Unavailable;
    }
}

Stoner::RHI::ERHIResult FVulkanSwapchain::Reconfigure(
    const Stoner::RHI::FRHISwapchainDesc& Request)
{
    using namespace Stoner::RHI;
    if (!bValid || !Surface || !Surface->IsValid())
    {
        return ERHIResult::InvalidState;
    }
    if (State == ERHISwapchainState::Acquired)
    {
        return ERHIResult::NotReady;
    }
    const auto NativeContext = Surface->GetNativeContext();
    if (NativeContext && NativeContext->IsLabPresentationActive())
    {
        FRHIResolvedPresentationState LabResolvedState;
        const ERHIResult LabResult = NativeContext->ConfigureLabPresentation(
            Request, LabResolvedState);
        if (Request.IsZeroDrawable())
        {
            // ConfigureLabPresentation keeps the native runtime owners alive
            // while it pauses. Clear only this wrapper's borrowed-image view;
            // the Context remains responsible for native retirement.
            if (LabResult != ERHIResult::NotReady)
            {
                return LabResult;
            }
            for (FLabBorrowedImage& Image : LabBorrowedImages)
            {
                Image = {};
            }
            Images.clear();
            Desc = Request;
            CurrentFrameIndex = 0;
            AcquiredGeneration = 0;
            AcquiredFrameToken = 0;
            NativeFrameBindings = {};
            ResolvedState = {};
            State = ERHISwapchainState::Paused;
            bLabPresentation = true;
            return ERHIResult::NotReady;
        }
        if (LabResult != ERHIResult::Success)
        {
            // A pending replacement leaves the currently published wrapper
            // generation untouched. The Context owns the pending request.
            return LabResult;
        }
        if (!LabResolvedState.IsValid() ||
            LabResolvedState.ModeGeneration == 0 ||
            LabResolvedState.SwapchainImageGeneration == 0)
        {
            return ERHIResult::Failed;
        }
        const Stoner::Core::uint32 NativeImageCount =
            NativeContext->GetVisiblePresentationImageCount();
        if (NativeImageCount == 0 ||
            NativeImageCount > Stoner::RHI::MaxRHIPresentationImageLeases)
        {
            return ERHIResult::Failed;
        }
        for (FLabBorrowedImage& Image : LabBorrowedImages)
        {
            Image = {};
        }
        Images.clear();
        Desc = Request;
        FrameCount = NativeImageCount;
        CurrentFrameIndex = 0;
        AcquiredGeneration = 0;
        AcquiredFrameToken = 0;
        NativeFrameBindings = {};
        Generation = LabResolvedState.ModeGeneration;
        ResolvedState = std::move(LabResolvedState);
        State = ERHISwapchainState::Ready;
        bLabPresentation = true;
        return ERHIResult::Success;
    }
    if (Request.IsZeroDrawable())
    {
        InvalidateImages();
        Images.clear();
        Desc = Request;
        FrameCount = Request.FramesInFlight;
        CurrentFrameIndex = 0;
        AcquiredGeneration = 0;
        AcquiredFrameToken = 0;
        NativeFrameBindings = {};
        ++Generation;
        ResolvedState = {};
        State = ERHISwapchainState::Paused;
        return ERHIResult::NotReady;
    }
    FRHIPresentationCapabilities Capabilities;
    if (!Request.IsExactPresentationRequestValid() ||
        Request.FramesInFlight > MaxFrameCount ||
        Surface->QueryCapabilities(Capabilities) != ERHIResult::Success ||
        Request.SurfaceCapabilityGeneration !=
            Capabilities.CapabilityGeneration ||
        !Capabilities.SupportsPair(
            Request.PreferredFormat, Request.PreferredColorSpace) ||
        (Request.bHasHDRMetadata && !Capabilities.bSupportsHDRMetadata) ||
        (Request.NativeEncoding ==
                ERHIPresentationNativeEncoding::ScRgb80 &&
            !Capabilities.bSupportsExtendedRange) ||
        !IsEncodingPairCompatible(Request))
    {
        return ERHIResult::Unsupported;
    }

    Stoner::Core::TArray<Stoner::Core::TSharedPtr<IRHITexture>> NewImages;
    FRHIResolvedPresentationState NativeResolved;
    if (NativeContext)
    {
        const ERHIResult NativeResult =
            NativeContext->PrepareVisibleImage(Request, NativeResolved);
        if (NativeResult != ERHIResult::Success)
        {
            return NativeResult;
        }
    }
    FRHISwapchainDesc ImageRequest = Request;
    if (NativeResolved.IsValid())
    {
        ImageRequest.Width = NativeResolved.Width;
        ImageRequest.Height = NativeResolved.Height;
        ImageRequest.FramesInFlight = std::max(
            Request.FramesInFlight,
            NativeContext->GetVisiblePresentationImageCount());
    }
    if (!BuildImages(ImageRequest, NewImages))
    {
        return ERHIResult::Failed;
    }

    InvalidateImages();
    Images = std::move(NewImages);
    Desc = Request;
    FrameCount = Request.FramesInFlight;
    CurrentFrameIndex = 0;
    AcquiredGeneration = 0;
    AcquiredFrameToken = 0;
    NativeFrameBindings = {};
    if (NativeResolved.IsValid())
    {
        Generation = NativeResolved.ModeGeneration;
        ResolvedState = std::move(NativeResolved);
    }
    else
    {
        ++Generation;
        ResolvedState.ModeGeneration = Generation;
        ResolvedState.Width = Request.Width;
        ResolvedState.Height = Request.Height;
        ResolvedState.Format = Request.PreferredFormat;
        ResolvedState.ColorSpace = Request.PreferredColorSpace;
        ResolvedState.NativeEncoding = Request.NativeEncoding;
        ResolvedState.DisplayAdaptation = Request.DisplayAdaptation;
        ResolvedState.bHasHDRMetadata = Request.bHasHDRMetadata;
        ResolvedState.MetadataDigest = Request.bHasHDRMetadata
            ? Request.HDRMetadata.CanonicalDigest : Stoner::Core::FString{};
        ResolvedState.ReferenceWhiteNits = Request.ReferenceWhiteNits;
        ResolvedState.TargetPeakNits = Request.TargetPeakNits;
        ResolvedState.SwapchainImageGeneration = Generation;
    }
    State = ERHISwapchainState::Ready;
    return ERHIResult::Success;
}

Stoner::RHI::ERHISwapchainState FVulkanSwapchain::GetState() const noexcept
{
    if (!bValid || (Surface && !Surface->IsValid()))
    {
        return Stoner::RHI::ERHISwapchainState::Unavailable;
    }
    return State;
}

Stoner::Core::uint32 FVulkanSwapchain::GetFrameCount() const noexcept
{
    return FrameCount;
}

Stoner::Core::uint32 FVulkanSwapchain::GetCurrentFrameIndex() const noexcept
{
    return CurrentFrameIndex;
}

Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>
FVulkanSwapchain::GetImage(Stoner::Core::uint32 ImageIndex) const
{
    if (!bValid || (Surface && !Surface->IsValid()))
    {
        return nullptr;
    }
    if (bLabPresentation)
    {
        if (State != Stoner::RHI::ERHISwapchainState::Ready ||
            ImageIndex >= LabBorrowedImages.size())
        {
            return nullptr;
        }
        const FLabBorrowedImage& Borrowed = LabBorrowedImages[ImageIndex];
        if (Borrowed.Generation != Generation || !Borrowed.Texture ||
            Borrowed.Texture->GetLifecycleState() !=
                Stoner::RHI::ERHIResourceLifecycleState::Valid)
        {
            return nullptr;
        }
        return Borrowed.Texture;
    }
    if (ImageIndex >= Images.size())
    {
        return nullptr;
    }
    return Images[ImageIndex];
}

Stoner::RHI::ERHIResult FVulkanSwapchain::AcquireBorrowedTarget(
    Stoner::Core::uint64 FrameToken,
    Stoner::Core::uint32 FrameSlotIndex,
    Stoner::RHI::FRHIBorrowedAcquiredTarget& OutTarget)
{
    OutTarget = {};
    if (!bValid || (Surface && !Surface->IsValid()))
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    const auto NativeContext = Surface ? Surface->GetNativeContext() : nullptr;
    if (!NativeContext || !NativeContext->IsLabPresentationActive())
    {
        return Stoner::RHI::ERHIResult::Unsupported;
    }

    const Stoner::RHI::ERHIResult Result =
        NativeContext->AcquireLabBorrowedTarget(
            FrameToken, FrameSlotIndex, OutTarget);
    if (Result != Stoner::RHI::ERHIResult::Success)
    {
        return Result;
    }
    if (!bLabPresentation || State != Stoner::RHI::ERHISwapchainState::Ready ||
        !OutTarget.IsValid() || !OutTarget.Frame.Matches(ResolvedState) ||
        OutTarget.Frame.ModeGeneration != Generation ||
        OutTarget.Frame.ImageIndex >= LabBorrowedImages.size())
    {
        (void)NativeContext->ReleaseLabBorrowedTarget(OutTarget, nullptr);
        OutTarget = {};
        return Stoner::RHI::ERHIResult::InvalidState;
    }

    FLabBorrowedImage& Borrowed = LabBorrowedImages[OutTarget.Frame.ImageIndex];
    Borrowed.Texture = OutTarget.Texture;
    Borrowed.Generation = OutTarget.Frame.ModeGeneration;
    Borrowed.FrameToken = OutTarget.Frame.FrameToken;
    CurrentFrameIndex = OutTarget.Frame.ImageIndex;
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanSwapchain::PresentBorrowedTarget(
    const Stoner::RHI::FRHIBorrowedAcquiredTarget& Target,
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHISemaphore>&
        RenderFinishedSemaphore,
    Stoner::RHI::FRHIPresentationLease& OutPresentationLease)
{
    OutPresentationLease = {};
    if (!bValid || (Surface && !Surface->IsValid()))
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    const auto NativeContext = Surface ? Surface->GetNativeContext() : nullptr;
    if (!NativeContext || !NativeContext->IsLabPresentationActive())
    {
        return Stoner::RHI::ERHIResult::Unsupported;
    }
    if (!bLabPresentation || !Target.IsValid() ||
        !Target.Frame.Matches(ResolvedState) ||
        Target.Frame.ModeGeneration != Generation)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    return NativeContext->PresentLabBorrowedTarget(
        Target, RenderFinishedSemaphore, OutPresentationLease);
}

Stoner::RHI::ERHIResult FVulkanSwapchain::PresentBorrowedTarget(
    const Stoner::RHI::FRHIBorrowedAcquiredTarget& Target,
    const Stoner::RHI::FRHIRenderLease& RenderLease,
    Stoner::RHI::FRHIPresentationLease& OutPresentationLease)
{
    OutPresentationLease = {};
    if (!bValid || (Surface && !Surface->IsValid()))
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    const auto NativeContext = Surface ? Surface->GetNativeContext() : nullptr;
    if (!NativeContext || !NativeContext->IsLabPresentationActive())
    {
        return Stoner::RHI::ERHIResult::Unsupported;
    }
    if (!bLabPresentation || !Target.IsValid() ||
        !Target.Frame.Matches(ResolvedState) ||
        Target.Frame.ModeGeneration != Generation)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    return NativeContext->PresentLabBorrowedTarget(
        Target, RenderLease, OutPresentationLease);
}

Stoner::RHI::ERHIResult FVulkanSwapchain::ReleaseBorrowedTarget(
    const Stoner::RHI::FRHIBorrowedAcquiredTarget& Target,
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIFence>&
        RenderCompletionFence)
{
    if (Surface && !Surface->IsValid())
    {
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    const auto NativeContext = Surface ? Surface->GetNativeContext() : nullptr;
    if (!NativeContext || !NativeContext->IsLabPresentationActive())
    {
        return Stoner::RHI::ERHIResult::Unsupported;
    }
    // Cancellation remains valid after the borrowed wrapper has been
    // invalidated: Context owns the native image/acquire record until the
    // runtime proves retirement. Keep structural identity checks here, while
    // Context performs the complete bridge lookup and lifecycle handling.
    if (!bLabPresentation || !Target.Texture || !Target.Frame.IsValid() ||
        Target.Frame.ImageIndex >= Stoner::RHI::MaxRHIPresentationImageLeases ||
        Target.FrameSlotIndex >= Stoner::RHI::MaxRHIFrameSlots)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    return NativeContext->ReleaseLabBorrowedTarget(
        Target, RenderCompletionFence);
}

const FVulkanNativeFrameBindings*
FVulkanSwapchain::GetNativeFrameBindings() const noexcept
{
    return State == Stoner::RHI::ERHISwapchainState::Acquired &&
            NativeFrameBindings.FrameToken != 0
        ? &NativeFrameBindings : nullptr;
}

Stoner::RHI::ERHIResult FVulkanSwapchain::ValidateAcquire() const noexcept
{
    if (!bValid)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (Surface && !Surface->IsValid())
    {
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    if (State == Stoner::RHI::ERHISwapchainState::ResizeRequired)
    {
        return Stoner::RHI::ERHIResult::ResizeRequired;
    }
    if (State == Stoner::RHI::ERHISwapchainState::Unavailable)
    {
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    if (State == Stoner::RHI::ERHISwapchainState::Acquired)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    return Stoner::RHI::ERHIResult::Success;
}

void FVulkanSwapchain::CommitAcquire(Stoner::Core::uint32& OutFrameIndex) noexcept
{
    OutFrameIndex = CurrentFrameIndex;
    AcquiredGeneration = Generation;
    State = Stoner::RHI::ERHISwapchainState::Acquired;
}

Stoner::RHI::ERHIResult FVulkanSwapchain::AcquireNextFrame(
    Stoner::Core::uint32& OutFrameIndex)
{
    if (Surface)
    {
        const auto NativeContext = Surface->GetNativeContext();
        if (NativeContext && NativeContext->IsLabPresentationActive())
        {
            return Stoner::RHI::ERHIResult::Unsupported;
        }
    }
    if (Surface && Surface->GetNativeContext())
    {
        Stoner::RHI::FRHIPresentationFrame Frame;
        const auto Result = AcquireNextFrame(NextFrameToken++, Frame);
        OutFrameIndex = Result == Stoner::RHI::ERHIResult::Success
            ? Frame.ImageIndex : 0;
        return Result;
    }
    const Stoner::RHI::ERHIResult Result = ValidateAcquire();
    if (Result != Stoner::RHI::ERHIResult::Success)
    {
        return Result;
    }

    CommitAcquire(OutFrameIndex);
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanSwapchain::AcquireNextFrame(
    Stoner::Core::uint64 FrameToken,
    Stoner::RHI::FRHIPresentationFrame& OutFrame)
{
    using namespace Stoner::RHI;
    OutFrame = {};
    if (FrameToken == 0)
    {
        return ERHIResult::InvalidState;
    }
    if (Surface)
    {
        const auto NativeContext = Surface->GetNativeContext();
        if (NativeContext && NativeContext->IsLabPresentationActive())
        {
            return ERHIResult::Unsupported;
        }
    }
    const ERHIResult Validation = ValidateAcquire();
    if (Validation != ERHIResult::Success)
    {
        return Validation;
    }

    const auto NativeContext = Surface ? Surface->GetNativeContext() : nullptr;
    if (NativeContext)
    {
        FVulkanNativeFrameBindings Native;
        const ERHIResult NativeResult =
            NativeContext->AcquireVisibleFrame(FrameToken, Native);
        if (NativeResult != ERHIResult::Success)
        {
            if (NativeResult == ERHIResult::ResizeRequired)
            {
                State = ERHISwapchainState::ResizeRequired;
            }
            return NativeResult;
        }
        if (Native.ModeGeneration != ResolvedState.ModeGeneration ||
            Native.ImageIndex >= Images.size() || !Native.OutputTexture)
        {
            State = ERHISwapchainState::Unavailable;
            return ERHIResult::InvalidState;
        }
        CurrentFrameIndex = Native.ImageIndex;
        Images[CurrentFrameIndex] = Native.OutputTexture;
        AcquiredGeneration = Generation;
        AcquiredFrameToken = FrameToken;
        NativeFrameBindings = std::move(Native);
        State = ERHISwapchainState::Acquired;
    }
    else
    {
        Stoner::Core::uint32 ImageIndex = 0;
        CommitAcquire(ImageIndex);
        AcquiredFrameToken = FrameToken;
    }

    OutFrame.FrameToken = FrameToken;
    OutFrame.ModeGeneration = ResolvedState.ModeGeneration;
    OutFrame.SwapchainImageGeneration =
        ResolvedState.SwapchainImageGeneration;
    OutFrame.ImageIndex = CurrentFrameIndex;
    OutFrame.Width = ResolvedState.Width;
    OutFrame.Height = ResolvedState.Height;
    OutFrame.Format = ResolvedState.Format;
    OutFrame.ColorSpace = ResolvedState.ColorSpace;
    OutFrame.DisplayAdaptation = ResolvedState.DisplayAdaptation;
    OutFrame.MetadataDigest = ResolvedState.MetadataDigest;
    return OutFrame.IsValid()
        ? ERHIResult::Success : ERHIResult::InvalidState;
}

Stoner::RHI::ERHIResult FVulkanSwapchain::AcquireNextFrame(
    Stoner::Core::uint32& OutFrameIndex,
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHISemaphore>& SignalSemaphore)
{
    if (Surface)
    {
        const auto NativeContext = Surface->GetNativeContext();
        if (NativeContext && NativeContext->IsLabPresentationActive())
        {
            return Stoner::RHI::ERHIResult::Unsupported;
        }
    }
    if (!SignalSemaphore)
    {
        return AcquireNextFrame(OutFrameIndex);
    }

    const Stoner::RHI::ERHIResult Result = ValidateAcquire();
    if (Result != Stoner::RHI::ERHIResult::Success)
    {
        return Result;
    }
    if (SignalSemaphore->IsSignaled())
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }

    const Stoner::RHI::ERHIResult SignalResult = SignalSemaphore->Signal();
    if (SignalResult != Stoner::RHI::ERHIResult::Success)
    {
        return SignalResult;
    }

    const Stoner::RHI::ERHIResult AcquireResult =
        AcquireNextFrame(OutFrameIndex);
    if (AcquireResult != Stoner::RHI::ERHIResult::Success)
    {
        (void)SignalSemaphore->Consume();
    }
    return AcquireResult;
}

Stoner::RHI::ERHIResult FVulkanSwapchain::ValidatePresent(
    Stoner::Core::uint32 FrameIndex) const noexcept
{
    if (!bValid)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (Surface && !Surface->IsValid())
    {
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    if (State == Stoner::RHI::ERHISwapchainState::ResizeRequired)
    {
        return Stoner::RHI::ERHIResult::ResizeRequired;
    }
    if (State == Stoner::RHI::ERHISwapchainState::Unavailable)
    {
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    if (State != Stoner::RHI::ERHISwapchainState::Acquired ||
        FrameIndex != CurrentFrameIndex ||
        AcquiredGeneration != Generation)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    return Stoner::RHI::ERHIResult::Success;
}

void FVulkanSwapchain::CommitPresent() noexcept
{
    CurrentFrameIndex = (CurrentFrameIndex + 1) % FrameCount;
    AcquiredGeneration = 0;
    AcquiredFrameToken = 0;
    NativeFrameBindings = {};
    State = Stoner::RHI::ERHISwapchainState::Ready;
}

Stoner::RHI::ERHIResult FVulkanSwapchain::Present(
    Stoner::Core::uint32 FrameIndex)
{
    if (Surface)
    {
        const auto NativeContext = Surface->GetNativeContext();
        if (NativeContext && NativeContext->IsLabPresentationActive())
        {
            return Stoner::RHI::ERHIResult::Unsupported;
        }
    }
    const Stoner::RHI::ERHIResult Result = ValidatePresent(FrameIndex);
    if (Result != Stoner::RHI::ERHIResult::Success)
    {
        return Result;
    }

    if (const auto NativeContext = Surface
            ? Surface->GetNativeContext() : nullptr)
    {
        if (AcquiredFrameToken == 0 ||
            NativeFrameBindings.FrameToken != AcquiredFrameToken)
        {
            return Stoner::RHI::ERHIResult::InvalidState;
        }
        const auto NativeResult = NativeContext
            ->SubmitAndPresentVisibleFrame(NativeFrameBindings);
        if (NativeResult != Stoner::RHI::ERHIResult::Success)
        {
            if (NativeResult == Stoner::RHI::ERHIResult::ResizeRequired)
            {
                State = Stoner::RHI::ERHISwapchainState::ResizeRequired;
            }
            return NativeResult;
        }
    }
    CommitPresent();
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanSwapchain::Present(
    const Stoner::RHI::FRHIPresentationFrame& Frame)
{
    if (Surface)
    {
        const auto NativeContext = Surface->GetNativeContext();
        if (NativeContext && NativeContext->IsLabPresentationActive())
        {
            return Stoner::RHI::ERHIResult::Unsupported;
        }
    }
    if (!Frame.Matches(ResolvedState) ||
        Frame.FrameToken != AcquiredFrameToken ||
        Frame.ImageIndex != CurrentFrameIndex)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    return Present(Frame.ImageIndex);
}

Stoner::RHI::ERHIResult FVulkanSwapchain::Present(
    Stoner::Core::uint32 FrameIndex,
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHISemaphore>& WaitSemaphore)
{
    if (Surface)
    {
        const auto NativeContext = Surface->GetNativeContext();
        if (NativeContext && NativeContext->IsLabPresentationActive())
        {
            return Stoner::RHI::ERHIResult::Unsupported;
        }
    }
    if (!WaitSemaphore)
    {
        return Present(FrameIndex);
    }

    const Stoner::RHI::ERHIResult Result = ValidatePresent(FrameIndex);
    if (Result != Stoner::RHI::ERHIResult::Success)
    {
        return Result;
    }
    if (!WaitSemaphore->IsSignaled())
    {
        return Stoner::RHI::ERHIResult::NotReady;
    }

    const Stoner::RHI::ERHIResult ConsumeResult = WaitSemaphore->Consume();
    if (ConsumeResult != Stoner::RHI::ERHIResult::Success)
    {
        return ConsumeResult;
    }

    return Present(FrameIndex);
}

Stoner::RHI::ERHIResult FVulkanSwapchain::Recreate(
    Stoner::Core::uint32 NewFrameCount)
{
    if (!bValid)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (Surface && !Surface->IsValid())
    {
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    if (NewFrameCount == 0)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (NewFrameCount > MaxFrameCount)
    {
        return Stoner::RHI::ERHIResult::Unsupported;
    }

    if (Surface && Surface->GetNativeContext())
    {
        Stoner::RHI::FRHISwapchainDesc Request = Desc;
        Request.FramesInFlight = NewFrameCount;
        return Reconfigure(Request);
    }

    if (Surface && !RebuildImages(NewFrameCount))
    {
        return Stoner::RHI::ERHIResult::Failed;
    }

    FrameCount = NewFrameCount;
    Desc.FramesInFlight = NewFrameCount;
    CurrentFrameIndex = 0;
    AcquiredGeneration = 0;
    AcquiredFrameToken = 0;
    NativeFrameBindings = {};
    ++Generation;
    State = Stoner::RHI::ERHISwapchainState::Ready;
    return Stoner::RHI::ERHIResult::Success;
}

void FVulkanSwapchain::SimulateResizeRequired() noexcept
{
    if (bValid && (!Surface || Surface->IsValid()))
    {
        State = Stoner::RHI::ERHISwapchainState::ResizeRequired;
    }
}

void FVulkanSwapchain::SetUnavailable() noexcept
{
    if (bValid)
    {
        State = Stoner::RHI::ERHISwapchainState::Unavailable;
    }
}

void FVulkanSwapchain::InvalidateImages() noexcept
{
    for (const auto& Image : Images)
    {
        if (Image)
        {
            (void)Image->Invalidate();
        }
    }
}

bool FVulkanSwapchain::RebuildImages(Stoner::Core::uint32 NewFrameCount)
{
    if (!Surface || !Surface->IsValid() || NewFrameCount == 0)
    {
        return false;
    }

    const Stoner::RHI::FRHITextureDesc ImageDesc = MakeSwapchainImageDesc(Desc);
    if (!Stoner::RHI::IsValidRHITextureDesc(ImageDesc))
    {
        return false;
    }

    Stoner::Core::TArray<Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>>
        NewImages;
    NewImages.reserve(NewFrameCount);
    for (Stoner::Core::uint32 Index = 0; Index < NewFrameCount; ++Index)
    {
        NewImages.push_back(
            Stoner::Core::MakeShared<FVulkanSwapchainImage>(ImageDesc));
    }

    InvalidateImages();
    Images = std::move(NewImages);
    return true;
}

bool FVulkanSwapchain::BuildImages(
    const Stoner::RHI::FRHISwapchainDesc& InDesc,
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<
        Stoner::RHI::IRHITexture>>& OutImages) const
{
    OutImages.clear();
    if (!Surface || !Surface->IsValid() || InDesc.FramesInFlight == 0)
    {
        return false;
    }
    const Stoner::RHI::FRHITextureDesc ImageDesc =
        MakeSwapchainImageDesc(InDesc);
    if (!Stoner::RHI::IsValidRHITextureDesc(ImageDesc))
    {
        return false;
    }
    try
    {
        OutImages.reserve(InDesc.FramesInFlight);
        for (Stoner::Core::uint32 Index = 0;
             Index < InDesc.FramesInFlight; ++Index)
        {
            OutImages.push_back(
                Stoner::Core::MakeShared<FVulkanSwapchainImage>(ImageDesc));
        }
    }
    catch (...)
    {
        OutImages.clear();
        return false;
    }
    return true;
}

void FVulkanSwapchain::Invalidate() noexcept
{
    if (!bValid)
    {
        return;
    }
    if (bLabPresentation)
    {
        // Borrowed target textures are owned by the native Context and may
        // still be held by the renderer. Dropping this wrapper's cache does
        // not establish presentation or render completion proof.
        for (FLabBorrowedImage& Image : LabBorrowedImages)
        {
            Image = {};
        }
        Images.clear();
    }
    else
    {
        InvalidateImages();
    }
    bValid = false;
    AcquiredGeneration = 0;
    AcquiredFrameToken = 0;
    NativeFrameBindings = {};
    State = Stoner::RHI::ERHISwapchainState::Unavailable;
}

} // namespace Stoner::Backend::Vulkan
