#include "OutputDeviceProfileTests.h"

#include "Renderer/FOutputTransformSettings.h"

#include <array>
#include <iostream>
#include <set>

namespace
{

using namespace Stoner;
using namespace Stoner::Renderer;

void Record(FOutputDeviceProfileTestResult& Result, bool bPassed,
    const char* Name)
{
    (bPassed ? ++Result.Passed : ++Result.Failed);
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

struct FProfileCase
{
    const char* Id;
    EOutputDynamicRange Range;
    RHI::ERHIPresentationNativeEncoding RequestedEncoding;
    RHI::ERHIFormat Format;
    RHI::ERHIPresentationColorSpace ColorSpace;
    EOutputTransferFunction Transfer;
    ERenderGraphColorDomain EncodedDomain;
    float PeakNits;
};

FOutputTransformSettings MakeSettings(const FProfileCase& Value)
{
    FOutputTransformSettings Settings;
    Settings.DynamicRange = Value.Range;
    Settings.OutputDeviceProfileId = Value.Id;
    Settings.PreferredNativeEncoding = Value.RequestedEncoding;
    Settings.NativeReferenceWhiteNits =
        Value.RequestedEncoding ==
            RHI::ERHIPresentationNativeEncoding::MetalEdr
        ? 100.0f : 0.0f;
    return Settings;
}

} // namespace

FOutputDeviceProfileTestResult RunOutputDeviceProfileTests()
{
    FOutputDeviceProfileTestResult Result;
    FOutputTransformSettingsValidator Validator;
    const auto Profiles = Validator.GetProfiles();
    std::set<std::string> ProfileIds;
    bool bProfilesValid = Profiles.size() == 7;
    for (const auto& Profile : Profiles)
    {
        bProfilesValid = bProfilesValid && Profile.IsValid() &&
            ProfileIds.insert(Profile.ProfileId.ToStdString()).second;
    }
    Record(Result, bProfilesValid && ProfileIds.size() == 7,
        "profile registry exposes seven unique frozen profiles");

    constexpr std::array<FProfileCase, 7> Cases = {{
        {"Sdr.sRGB.v1", EOutputDynamicRange::SDR,
            RHI::ERHIPresentationNativeEncoding::Unknown,
            RHI::ERHIFormat::R8G8B8A8_UNorm,
            RHI::ERHIPresentationColorSpace::SrgbNonlinear,
            EOutputTransferFunction::Srgb,
            ERenderGraphColorDomain::EncodedSrgb, 100.0f},
        {"Sdr.BT709.v1", EOutputDynamicRange::SDR,
            RHI::ERHIPresentationNativeEncoding::Unknown,
            RHI::ERHIFormat::R8G8B8A8_UNorm,
            RHI::ERHIPresentationColorSpace::Bt709Nonlinear,
            EOutputTransferFunction::Bt709,
            ERenderGraphColorDomain::EncodedBt709, 100.0f},
        {"Sdr.ExplicitGamma22.v1", EOutputDynamicRange::SDR,
            RHI::ERHIPresentationNativeEncoding::Unknown,
            RHI::ERHIFormat::R8G8B8A8_UNorm,
            RHI::ERHIPresentationColorSpace::SdrPassThrough,
            EOutputTransferFunction::Gamma22,
            ERenderGraphColorDomain::EncodedGamma22, 100.0f},
        {"Hdr.PQ.Rec2020.1000.v1", EOutputDynamicRange::HDR,
            RHI::ERHIPresentationNativeEncoding::Pq,
            RHI::ERHIFormat::R10G10B10A2_UNorm,
            RHI::ERHIPresentationColorSpace::Hdr10St2084,
            EOutputTransferFunction::St2084,
            ERenderGraphColorDomain::EncodedPqRec2020D65, 1000.0f},
        {"Hdr.PQ.Rec2020.2000.v1", EOutputDynamicRange::HDR,
            RHI::ERHIPresentationNativeEncoding::Pq,
            RHI::ERHIFormat::R10G10B10A2_UNorm,
            RHI::ERHIPresentationColorSpace::Hdr10St2084,
            EOutputTransferFunction::St2084,
            ERenderGraphColorDomain::EncodedPqRec2020D65, 2000.0f},
        {"Hdr.Linear.1000.v1", EOutputDynamicRange::HDR,
            RHI::ERHIPresentationNativeEncoding::ScRgb80,
            RHI::ERHIFormat::R16G16B16A16_Float,
            RHI::ERHIPresentationColorSpace::ExtendedSrgbLinear,
            EOutputTransferFunction::ScRgb80,
            ERenderGraphColorDomain::ExtendedSrgbLinear, 1000.0f},
        {"Hdr.Linear.2000.v1", EOutputDynamicRange::HDR,
            RHI::ERHIPresentationNativeEncoding::MetalEdr,
            RHI::ERHIFormat::R16G16B16A16_Float,
            RHI::ERHIPresentationColorSpace::ExtendedSrgbLinear,
            EOutputTransferFunction::MetalEdr,
            ERenderGraphColorDomain::ExtendedSrgbLinear, 2000.0f}
    }};
    bool bResolved = true;
    for (const FProfileCase& Value : Cases)
    {
        const auto Validation = Validator.Validate(MakeSettings(Value));
        bResolved = bResolved && Validation.Succeeded() &&
            Validation.Settings.OutputDeviceProfileId == Value.Id &&
            Validation.Settings.OutputFormat == Value.Format &&
            Validation.Settings.ColorSpace == Value.ColorSpace &&
            Validation.Settings.Transfer == Value.Transfer &&
            Validation.Settings.EncodedDomain == Value.EncodedDomain &&
            Validation.Settings.TargetPeakNits == Value.PeakNits &&
            Validation.Settings.ViewingTransformApplicationCount == 1 &&
            Validation.Settings.OutputTransferApplicationCount == 1 &&
            Validation.Settings.GamutConversionApplicationCount == 1;
    }
    Record(Result, bResolved,
        "all seven profiles resolve exact format color-space and transfer ownership");

    FOutputTransformSettings Default;
    const auto DefaultResolved = Validator.Validate(Default);
    Record(Result, DefaultResolved.Succeeded() &&
        DefaultResolved.Settings.OutputDeviceProfileId ==
            GDefaultSDROutputDeviceProfile &&
        DefaultResolved.Settings.SDRToneMapVersion ==
            GDefaultSDRToneMapVersion &&
        DefaultResolved.Settings.ProfileRegistryDigest ==
            GOutputTransformProfileRegistrySha256,
        "empty SDR selections materialize the frozen profile and Strategy defaults");

    auto FamilyMismatch = MakeSettings(Cases[3]);
    FamilyMismatch.DynamicRange = EOutputDynamicRange::SDR;
    const auto FamilyRejected = Validator.Validate(FamilyMismatch);
    auto EncodingMismatch = MakeSettings(Cases[5]);
    EncodingMismatch.PreferredNativeEncoding =
        RHI::ERHIPresentationNativeEncoding::Pq;
    const auto EncodingRejected = Validator.Validate(EncodingMismatch);
    Record(Result, !FamilyRejected.Succeeded() &&
        !EncodingRejected.Succeeded() &&
        FamilyRejected.Result == EOutputTransformResult::Unsupported &&
        EncodingRejected.Result == EOutputTransformResult::Unsupported,
        "profile family and native encoding mismatches fail without fallback");

    FOutputTransformSettings Unknown;
    Unknown.OutputDeviceProfileId = "Sdr.Missing.v1";
    Record(Result, !Validator.Validate(Unknown).Succeeded(),
        "unknown profile identity fails closed");
    return Result;
}
