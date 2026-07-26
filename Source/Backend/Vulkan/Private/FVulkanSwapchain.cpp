#include "VulkanRHI/FVulkanSwapchain.h"

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
        Stoner::RHI::ERHITextureUsage::Present;
    return ImageDesc;
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
    , Surface(std::move(InSurface))
    , Desc(InDesc)
    , bValid(Surface && Surface->IsValid() && InDesc.IsValid() &&
          InMaxFrameCount > 0 && InDesc.FramesInFlight <= InMaxFrameCount)
{
    if (!bValid || !RebuildImages(FrameCount))
    {
        bValid = false;
        State = Stoner::RHI::ERHISwapchainState::Unavailable;
    }
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
    if (!bValid || (Surface && !Surface->IsValid()) || ImageIndex >= Images.size())
    {
        return nullptr;
    }
    return Images[ImageIndex];
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
    const Stoner::RHI::ERHIResult Result = ValidateAcquire();
    if (Result != Stoner::RHI::ERHIResult::Success)
    {
        return Result;
    }

    CommitAcquire(OutFrameIndex);
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanSwapchain::AcquireNextFrame(
    Stoner::Core::uint32& OutFrameIndex,
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHISemaphore>& SignalSemaphore)
{
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

    CommitAcquire(OutFrameIndex);
    return Stoner::RHI::ERHIResult::Success;
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
    State = Stoner::RHI::ERHISwapchainState::Ready;
}

Stoner::RHI::ERHIResult FVulkanSwapchain::Present(
    Stoner::Core::uint32 FrameIndex)
{
    const Stoner::RHI::ERHIResult Result = ValidatePresent(FrameIndex);
    if (Result != Stoner::RHI::ERHIResult::Success)
    {
        return Result;
    }

    CommitPresent();
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanSwapchain::Present(
    Stoner::Core::uint32 FrameIndex,
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHISemaphore>& WaitSemaphore)
{
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

    CommitPresent();
    return Stoner::RHI::ERHIResult::Success;
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

    if (Surface && !RebuildImages(NewFrameCount))
    {
        return Stoner::RHI::ERHIResult::Failed;
    }

    FrameCount = NewFrameCount;
    Desc.FramesInFlight = NewFrameCount;
    CurrentFrameIndex = 0;
    AcquiredGeneration = 0;
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

void FVulkanSwapchain::Invalidate() noexcept
{
    if (!bValid)
    {
        return;
    }
    InvalidateImages();
    bValid = false;
    AcquiredGeneration = 0;
    State = Stoner::RHI::ERHISwapchainState::Unavailable;
}

} // namespace Stoner::Backend::Vulkan
