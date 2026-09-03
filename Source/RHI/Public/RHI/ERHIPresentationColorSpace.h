#pragma once

namespace Stoner::RHI
{

// Native surface interpretation only. Renderer remains the authority for tone
// mapping, target peak, gamut conversion, and output-device profile selection.
enum class ERHIPresentationColorSpace
{
    Unknown,
    SrgbNonlinear,
    Bt709Nonlinear,
    SdrPassThrough,
    Hdr10St2084,
    ExtendedSrgbLinear
};

enum class ERHIPresentationNativeEncoding
{
    Unknown,
    SdrExplicit,
    Pq,
    ScRgb80,
    MetalEdr
};

enum class ERHIPresentationDisplayAdaptation
{
    None,
    // The native compositor may color-match the declared display-referred
    // color space to the current display. This does not request native tone
    // mapping and does not transfer output-transform ownership from Renderer.
    SystemColorManagement
};

[[nodiscard]] constexpr bool IsValidPresentationColorSpace(
    ERHIPresentationColorSpace ColorSpace) noexcept
{
    return ColorSpace != ERHIPresentationColorSpace::Unknown;
}

[[nodiscard]] constexpr bool IsValidPresentationNativeEncoding(
    ERHIPresentationNativeEncoding Encoding) noexcept
{
    return Encoding != ERHIPresentationNativeEncoding::Unknown;
}

[[nodiscard]] constexpr const char* ToString(
    ERHIPresentationColorSpace ColorSpace) noexcept
{
    switch (ColorSpace)
    {
    case ERHIPresentationColorSpace::SrgbNonlinear: return "srgb-nonlinear";
    case ERHIPresentationColorSpace::Bt709Nonlinear: return "bt709-nonlinear";
    case ERHIPresentationColorSpace::SdrPassThrough: return "sdr-pass-through";
    case ERHIPresentationColorSpace::Hdr10St2084: return "hdr10-st2084";
    case ERHIPresentationColorSpace::ExtendedSrgbLinear: return "extended-srgb-linear";
    case ERHIPresentationColorSpace::Unknown: return "unknown";
    }
    return "unknown";
}

[[nodiscard]] constexpr const char* ToString(
    ERHIPresentationNativeEncoding Encoding) noexcept
{
    switch (Encoding)
    {
    case ERHIPresentationNativeEncoding::SdrExplicit: return "sdr-explicit";
    case ERHIPresentationNativeEncoding::Pq: return "pq";
    case ERHIPresentationNativeEncoding::ScRgb80: return "scrgb80";
    case ERHIPresentationNativeEncoding::MetalEdr: return "metal-edr";
    case ERHIPresentationNativeEncoding::Unknown: return "unknown";
    }
    return "unknown";
}

[[nodiscard]] constexpr const char* ToString(
    ERHIPresentationDisplayAdaptation Adaptation) noexcept
{
    switch (Adaptation)
    {
    case ERHIPresentationDisplayAdaptation::None: return "none";
    case ERHIPresentationDisplayAdaptation::SystemColorManagement:
        return "system-color-management";
    }
    return "none";
}

} // namespace Stoner::RHI
