#include "FOutputTransformReference.h"

#include "FOutputTransformAces2Reference.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace Stoner::Renderer
{
namespace
{

using FRgb = FOutputTransformReferenceRgb;
using FStatus = EOutputTransformReferenceStatus;

constexpr std::array<double, 9> Rec709ToRec2020 = {
    0.62740389593469903, 0.32928303837788381, 0.04331306568741722,
    0.06909728935823199, 0.91954039507545904, 0.01136231556630916,
    0.01639143887515023, 0.08801330787722578, 0.89559525324762401};
constexpr std::array<double, 9> Rec2020ToRec709 = {
    1.66049100210843470, -0.58764113878854951, -0.07284986331988486,
    -0.12455047452159052, 1.13289989712595980, -0.00834942260436949,
    -0.01815076335490522, -0.10057889800800736, 1.11872966136291250};
constexpr std::array<double, 9> Rec709ToXyz = {
    0.41239079926595928, 0.35758433938387801, 0.18048078840183429,
    0.21263900587151024, 0.71516867876775603, 0.07219231536073371,
    0.01933081871559182, 0.11919477979462598, 0.95053215224966070};
constexpr std::array<double, 9> Rec2020ToXyz = {
    0.63695804830129110, 0.14461690358620832, 0.16888097516417208,
    0.26270021201126698, 0.67799807151887082, 0.05930171646986195,
    0.0, 0.02807269304908743, 1.06098505771079090};
constexpr std::array<double, 9> Rec709D65ToAces2065_1D60 = {
    0.43963298191949124, 0.38298869815155401, 0.17737831992895473,
    0.08977644295884223, 0.81343942874898045, 0.09678412829217702,
    0.01754117038317271, 0.11154655330238701, 0.87091227631444046};

FRgb Multiply(const std::array<double, 9>& Matrix, const FRgb& Value) noexcept
{
    return {
        Matrix[0] * Value.R + Matrix[1] * Value.G + Matrix[2] * Value.B,
        Matrix[3] * Value.R + Matrix[4] * Value.G + Matrix[5] * Value.B,
        Matrix[6] * Value.R + Matrix[7] * Value.G + Matrix[8] * Value.B};
}

FOutputTransformReferenceRgbResult Success(const FRgb& Value) noexcept
{
    return {FStatus::Success, Value};
}

FOutputTransformReferenceRgbResult Failure(FStatus Status) noexcept
{
    return {Status, {}};
}

FOutputTransformReferenceScalarResult ScalarSuccess(double Value) noexcept
{
    return {FStatus::Success, Value};
}

FOutputTransformReferenceScalarResult ScalarFailure(FStatus Status) noexcept
{
    return {Status, 0.0};
}

double KhronosNeutralOffset(double MinimumChannel) noexcept
{
    return MinimumChannel < 0.08
        ? MinimumChannel - 6.25 * MinimumChannel * MinimumChannel
        : 0.04;
}

double PqEncode(double Nits) noexcept
{
    constexpr double M1 = 0.1593017578125;
    constexpr double M2 = 78.84375;
    constexpr double C1 = 0.8359375;
    constexpr double C2 = 18.8515625;
    constexpr double C3 = 18.6875;
    const double Normalized = std::clamp(Nits / 10000.0, 0.0, 1.0);
    const double Power = std::pow(Normalized, M1);
    return std::pow((C1 + C2 * Power) / (1.0 + C3 * Power), M2);
}

double PqDecode(double Encoded) noexcept
{
    constexpr double M1 = 0.1593017578125;
    constexpr double M2 = 78.84375;
    constexpr double C1 = 0.8359375;
    constexpr double C2 = 18.8515625;
    constexpr double C3 = 18.6875;
    const double Power = std::pow(std::clamp(Encoded, 0.0, 1.0), 1.0 / M2);
    const double Numerator = std::max(Power - C1, 0.0);
    const double Denominator = std::max(
        C2 - C3 * Power, std::numeric_limits<double>::min());
    return 10000.0 * std::pow(Numerator / Denominator, 1.0 / M1);
}

double HalfNativeStep(double NativeValue) noexcept
{
    const double Magnitude = std::abs(NativeValue);
    constexpr double SmallestSubnormal = 0x1p-24;
    constexpr double SmallestNormal = 0x1p-14;
    if (Magnitude < SmallestNormal)
    {
        return SmallestSubnormal;
    }
    const int Exponent = std::ilogb(Magnitude);
    return std::ldexp(1.0, Exponent - 10);
}

double NativeScale(EOutputTransformReferenceNativeEncoding Encoding,
    double NativeReferenceWhiteNits) noexcept
{
    return Encoding == EOutputTransformReferenceNativeEncoding::ScRgb80
        ? 80.0
        : NativeReferenceWhiteNits;
}

} // namespace

bool FOutputTransformReference::IsImplemented() noexcept
{
    return true;
}

bool FOutputTransformReference::IsFinite(
    const FOutputTransformReferenceRgb& Value) noexcept
{
    return std::isfinite(Value.R) && std::isfinite(Value.G) &&
        std::isfinite(Value.B);
}

FOutputTransformReferenceRgbResult FOutputTransformReference::ApplyManualExposure(
    const FOutputTransformReferenceRgb& SceneLinear, double Stops) noexcept
{
    if (!IsFinite(SceneLinear) || !std::isfinite(Stops))
    {
        return Failure(FStatus::NonFiniteInput);
    }
    if (Stops < MinimumExposureStops || Stops > MaximumExposureStops)
    {
        return Failure(FStatus::ExposureOutOfRange);
    }
    const double Scale = std::exp2(Stops);
    const FRgb Exposed = {
        SceneLinear.R * Scale, SceneLinear.G * Scale, SceneLinear.B * Scale};
    return IsFinite(Exposed) ? Success(Exposed) : Failure(FStatus::NonFiniteInput);
}

FOutputTransformReferenceRgbResult
FOutputTransformReference::ClampAtTransformBoundary(
    const FOutputTransformReferenceRgb& ExposedLinear) noexcept
{
    if (!IsFinite(ExposedLinear))
    {
        return Failure(FStatus::NonFiniteInput);
    }
    return Success({std::max(0.0, ExposedLinear.R),
        std::max(0.0, ExposedLinear.G), std::max(0.0, ExposedLinear.B)});
}

FOutputTransformReferenceRgbResult FOutputTransformReference::ApplySdrToneMap(
    const FOutputTransformReferenceRgb& ExposedLinear,
    EOutputTransformReferenceSdrToneMap Strategy) noexcept
{
    const auto Boundary = ClampAtTransformBoundary(ExposedLinear);
    if (!Boundary.IsSuccess())
    {
        return Boundary;
    }
    const FRgb Input = Boundary.Value;
    if (Strategy == EOutputTransformReferenceSdrToneMap::KhronosPbrNeutral)
    {
        const double Minimum = std::min({Input.R, Input.G, Input.B});
        const double Offset = KhronosNeutralOffset(Minimum);
        FRgb Color = {Input.R - Offset, Input.G - Offset, Input.B - Offset};
        const double Peak = std::max({Color.R, Color.G, Color.B});
        if (Peak < 0.76)
        {
            return Success(Color);
        }
        constexpr double DistanceToOne = 0.24;
        const double NewPeak = 1.0 - DistanceToOne * DistanceToOne /
            (Peak + DistanceToOne - 0.76);
        Color.R *= NewPeak / Peak;
        Color.G *= NewPeak / Peak;
        Color.B *= NewPeak / Peak;
        const double Desaturation = 1.0 -
            1.0 / (0.15 * (Peak - NewPeak) + 1.0);
        Color.R = Color.R * (1.0 - Desaturation) + NewPeak * Desaturation;
        Color.G = Color.G * (1.0 - Desaturation) + NewPeak * Desaturation;
        Color.B = Color.B * (1.0 - Desaturation) + NewPeak * Desaturation;
        return Success(Color);
    }
    if (Strategy == EOutputTransformReferenceSdrToneMap::NarkowiczAcesFit)
    {
        const auto Curve = [](double Value) {
            return std::clamp(Value * (2.51 * Value + 0.03) /
                    (Value * (2.43 * Value + 0.59) + 0.14),
                0.0, 1.0);
        };
        return Success({Curve(Input.R), Curve(Input.G), Curve(Input.B)});
    }
    if (Strategy == EOutputTransformReferenceSdrToneMap::ExtendedReinhardRec709)
    {
        constexpr double WhiteSquared = 16.0;
        const double Luminance =
            0.2126 * Input.R + 0.7152 * Input.G + 0.0722 * Input.B;
        if (Luminance == 0.0)
        {
            return Success({});
        }
        const double Mapped = Luminance * (1.0 + Luminance / WhiteSquared) /
            (1.0 + Luminance);
        const double Scale = Mapped / Luminance;
        return Success({
            std::clamp(Input.R * Scale, 0.0, 1.0),
            std::clamp(Input.G * Scale, 0.0, 1.0),
            std::clamp(Input.B * Scale, 0.0, 1.0)});
    }
    return Failure(FStatus::InvalidEncoding);
}

FOutputTransformReferenceRgbResult
FOutputTransformReference::ApplyAces2HdrViewing(
    const FOutputTransformReferenceRgb& ExposedLinearRec709,
    double TargetPeakNits,
    EOutputTransformReferenceColorSpace OutputColorSpace) noexcept
{
    if (TargetPeakNits != 1000.0 && TargetPeakNits != 2000.0)
    {
        return Failure(FStatus::InvalidPeakLuminance);
    }
    const auto Boundary = ClampAtTransformBoundary(ExposedLinearRec709);
    if (!Boundary.IsSuccess())
    {
        return Boundary;
    }
    const FRgb Aces = Multiply(Rec709D65ToAces2065_1D60, Boundary.Value);
    const auto Gamut = OutputColorSpace == EOutputTransformReferenceColorSpace::Rec709D65
        ? Private::EAces2ReferenceGamut::Rec709D65
        : Private::EAces2ReferenceGamut::Rec2020D65;
    const std::array<double, 3> Output = Private::ApplyAces2OutputTransform(
        {Aces.R, Aces.G, Aces.B}, TargetPeakNits, Gamut);
    const FRgb Result = {Output[0], Output[1], Output[2]};
    return IsFinite(Result) ? Success(Result) : Failure(FStatus::NonFiniteInput);
}

FOutputTransformReferenceRgbResult FOutputTransformReference::ConvertColorSpace(
    const FOutputTransformReferenceRgb& Value,
    EOutputTransformReferenceColorSpace Source,
    EOutputTransformReferenceColorSpace Destination) noexcept
{
    if (!IsFinite(Value))
    {
        return Failure(FStatus::NonFiniteInput);
    }
    if (Source == Destination)
    {
        return Success(Value);
    }
    return Success(Multiply(
        Source == EOutputTransformReferenceColorSpace::Rec709D65
            ? Rec709ToRec2020
            : Rec2020ToRec709,
        Value));
}

FOutputTransformReferenceXyz FOutputTransformReference::ConvertToXyz(
    const FOutputTransformReferenceRgb& Value,
    EOutputTransformReferenceColorSpace Source) noexcept
{
    const FRgb Converted = Multiply(
        Source == EOutputTransformReferenceColorSpace::Rec709D65
            ? Rec709ToXyz
            : Rec2020ToXyz,
        Value);
    return {Converted.R, Converted.G, Converted.B};
}

FOutputTransformReferenceScalarResult FOutputTransformReference::EncodeTransfer(
    double LinearValue, EOutputTransformReferenceTransfer Transfer) noexcept
{
    if (!std::isfinite(LinearValue))
    {
        return ScalarFailure(FStatus::NonFiniteInput);
    }
    if (Transfer == EOutputTransformReferenceTransfer::St2084)
    {
        return ScalarSuccess(PqEncode(LinearValue));
    }
    const double Linear = std::max(0.0, LinearValue);
    if (Transfer == EOutputTransformReferenceTransfer::Srgb)
    {
        return ScalarSuccess(Linear <= 0.0031308
            ? 12.92 * Linear
            : 1.055 * std::pow(Linear, 1.0 / 2.4) - 0.055);
    }
    if (Transfer == EOutputTransformReferenceTransfer::Bt709)
    {
        return ScalarSuccess(Linear < 0.018
            ? 4.5 * Linear
            : 1.099 * std::pow(Linear, 0.45) - 0.099);
    }
    if (Transfer == EOutputTransformReferenceTransfer::Gamma22)
    {
        return ScalarSuccess(std::pow(Linear, 1.0 / 2.2));
    }
    return ScalarFailure(FStatus::InvalidEncoding);
}

FOutputTransformReferenceScalarResult FOutputTransformReference::DecodeTransfer(
    double EncodedValue, EOutputTransformReferenceTransfer Transfer) noexcept
{
    if (!std::isfinite(EncodedValue))
    {
        return ScalarFailure(FStatus::NonFiniteInput);
    }
    if (Transfer == EOutputTransformReferenceTransfer::St2084)
    {
        return ScalarSuccess(PqDecode(EncodedValue));
    }
    const double Encoded = std::max(0.0, EncodedValue);
    if (Transfer == EOutputTransformReferenceTransfer::Srgb)
    {
        return ScalarSuccess(Encoded <= 0.04045
            ? Encoded / 12.92
            : std::pow((Encoded + 0.055) / 1.055, 2.4));
    }
    if (Transfer == EOutputTransformReferenceTransfer::Bt709)
    {
        return ScalarSuccess(Encoded < 0.081
            ? Encoded / 4.5
            : std::pow((Encoded + 0.099) / 1.099, 1.0 / 0.45));
    }
    if (Transfer == EOutputTransformReferenceTransfer::Gamma22)
    {
        return ScalarSuccess(std::pow(Encoded, 2.2));
    }
    return ScalarFailure(FStatus::InvalidEncoding);
}

FOutputTransformReferenceRgbResult FOutputTransformReference::EncodeLinearHdr(
    const FOutputTransformReferenceRgb& AbsoluteNits,
    EOutputTransformReferenceNativeEncoding Encoding,
    double NativeReferenceWhiteNits) noexcept
{
    if (!IsFinite(AbsoluteNits))
    {
        return Failure(FStatus::NonFiniteInput);
    }
    if (Encoding == EOutputTransformReferenceNativeEncoding::PqPacked10)
    {
        return Success({PqEncode(AbsoluteNits.R), PqEncode(AbsoluteNits.G),
            PqEncode(AbsoluteNits.B)});
    }
    if (NativeReferenceWhiteNits <= 0.0 || !std::isfinite(NativeReferenceWhiteNits))
    {
        return Failure(FStatus::InvalidReferenceWhite);
    }
    const double Scale = NativeScale(Encoding, NativeReferenceWhiteNits);
    return Success({AbsoluteNits.R / Scale, AbsoluteNits.G / Scale,
        AbsoluteNits.B / Scale});
}

FOutputTransformReferenceRgbResult FOutputTransformReference::DecodeLinearHdr(
    const FOutputTransformReferenceRgb& NativeValues,
    EOutputTransformReferenceNativeEncoding Encoding,
    double NativeReferenceWhiteNits) noexcept
{
    if (!IsFinite(NativeValues))
    {
        return Failure(FStatus::NonFiniteInput);
    }
    if (Encoding == EOutputTransformReferenceNativeEncoding::PqPacked10)
    {
        return Success({PqDecode(NativeValues.R), PqDecode(NativeValues.G),
            PqDecode(NativeValues.B)});
    }
    if (NativeReferenceWhiteNits <= 0.0 || !std::isfinite(NativeReferenceWhiteNits))
    {
        return Failure(FStatus::InvalidReferenceWhite);
    }
    const double Scale = NativeScale(Encoding, NativeReferenceWhiteNits);
    return Success({NativeValues.R * Scale, NativeValues.G * Scale,
        NativeValues.B * Scale});
}

double FOutputTransformReference::ComputeCpuTolerance(double Expected) noexcept
{
    return std::max(1e-10, 1e-10 * std::abs(Expected));
}

FOutputTransformReferenceScalarResult
FOutputTransformReference::ComputeNativeQuantizationStep(
    double ExpectedNits, EOutputTransformReferenceNativeEncoding Encoding,
    double NativeReferenceWhiteNits) noexcept
{
    if (!std::isfinite(ExpectedNits))
    {
        return ScalarFailure(FStatus::NonFiniteInput);
    }
    if (Encoding == EOutputTransformReferenceNativeEncoding::PqPacked10)
    {
        const double Code = PqEncode(ExpectedNits) * 1023.0;
        int LowerCode = static_cast<int>(std::floor(Code));
        int UpperCode = static_cast<int>(std::ceil(Code));
        if (LowerCode == UpperCode)
        {
            LowerCode = std::max(0, LowerCode - 1);
            UpperCode = std::min(1023, UpperCode + 1);
        }
        const double LowerNits = PqDecode(static_cast<double>(LowerCode) / 1023.0);
        const double UpperNits = PqDecode(static_cast<double>(UpperCode) / 1023.0);
        return ScalarSuccess(std::max(std::abs(ExpectedNits - LowerNits),
            std::abs(UpperNits - ExpectedNits)));
    }
    if (NativeReferenceWhiteNits <= 0.0 || !std::isfinite(NativeReferenceWhiteNits))
    {
        return ScalarFailure(FStatus::InvalidReferenceWhite);
    }
    const double Scale = NativeScale(Encoding, NativeReferenceWhiteNits);
    return ScalarSuccess(HalfNativeStep(ExpectedNits / Scale) * Scale);
}

FOutputTransformReferenceRgbResult
FOutputTransformReference::ComputeHdrRgbTolerance(
    const FOutputTransformReferenceRgb& ExpectedNits,
    EOutputTransformReferenceNativeEncoding Encoding,
    double NativeReferenceWhiteNits) noexcept
{
    if (!IsFinite(ExpectedNits))
    {
        return Failure(FStatus::NonFiniteInput);
    }
    const auto ComponentTolerance = [Encoding, NativeReferenceWhiteNits](double Value) {
        const auto Step = ComputeNativeQuantizationStep(
            Value, Encoding, NativeReferenceWhiteNits);
        if (!Step.IsSuccess())
        {
            return Step;
        }
        const double Multiplier =
            Encoding == EOutputTransformReferenceNativeEncoding::PqPacked10
            ? 1.5
            : 2.0;
        return ScalarSuccess(std::max({0.02,
            0.0025 * std::max(1.0, std::abs(Value)),
            Multiplier * Step.Value}));
    };
    const auto Red = ComponentTolerance(ExpectedNits.R);
    const auto Green = ComponentTolerance(ExpectedNits.G);
    const auto Blue = ComponentTolerance(ExpectedNits.B);
    if (!Red.IsSuccess() || !Green.IsSuccess() || !Blue.IsSuccess())
    {
        return Failure(!Red.IsSuccess() ? Red.Status
            : (!Green.IsSuccess() ? Green.Status : Blue.Status));
    }
    return Success({Red.Value, Green.Value, Blue.Value});
}

FOutputTransformReferenceXyz
FOutputTransformReference::PropagateRgbToleranceToXyz(
    const FOutputTransformReferenceRgb& RgbTolerance,
    EOutputTransformReferenceColorSpace Source) noexcept
{
    const auto& Matrix = Source == EOutputTransformReferenceColorSpace::Rec709D65
        ? Rec709ToXyz
        : Rec2020ToXyz;
    constexpr double Epsilon = 1e-6;
    return {
        std::abs(Matrix[0]) * RgbTolerance.R +
            std::abs(Matrix[1]) * RgbTolerance.G +
            std::abs(Matrix[2]) * RgbTolerance.B + Epsilon,
        std::abs(Matrix[3]) * RgbTolerance.R +
            std::abs(Matrix[4]) * RgbTolerance.G +
            std::abs(Matrix[5]) * RgbTolerance.B + Epsilon,
        std::abs(Matrix[6]) * RgbTolerance.R +
            std::abs(Matrix[7]) * RgbTolerance.G +
            std::abs(Matrix[8]) * RgbTolerance.B + Epsilon};
}

} // namespace Stoner::Renderer
