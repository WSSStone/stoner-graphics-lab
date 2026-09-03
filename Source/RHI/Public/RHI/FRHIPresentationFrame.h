#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIFormat.h"
#include "RHI/ERHIPresentationColorSpace.h"
#include "RHI/FRHIResolvedPresentationState.h"

namespace Stoner::RHI
{

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

} // namespace Stoner::RHI
