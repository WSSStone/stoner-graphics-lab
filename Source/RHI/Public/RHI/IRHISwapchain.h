#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIResult.h"
#include "RHI/FRHIPresentationFrame.h"
#include "RHI/FRHISwapchainDesc.h"
#include "RHI/FRHIResolvedPresentationState.h"
#include "RHI/IRHISemaphore.h"

namespace Stoner::RHI
{

class IRHITexture;

enum class ERHISwapchainState
{
    Ready,
    Acquired,
    ResizeRequired,
    Paused,
    Unavailable
};

class IRHISwapchain
{
public:
    virtual ~IRHISwapchain() = default;

    [[nodiscard]] virtual ERHISwapchainState GetState() const noexcept = 0;
    [[nodiscard]] virtual Stoner::Core::uint32 GetFrameCount() const noexcept = 0;
    [[nodiscard]] virtual Stoner::Core::uint32 GetCurrentFrameIndex() const noexcept = 0;

    virtual ERHIResult AcquireNextFrame(Stoner::Core::uint32& OutFrameIndex) = 0;
    virtual ERHIResult Present(Stoner::Core::uint32 FrameIndex) = 0;
    virtual ERHIResult AcquireNextFrame(
        Stoner::Core::uint64 FrameToken,
        FRHIPresentationFrame& OutFrame)
    {
        OutFrame = {};
        if (FrameToken == 0)
        {
            return ERHIResult::InvalidState;
        }
        Stoner::Core::uint32 ImageIndex = 0;
        const ERHIResult Result = AcquireNextFrame(ImageIndex);
        if (Result != ERHIResult::Success)
        {
            return Result;
        }
        const FRHIResolvedPresentationState& Resolved =
            GetResolvedPresentationState();
        if (!Resolved.IsValid())
        {
            return ERHIResult::InvalidState;
        }
        OutFrame.FrameToken = FrameToken;
        OutFrame.ModeGeneration = Resolved.ModeGeneration;
        OutFrame.SwapchainImageGeneration =
            Resolved.SwapchainImageGeneration;
        OutFrame.ImageIndex = ImageIndex;
        OutFrame.Width = Resolved.Width;
        OutFrame.Height = Resolved.Height;
        OutFrame.Format = Resolved.Format;
        OutFrame.ColorSpace = Resolved.ColorSpace;
        OutFrame.DisplayAdaptation = Resolved.DisplayAdaptation;
        OutFrame.MetadataDigest = Resolved.MetadataDigest;
        return OutFrame.IsValid() ? ERHIResult::Success
                                  : ERHIResult::InvalidState;
    }
    virtual ERHIResult Present(const FRHIPresentationFrame& Frame)
    {
        if (!Frame.Matches(GetResolvedPresentationState()) ||
            Frame.ImageIndex != GetCurrentFrameIndex())
        {
            return ERHIResult::InvalidState;
        }
        return Present(Frame.ImageIndex);
    }
    [[nodiscard]] virtual Stoner::Core::TSharedPtr<IRHITexture> GetImage(Stoner::Core::uint32) const { return nullptr; }
    [[nodiscard]] virtual Stoner::Core::uint64 GetGeneration() const noexcept { return 1; }
    [[nodiscard]] virtual const FRHIResolvedPresentationState&
    GetResolvedPresentationState() const noexcept
    {
        static const FRHIResolvedPresentationState Empty;
        return Empty;
    }
    virtual ERHIResult Reconfigure(const FRHISwapchainDesc&)
    {
        return ERHIResult::Unsupported;
    }
    [[nodiscard]] virtual Stoner::Core::TSharedPtr<IRHITexture>
    GetImageForGeneration(
        Stoner::Core::uint32 ImageIndex,
        Stoner::Core::uint64 ExpectedGeneration) const
    {
        if (ExpectedGeneration == 0 || ExpectedGeneration != GetGeneration())
        {
            return nullptr;
        }
        return GetImage(ImageIndex);
    }
    virtual ERHIResult AcquireNextFrame(
        Stoner::Core::uint32&,
        const Stoner::Core::TSharedPtr<IRHISemaphore>&)
    {
        return ERHIResult::Unsupported;
    }
    virtual ERHIResult Present(
        Stoner::Core::uint32,
        const Stoner::Core::TSharedPtr<IRHISemaphore>&)
    {
        return ERHIResult::Unsupported;
    }
};

} // namespace Stoner::RHI
