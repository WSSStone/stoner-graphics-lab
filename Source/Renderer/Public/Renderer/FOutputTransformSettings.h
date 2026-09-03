#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FHDRSceneColorHandoff.h"
#include "Renderer/FOutputTransformDiagnostics.h"
#include "Renderer/FPostProcessInsertion.h"
#include "RHI/ERHIPresentationColorSpace.h"

#include <span>

namespace Stoner::Renderer
{

inline constexpr const char* GDefaultSDRToneMapVersion =
    "Sdr.KhronosPbrNeutral.v1";
inline constexpr const char* GDefaultSDROutputDeviceProfile = "Sdr.sRGB.v1";
inline constexpr const char* GInitialHDRViewingVersion =
    "Hdr.ACES2.0.0_2025-04-04.Rec2020D65.v1";
inline constexpr const char* GOutputTransformProfileRegistryId =
    "output-transform-profiles-v1";
inline constexpr const char* GOutputTransformProfileRegistrySha256 =
    "6e3373ab31b36aff9e91d1b6b854d75d1171226e57bc3c236f8a1bbce1e1f7d9";
inline constexpr const char* GOutputTransformVectorSetId =
    "output-transform-vectors-v1";
inline constexpr const char* GOutputTransformVectorSetSha256 =
    "16189846ba601040696fbb1db6120eed36253feffeae02362a691e250c24aa76";
inline constexpr const char* GOutputTransformVectorManifestSha256 =
    "77aabc634d565862079cc6aa55625bccfc934632d7d4068d360654952aaa121d";
inline constexpr const char* GOutputTransformConstantsSha256 =
    "71cdf65fd28460f58a26b94cdc4c722320f240864fddb8e2173c7210e7aff9ae";
inline constexpr const char* GOutputTransformTolerancePolicySha256 =
    "4e3665150d985cfb6b2e05ac46490f5dde295be52730f8afd3f18f4bf20015ce";

enum class EOutputDynamicRange { SDR, HDR };
enum class EOutputTransformSettingsState
{
    Authored,
    Validating,
    Resolved,
    Rejected
};
enum class EOutputProfileStorageClass { UNorm8, Packed10UNorm, Float16 };
enum class EOutputMetadataPolicy { None, HDR10Static, EDRState };
enum class EOutputComparisonDomain { LinearRec709, AbsoluteNitsXyz };
enum class EOutputTransformStrategyFamily { SDRToneMap, HDRViewing };

struct FOutputTransformStrategyIdentity
{
    Stoner::Core::FString VersionId;
    EOutputTransformStrategyFamily Family =
        EOutputTransformStrategyFamily::SDRToneMap;
    Stoner::Core::uint32 ShaderStrategyIndex = 0;
    Stoner::Core::FString ConstantsDigest;
    Stoner::Core::FString ShaderVariantId;
    Stoner::Core::FString ReferenceVectorSetId;
    Stoner::Core::FString ReferenceVectorSetDigest;

    [[nodiscard]] bool IsValid() const noexcept;
};

struct FOutputDeviceProfile
{
    Stoner::Core::FString ProfileId;
    Stoner::Core::FString ProfileVersion;
    EOutputDynamicRange DynamicRange = EOutputDynamicRange::SDR;
    EOutputColorPrimaries TargetPrimaries = EOutputColorPrimaries::Rec709;
    EOutputWhitePoint WhitePoint = EOutputWhitePoint::D65;
    float ReferenceWhiteNits = 0.0f;
    float TargetPeakNits = 0.0f;
    EOutputTransferFunction Transfer = EOutputTransferFunction::Srgb;
    EOutputProfileStorageClass StorageClass =
        EOutputProfileStorageClass::UNorm8;
    Stoner::RHI::ERHIFormat Format = Stoner::RHI::ERHIFormat::Unknown;
    Stoner::RHI::ERHIPresentationColorSpace ColorSpace =
        Stoner::RHI::ERHIPresentationColorSpace::Unknown;
    EOutputMetadataPolicy MetadataPolicy = EOutputMetadataPolicy::None;
    EOutputComparisonDomain ComparisonDomain =
        EOutputComparisonDomain::LinearRec709;
    Stoner::RHI::ERHIPresentationNativeEncoding PrimaryNativeEncoding =
        Stoner::RHI::ERHIPresentationNativeEncoding::Unknown;
    Stoner::RHI::ERHIPresentationNativeEncoding SecondaryNativeEncoding =
        Stoner::RHI::ERHIPresentationNativeEncoding::Unknown;
    ERenderGraphColorDomain DisplayLinearDomain =
        ERenderGraphColorDomain::Unspecified;
    ERenderGraphColorDomain EncodedDomain =
        ERenderGraphColorDomain::Unspecified;
    Stoner::Core::FString TolerancePolicyId;

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] bool SupportsNativeEncoding(
        Stoner::RHI::ERHIPresentationNativeEncoding Encoding) const noexcept;
};

