#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIFormat.h"
#include "RHI/ERHIPresentationColorSpace.h"
#include "RHI/ERHIPresentationRetirement.h"

#include <cmath>

namespace Stoner::RHI
{

struct FRHIPresentationFormatColorSpacePair
{
    ERHIFormat Format = ERHIFormat::Unknown;
    ERHIPresentationColorSpace ColorSpace = ERHIPresentationColorSpace::Unknown;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return IsValidRHIFormat(Format) &&
            IsValidPresentationColorSpace(ColorSpace);
    }

    [[nodiscard]] friend bool operator==(
        const FRHIPresentationFormatColorSpacePair& Left,
        const FRHIPresentationFormatColorSpacePair& Right) noexcept = default;
};

struct FRHIPresentationCapabilities
{
    Stoner::Core::uint64 SurfaceId = 0;
    Stoner::Core::uint64 CapabilityGeneration = 0;
    Stoner::Core::TArray<FRHIPresentationFormatColorSpacePair> SupportedPairs;
    bool bSupportsHDRMetadata = false;
    bool bSupportsExtendedRange = false;
    float NativeReferenceWhiteNits = 0.0f;
    float CurrentHeadroom = 1.0f;
    float PotentialHeadroom = 1.0f;
    Stoner::Core::FString CapabilityDigest;

    // True only when presentation completion can be observed independently
    // from render-fence completion (for example, Vulkan maintenance1). Keep
    // this extension at the end so established aggregate initialization stays
    // source-compatible.
    bool bSupportsIndependentPresentationCompletion = false;

    // Backend-neutral presentation-retirement selection.  These fields are
    // appended so established aggregate initialization remains source-
    // compatible.  The selected mode is deliberately separate from the
    // device's deferred-submit capability.
    ERHIPresentationRetirementMode PresentationRetirementMode =
        ERHIPresentationRetirementMode::Unknown;
    ERHIPresentationRetirementReason PresentationRetirementReason =
        ERHIPresentationRetirementReason::Unknown;
    bool bOptionalPresentationFenceAdvertised = false;
    bool bOptionalPresentationFenceEnabled = false;

    [[nodiscard]] bool HasUniqueSupportedPairs() const noexcept
    {
        for (Stoner::Core::uint32 Left = 0; Left < SupportedPairs.size(); ++Left)
        {
            for (Stoner::Core::uint32 Right = Left + 1;
                 Right < SupportedPairs.size(); ++Right)
            {
                if (SupportedPairs[Left] == SupportedPairs[Right])
                {
                    return false;
                }
            }
        }
        return true;
    }

    [[nodiscard]] bool SupportsPair(
        ERHIFormat Format,
        ERHIPresentationColorSpace ColorSpace) const noexcept
    {
        for (const FRHIPresentationFormatColorSpacePair& Pair : SupportedPairs)
        {
            if (Pair.Format == Format && Pair.ColorSpace == ColorSpace)
            {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool IsValid() const noexcept
    {
        if (SurfaceId == 0 || CapabilityGeneration == 0 ||
            SupportedPairs.empty() || CapabilityDigest.IsEmpty() ||
            !HasUniqueSupportedPairs() || !std::isfinite(NativeReferenceWhiteNits) ||
            !std::isfinite(CurrentHeadroom) || !std::isfinite(PotentialHeadroom) ||
            NativeReferenceWhiteNits < 0.0f || CurrentHeadroom < 1.0f ||
            PotentialHeadroom < CurrentHeadroom)
        {
            return false;
        }
        for (const FRHIPresentationFormatColorSpacePair& Pair : SupportedPairs)
        {
            if (!Pair.IsValid())
            {
                return false;
            }
        }
        return !bSupportsExtendedRange || PotentialHeadroom > 1.0f;
    }
};

} // namespace Stoner::RHI
