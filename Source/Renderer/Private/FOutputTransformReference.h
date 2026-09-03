#pragma once

#include <array>
#include <cstdint>

namespace Stoner::Renderer
{

struct FOutputTransformReferenceRgb
{
    double R = 0.0;
    double G = 0.0;
    double B = 0.0;
};

struct FOutputTransformReferenceXyz
{
    double X = 0.0;
    double Y = 0.0;
    double Z = 0.0;
};

enum class EOutputTransformReferenceStatus : std::uint8_t
{
    Success,
    NotImplemented,
    NonFiniteInput,
    ExposureOutOfRange,
    InvalidPeakLuminance,
    InvalidReferenceWhite,
    InvalidEncoding
};

enum class EOutputTransformReferenceSdrToneMap : std::uint8_t
{
    KhronosPbrNeutral,
    NarkowiczAcesFit,
    ExtendedReinhardRec709
};

enum class EOutputTransformReferenceTransfer : std::uint8_t
{
    Srgb,
    Bt709,
    Gamma22,
    St2084
};

enum class EOutputTransformReferenceColorSpace : std::uint8_t
{
    Rec709D65,
    Rec2020D65
};

enum class EOutputTransformReferenceNativeEncoding : std::uint8_t
{
    PqPacked10,
    ScRgb80,
    MetalEdr
};

struct FOutputTransformReferenceRgbResult
{
    EOutputTransformReferenceStatus Status =
        EOutputTransformReferenceStatus::NotImplemented;
    FOutputTransformReferenceRgb Value;

    [[nodiscard]] bool IsSuccess() const noexcept
    {
        return Status == EOutputTransformReferenceStatus::Success;
    }
};

struct FOutputTransformReferenceScalarResult
{
    EOutputTransformReferenceStatus Status =
        EOutputTransformReferenceStatus::NotImplemented;
    double Value = 0.0;

    [[nodiscard]] bool IsSuccess() const noexcept
    {
        return Status == EOutputTransformReferenceStatus::Success;
    }
};

class FOutputTransformReference final
{
public:
    static constexpr double MinimumExposureStops = -16.0;
    static constexpr double MaximumExposureStops = 16.0;
    static constexpr std::array<double, 8> ExposureSamples = {
        -16.0, -8.0, -1.0, 0.0, 1.0, 8.0, 15.0, 16.0};

    [[nodiscard]] static bool IsImplemented() noexcept;
    [[nodiscard]] static bool IsFinite(
        const FOutputTransformReferenceRgb& Value) noexcept;
    [[nodiscard]] static FOutputTransformReferenceRgbResult ApplyManualExposure(
        const FOutputTransformReferenceRgb& SceneLinear, double Stops) noexcept;
    [[nodiscard]] static FOutputTransformReferenceRgbResult ClampAtTransformBoundary(
        const FOutputTransformReferenceRgb& ExposedLinear) noexcept;
    [[nodiscard]] static FOutputTransformReferenceRgbResult ApplySdrToneMap(
        const FOutputTransformReferenceRgb& ExposedLinear,
        EOutputTransformReferenceSdrToneMap Strategy) noexcept;
    [[nodiscard]] static FOutputTransformReferenceRgbResult ApplyAces2HdrViewing(
        const FOutputTransformReferenceRgb& ExposedLinearRec709,
        double TargetPeakNits,
        EOutputTransformReferenceColorSpace OutputColorSpace) noexcept;
    [[nodiscard]] static FOutputTransformReferenceRgbResult ConvertColorSpace(
        const FOutputTransformReferenceRgb& Value,
        EOutputTransformReferenceColorSpace Source,
        EOutputTransformReferenceColorSpace Destination) noexcept;
    [[nodiscard]] static FOutputTransformReferenceXyz ConvertToXyz(
        const FOutputTransformReferenceRgb& Value,
        EOutputTransformReferenceColorSpace Source) noexcept;
    [[nodiscard]] static FOutputTransformReferenceScalarResult EncodeTransfer(
        double LinearValue, EOutputTransformReferenceTransfer Transfer) noexcept;
    [[nodiscard]] static FOutputTransformReferenceScalarResult DecodeTransfer(
        double EncodedValue, EOutputTransformReferenceTransfer Transfer) noexcept;
    [[nodiscard]] static FOutputTransformReferenceRgbResult EncodeLinearHdr(
        const FOutputTransformReferenceRgb& AbsoluteNits,
        EOutputTransformReferenceNativeEncoding Encoding,
        double NativeReferenceWhiteNits) noexcept;
    [[nodiscard]] static FOutputTransformReferenceRgbResult DecodeLinearHdr(
        const FOutputTransformReferenceRgb& NativeValues,
        EOutputTransformReferenceNativeEncoding Encoding,
        double NativeReferenceWhiteNits) noexcept;
    [[nodiscard]] static double ComputeCpuTolerance(double Expected) noexcept;
    [[nodiscard]] static FOutputTransformReferenceScalarResult
        ComputeNativeQuantizationStep(double ExpectedNits,
            EOutputTransformReferenceNativeEncoding Encoding,
            double NativeReferenceWhiteNits) noexcept;
    [[nodiscard]] static FOutputTransformReferenceRgbResult ComputeHdrRgbTolerance(
        const FOutputTransformReferenceRgb& ExpectedNits,
        EOutputTransformReferenceNativeEncoding Encoding,
        double NativeReferenceWhiteNits) noexcept;
    [[nodiscard]] static FOutputTransformReferenceXyz PropagateRgbToleranceToXyz(
        const FOutputTransformReferenceRgb& RgbTolerance,
        EOutputTransformReferenceColorSpace Source) noexcept;
};

} // namespace Stoner::Renderer
