#include "Renderer/FOutputTransformSettings.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <utility>

namespace Stoner::Renderer
{

namespace
{

constexpr Stoner::Core::uint32 GMaximumOutputExtent = 32768;

Stoner::Core::FString BuildPipelineKey(
    const FOutputTransformStrategyIdentity& Strategy,
    const FOutputDeviceProfile& Profile,
    Stoner::RHI::ERHIPresentationNativeEncoding NativeEncoding,
    float ReferenceWhiteNits)
{
    std::ostringstream Stream;
    Stream.imbue(std::locale::classic());
    Stream << "OutputTransform.v1|strategy=" << Strategy.VersionId.CStr()
        << "|strategyIndex=" << Strategy.ShaderStrategyIndex
        << "|profile=" << Profile.ProfileId.CStr()
        << "|profileVersion=" << Profile.ProfileVersion.CStr()
        << "|native=" << Stoner::RHI::ToString(NativeEncoding)
        << "|referenceWhite=" << std::setprecision(9) << ReferenceWhiteNits
        << "|peak=" << Profile.TargetPeakNits
        << "|format=" << static_cast<int>(Profile.Format)
        << "|colorSpace=" << static_cast<int>(Profile.ColorSpace)
        << "|transfer=" << static_cast<int>(Profile.Transfer)
        << "|storage=" << static_cast<int>(Profile.StorageClass)
        << "|metadata=" << static_cast<int>(Profile.MetadataPolicy)
        << "|comparison=" << static_cast<int>(Profile.ComparisonDomain)
        << "|displayDomain=" << static_cast<int>(Profile.DisplayLinearDomain)
        << "|encodedDomain=" << static_cast<int>(Profile.EncodedDomain)
        << "|registry=" << GOutputTransformProfileRegistrySha256
        << "|vectors=" << GOutputTransformVectorSetSha256
        << "|manifest=" << GOutputTransformVectorManifestSha256
        << "|constants=" << Strategy.ConstantsDigest.CStr()
        << "|tolerance=" << GOutputTransformTolerancePolicySha256
        << "|shader=" << Strategy.ShaderVariantId.CStr();
    return Stoner::Core::FString(Stream.str());
}

FOutputDeviceProfile MakeProfile(const char* Id,
    EOutputDynamicRange Range,
    EOutputColorPrimaries Primaries,
    float ReferenceWhite,
    float Peak,
    EOutputTransferFunction Transfer,
    EOutputProfileStorageClass Storage,
    Stoner::RHI::ERHIFormat Format,
    Stoner::RHI::ERHIPresentationColorSpace ColorSpace,
    EOutputMetadataPolicy Metadata,
    EOutputComparisonDomain Comparison,
    Stoner::RHI::ERHIPresentationNativeEncoding PrimaryEncoding,
    Stoner::RHI::ERHIPresentationNativeEncoding SecondaryEncoding,
    ERenderGraphColorDomain DisplayDomain,
    ERenderGraphColorDomain EncodedDomain,
    const char* Tolerance)
{
    FOutputDeviceProfile Result;
    Result.ProfileId = Id;
    Result.ProfileVersion = "1";
    Result.DynamicRange = Range;
    Result.TargetPrimaries = Primaries;
    Result.ReferenceWhiteNits = ReferenceWhite;
    Result.TargetPeakNits = Peak;
    Result.Transfer = Transfer;
    Result.StorageClass = Storage;
    Result.Format = Format;
    Result.ColorSpace = ColorSpace;
    Result.MetadataPolicy = Metadata;
    Result.ComparisonDomain = Comparison;
    Result.PrimaryNativeEncoding = PrimaryEncoding;
    Result.SecondaryNativeEncoding = SecondaryEncoding;
    Result.DisplayLinearDomain = DisplayDomain;
    Result.EncodedDomain = EncodedDomain;
    Result.TolerancePolicyId = Tolerance;
    return Result;
}

const std::array<FOutputDeviceProfile, 7>& Profiles()
{
    using namespace Stoner::RHI;
    static const std::array<FOutputDeviceProfile, 7> Values = {
        MakeProfile("Sdr.sRGB.v1", EOutputDynamicRange::SDR,
            EOutputColorPrimaries::Rec709, 100.0f, 100.0f,
            EOutputTransferFunction::Srgb, EOutputProfileStorageClass::UNorm8,
            ERHIFormat::R8G8B8A8_UNorm,
            ERHIPresentationColorSpace::SrgbNonlinear,
            EOutputMetadataPolicy::None, EOutputComparisonDomain::LinearRec709,
            ERHIPresentationNativeEncoding::SdrExplicit,
            ERHIPresentationNativeEncoding::Unknown,
            ERenderGraphColorDomain::DisplayLinearRec709D65,
            ERenderGraphColorDomain::EncodedSrgb, "sdr-linear-rgb-v1"),
        MakeProfile("Sdr.BT709.v1", EOutputDynamicRange::SDR,
            EOutputColorPrimaries::Rec709, 100.0f, 100.0f,
            EOutputTransferFunction::Bt709, EOutputProfileStorageClass::UNorm8,
            ERHIFormat::R8G8B8A8_UNorm,
            ERHIPresentationColorSpace::Bt709Nonlinear,
            EOutputMetadataPolicy::None, EOutputComparisonDomain::LinearRec709,
            ERHIPresentationNativeEncoding::SdrExplicit,
            ERHIPresentationNativeEncoding::Unknown,
            ERenderGraphColorDomain::DisplayLinearRec709D65,
            ERenderGraphColorDomain::EncodedBt709, "sdr-linear-rgb-v1"),
        MakeProfile("Sdr.ExplicitGamma22.v1", EOutputDynamicRange::SDR,
            EOutputColorPrimaries::Rec709, 100.0f, 100.0f,
            EOutputTransferFunction::Gamma22,
            EOutputProfileStorageClass::UNorm8,
            ERHIFormat::R8G8B8A8_UNorm,
            ERHIPresentationColorSpace::SdrPassThrough,
            EOutputMetadataPolicy::None, EOutputComparisonDomain::LinearRec709,
            ERHIPresentationNativeEncoding::SdrExplicit,
            ERHIPresentationNativeEncoding::Unknown,
            ERenderGraphColorDomain::DisplayLinearRec709D65,
            ERenderGraphColorDomain::EncodedGamma22, "sdr-linear-rgb-v1"),
        MakeProfile("Hdr.PQ.Rec2020.1000.v1", EOutputDynamicRange::HDR,
            EOutputColorPrimaries::Rec2020, 100.0f, 1000.0f,
            EOutputTransferFunction::St2084,
            EOutputProfileStorageClass::Packed10UNorm,
            ERHIFormat::R10G10B10A2_UNorm,
            ERHIPresentationColorSpace::Hdr10St2084,
            EOutputMetadataPolicy::HDR10Static,
            EOutputComparisonDomain::AbsoluteNitsXyz,
            ERHIPresentationNativeEncoding::Pq,
            ERHIPresentationNativeEncoding::Unknown,
            ERenderGraphColorDomain::DisplayLinearRec2020D65,
            ERenderGraphColorDomain::EncodedPqRec2020D65,
            "hdr-pq10-decoded-v1"),
        MakeProfile("Hdr.PQ.Rec2020.2000.v1", EOutputDynamicRange::HDR,
            EOutputColorPrimaries::Rec2020, 100.0f, 2000.0f,
            EOutputTransferFunction::St2084,
            EOutputProfileStorageClass::Packed10UNorm,
            ERHIFormat::R10G10B10A2_UNorm,
            ERHIPresentationColorSpace::Hdr10St2084,
            EOutputMetadataPolicy::HDR10Static,
            EOutputComparisonDomain::AbsoluteNitsXyz,
            ERHIPresentationNativeEncoding::Pq,
            ERHIPresentationNativeEncoding::Unknown,
            ERenderGraphColorDomain::DisplayLinearRec2020D65,
            ERenderGraphColorDomain::EncodedPqRec2020D65,
            "hdr-pq10-decoded-v1"),
        MakeProfile("Hdr.Linear.1000.v1", EOutputDynamicRange::HDR,
            EOutputColorPrimaries::Rec709, 80.0f, 1000.0f,
            EOutputTransferFunction::Linear, EOutputProfileStorageClass::Float16,
            ERHIFormat::R16G16B16A16_Float,
            ERHIPresentationColorSpace::ExtendedSrgbLinear,
            EOutputMetadataPolicy::EDRState,
            EOutputComparisonDomain::AbsoluteNitsXyz,
            ERHIPresentationNativeEncoding::ScRgb80,
            ERHIPresentationNativeEncoding::MetalEdr,
            ERenderGraphColorDomain::DisplayLinearRec709D65,
            ERenderGraphColorDomain::ExtendedSrgbLinear,
            "hdr-fp16-decoded-v1"),
        MakeProfile("Hdr.Linear.2000.v1", EOutputDynamicRange::HDR,
            EOutputColorPrimaries::Rec709, 80.0f, 2000.0f,
            EOutputTransferFunction::Linear, EOutputProfileStorageClass::Float16,
            ERHIFormat::R16G16B16A16_Float,
            ERHIPresentationColorSpace::ExtendedSrgbLinear,
            EOutputMetadataPolicy::EDRState,
            EOutputComparisonDomain::AbsoluteNitsXyz,
            ERHIPresentationNativeEncoding::ScRgb80,
            ERHIPresentationNativeEncoding::MetalEdr,
            ERenderGraphColorDomain::DisplayLinearRec709D65,
            ERenderGraphColorDomain::ExtendedSrgbLinear,
            "hdr-fp16-decoded-v1")};
    return Values;
}

FOutputTransformStrategyIdentity MakeStrategy(const char* Id,
    EOutputTransformStrategyFamily Family,
    Stoner::Core::uint32 Index)
{
    FOutputTransformStrategyIdentity Result;
    Result.VersionId = Id;
    Result.Family = Family;
    Result.ShaderStrategyIndex = Index;
    Result.ConstantsDigest = GOutputTransformConstantsSha256;
    Result.ShaderVariantId = Stoner::Core::FString(
        "OutputTransform.v1.strategy" + std::to_string(Index));
    Result.ReferenceVectorSetId = GOutputTransformVectorSetId;
    Result.ReferenceVectorSetDigest = GOutputTransformVectorSetSha256;
    return Result;
}

const std::array<FOutputTransformStrategyIdentity, 4>& Strategies()
{
    static const std::array<FOutputTransformStrategyIdentity, 4> Values = {
        MakeStrategy("Sdr.KhronosPbrNeutral.v1",
            EOutputTransformStrategyFamily::SDRToneMap, 1),
        MakeStrategy("Sdr.NarkowiczAcesFit.v1",
            EOutputTransformStrategyFamily::SDRToneMap, 2),
        MakeStrategy("Sdr.ExtendedReinhardRec709.v1",
            EOutputTransformStrategyFamily::SDRToneMap, 3),
        MakeStrategy(GInitialHDRViewingVersion,
            EOutputTransformStrategyFamily::HDRViewing, 4)};
    return Values;
}

void AddSettingsFailure(FOutputTransformSettingsValidationResult& Out,
    EOutputTransformResult Result,
    const char* Code,
    const char* Message)
{
    Out.Result = Result;
    Out.Settings.State = EOutputTransformSettingsState::Rejected;
    Out.Diagnostics.Add(EOutputTransformDiagnosticSeverity::Error, Result,
        Code, "Settings", "OutputTransformSettings", Message);
}

} // namespace

FHDRSceneColorHandoff FHDRSceneColorHandoff::Declare(
    const FHDRSceneColorHandoffDesc& InDesc) noexcept
{
    FHDRSceneColorHandoff Handoff;
    Handoff.Desc = InDesc;
    Handoff.State = Handoff.IsMetadataValid()
        ? EHDRSceneColorState::Declared
        : EHDRSceneColorState::Failed;
    return Handoff;
}

bool FHDRSceneColorHandoff::BindProducer(
    FRenderGraphResourceHandle InResource) noexcept
{
    if (State != EHDRSceneColorState::Declared || !InResource.IsValid())
    {
        return false;
    }
    Resource = InResource;
    State = EHDRSceneColorState::ProducerBound;
    return true;
}

bool FHDRSceneColorHandoff::MarkProduced() noexcept
{
    if (State != EHDRSceneColorState::ProducerBound || !Resource.IsValid())
    {
        return false;
    }
    State = EHDRSceneColorState::Produced;
    return true;
}

bool FHDRSceneColorHandoff::MarkConsumed() noexcept
{
    if (State != EHDRSceneColorState::Produced || !Resource.IsValid())
    {
        return false;
    }
    State = EHDRSceneColorState::Consumed;
    return true;
}

void FHDRSceneColorHandoff::Fail() noexcept
{
    State = EHDRSceneColorState::Failed;
    Resource = {};
}

bool FHDRSceneColorHandoff::IsMetadataValid() const noexcept
{
    return Desc.SceneColorId != 0 && Desc.ViewId != 0 &&
        Desc.FrameToken != 0 && Desc.Width != 0 && Desc.Height != 0 &&
        Desc.Width <= GMaximumOutputExtent &&
        Desc.Height <= GMaximumOutputExtent &&
        Desc.Format == Stoner::RHI::ERHIFormat::R16G16B16A16_Float &&
        Desc.SampleCount == Stoner::RHI::ERHISampleCount::One &&
        Desc.Primaries == EOutputColorPrimaries::Rec709 &&
        Desc.WhitePoint == EOutputWhitePoint::D65 &&
        Desc.Transfer == EOutputTransferFunction::Linear &&
        Desc.AlphaMode == EOutputAlphaMode::OpaqueOne && Desc.bFinite &&
        Desc.bOpaqueAlpha;
}

bool FHDRSceneColorHandoff::IsReadyForConsumption() const noexcept
{
    return State == EHDRSceneColorState::Produced && Resource.IsValid() &&
        IsMetadataValid();
}

const char* ToString(EHDRSceneColorProducer Producer) noexcept
{
    switch (Producer)
    {
    case EHDRSceneColorProducer::Forward: return "Forward";
    case EHDRSceneColorProducer::Deferred: return "Deferred";
    }
    return "Unknown";
}

const char* ToString(EHDRSceneColorState State) noexcept
{
    switch (State)
    {
    case EHDRSceneColorState::Declared: return "Declared";
    case EHDRSceneColorState::ProducerBound: return "ProducerBound";
    case EHDRSceneColorState::Produced: return "Produced";
    case EHDRSceneColorState::Consumed: return "Consumed";
    case EHDRSceneColorState::Failed: return "Failed";
    }
    return "Unknown";
}

const char* ToString(EOutputColorPrimaries Primaries) noexcept
{
    switch (Primaries)
    {
    case EOutputColorPrimaries::Rec709: return "Rec709";
    case EOutputColorPrimaries::Rec2020: return "Rec2020";
    }
    return "Unknown";
}

const char* ToString(EOutputTransferFunction Transfer) noexcept
{
    switch (Transfer)
    {
    case EOutputTransferFunction::Linear: return "Linear";
    case EOutputTransferFunction::Srgb: return "sRGB";
    case EOutputTransferFunction::Bt709: return "BT709";
    case EOutputTransferFunction::Gamma22: return "Gamma22";
    case EOutputTransferFunction::St2084: return "ST2084";
    case EOutputTransferFunction::ScRgb80: return "scRGB80";
    case EOutputTransferFunction::MetalEdr: return "MetalEDR";
    }
    return "Unknown";
}

void FOutputTransformDiagnosticLog::Add(
    EOutputTransformDiagnosticSeverity Severity,
    EOutputTransformResult Result,
    Stoner::Core::FString Code,
    Stoner::Core::FString Stage,
    Stoner::Core::FString Subject,
    Stoner::Core::FString Message)
{
    if (Records.size() >= MaximumRecords)
    {
        return;
    }
    if (Severity == EOutputTransformDiagnosticSeverity::Error &&
        FirstErrorIndex == MaximumRecords)
    {
        FirstErrorIndex = static_cast<Stoner::Core::uint32>(Records.size());
    }
    Records.push_back({Severity, Result, std::move(Code), std::move(Stage),
        std::move(Subject), std::move(Message)});
}

void FOutputTransformDiagnosticLog::Merge(
    const FOutputTransformDiagnosticLog& Other)
{
    for (const FOutputTransformDiagnostic& Record : Other.GetRecords())
    {
        Add(Record.Severity, Record.Result, Record.Code, Record.Stage,
            Record.Subject, Record.Message);
    }
}

bool FOutputTransformDiagnosticLog::HasError() const noexcept
{
    return FirstErrorIndex < Records.size();
}

const FOutputTransformDiagnostic*
FOutputTransformDiagnosticLog::GetFirstError() const noexcept
{
    return HasError() ? &Records[FirstErrorIndex] : nullptr;
}

Stoner::Core::FString FOutputTransformDiagnosticLog::Dump() const
{
    std::ostringstream Stream;
    for (Stoner::Core::uint32 Index = 0; Index < Records.size(); ++Index)
    {
        const FOutputTransformDiagnostic& Record = Records[Index];
        Stream << Index << ':' << ToString(Record.Severity) << ':'
            << ToString(Record.Result) << ":code=" << Record.Code.CStr()
            << ":stage=" << Record.Stage.CStr() << ":subject="
            << Record.Subject.CStr() << ":message=" << Record.Message.CStr()
            << '\n';
    }
    return Stoner::Core::FString(Stream.str());
}

const char* ToString(EOutputTransformResult Result) noexcept
{
    switch (Result)
    {
    case EOutputTransformResult::Success: return "Success";
    case EOutputTransformResult::InvalidHandoff: return "InvalidHandoff";
    case EOutputTransformResult::InvalidSettings: return "InvalidSettings";
    case EOutputTransformResult::Unsupported: return "Unsupported";
    case EOutputTransformResult::InvalidGraph: return "InvalidGraph";
    case EOutputTransformResult::DuplicateFormalWriter: return "DuplicateFormalWriter";
    case EOutputTransformResult::InvalidBinding: return "InvalidBinding";
    case EOutputTransformResult::ExecutionFailed: return "ExecutionFailed";
    case EOutputTransformResult::TerminalFailed: return "TerminalFailed";
    }
    return "Unknown";
}

const char* ToString(EOutputTransformDiagnosticSeverity Severity) noexcept
{
    switch (Severity)
    {
    case EOutputTransformDiagnosticSeverity::Info: return "Info";
    case EOutputTransformDiagnosticSeverity::Warning: return "Warning";
    case EOutputTransformDiagnosticSeverity::Error: return "Error";
    }
    return "Unknown";
}

bool FOutputTransformStrategyIdentity::IsValid() const noexcept
{
    return !VersionId.IsEmpty() && ShaderStrategyIndex != 0 &&
        ConstantsDigest.Len() == 64 && !ShaderVariantId.IsEmpty() &&
        !ReferenceVectorSetId.IsEmpty() &&
        ReferenceVectorSetDigest.Len() == 64;
}

bool FOutputDeviceProfile::SupportsNativeEncoding(
    Stoner::RHI::ERHIPresentationNativeEncoding Encoding) const noexcept
{
    return Encoding == PrimaryNativeEncoding ||
        (SecondaryNativeEncoding !=
            Stoner::RHI::ERHIPresentationNativeEncoding::Unknown &&
            Encoding == SecondaryNativeEncoding);
}

bool FOutputDeviceProfile::IsValid() const noexcept
{
    return !ProfileId.IsEmpty() && !ProfileVersion.IsEmpty() &&
        WhitePoint == EOutputWhitePoint::D65 &&
        std::isfinite(ReferenceWhiteNits) && ReferenceWhiteNits > 0.0f &&
        std::isfinite(TargetPeakNits) &&
        TargetPeakNits >= ReferenceWhiteNits &&
        Stoner::RHI::IsValidRHIFormat(Format) &&
        Stoner::RHI::IsValidPresentationColorSpace(ColorSpace) &&
        Stoner::RHI::IsValidPresentationNativeEncoding(
            PrimaryNativeEncoding) &&
        DisplayLinearDomain != ERenderGraphColorDomain::Unspecified &&
        EncodedDomain != ERenderGraphColorDomain::Unspecified &&
        !TolerancePolicyId.IsEmpty();
}

bool FResolvedOutputTransformSettings::IsValid() const noexcept
{
    return State == EOutputTransformSettingsState::Resolved &&
        std::isfinite(ManualExposureStops) && ManualExposureStops >= -16.0f &&
        ManualExposureStops <= 16.0f &&
        !OutputDeviceProfileId.IsEmpty() &&
        !OutputDeviceProfileVersion.IsEmpty() &&
        !TransformStrategyVersion.IsEmpty() &&
        TransformStrategyIndex != 0 &&
        Stoner::RHI::IsValidRHIFormat(OutputFormat) &&
        Stoner::RHI::IsValidPresentationColorSpace(ColorSpace) &&
        Stoner::RHI::IsValidPresentationNativeEncoding(NativeEncoding) &&
        DisplayLinearDomain != ERenderGraphColorDomain::Unspecified &&
        EncodedDomain != ERenderGraphColorDomain::Unspecified &&
        std::isfinite(ReferenceWhiteNits) && ReferenceWhiteNits > 0.0f &&
        std::isfinite(TargetPeakNits) &&
        TargetPeakNits >= ReferenceWhiteNits &&
        !ProfileRegistryId.IsEmpty() && ProfileRegistryDigest.Len() == 64 &&
        !ReferenceVectorSetId.IsEmpty() &&
        ReferenceVectorSetDigest.Len() == 64 &&
        VectorManifestDigest.Len() == 64 &&
        TransformConstantsDigest.Len() == 64 &&
        !TolerancePolicyId.IsEmpty() && TolerancePolicyDigest.Len() == 64 &&
        !ShaderVariantId.IsEmpty() && !PipelineKey.IsEmpty() &&
        ViewingTransformApplicationCount == 1 &&
        GamutConversionApplicationCount == 1 &&
        OutputTransferApplicationCount == 1 &&
        (bRequirePresentation || bRequireReadback) &&
        ((DynamicRange == EOutputDynamicRange::SDR &&
             !SDRToneMapVersion.IsEmpty() && HDRViewingVersion.IsEmpty()) ||
            (DynamicRange == EOutputDynamicRange::HDR &&
                SDRToneMapVersion.IsEmpty() && !HDRViewingVersion.IsEmpty()));
}

FOutputTransformSettingsValidationResult
FOutputTransformSettingsValidator::Validate(
    const FOutputTransformSettings& Settings) const
{
    FOutputTransformSettingsValidationResult Out;
    Out.Settings.State = EOutputTransformSettingsState::Validating;
    const auto& FrozenProfiles = Profiles();
    bool bRegistryValid = true;
    for (Stoner::Core::uint32 Index = 0; Index < FrozenProfiles.size(); ++Index)
    {
        bRegistryValid = bRegistryValid && FrozenProfiles[Index].IsValid();
        for (Stoner::Core::uint32 Other = Index + 1;
            Other < FrozenProfiles.size(); ++Other)
        {
            bRegistryValid = bRegistryValid &&
                FrozenProfiles[Index].ProfileId !=
                    FrozenProfiles[Other].ProfileId;
        }
    }
    if (!bRegistryValid)
    {
        AddSettingsFailure(Out, EOutputTransformResult::InvalidSettings,
            "OT-SETTINGS-REGISTRY",
            "frozen output-device profile registry is invalid");
        return Out;
    }
    if (!std::isfinite(Settings.ManualExposureStops) ||
        Settings.ManualExposureStops < -16.0f ||
        Settings.ManualExposureStops > 16.0f)
    {
        AddSettingsFailure(Out, EOutputTransformResult::InvalidSettings,
            "OT-SETTINGS-EXPOSURE",
            "manual exposure must be finite and within [-16,+16] stops");
        return Out;
    }
    if (!Settings.bRequirePresentation && !Settings.bRequireReadback)
    {
        AddSettingsFailure(Out, EOutputTransformResult::InvalidSettings,
            "OT-SETTINGS-OUTPUT",
            "at least one of presentation or readback must be requested");
        return Out;
    }
    const Stoner::Core::FString ProfileId =
        Settings.OutputDeviceProfileId.IsEmpty() &&
            Settings.DynamicRange == EOutputDynamicRange::SDR
        ? Stoner::Core::FString(GDefaultSDROutputDeviceProfile)
        : Settings.OutputDeviceProfileId;
    const FOutputDeviceProfile* Profile = FindProfile(ProfileId);
    if (!Profile || Profile->DynamicRange != Settings.DynamicRange)
    {
        AddSettingsFailure(Out, EOutputTransformResult::Unsupported,
            "OT-SETTINGS-PROFILE",
            "output profile must exist and match the requested dynamic range");
        return Out;
    }

    Stoner::Core::FString TransformVersion;
    if (Settings.DynamicRange == EOutputDynamicRange::SDR)
    {
        if (!Settings.HDRViewingVersion.IsEmpty())
        {
            AddSettingsFailure(Out, EOutputTransformResult::Unsupported,
                "OT-SETTINGS-FAMILY",
                "SDR cannot select an HDR viewing transform");
            return Out;
        }
        TransformVersion = Settings.SDRToneMapVersion.IsEmpty()
            ? Stoner::Core::FString(GDefaultSDRToneMapVersion)
            : Settings.SDRToneMapVersion;
    }
    else
    {
        if (!Settings.SDRToneMapVersion.IsEmpty())
        {
            AddSettingsFailure(Out, EOutputTransformResult::Unsupported,
                "OT-SETTINGS-FAMILY",
                "HDR cannot run an SDR tone-map Strategy first");
            return Out;
        }
        TransformVersion = Settings.HDRViewingVersion.IsEmpty()
            ? Stoner::Core::FString(GInitialHDRViewingVersion)
            : Settings.HDRViewingVersion;
    }
    const FOutputTransformStrategyIdentity* Strategy =
        FindStrategy(TransformVersion);
    const EOutputTransformStrategyFamily RequiredFamily =
        Settings.DynamicRange == EOutputDynamicRange::SDR
        ? EOutputTransformStrategyFamily::SDRToneMap
        : EOutputTransformStrategyFamily::HDRViewing;
    if (!Strategy || Strategy->Family != RequiredFamily)
    {
        AddSettingsFailure(Out, EOutputTransformResult::Unsupported,
            "OT-SETTINGS-STRATEGY",
            "unknown or incompatible transform Strategy version");
        return Out;
    }

    Stoner::RHI::ERHIPresentationNativeEncoding NativeEncoding =
        Settings.PreferredNativeEncoding;
    if (Profile->DynamicRange == EOutputDynamicRange::SDR)
    {
        if (NativeEncoding !=
                Stoner::RHI::ERHIPresentationNativeEncoding::Unknown &&
            NativeEncoding !=
                Stoner::RHI::ERHIPresentationNativeEncoding::SdrExplicit)
        {
            AddSettingsFailure(Out, EOutputTransformResult::Unsupported,
                "OT-SETTINGS-ENCODING", "SDR requires explicit SDR encoding");
            return Out;
        }
        NativeEncoding =
            Stoner::RHI::ERHIPresentationNativeEncoding::SdrExplicit;
    }
    else if (Profile->PrimaryNativeEncoding ==
        Stoner::RHI::ERHIPresentationNativeEncoding::Pq)
    {
        if (NativeEncoding !=
                Stoner::RHI::ERHIPresentationNativeEncoding::Unknown &&
            NativeEncoding != Stoner::RHI::ERHIPresentationNativeEncoding::Pq)
        {
            AddSettingsFailure(Out, EOutputTransformResult::Unsupported,
                "OT-SETTINGS-ENCODING", "PQ profile requires PQ native encoding");
            return Out;
        }
        NativeEncoding = Stoner::RHI::ERHIPresentationNativeEncoding::Pq;
    }
    else if (!Profile->SupportsNativeEncoding(NativeEncoding))
    {
        AddSettingsFailure(Out, EOutputTransformResult::Unsupported,
            "OT-SETTINGS-ENCODING",
            "linear HDR requires an explicit scRGB80 or Metal EDR encoding");
        return Out;
    }
    const float ReferenceWhite = NativeEncoding ==
        Stoner::RHI::ERHIPresentationNativeEncoding::MetalEdr
        ? Settings.NativeReferenceWhiteNits
        : Profile->ReferenceWhiteNits;
    if (!std::isfinite(ReferenceWhite) || ReferenceWhite <= 0.0f ||
        (NativeEncoding !=
            Stoner::RHI::ERHIPresentationNativeEncoding::MetalEdr &&
            Settings.NativeReferenceWhiteNits != 0.0f))
    {
        AddSettingsFailure(Out, EOutputTransformResult::InvalidSettings,
            "OT-SETTINGS-REFERENCE-WHITE",
            "native reference white is required only for Metal EDR");
        return Out;
    }

    FResolvedOutputTransformSettings Resolved;
    Resolved.State = EOutputTransformSettingsState::Resolved;
    Resolved.ManualExposureStops = Settings.ManualExposureStops == 0.0f
        ? 0.0f : Settings.ManualExposureStops;
    Resolved.DynamicRange = Settings.DynamicRange;
    Resolved.SDRToneMapVersion = Settings.DynamicRange == EOutputDynamicRange::SDR
        ? TransformVersion : Stoner::Core::FString{};
    Resolved.HDRViewingVersion = Settings.DynamicRange == EOutputDynamicRange::HDR
        ? TransformVersion : Stoner::Core::FString{};
    Resolved.OutputDeviceProfileId = Profile->ProfileId;
    Resolved.OutputDeviceProfileVersion = Profile->ProfileVersion;
    Resolved.TransformStrategyVersion = Strategy->VersionId;
    Resolved.TransformStrategyIndex = Strategy->ShaderStrategyIndex;
    Resolved.OutputFormat = Profile->Format;
    Resolved.ColorSpace = Profile->ColorSpace;
    Resolved.NativeEncoding = NativeEncoding;
    Resolved.TargetPrimaries = Profile->TargetPrimaries;
    Resolved.WhitePoint = Profile->WhitePoint;
    Resolved.Transfer = NativeEncoding ==
        Stoner::RHI::ERHIPresentationNativeEncoding::ScRgb80
        ? EOutputTransferFunction::ScRgb80
        : (NativeEncoding == Stoner::RHI::ERHIPresentationNativeEncoding::MetalEdr
            ? EOutputTransferFunction::MetalEdr : Profile->Transfer);
    Resolved.DisplayLinearDomain = Profile->DisplayLinearDomain;
    Resolved.EncodedDomain = Profile->EncodedDomain;
    Resolved.StorageClass = Profile->StorageClass;
    Resolved.MetadataPolicy = Profile->MetadataPolicy;
    Resolved.ComparisonDomain = Profile->ComparisonDomain;
    Resolved.ReferenceWhiteNits = ReferenceWhite;
    Resolved.TargetPeakNits = Profile->TargetPeakNits;
    Resolved.ProfileRegistryId = GOutputTransformProfileRegistryId;
    Resolved.ProfileRegistryDigest = GOutputTransformProfileRegistrySha256;
    Resolved.ReferenceVectorSetId = GOutputTransformVectorSetId;
    Resolved.ReferenceVectorSetDigest = GOutputTransformVectorSetSha256;
    Resolved.VectorManifestDigest = GOutputTransformVectorManifestSha256;
    Resolved.TransformConstantsDigest = Strategy->ConstantsDigest;
    Resolved.TolerancePolicyId = Profile->TolerancePolicyId;
    Resolved.TolerancePolicyDigest = GOutputTransformTolerancePolicySha256;
    Resolved.ShaderVariantId = Strategy->ShaderVariantId;
    Resolved.PipelineKey = BuildPipelineKey(
        *Strategy, *Profile, NativeEncoding, ReferenceWhite);
    Resolved.bRequirePresentation = Settings.bRequirePresentation;
    Resolved.bRequireReadback = Settings.bRequireReadback;
    Out.Settings = std::move(Resolved);
    Out.Result = EOutputTransformResult::Success;
    Out.Diagnostics.Add(EOutputTransformDiagnosticSeverity::Info,
        EOutputTransformResult::Success, "OT-SETTINGS-RESOLVED", "Settings",
        ProfileId, "output transform settings resolved without native fallback");
    return Out;
}

std::span<const FOutputDeviceProfile>
FOutputTransformSettingsValidator::GetProfiles() const
{
    return Profiles();
}

const FOutputDeviceProfile* FOutputTransformSettingsValidator::FindProfile(
    const Stoner::Core::FString& ProfileId) const
{
    const auto& Values = Profiles();
    const auto Found = std::find_if(Values.begin(), Values.end(),
        [&ProfileId](const FOutputDeviceProfile& Value)
        { return Value.ProfileId == ProfileId; });
    return Found == Values.end() ? nullptr : &*Found;
}

const FOutputTransformStrategyIdentity*
FOutputTransformSettingsValidator::FindStrategy(
    const Stoner::Core::FString& VersionId) const
{
    const auto& Values = Strategies();
    const auto Found = std::find_if(Values.begin(), Values.end(),
        [&VersionId](const FOutputTransformStrategyIdentity& Value)
        { return Value.VersionId == VersionId; });
    return Found == Values.end() ? nullptr : &*Found;
}

const char* ToString(EOutputDynamicRange DynamicRange) noexcept
{
    switch (DynamicRange)
    {
    case EOutputDynamicRange::SDR: return "SDR";
    case EOutputDynamicRange::HDR: return "HDR";
    }
    return "Unknown";
}

const char* ToString(EOutputProfileStorageClass Storage) noexcept
{
    switch (Storage)
    {
    case EOutputProfileStorageClass::UNorm8: return "UNorm8";
    case EOutputProfileStorageClass::Packed10UNorm: return "Packed10UNorm";
    case EOutputProfileStorageClass::Float16: return "Float16";
    }
    return "Unknown";
}

const char* ToString(EOutputMetadataPolicy Policy) noexcept
{
    switch (Policy)
    {
    case EOutputMetadataPolicy::None: return "None";
    case EOutputMetadataPolicy::HDR10Static: return "HDR10Static";
    case EOutputMetadataPolicy::EDRState: return "EDRState";
    }
    return "Unknown";
}

const char* ToString(EOutputComparisonDomain Domain) noexcept
{
    switch (Domain)
    {
    case EOutputComparisonDomain::LinearRec709: return "LinearRec709";
    case EOutputComparisonDomain::AbsoluteNitsXyz: return "AbsoluteNitsXyz";
    }
    return "Unknown";
}

} // namespace Stoner::Renderer
