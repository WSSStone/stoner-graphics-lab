#pragma once

#include "Core/CoreMinimal.h"

#include <cmath>

namespace Stoner::RHI
{

struct FRHIHDRMetadata
{
    Stoner::Core::uint32 Version = 1;
    bool bPresent = false;
    float DisplayPrimaryRedX = 0.0f;
    float DisplayPrimaryRedY = 0.0f;
    float DisplayPrimaryGreenX = 0.0f;
    float DisplayPrimaryGreenY = 0.0f;
    float DisplayPrimaryBlueX = 0.0f;
    float DisplayPrimaryBlueY = 0.0f;
    float WhitePointX = 0.0f;
    float WhitePointY = 0.0f;
    float MasteringDisplayMinLuminanceNits = 0.0f;
    float MasteringDisplayMaxLuminanceNits = 0.0f;
    float MaxContentLightLevelNits = 0.0f;
    float MaxFrameAverageLightLevelNits = 0.0f;
    Stoner::Core::FString CanonicalDigest;

    [[nodiscard]] bool IsValid() const noexcept
    {
        if (!bPresent)
        {
            return Version == 1 && CanonicalDigest.IsEmpty() &&
                DisplayPrimaryRedX == 0.0f && DisplayPrimaryRedY == 0.0f &&
                DisplayPrimaryGreenX == 0.0f && DisplayPrimaryGreenY == 0.0f &&
                DisplayPrimaryBlueX == 0.0f && DisplayPrimaryBlueY == 0.0f &&
                WhitePointX == 0.0f && WhitePointY == 0.0f &&
                MasteringDisplayMinLuminanceNits == 0.0f &&
                MasteringDisplayMaxLuminanceNits == 0.0f &&
                MaxContentLightLevelNits == 0.0f &&
                MaxFrameAverageLightLevelNits == 0.0f;
        }
        const auto IsChromaticity = [](float X, float Y) noexcept {
            return std::isfinite(X) && std::isfinite(Y) && X > 0.0f &&
                Y > 0.0f && X < 1.0f && Y < 1.0f && X + Y <= 1.0f;
        };
        return Version == 1 && !CanonicalDigest.IsEmpty() &&
            IsChromaticity(DisplayPrimaryRedX, DisplayPrimaryRedY) &&
            IsChromaticity(DisplayPrimaryGreenX, DisplayPrimaryGreenY) &&
            IsChromaticity(DisplayPrimaryBlueX, DisplayPrimaryBlueY) &&
            IsChromaticity(WhitePointX, WhitePointY) &&
            std::isfinite(MasteringDisplayMinLuminanceNits) &&
            std::isfinite(MasteringDisplayMaxLuminanceNits) &&
            std::isfinite(MaxContentLightLevelNits) &&
            std::isfinite(MaxFrameAverageLightLevelNits) &&
            MasteringDisplayMinLuminanceNits >= 0.0f &&
            MasteringDisplayMaxLuminanceNits > MasteringDisplayMinLuminanceNits &&
            MaxContentLightLevelNits > 0.0f &&
            MaxContentLightLevelNits <= MasteringDisplayMaxLuminanceNits &&
            MaxFrameAverageLightLevelNits > 0.0f &&
            MaxFrameAverageLightLevelNits <= MaxContentLightLevelNits;
    }

    [[nodiscard]] friend bool operator==(
        const FRHIHDRMetadata& Left,
        const FRHIHDRMetadata& Right) noexcept = default;
};

} // namespace Stoner::RHI
