#include "RendererOutputTransformMathTests.h"

#include "FOutputTransformReference.h"
#include "FOutputTransformAces2Reference.h"
#include "FOutputTransformShaderParameters.h"
#include "Renderer/FOutputTransformSettings.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>

namespace
{

using namespace Stoner::Renderer;

void Record(FRendererOutputTransformMathTestResult& Result, bool bPassed,
    const char* Name)
{
    (bPassed ? ++Result.Passed : ++Result.Failed);
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

bool Near(double Left, double Right, double Tolerance = 1e-10)
{
    return std::abs(Left - Right) <= Tolerance;
}

bool Near(const FOutputTransformReferenceRgb& Left,
    const FOutputTransformReferenceRgb& Right, double Tolerance = 1e-10)
{
    return Near(Left.R, Right.R, Tolerance) &&
        Near(Left.G, Right.G, Tolerance) &&
        Near(Left.B, Right.B, Tolerance);
}

void TestExposureAndBoundary(FRendererOutputTransformMathTestResult& Result)
{
    const FOutputTransformReferenceRgb Input{-0.25, 0.5, 8.0};
    const auto Neutral = FOutputTransformReference::ApplyManualExposure(Input, 0.0);
    const auto Up = FOutputTransformReference::ApplyManualExposure(Input, 1.0);
    const auto Down = FOutputTransformReference::ApplyManualExposure(Input, -1.0);
    Record(Result, Neutral.IsSuccess() && Near(Neutral.Value, Input),
        "zero-stop exposure preserves finite scene-linear values");
    Record(Result, Up.IsSuccess() && Near(Up.Value, {-0.5, 1.0, 16.0}),
        "one-stop exposure increase doubles the pre-tonemap signal");
    Record(Result, Down.IsSuccess() && Near(Down.Value, {-0.125, 0.25, 4.0}),
        "one-stop exposure decrease halves the pre-tonemap signal");

    const auto Boundary =
        FOutputTransformReference::ClampAtTransformBoundary(Neutral.Value);
    Record(Result, Boundary.IsSuccess() && Near(Boundary.Value, {0.0, 0.5, 8.0}),
        "finite negative values remain inspectable until the transform boundary");

    const auto TooHigh = FOutputTransformReference::ApplyManualExposure(Input, 17.0);
    const auto NotFinite = FOutputTransformReference::ApplyManualExposure(
        {0.0, std::numeric_limits<double>::infinity(), 1.0}, 0.0);
    Record(Result,
        TooHigh.Status == EOutputTransformReferenceStatus::ExposureOutOfRange &&
        NotFinite.Status == EOutputTransformReferenceStatus::NonFiniteInput,
        "exposure rejects out-of-range and non-finite inputs");

    constexpr std::array<double, 8> Expected =
        {-16.0, -8.0, -1.0, 0.0, 1.0, 8.0, 15.0, 16.0};
    Record(Result, FOutputTransformReference::ExposureSamples == Expected,
        "the finite exposure sample set includes every one-stop endpoint");
}

void TestSdrStrategies(FRendererOutputTransformMathTestResult& Result)
{
    const auto Khronos = FOutputTransformReference::ApplySdrToneMap(
        {1.0, 1.0, 1.0},
        EOutputTransformReferenceSdrToneMap::KhronosPbrNeutral);
    const auto Narkowicz = FOutputTransformReference::ApplySdrToneMap(
        {1.0, 1.0, 1.0},
        EOutputTransformReferenceSdrToneMap::NarkowiczAcesFit);
    const auto Reinhard = FOutputTransformReference::ApplySdrToneMap(
        {1.0, 1.0, 1.0},
        EOutputTransformReferenceSdrToneMap::ExtendedReinhardRec709);
    const auto ReinhardOverWhite = FOutputTransformReference::ApplySdrToneMap(
        {128.0, 128.0, 128.0},
        EOutputTransformReferenceSdrToneMap::ExtendedReinhardRec709);
    Record(Result, Khronos.IsSuccess() &&
        Near(Khronos.Value, {0.8690909090909091, 0.8690909090909091,
            0.8690909090909091}),
        "Khronos PBR Neutral follows the frozen analytic curve");
    Record(Result, Narkowicz.IsSuccess() &&
        Near(Narkowicz.Value, {0.8037974683544302, 0.8037974683544302,
            0.8037974683544302}),
        "Narkowicz remains the explicitly named sampled ACES fit");
    Record(Result, Reinhard.IsSuccess() &&
        Near(Reinhard.Value, {0.53125, 0.53125, 0.53125}) &&
        ReinhardOverWhite.IsSuccess() &&
        Near(ReinhardOverWhite.Value, {1.0, 1.0, 1.0}),
        "Extended Reinhard uses frozen Rec709 luminance and Lwhite four");
}

void TestTransfersAndGamut(FRendererOutputTransformMathTestResult& Result)
{
    constexpr std::array<EOutputTransformReferenceTransfer, 4> Transfers = {
        EOutputTransformReferenceTransfer::Srgb,
        EOutputTransformReferenceTransfer::Bt709,
        EOutputTransformReferenceTransfer::Gamma22,
        EOutputTransformReferenceTransfer::St2084};
    bool bRoundTrips = true;
    for (const auto Transfer : Transfers)
    {
        const double Linear = Transfer == EOutputTransformReferenceTransfer::St2084
            ? 100.0
            : 0.18;
        const auto Encoded =
            FOutputTransformReference::EncodeTransfer(Linear, Transfer);
        const auto Decoded = Encoded.IsSuccess()
            ? FOutputTransformReference::DecodeTransfer(Encoded.Value, Transfer)
            : FOutputTransformReferenceScalarResult{};
        bRoundTrips = bRoundTrips && Encoded.IsSuccess() && Decoded.IsSuccess() &&
            Near(Decoded.Value, Linear, FOutputTransformReference::ComputeCpuTolerance(Linear));
    }
    Record(Result, bRoundTrips,
        "sRGB BT709 gamma22 and absolute ST2084 round-trip in double precision");

    const auto Rec2020 = FOutputTransformReference::ConvertColorSpace(
        {1.0, 1.0, 1.0}, EOutputTransformReferenceColorSpace::Rec709D65,
        EOutputTransformReferenceColorSpace::Rec2020D65);
    const auto Rec709 = Rec2020.IsSuccess()
        ? FOutputTransformReference::ConvertColorSpace(Rec2020.Value,
            EOutputTransformReferenceColorSpace::Rec2020D65,
            EOutputTransformReferenceColorSpace::Rec709D65)
        : FOutputTransformReferenceRgbResult{};
    Record(Result, Rec709.IsSuccess() && Near(Rec709.Value, {1.0, 1.0, 1.0}),
        "frozen Rec709 and Rec2020 D65 matrices preserve neutral and invert");
}

void TestHdrAndTolerances(FRendererOutputTransformMathTestResult& Result)
{
    const auto Direct1000 = Stoner::Renderer::Private::ApplyAces2OutputTransform(
        {0.5, 0.4, 0.3}, 1000.0,
        Stoner::Renderer::Private::EAces2ReferenceGamut::Rec2020D65);
    const auto Direct2000 = Stoner::Renderer::Private::ApplyAces2OutputTransform(
        {0.5, 0.4, 0.3}, 2000.0,
        Stoner::Renderer::Private::EAces2ReferenceGamut::Rec2020D65);
    const auto Direct1000Xyz = FOutputTransformReference::ConvertToXyz(
        {Direct1000[0] / 100.0, Direct1000[1] / 100.0,
            Direct1000[2] / 100.0},
        EOutputTransformReferenceColorSpace::Rec2020D65);
    const auto Direct2000Xyz = FOutputTransformReference::ConvertToXyz(
        {Direct2000[0] / 100.0, Direct2000[1] / 100.0,
            Direct2000[2] / 100.0},
        EOutputTransformReferenceColorSpace::Rec2020D65);
    Record(Result,
        Near(Direct1000Xyz.X, 0.46536580, 1e-4) &&
        Near(Direct1000Xyz.Y, 0.43852842, 1e-4) &&
        Near(Direct1000Xyz.Z, 0.33688098, 1e-4) &&
        Near(Direct2000Xyz.X, 0.51225960, 1e-4) &&
        Near(Direct2000Xyz.Y, 0.48264492, 1e-4) &&
        Near(Direct2000Xyz.Z, 0.37060046, 1e-4),
        "ACES2 1000/2000-nit Rec2020 output matches released reference points");

    const auto Hdr1000 = FOutputTransformReference::ApplyAces2HdrViewing(
        {0.18, 0.18, 0.18}, 1000.0,
        EOutputTransformReferenceColorSpace::Rec2020D65);
    const auto Hdr2000 = FOutputTransformReference::ApplyAces2HdrViewing(
        {0.18, 0.18, 0.18}, 2000.0,
        EOutputTransformReferenceColorSpace::Rec2020D65);
    Record(Result, Hdr1000.IsSuccess() && Hdr2000.IsSuccess() &&
        FOutputTransformReference::IsFinite(Hdr1000.Value) &&
        Hdr1000.Value.R >= 0.0 && Hdr1000.Value.R <= 1000.0 &&
        Hdr2000.Value.R >= 0.0 && Hdr2000.Value.R <= 2000.0,
        "official ACES2 HDR viewing presets produce finite absolute nits");

    const auto PqStep = FOutputTransformReference::ComputeNativeQuantizationStep(
        100.0, EOutputTransformReferenceNativeEncoding::PqPacked10, 100.0);
    const auto PqTolerance = FOutputTransformReference::ComputeHdrRgbTolerance(
        {100.0, 500.0, 1000.0},
        EOutputTransformReferenceNativeEncoding::PqPacked10, 100.0);
    const auto XyzTolerance = FOutputTransformReference::PropagateRgbToleranceToXyz(
        PqTolerance.Value, EOutputTransformReferenceColorSpace::Rec2020D65);
    Record(Result, PqStep.IsSuccess() && PqStep.Value > 0.0 &&
        PqTolerance.IsSuccess() && PqTolerance.Value.R >= 0.25 &&
        XyzTolerance.X > 0.0 && XyzTolerance.Y > 0.0 && XyzTolerance.Z > 0.0,
        "native quantization and matrix-propagated XYZ tolerances are positive and bounded");

    const auto ScRgb = FOutputTransformReference::EncodeLinearHdr(
        {80.0, 1000.0, 2000.0},
        EOutputTransformReferenceNativeEncoding::ScRgb80, 100.0);
    const auto ScRgbDecoded = ScRgb.IsSuccess()
        ? FOutputTransformReference::DecodeLinearHdr(ScRgb.Value,
            EOutputTransformReferenceNativeEncoding::ScRgb80, 100.0)
        : FOutputTransformReferenceRgbResult{};
    const auto Edr = FOutputTransformReference::EncodeLinearHdr(
        {100.0, 1000.0, 2000.0},
        EOutputTransformReferenceNativeEncoding::MetalEdr, 100.0);
    Record(Result, ScRgbDecoded.IsSuccess() &&
        Near(ScRgbDecoded.Value, {80.0, 1000.0, 2000.0}) &&
        Edr.IsSuccess() && Near(Edr.Value, {1.0, 10.0, 20.0}),
        "scRGB80 and Metal EDR use distinct explicit luminance packers");
}

void TestRuntimeStrategyAndShaderBindings(
    FRendererOutputTransformMathTestResult& Result)
{
    FOutputTransformSettingsValidator Validator;
    constexpr std::array<const char*, 3> Strategies = {
        "Sdr.KhronosPbrNeutral.v1",
        "Sdr.NarkowiczAcesFit.v1",
        "Sdr.ExtendedReinhardRec709.v1"};
    bool bStrategies = true;
    Stoner::Core::TArray<Stoner::Core::uint32> Indices;
    for (const char* Version : Strategies)
    {
        FOutputTransformSettings Settings;
        Settings.SDRToneMapVersion = Version;
        const auto Resolved = Validator.Validate(Settings);
        const auto* Strategy = Validator.FindStrategy(Version);
        bStrategies = bStrategies && Resolved.Succeeded() && Strategy &&
            Strategy->VersionId == Version &&
            Resolved.Settings.TransformStrategyVersion == Version &&
            !Resolved.Settings.TransformConstantsDigest.IsEmpty();
        if (Strategy) Indices.push_back(Strategy->ShaderStrategyIndex);
    }
    Record(Result, bStrategies && Indices.size() == 3 &&
        Indices[0] != Indices[1] && Indices[1] != Indices[2] &&
        Indices[0] != Indices[2],
        "runtime selects three distinct versioned SDR Strategies");

    FOutputTransformSettings Default;
    Default.ManualExposureStops = -0.0f;
    const auto DefaultResolved = Validator.Validate(Default);
    const auto Parameters = DefaultResolved.Succeeded()
        ? Private::FOutputTransformShaderParameterBuilder::Build(
            DefaultResolved.Settings,
            Private::EOutputTransformShaderStageMode::ManualExposure)
        : Private::FOutputTransformShaderParameterBinding{};
    Record(Result, DefaultResolved.Succeeded() &&
        DefaultResolved.Settings.ManualExposureStops == 0.0f &&
        DefaultResolved.Settings.SDRToneMapVersion ==
            GDefaultSDRToneMapVersion &&
        DefaultResolved.Settings.OutputDeviceProfileId ==
            GDefaultSDROutputDeviceProfile &&
        Parameters.IsValid() && Parameters.ExposureApplicationCount == 1 &&
        Near(Parameters.Parameters.ExposureScale, 1.0),
        "frozen defaults materialize explicitly and bind exposure exactly once");

    FOutputTransformSettings OneStop = Default;
    OneStop.ManualExposureStops = 1.0f;
    const auto OneStopResolved = Validator.Validate(OneStop);
    const auto OneStopParameters = OneStopResolved.Succeeded()
        ? Private::FOutputTransformShaderParameterBuilder::Build(
            OneStopResolved.Settings,
            Private::EOutputTransformShaderStageMode::ManualExposure)
        : Private::FOutputTransformShaderParameterBinding{};
    Record(Result, OneStopParameters.IsValid() &&
        OneStopParameters.ExposureApplicationCount == 1 &&
        Near(OneStopParameters.Parameters.ExposureScale, 2.0),
        "runtime one-stop exposure binding is the CPU-oracle factor of two");

    FOutputTransformSettings Unknown = Default;
    Unknown.SDRToneMapVersion = "Sdr.Unknown.v9";
    const auto Rejected = Validator.Validate(Unknown);
    Record(Result, !Rejected.Succeeded() &&
        Rejected.Result == EOutputTransformResult::Unsupported &&
        Rejected.Diagnostics.HasError(),
        "runtime rejects unknown Strategy versions without fallback");
}

} // namespace

FRendererOutputTransformMathTestResult RunRendererOutputTransformMathTests()
{
    FRendererOutputTransformMathTestResult Result;
    Record(Result, FOutputTransformReference::IsImplemented(),
        "the Feature 029 double-precision color oracle is implemented");
    TestExposureAndBoundary(Result);
    TestSdrStrategies(Result);
    TestTransfersAndGamut(Result);
    TestHdrAndTolerances(Result);
    TestRuntimeStrategyAndShaderBindings(Result);
    return Result;
}
