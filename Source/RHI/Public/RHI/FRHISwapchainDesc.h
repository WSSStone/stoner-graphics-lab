#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIFormat.h"
#include "RHI/ERHIPresentationColorSpace.h"
#include "RHI/FRHIHDRMetadata.h"

namespace Stoner::RHI
{

struct FRHISwapchainDesc
{
    Stoner::Core::uint32 Width = 0;
    Stoner::Core::uint32 Height = 0;
    Stoner::Core::uint32 FramesInFlight = 2;
    ERHIFormat PreferredFormat = ERHIFormat::B8G8R8A8_UNorm;
    ERHIPresentationColorSpace PreferredColorSpace =
        ERHIPresentationColorSpace::SrgbNonlinear;
    ERHIPresentationNativeEncoding NativeEncoding =
        ERHIPresentationNativeEncoding::SdrExplicit;
    ERHIPresentationDisplayAdaptation DisplayAdaptation =
        ERHIPresentationDisplayAdaptation::None;
    Stoner::Core::uint64 SurfaceCapabilityGeneration = 0;
    float ReferenceWhiteNits = 100.0f;
    float TargetPeakNits = 100.0f;
    bool bHasHDRMetadata = false;
    FRHIHDRMetadata HDRMetadata;
    bool bVSync = true;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return Width > 0 && Height > 0 && FramesInFlight > 0;
    }

    [[nodiscard]] bool IsZeroDrawable() const noexcept
    {
        return Width == 0 || Height == 0;
    }

    [[nodiscard]] bool IsExactPresentationRequestValid() const noexcept
    {
        return IsValid() && SurfaceCapabilityGeneration != 0 &&
            IsValidRHIFormat(PreferredFormat) &&
            IsValidPresentationColorSpace(PreferredColorSpace) &&
            IsValidPresentationNativeEncoding(NativeEncoding) &&
            ReferenceWhiteNits > 0.0f &&
            TargetPeakNits >= ReferenceWhiteNits &&
            bHasHDRMetadata == HDRMetadata.bPresent && HDRMetadata.IsValid();
    }
};

} // namespace Stoner::RHI
