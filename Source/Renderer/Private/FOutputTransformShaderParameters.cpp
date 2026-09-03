#include "FOutputTransformShaderParameters.h"

#include <array>
#include <cmath>

namespace Stoner::Renderer::Private
{

namespace
{

Stoner::Core::uint32 ProfileIndex(const Stoner::Core::FString& Id) noexcept
{
    constexpr std::array<const char*, 7> Values = {
        "Sdr.sRGB.v1", "Sdr.BT709.v1", "Sdr.ExplicitGamma22.v1",
        "Hdr.PQ.Rec2020.1000.v1", "Hdr.PQ.Rec2020.2000.v1",
        "Hdr.Linear.1000.v1", "Hdr.Linear.2000.v1"};
    for (Stoner::Core::uint32 Index = 0; Index < Values.size(); ++Index)
    {
        if (Id == Values[Index]) return Index + 1;
    }
    return 0;
}

EOutputTransformReferenceSdrToneMap SdrStrategy(
    Stoner::Core::uint32 Index) noexcept
{
    switch (Index)
    {
    case 1: return EOutputTransformReferenceSdrToneMap::KhronosPbrNeutral;
    case 2: return EOutputTransformReferenceSdrToneMap::NarkowiczAcesFit;
    case 3: return EOutputTransformReferenceSdrToneMap::ExtendedReinhardRec709;
    default: return EOutputTransformReferenceSdrToneMap::KhronosPbrNeutral;
    }
}

EOutputTransformReferenceTransfer Transfer(
    EOutputTransferFunction Value) noexcept
{
    switch (Value)
    {
    case EOutputTransferFunction::Srgb:
        return EOutputTransformReferenceTransfer::Srgb;
    case EOutputTransferFunction::Bt709:
        return EOutputTransformReferenceTransfer::Bt709;
    case EOutputTransferFunction::Gamma22:
        return EOutputTransformReferenceTransfer::Gamma22;
    case EOutputTransferFunction::St2084:
        return EOutputTransformReferenceTransfer::St2084;
    default:
        return EOutputTransformReferenceTransfer::Srgb;
    }
}

EOutputTransformReferenceNativeEncoding NativeEncoding(
    Stoner::RHI::ERHIPresentationNativeEncoding Value) noexcept
{
    switch (Value)
    {
    case Stoner::RHI::ERHIPresentationNativeEncoding::Pq:
        return EOutputTransformReferenceNativeEncoding::PqPacked10;
    case Stoner::RHI::ERHIPresentationNativeEncoding::ScRgb80:
        return EOutputTransformReferenceNativeEncoding::ScRgb80;
    case Stoner::RHI::ERHIPresentationNativeEncoding::MetalEdr:
        return EOutputTransformReferenceNativeEncoding::MetalEdr;
    default:
        return EOutputTransformReferenceNativeEncoding::PqPacked10;
    }
}

bool EncodeSdr(const FOutputTransformReferenceRgb& Linear,
    EOutputTransformReferenceTransfer TransferFunction,
    FOutputTransformReferenceRgb& OutEncoded,
    FOutputTransformReferenceRgb& OutDecoded) noexcept
{
    const auto Encode = [TransferFunction](double Value)
    { return FOutputTransformReference::EncodeTransfer(Value, TransferFunction); };
    const auto R = Encode(Linear.R);
    const auto G = Encode(Linear.G);
    const auto B = Encode(Linear.B);
    if (!R.IsSuccess() || !G.IsSuccess() || !B.IsSuccess()) return false;
    OutEncoded = {R.Value, G.Value, B.Value};
    const auto DR = FOutputTransformReference::DecodeTransfer(
        R.Value, TransferFunction);
    const auto DG = FOutputTransformReference::DecodeTransfer(
        G.Value, TransferFunction);
    const auto DB = FOutputTransformReference::DecodeTransfer(
        B.Value, TransferFunction);
    if (!DR.IsSuccess() || !DG.IsSuccess() || !DB.IsSuccess()) return false;
    OutDecoded = {DR.Value, DG.Value, DB.Value};
    return true;
}

} // namespace

bool FOutputTransformShaderParameterBinding::IsValid() const noexcept
{
    return std::isfinite(Parameters.ExposureScale) &&
        Parameters.ExposureScale > 0.0f &&
        std::isfinite(Parameters.ReferenceWhiteNits) &&
        Parameters.ReferenceWhiteNits > 0.0f &&
        std::isfinite(Parameters.TargetPeakNits) &&
        Parameters.TargetPeakNits >= Parameters.ReferenceWhiteNits &&
        Parameters.TransformStrategy != 0 && Parameters.OutputProfile != 0 &&
        !PipelineKey.IsEmpty() && ProfileRegistryDigest.Len() == 64 &&
        ReferenceVectorSetDigest.Len() == 64 &&
        TransformConstantsDigest.Len() == 64 &&
        TolerancePolicyDigest.Len() == 64 &&
        ExposureApplicationCount <= 1;
}

FOutputTransformShaderParameterBinding
FOutputTransformShaderParameterBuilder::Build(
    const FResolvedOutputTransformSettings& Settings,
    EOutputTransformShaderStageMode Stage) noexcept
{
    FOutputTransformShaderParameterBinding Out;
    if (!Settings.IsValid()) return Out;
    Out.Parameters.ExposureScale = Stage ==
        EOutputTransformShaderStageMode::ManualExposure
        ? std::exp2(Settings.ManualExposureStops) : 1.0f;
    Out.Parameters.ReferenceWhiteNits = Settings.ReferenceWhiteNits;
    Out.Parameters.TargetPeakNits = Settings.TargetPeakNits;
    Out.Parameters.StageMode = static_cast<Stoner::Core::uint32>(Stage);
    Out.Parameters.TransformStrategy = Settings.TransformStrategyIndex;
    Out.Parameters.OutputProfile = ProfileIndex(Settings.OutputDeviceProfileId);
    Out.Parameters.NativeEncoding =
        static_cast<Stoner::Core::uint32>(Settings.NativeEncoding);
    Out.PipelineKey = Settings.PipelineKey;
    Out.ProfileRegistryDigest = Settings.ProfileRegistryDigest;
    Out.ReferenceVectorSetDigest = Settings.ReferenceVectorSetDigest;
    Out.TransformConstantsDigest = Settings.TransformConstantsDigest;
    Out.TolerancePolicyDigest = Settings.TolerancePolicyDigest;
    Out.ExposureApplicationCount = Stage ==
        EOutputTransformShaderStageMode::ManualExposure ? 1U : 0U;
    return Out;
}

bool FFrozenOutputTransformConformanceAuthority::IsValid() const noexcept
{
    return ProfileRegistryDigest.Len() == 64 &&
        VectorManifestDigest.Len() == 64 && VectorSetDigest.Len() == 64 &&
        CaseCount == 32 && ExpectationCount == 224 &&
        !bCanGenerateExpectedValues;
}

bool FOutputTransformConformanceReport::IsValid() const noexcept
{
    return bSucceeded && bComparedDecodedDomain &&
        Stoner::RHI::IsValidPresentationNativeEncoding(StorageEncoding) &&
        FOutputTransformReference::IsFinite(Encoded) &&
        FOutputTransformReference::IsFinite(Decoded) &&
        FOutputTransformReference::IsFinite(DecodedTolerance) &&
        std::isfinite(ExpectedXyz.X) && std::isfinite(ExpectedXyz.Y) &&
        std::isfinite(ExpectedXyz.Z);
}

FFrozenOutputTransformConformanceAuthority
LoadFrozenOutputTransformConformanceAuthority()
{
    return {GOutputTransformProfileRegistrySha256,
        GOutputTransformVectorManifestSha256,
        GOutputTransformVectorSetSha256, 32, 224, false};
}

FOutputTransformConformanceReport EvaluateOutputTransformConformance(
    const FResolvedOutputTransformSettings& Settings,
    FOutputTransformReferenceRgb SceneLinear) noexcept
{
    FOutputTransformConformanceReport Out;
    if (!Settings.IsValid() ||
        !FOutputTransformReference::IsFinite(SceneLinear)) return Out;
    const auto Exposed = FOutputTransformReference::ApplyManualExposure(
        SceneLinear, Settings.ManualExposureStops);
    const auto Boundary = Exposed.IsSuccess()
        ? FOutputTransformReference::ClampAtTransformBoundary(Exposed.Value)
        : FOutputTransformReferenceRgbResult{};
    if (!Boundary.IsSuccess()) return Out;

    Out.ComparisonDomain = Settings.ComparisonDomain;
    Out.StorageEncoding = Settings.NativeEncoding;
    Out.bComparedDecodedDomain = true;
    if (Settings.DynamicRange == EOutputDynamicRange::SDR)
    {
        const auto Display = FOutputTransformReference::ApplySdrToneMap(
            Boundary.Value, SdrStrategy(Settings.TransformStrategyIndex));
        if (!Display.IsSuccess() || !EncodeSdr(Display.Value,
                Transfer(Settings.Transfer), Out.Encoded, Out.Decoded)) return Out;
        Out.DecodedTolerance = {2.0 / 255.0, 2.0 / 255.0, 2.0 / 255.0};
        Out.ExpectedXyz = FOutputTransformReference::ConvertToXyz(
            Out.Decoded, EOutputTransformReferenceColorSpace::Rec709D65);
    }
    else
    {
        const EOutputTransformReferenceColorSpace ColorSpace =
            Settings.TargetPrimaries == EOutputColorPrimaries::Rec2020
            ? EOutputTransformReferenceColorSpace::Rec2020D65
            : EOutputTransformReferenceColorSpace::Rec709D65;
        const auto Display = FOutputTransformReference::ApplyAces2HdrViewing(
            Boundary.Value, Settings.TargetPeakNits, ColorSpace);
        if (!Display.IsSuccess()) return Out;
        const auto Encoding = NativeEncoding(Settings.NativeEncoding);
        const auto Encoded = Settings.NativeEncoding ==
            Stoner::RHI::ERHIPresentationNativeEncoding::Pq
            ? FOutputTransformReferenceRgbResult{
                EOutputTransformReferenceStatus::Success,
                {FOutputTransformReference::EncodeTransfer(Display.Value.R,
                    EOutputTransformReferenceTransfer::St2084).Value,
                 FOutputTransformReference::EncodeTransfer(Display.Value.G,
                    EOutputTransformReferenceTransfer::St2084).Value,
                 FOutputTransformReference::EncodeTransfer(Display.Value.B,
                    EOutputTransformReferenceTransfer::St2084).Value}}
            : FOutputTransformReference::EncodeLinearHdr(
                Display.Value, Encoding, Settings.ReferenceWhiteNits);
        if (!Encoded.IsSuccess()) return Out;
        Out.Encoded = Encoded.Value;
        if (Settings.NativeEncoding ==
            Stoner::RHI::ERHIPresentationNativeEncoding::Pq)
        {
            Out.Decoded = {
                FOutputTransformReference::DecodeTransfer(Out.Encoded.R,
                    EOutputTransformReferenceTransfer::St2084).Value,
                FOutputTransformReference::DecodeTransfer(Out.Encoded.G,
                    EOutputTransformReferenceTransfer::St2084).Value,
                FOutputTransformReference::DecodeTransfer(Out.Encoded.B,
                    EOutputTransformReferenceTransfer::St2084).Value};
        }
        else
        {
            const auto Decoded = FOutputTransformReference::DecodeLinearHdr(
                Out.Encoded, Encoding, Settings.ReferenceWhiteNits);
            if (!Decoded.IsSuccess()) return Out;
            Out.Decoded = Decoded.Value;
        }
        const auto Tolerance = FOutputTransformReference::ComputeHdrRgbTolerance(
            Display.Value, Encoding, Settings.ReferenceWhiteNits);
        if (!Tolerance.IsSuccess()) return Out;
        Out.DecodedTolerance = Tolerance.Value;
        Out.ExpectedXyz = FOutputTransformReference::ConvertToXyz(
            Out.Decoded, ColorSpace);
    }
    Out.bSucceeded = true;
    return Out;
}

} // namespace Stoner::Renderer::Private