struct FOutputTransformSettings
{
    float ManualExposureStops = 0.0f;
    EOutputDynamicRange DynamicRange = EOutputDynamicRange::SDR;
    Stoner::Core::FString SDRToneMapVersion;
    Stoner::Core::FString HDRViewingVersion;
    Stoner::Core::FString OutputDeviceProfileId;
    Stoner::RHI::ERHIPresentationNativeEncoding PreferredNativeEncoding =
        Stoner::RHI::ERHIPresentationNativeEncoding::Unknown;
    float NativeReferenceWhiteNits = 0.0f;
    FPostProcessComposite PreTonemapOperations;
    FPostProcessComposite PostTonemapOperations;
    FOutputTransformDebugBypassRequest DiagnosticBypass;
    bool bRequirePresentation = true;
    bool bRequireReadback = false;
};

struct FResolvedOutputTransformSettings
{
    EOutputTransformSettingsState State =
        EOutputTransformSettingsState::Rejected;
    float ManualExposureStops = 0.0f;
    EOutputDynamicRange DynamicRange = EOutputDynamicRange::SDR;
    Stoner::Core::FString SDRToneMapVersion;
    Stoner::Core::FString HDRViewingVersion;
    Stoner::Core::FString OutputDeviceProfileId;
    Stoner::Core::FString OutputDeviceProfileVersion;
    Stoner::Core::FString TransformStrategyVersion;
    Stoner::Core::uint32 TransformStrategyIndex = 0;
    Stoner::RHI::ERHIFormat OutputFormat =
        Stoner::RHI::ERHIFormat::Unknown;
    Stoner::RHI::ERHIPresentationColorSpace ColorSpace =
        Stoner::RHI::ERHIPresentationColorSpace::Unknown;
    Stoner::RHI::ERHIPresentationNativeEncoding NativeEncoding =
        Stoner::RHI::ERHIPresentationNativeEncoding::Unknown;
    EOutputColorPrimaries TargetPrimaries = EOutputColorPrimaries::Rec709;
    EOutputWhitePoint WhitePoint = EOutputWhitePoint::D65;
    EOutputTransferFunction Transfer = EOutputTransferFunction::Srgb;
    ERenderGraphColorDomain DisplayLinearDomain =
        ERenderGraphColorDomain::DisplayLinearRec709D65;
    ERenderGraphColorDomain EncodedDomain =
        ERenderGraphColorDomain::EncodedSrgb;
    EOutputProfileStorageClass StorageClass =
        EOutputProfileStorageClass::UNorm8;
    EOutputMetadataPolicy MetadataPolicy = EOutputMetadataPolicy::None;
    EOutputComparisonDomain ComparisonDomain =
        EOutputComparisonDomain::LinearRec709;
    float ReferenceWhiteNits = 100.0f;
    float TargetPeakNits = 100.0f;
    Stoner::Core::FString ProfileRegistryId;
    Stoner::Core::FString ProfileRegistryDigest;
    Stoner::Core::FString ReferenceVectorSetId;
    Stoner::Core::FString ReferenceVectorSetDigest;
    Stoner::Core::FString VectorManifestDigest;
    Stoner::Core::FString TransformConstantsDigest;
    Stoner::Core::FString TolerancePolicyId;
    Stoner::Core::FString TolerancePolicyDigest;
    Stoner::Core::FString ShaderVariantId;
    Stoner::Core::FString PipelineKey;
    Stoner::Core::uint32 ViewingTransformApplicationCount = 1;
    Stoner::Core::uint32 GamutConversionApplicationCount = 1;
    Stoner::Core::uint32 OutputTransferApplicationCount = 1;
    bool bRequirePresentation = true;
    bool bRequireReadback = false;

    [[nodiscard]] bool IsValid() const noexcept;
};

struct FOutputTransformSettingsValidationResult
{
    EOutputTransformResult Result = EOutputTransformResult::InvalidSettings;
    FResolvedOutputTransformSettings Settings;
    FOutputTransformDiagnosticLog Diagnostics;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return Result == EOutputTransformResult::Success && Settings.IsValid();
    }
};

class FOutputTransformSettingsValidator
{
public:
    [[nodiscard]] FOutputTransformSettingsValidationResult Validate(
        const FOutputTransformSettings& Settings) const;
    [[nodiscard]] std::span<const FOutputDeviceProfile> GetProfiles() const;
    [[nodiscard]] const FOutputDeviceProfile* FindProfile(
        const Stoner::Core::FString& ProfileId) const;
    [[nodiscard]] const FOutputTransformStrategyIdentity* FindStrategy(
        const Stoner::Core::FString& VersionId) const;
};

[[nodiscard]] const char* ToString(EOutputDynamicRange DynamicRange) noexcept;
[[nodiscard]] const char* ToString(EOutputProfileStorageClass Storage) noexcept;
[[nodiscard]] const char* ToString(EOutputMetadataPolicy Policy) noexcept;
[[nodiscard]] const char* ToString(EOutputComparisonDomain Domain) noexcept;

} // namespace Stoner::Renderer
