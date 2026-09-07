#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIFormat.h"
#include "RHI/ERHIPresentationColorSpace.h"
#include "RHI/FRHIResolvedPresentationState.h"
#include "RHI/IRHITexture.h"

namespace Stoner::RHI
{

class IRHIFence;
class IRHISemaphore;
class IRHITexture;

// Immutable provenance for one acquired native presentation image.  A ticket
// is valid only for the exact swapchain mode/image generation that issued it.
struct FRHIPresentationFrame
{
    Stoner::Core::uint64 FrameToken = 0;
    Stoner::Core::uint64 ModeGeneration = 0;
    Stoner::Core::uint64 SwapchainImageGeneration = 0;
    Stoner::Core::uint32 ImageIndex = 0;
    Stoner::Core::uint32 Width = 0;
    Stoner::Core::uint32 Height = 0;
    ERHIFormat Format = ERHIFormat::Unknown;
    ERHIPresentationColorSpace ColorSpace =
        ERHIPresentationColorSpace::Unknown;
    ERHIPresentationDisplayAdaptation DisplayAdaptation =
        ERHIPresentationDisplayAdaptation::None;
    Stoner::Core::FString MetadataDigest;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return FrameToken != 0 && ModeGeneration != 0 &&
            SwapchainImageGeneration != 0 && Width != 0 && Height != 0 &&
            IsValidRHIFormat(Format) &&
            IsValidPresentationColorSpace(ColorSpace);
    }

    [[nodiscard]] bool Matches(
        const FRHIResolvedPresentationState& State) const noexcept
    {
        return IsValid() && State.IsValid() &&
            ModeGeneration == State.ModeGeneration &&
            SwapchainImageGeneration == State.SwapchainImageGeneration &&
            Width == State.Width && Height == State.Height &&
            Format == State.Format && ColorSpace == State.ColorSpace &&
            DisplayAdaptation == State.DisplayAdaptation &&
            MetadataDigest == State.MetadataDigest;
    }

    [[nodiscard]] friend bool operator==(
        const FRHIPresentationFrame& Left,
        const FRHIPresentationFrame& Right) noexcept = default;
};

inline constexpr Stoner::Core::uint32 MaxRHIFrameSlots = 2;
inline constexpr Stoner::Core::uint32 MaxRHIPresentationImageLeases = 8;

// A borrowed target is an abstract RHI texture attached to one exact
// acquired presentation frame. It carries no native image or window handle.
struct FRHIBorrowedAcquiredTarget
{
    Stoner::Core::TSharedPtr<IRHITexture> Texture;
    // Optional acquire synchronization. Vulkan uses this for image
    // readiness; Metal may represent acquire readiness privately.
    Stoner::Core::TSharedPtr<IRHISemaphore> AcquireSemaphore;
    FRHIPresentationFrame Frame;
    Stoner::Core::uint32 FrameSlotIndex = 0;

    [[nodiscard]] bool IsValid() const noexcept
    {
        if (Texture == nullptr || Frame.IsValid() == false ||
            Texture->GetLifecycleState() != ERHIResourceLifecycleState::Valid ||
            Texture->GetDesc().Width != Frame.Width ||
            Texture->GetDesc().Height != Frame.Height ||
            Texture->GetFormat() != Frame.Format)
        {
            return false;
        }
        return
            Frame.ImageIndex < MaxRHIPresentationImageLeases &&
            FrameSlotIndex < MaxRHIFrameSlots;
    }

    [[nodiscard]] bool Matches(
        const FRHIPresentationFrame& OtherFrame) const noexcept
    {
        return IsValid() && OtherFrame.IsValid() && Frame == OtherFrame;
    }

    [[nodiscard]] bool Matches(
        const FRHIBorrowedAcquiredTarget& Other) const noexcept
    {
        return IsValid() && Other.IsValid() &&
            Texture == Other.Texture &&
            FrameSlotIndex == Other.FrameSlotIndex && Frame == Other.Frame;
    }
};

// Render completion governs reuse of slot-owned command/resources. It is
// deliberately represented separately from the presentation lease below.
struct FRHIRenderLease
{
    FRHIPresentationFrame Frame;
    Stoner::Core::uint32 FrameSlotIndex = 0;
    Stoner::Core::TSharedPtr<IRHIFence> CompletionFence;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return CompletionFence != nullptr && Frame.IsValid() &&
            Frame.ImageIndex < MaxRHIPresentationImageLeases &&
            FrameSlotIndex < MaxRHIFrameSlots;
    }

    [[nodiscard]] bool Matches(
        const FRHIBorrowedAcquiredTarget& Target) const noexcept
    {
        return IsValid() && Target.IsValid() &&
            FrameSlotIndex == Target.FrameSlotIndex && Frame == Target.Frame;
    }
};

// Presentation completion governs reuse of the image-indexed acquired image
// and render-finished semaphore. A queued/presented frame is not physical
// scanout, and this lease does not imply render-fence completion.
struct FRHIPresentationLease
{
    FRHIPresentationFrame Frame;
    Stoner::Core::TSharedPtr<IRHISemaphore> RenderFinishedSemaphore;
    Stoner::Core::TSharedPtr<IRHIFence> PresentationCompletionFence;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return PresentationCompletionFence != nullptr && Frame.IsValid() &&
            Frame.ImageIndex < MaxRHIPresentationImageLeases;
    }

    [[nodiscard]] bool Matches(
        const FRHIBorrowedAcquiredTarget& Target) const noexcept
    {
        return IsValid() && Target.IsValid() &&
            Frame == Target.Frame;
    }
};

} // namespace Stoner::RHI
