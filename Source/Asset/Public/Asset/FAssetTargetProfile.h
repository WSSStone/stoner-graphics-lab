#pragma once

#include "Asset/EAssetResult.h"
#include "Asset/FAssetDigest.h"
#include "Asset/FAssetParticipant.h"
#include "Core/FPlatformTypes.h"
#include "Core/FString.h"
#include "Core/TArray.h"

#include <variant>
#include <optional>

namespace Stoner::Asset
{

enum class EAssetTargetPlatform : Core::uint8
{
    Windows,
    MacOS,
    Linux,
    Android,
    IOS
};

enum class EAssetTargetCpuArchitecture : Core::uint8
{
    X86_64,
    Arm64
};

enum class EAssetGraphicsBackend : Core::uint8
{
    Vulkan,
    Metal,
    DirectX12,
    OpenGL,
    GLES
};

enum class EAssetShaderPayloadFormat : Core::uint8
{
    SpirV,
    MSL,
    DXIL,
    GLSL,
    ESSL,
    MetalLibrary
};

enum class EAssetTextureFallback : Core::uint8
{
    Fail,
    Uncompressed,
    PortableKTX2
};

enum class EAssetBuildOptimization : Core::uint8
{
    Development,
    Shipping
};

enum class EAssetBuildValidation : Core::uint8
{
    Standard,
    Strict
};

struct FAssetShaderPayloadChoice
{
    EAssetGraphicsBackend Backend = EAssetGraphicsBackend::Vulkan;
    Core::FString Profile;
    EAssetShaderPayloadFormat Format = EAssetShaderPayloadFormat::SpirV;

    [[nodiscard]] bool operator==(const FAssetShaderPayloadChoice&) const = default;
};

using FAssetProducerSettingValue =
    std::variant<bool, Core::int64, double, Core::FString>;

struct FAssetProducerSetting
{
    Core::FString Name;
    FAssetProducerSettingValue Value;

    [[nodiscard]] bool operator==(const FAssetProducerSetting&) const = default;
};

struct FAssetProducerSettingsRecord
{
    FAssetParticipantId Producer;
    Core::uint32 SchemaVersion = 0;
    Core::TArray<FAssetProducerSetting> Settings;

    [[nodiscard]] EAssetResult Validate() const noexcept;
    [[nodiscard]] const FAssetProducerSetting* Find(
        const Core::FString& Name) const noexcept;
    [[nodiscard]] bool operator==(const FAssetProducerSettingsRecord&) const = default;
};

struct FAssetTargetBuildPolicy
{
    EAssetBuildOptimization Optimization =
        EAssetBuildOptimization::Development;
    bool bIncludeDebugSymbols = false;
    EAssetBuildValidation Validation = EAssetBuildValidation::Standard;
    Core::TArray<FAssetProducerSettingsRecord> ProducerSettings;

    [[nodiscard]] EAssetResult Validate() const noexcept;
    [[nodiscard]] const FAssetProducerSettingsRecord* FindProducer(
        const FAssetParticipantId& Producer) const noexcept;
    [[nodiscard]] bool operator==(const FAssetTargetBuildPolicy&) const = default;
};

struct FAssetTargetLimits
{
    Core::uint32 MaxDiscoveredSources = 100000;
    Core::uint32 MaxAssets = 100000;
    Core::uint32 MaxDependencyEdges = 1000000;
    Core::uint32 MaxDependencyDepth = 256;
    Core::uint64 MaxSourceBytes = 1024ULL * 1024ULL * 1024ULL;
    Core::uint64 MaxPayloadBytes = 1024ULL * 1024ULL * 1024ULL;
    Core::uint64 MaxAggregateBytes = 8ULL * 1024ULL * 1024ULL * 1024ULL;
    Core::uint64 MaxManifestBytes = 256ULL * 1024ULL * 1024ULL;
    Core::uint32 MaxDiagnostics = 4096;

    [[nodiscard]] EAssetResult Validate() const noexcept;
    [[nodiscard]] bool operator==(const FAssetTargetLimits&) const = default;
};

struct FAssetMetalShaderTarget
{
    Core::FString DeploymentTarget = Core::FString("12.0");
    Core::FString MslVersion = Core::FString("2.4");
    Core::FString BindingPolicy = Core::FString("metal-direct-binding-v1");
    Core::uint32 NativeEvidenceSchemaVersion = 1;

    [[nodiscard]] EAssetResult Validate() const noexcept;
    [[nodiscard]] bool operator==(const FAssetMetalShaderTarget&) const = default;
};

struct FAssetTargetProfile
{
    static constexpr Core::uint32 CurrentSchemaVersion = 2;

    Core::FString Schema = Core::FString("stoner.asset-target-profile");
    Core::uint32 SchemaVersion = CurrentSchemaVersion;
    Core::FString DisplayName;
    EAssetTargetPlatform Platform = EAssetTargetPlatform::Linux;
    EAssetTargetCpuArchitecture CpuArchitecture =
        EAssetTargetCpuArchitecture::X86_64;
    EAssetGraphicsBackend GraphicsBackend = EAssetGraphicsBackend::Vulkan;
    Core::TArray<FAssetShaderPayloadChoice> ShaderPayloadChoices;
    std::optional<FAssetMetalShaderTarget> MetalShaderTarget;
    Core::TArray<Core::FString> TextureCapabilities;
    EAssetTextureFallback TextureFallback = EAssetTextureFallback::Fail;
    FAssetTargetBuildPolicy BuildPolicy;
    FAssetTargetLimits Limits;
    Core::TArray<Core::FString> RequiredExtensions;
    Core::TArray<Core::FString> OptionalExtensions;

    [[nodiscard]] EAssetResult Validate() const noexcept;
    [[nodiscard]] bool operator==(const FAssetTargetProfile&) const = default;
};

struct FAssetTargetProfileEvidence
{
    FAssetTargetProfile Profile;
    FAssetDigest EffectiveProfileDigest;
    Core::FString CanonicalEffectiveConfiguration;

    [[nodiscard]] EAssetResult Validate() const noexcept;
    [[nodiscard]] bool operator==(const FAssetTargetProfileEvidence&) const = default;
};

struct FAssetProfileProjectionEvidence
{
    FAssetParticipantId Producer;
    Core::uint32 ProducerSettingsSchemaVersion = 0;
    Core::FString CanonicalProducerSettings;
    FAssetDigest EffectiveSettingsDigest;
    Core::FString CanonicalRelevantProfile;
    FAssetDigest RelevantProfileDigest;

    [[nodiscard]] EAssetResult Validate() const noexcept;
    [[nodiscard]] bool operator==(const FAssetProfileProjectionEvidence&) const = default;
};

} // namespace Stoner::Asset
