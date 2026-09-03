#pragma once

#include <array>

namespace Stoner::Renderer::Private
{

enum class EAces2ReferenceGamut
{
    Rec709D65,
    Rec2020D65
};

// Repository-owned double-precision transliteration of the pinned ACES 2
// forward Output Transform. Input is ACES2065-1/AP0 scene-linear; output is
// display-linear absolute luminance in nits in the requested limiting gamut.
[[nodiscard]] std::array<double, 3> ApplyAces2OutputTransform(
    const std::array<double, 3>& Aces2065_1,
    double PeakLuminanceNits,
    EAces2ReferenceGamut LimitingGamut) noexcept;

} // namespace Stoner::Renderer::Private
