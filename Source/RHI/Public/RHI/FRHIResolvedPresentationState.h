#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIFormat.h"
#include "RHI/ERHIPresentationColorSpace.h"

#include <cmath>

namespace Stoner::RHI
{

struct FRHIResolvedPresentationState
{
    Stoner::Core::uint64 ModeGeneration = 0;
    Stoner::Core::uint32 Width = 0;
    Stoner::Core::uint32 Height = 0;
    ERHIFormat Format = ERHIFormat::Unknown;
    ERHIPresentationColorSpace ColorSpace = ERHIPresentationColorSpace::Unknown;
    ERHIPresentationNativeEncoding NativeEncoding =
        ERHIPresentationNativeEncoding::Unknown;
    ERHIPresentationDisplayAdaptation DisplayAdaptation =
        ERHIPresentationDisplayAdaptation::None;
    bool bHasHDRMetadata = false;
    Stoner::Core::FString MetadataDigest;
    float ReferenceWhiteNits = 0.0f;
    float TargetPeakNits = 0.0f;
    Stoner::Core::uint64 SwapchainImageGeneration = 0;

    [[nodiscard]] bool IsZeroDrawable() const noexcept
    {
        return Width == 0 || Height == 0;
    }

    [[nodiscard]] bool IsValid() const noexcept
    {
        return ModeGeneration != 0 && Width != 0 && Height != 0 &&
            IsValidRHIFormat(Format) &&
            IsValidPresentationColorSpace(ColorSpace) &&
            IsValidPresentationNativeEncoding(NativeEncoding) &&
            (!bHasHDRMetadata || !MetadataDigest.IsEmpty()) &&
            std::isfinite(ReferenceWhiteNits) && ReferenceWhiteNits > 0.0f &&
            std::isfinite(TargetPeakNits) &&
            TargetPeakNits >= ReferenceWhiteNits &&
            SwapchainImageGeneration != 0;
    }

    [[nodiscard]] friend bool operator==(
        const FRHIResolvedPresentationState& Left,
        const FRHIResolvedPresentationState& Right) noexcept = default;
};

} // namespace Stoner::RHI
