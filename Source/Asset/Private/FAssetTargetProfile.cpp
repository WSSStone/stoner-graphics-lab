#include "Asset/FAssetTargetProfile.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace Stoner::Asset
{
namespace
{

bool IsLowerToken(const Core::FString& Value, Core::usize Maximum)
{
    if (Value.IsEmpty() || Value.Len() > Maximum)
    {
        return false;
    }
    for (const unsigned char Character : Value.View())
    {
        if (!((Character >= 'a' && Character <= 'z') ||
              (Character >= '0' && Character <= '9') ||
              Character == '.' || Character == '_' || Character == '-'))
        {
            return false;
        }
    }
    const unsigned char First =
        static_cast<unsigned char>(Value.View().front());
    return (First >= 'a' && First <= 'z') ||
        (First >= '0' && First <= '9');
}

bool IsSettingName(const Core::FString& Value)
{
    if (Value.IsEmpty() || Value.Len() > 128)
    {
        return false;
    }
    for (const unsigned char Character : Value.View())
    {
        if (!((Character >= 'a' && Character <= 'z') ||
              (Character >= 'A' && Character <= 'Z') ||
              (Character >= '0' && Character <= '9') ||
              Character == '.' || Character == '_' || Character == '-'))
        {
            return false;
        }
    }
    const unsigned char First =
        static_cast<unsigned char>(Value.View().front());
    return (First >= 'a' && First <= 'z') ||
        (First >= 'A' && First <= 'Z');
}

bool IsStrictlySortedTokens(const Core::TArray<Core::FString>& Values)
{
    return std::adjacent_find(
               Values.begin(),
               Values.end(),
               [](const auto& Left, const auto& Right)
               {
                   return !(Left < Right);
               }) == Values.end();
}

template <typename T>
bool EnumInRange(T Value, T Last)
{
    return static_cast<Core::uint32>(Value) <=
        static_cast<Core::uint32>(Last);
}

} // namespace

EAssetResult FAssetProducerSettingsRecord::Validate() const noexcept
{
    if (!Producer.IsValid() || SchemaVersion == 0 || Settings.size() > 64)
    {
        return EAssetResult::InvalidInput;
    }
    Core::FString Previous;
    for (const FAssetProducerSetting& Setting : Settings)
    {
        if (!IsSettingName(Setting.Name) ||
            (!Previous.IsEmpty() && !(Previous < Setting.Name)))
        {
            return EAssetResult::InvalidInput;
        }
        if (const auto* Text = std::get_if<Core::FString>(&Setting.Value);
            Text != nullptr && Text->Len() > 1024)
        {
            return EAssetResult::InvalidInput;
        }
        if (const auto* Number = std::get_if<double>(&Setting.Value);
            Number != nullptr && !std::isfinite(*Number))
        {
            return EAssetResult::InvalidInput;
        }
        Previous = Setting.Name;
    }
    return EAssetResult::Success;
}

const FAssetProducerSetting* FAssetProducerSettingsRecord::Find(
    const Core::FString& Name) const noexcept
{
    const auto Iterator = std::lower_bound(
        Settings.begin(),
        Settings.end(),
        Name,
        [](const FAssetProducerSetting& Setting, const Core::FString& Candidate)
        {
            return Setting.Name < Candidate;
        });
    return Iterator != Settings.end() && Iterator->Name == Name
        ? &*Iterator
        : nullptr;
}

EAssetResult FAssetTargetBuildPolicy::Validate() const noexcept
{
    if (ProducerSettings.empty() || ProducerSettings.size() > 256)
    {
        return EAssetResult::InvalidInput;
    }
    for (Core::usize Index = 0; Index < ProducerSettings.size(); ++Index)
    {
        if (ProducerSettings[Index].Validate() != EAssetResult::Success ||
            (Index > 0 && !(ProducerSettings[Index - 1].Producer <
                            ProducerSettings[Index].Producer)))
        {
            return EAssetResult::InvalidInput;
        }
    }
    return EAssetResult::Success;
}

const FAssetProducerSettingsRecord* FAssetTargetBuildPolicy::FindProducer(
    const FAssetParticipantId& Producer) const noexcept
{
    const auto Iterator = std::lower_bound(
        ProducerSettings.begin(),
        ProducerSettings.end(),
        Producer,
        [](const FAssetProducerSettingsRecord& Record,
           const FAssetParticipantId& Candidate)
        {
            return Record.Producer < Candidate;
        });
    return Iterator != ProducerSettings.end() && Iterator->Producer == Producer
        ? &*Iterator
        : nullptr;
}

EAssetResult FAssetTargetLimits::Validate() const noexcept
{
    if (MaxDiscoveredSources == 0 || MaxDiscoveredSources > 100000 ||
        MaxAssets == 0 || MaxAssets > 100000 ||
        MaxDependencyEdges > 1000000 ||
        MaxDependencyDepth == 0 || MaxDependencyDepth > 256 ||
        MaxSourceBytes == 0 || MaxSourceBytes > 1024ULL * 1024ULL * 1024ULL ||
        MaxPayloadBytes == 0 || MaxPayloadBytes > 1024ULL * 1024ULL * 1024ULL ||
        MaxAggregateBytes == 0 || MaxAggregateBytes > 8ULL * 1024ULL * 1024ULL * 1024ULL ||
        MaxManifestBytes == 0 || MaxManifestBytes > 256ULL * 1024ULL * 1024ULL ||
        MaxDiagnostics == 0 || MaxDiagnostics > 4096)
    {
        return EAssetResult::InvalidInput;
    }
    return EAssetResult::Success;
}

EAssetResult FAssetTargetProfile::Validate() const noexcept
{
    if (Schema != Core::FString("stoner.asset-target-profile") ||
        SchemaVersion != CurrentSchemaVersion ||
        DisplayName.IsEmpty() || DisplayName.Len() > 128 ||
        ShaderPayloadChoices.empty() || ShaderPayloadChoices.size() > 32 ||
        TextureCapabilities.empty() || TextureCapabilities.size() > 64 ||
        BuildPolicy.Validate() != EAssetResult::Success ||
        Limits.Validate() != EAssetResult::Success ||
        RequiredExtensions.size() > 64 || OptionalExtensions.size() > 64 ||
        !IsStrictlySortedTokens(RequiredExtensions) ||
        !IsStrictlySortedTokens(OptionalExtensions) ||
        !EnumInRange(Platform, EAssetTargetPlatform::IOS) ||
        !EnumInRange(
            CpuArchitecture, EAssetTargetCpuArchitecture::Arm64) ||
        !EnumInRange(GraphicsBackend, EAssetGraphicsBackend::GLES) ||
        !EnumInRange(TextureFallback, EAssetTextureFallback::PortableKTX2) ||
        !EnumInRange(
            BuildPolicy.Optimization, EAssetBuildOptimization::Shipping) ||
        !EnumInRange(
            BuildPolicy.Validation, EAssetBuildValidation::Strict))
    {
        return EAssetResult::InvalidInput;
    }
    std::set<std::string> ShaderChoices;
    for (const FAssetShaderPayloadChoice& Choice : ShaderPayloadChoices)
    {
        if (!IsLowerToken(Choice.Profile, 128) ||
            !EnumInRange(Choice.Backend, EAssetGraphicsBackend::GLES) ||
            !EnumInRange(Choice.Format, EAssetShaderPayloadFormat::ESSL))
        {
            return EAssetResult::InvalidInput;
        }
        const std::string Key = std::to_string(static_cast<int>(Choice.Backend)) +
            ":" + Choice.Profile.ToStdString() + ":" +
            std::to_string(static_cast<int>(Choice.Format));
        if (!ShaderChoices.insert(Key).second)
        {
            return EAssetResult::InvalidInput;
        }
    }
    std::set<std::string> TextureChoices;
    for (const Core::FString& Capability : TextureCapabilities)
    {
        if (!IsLowerToken(Capability, 64) ||
            !TextureChoices.insert(Capability.ToStdString()).second)
        {
            return EAssetResult::InvalidInput;
        }
    }
    for (const Core::FString& Extension : RequiredExtensions)
    {
        if (!IsLowerToken(Extension, 128))
        {
            return EAssetResult::InvalidInput;
        }
    }
    for (const Core::FString& Extension : OptionalExtensions)
    {
        if (!IsLowerToken(Extension, 128))
        {
            return EAssetResult::InvalidInput;
        }
    }
    return EAssetResult::Success;
}

EAssetResult FAssetTargetProfileEvidence::Validate() const noexcept
{
    return Profile.Validate() == EAssetResult::Success &&
            EffectiveProfileDigest.IsAvailable() &&
            !CanonicalEffectiveConfiguration.IsEmpty()
        ? EAssetResult::Success
        : EAssetResult::InvalidInput;
}

EAssetResult FAssetProfileProjectionEvidence::Validate() const noexcept
{
    return Producer.IsValid() && ProducerSettingsSchemaVersion != 0 &&
            !CanonicalProducerSettings.IsEmpty() &&
            EffectiveSettingsDigest.IsAvailable() &&
            !CanonicalRelevantProfile.IsEmpty() &&
            RelevantProfileDigest.IsAvailable()
        ? EAssetResult::Success
        : EAssetResult::InvalidInput;
}

} // namespace Stoner::Asset
