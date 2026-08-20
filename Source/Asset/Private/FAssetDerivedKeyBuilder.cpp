#include "FAssetDerivedKeyBuilder.h"

#include <array>
#include <string_view>

namespace Stoner::Asset
{
namespace
{

enum class EFieldTag : Core::uint32
{
    KeyFormat = 1,
    AssetId = 2,
    SourceVersion = 3,
    SourceManifest = 4,
    Dependencies = 5,
    ImporterId = 6,
    ImporterVersion = 7,
    CookerId = 8,
    CookerVersion = 9,
    CodecId = 10,
    CodecVersion = 11,
    PayloadSchema = 12,
    EffectiveSettings = 13,
    RelevantProfile = 14,
    AdditionalEvidence = 15
};

void AppendU32(Core::TArray<Core::uint8>& Out, Core::uint32 Value)
{
    for (Core::uint32 Shift = 0; Shift < 32; Shift += 8)
    {
        Out.push_back(static_cast<Core::uint8>(Value >> Shift));
    }
}

void AppendU64(Core::TArray<Core::uint8>& Out, Core::uint64 Value)
{
    for (Core::uint32 Shift = 0; Shift < 64; Shift += 8)
    {
        Out.push_back(static_cast<Core::uint8>(Value >> Shift));
    }
}

void AppendBytes(
    Core::TArray<Core::uint8>& Out,
    std::span<const Core::uint8> Bytes)
{
    AppendU64(Out, static_cast<Core::uint64>(Bytes.size()));
    Out.insert(Out.end(), Bytes.begin(), Bytes.end());
}

void AppendString(Core::TArray<Core::uint8>& Out, std::string_view Text)
{
    AppendBytes(
        Out,
        std::span<const Core::uint8>(
            reinterpret_cast<const Core::uint8*>(Text.data()), Text.size()));
}

void AppendDigest(Core::TArray<Core::uint8>& Out, const FAssetDigest& Digest)
{
    AppendBytes(Out, Digest.GetBytes());
}

template <typename Callback>
void AppendField(
    Core::TArray<Core::uint8>& Out,
    EFieldTag Tag,
    Callback&& WritePayload)
{
    Core::TArray<Core::uint8> Payload;
    WritePayload(Payload);
    AppendU32(Out, static_cast<Core::uint32>(Tag));
    AppendBytes(Out, Payload);
}

void AppendVersion(Core::TArray<Core::uint8>& Out, const FAssetVersion& Version)
{
    AppendU32(Out, Version.SourceDigest.IsAvailable() ? 1U : 0U);
    if (Version.SourceDigest.IsAvailable()) AppendDigest(Out, Version.SourceDigest);
    AppendU32(Out, Version.ContentDigest.IsAvailable() ? 1U : 0U);
    if (Version.ContentDigest.IsAvailable()) AppendDigest(Out, Version.ContentDigest);
    AppendU32(Out, Version.CookDigest.IsAvailable() ? 1U : 0U);
    if (Version.CookDigest.IsAvailable()) AppendDigest(Out, Version.CookDigest);
    AppendString(Out, Version.Producer.ToString().View());
    AppendString(Out, Version.ProducerVersion.ToString().View());
}

} // namespace

EAssetResult FAssetDerivedKeyBuilder::BuildCanonicalStreamForTesting(
    const FAssetDerivedKeyEvidence& Evidence,
    Core::TArray<Core::uint8>& OutBytes)
{
    OutBytes.clear();
    const EAssetResult Validation = Evidence.Validate();
    if (Validation != EAssetResult::Success)
    {
        return Validation;
    }
    const std::string_view Domain = Evidence.KeyFormatVersion ==
            FAssetDerivedKeyEvidence::LegacyKeyFormatVersion
        ? "stoner.asset-derived-key.v1"
        : "stoner.asset-derived-key.v2";
    AppendString(OutBytes, Domain);
    AppendField(OutBytes, EFieldTag::KeyFormat, [&](auto& Out) {
        AppendU32(Out, Evidence.KeyFormatVersion);
    });
    AppendField(OutBytes, EFieldTag::AssetId, [&](auto& Out) {
        AppendString(Out, Evidence.AssetId.GetAssetType().View());
        AppendString(Out, Evidence.AssetId.GetLogicalPath().View());
        AppendU32(Out, Evidence.AssetId.GetSubresource().has_value() ? 1U : 0U);
        if (Evidence.AssetId.GetSubresource())
            AppendString(Out, Evidence.AssetId.GetSubresource()->View());
    });
    AppendField(OutBytes, EFieldTag::SourceVersion, [&](auto& Out) {
        AppendDigest(Out, Evidence.SourceVersion);
    });
    AppendField(OutBytes, EFieldTag::SourceManifest, [&](auto& Out) {
        AppendU64(Out, Evidence.SourceManifest.size());
        for (const auto& Source : Evidence.SourceManifest)
        {
            AppendString(Out, Source.Locator.GetScheme().View());
            AppendString(Out, Source.Locator.GetLocator().View());
            AppendDigest(Out, Source.Version);
        }
    });
    AppendField(OutBytes, EFieldTag::Dependencies, [&](auto& Out) {
        AppendU64(Out, Evidence.Dependencies.size());
        for (const auto& Dependency : Evidence.Dependencies)
        {
            AppendU32(Out, static_cast<Core::uint32>(Dependency.Role));
            AppendString(Out, Dependency.Id.GetAssetType().View());
            AppendString(Out, Dependency.Id.GetLogicalPath().View());
            AppendU32(Out, Dependency.Id.GetSubresource().has_value() ? 1U : 0U);
            if (Dependency.Id.GetSubresource())
                AppendString(Out, Dependency.Id.GetSubresource()->View());
            AppendVersion(Out, Dependency.Version);
        }
    });
    AppendField(OutBytes, EFieldTag::ImporterId, [&](auto& Out) {
        AppendString(Out, Evidence.ImporterId.ToString().View());
    });
    AppendField(OutBytes, EFieldTag::ImporterVersion, [&](auto& Out) {
        AppendString(Out, Evidence.ImporterVersion.ToString().View());
    });
    AppendField(OutBytes, EFieldTag::CookerId, [&](auto& Out) {
        AppendString(Out, Evidence.CookerId.ToString().View());
    });
    AppendField(OutBytes, EFieldTag::CookerVersion, [&](auto& Out) {
        AppendString(Out, Evidence.CookerVersion.ToString().View());
    });
    AppendField(OutBytes, EFieldTag::CodecId, [&](auto& Out) {
        AppendString(Out, Evidence.CodecId.ToString().View());
    });
    AppendField(OutBytes, EFieldTag::CodecVersion, [&](auto& Out) {
        AppendString(Out, Evidence.CodecVersion.ToString().View());
    });
    AppendField(OutBytes, EFieldTag::PayloadSchema, [&](auto& Out) {
        AppendU32(Out, Evidence.PayloadSchemaVersion);
    });
    AppendField(OutBytes, EFieldTag::EffectiveSettings, [&](auto& Out) {
        AppendDigest(Out, Evidence.EffectiveSettingsDigest);
    });
    AppendField(OutBytes, EFieldTag::RelevantProfile, [&](auto& Out) {
        AppendDigest(Out, Evidence.RelevantProfileDigest);
    });
    if (Evidence.KeyFormatVersion >=
        FAssetDerivedKeyEvidence::CurrentKeyFormatVersion)
    {
        AppendField(OutBytes, EFieldTag::AdditionalEvidence, [&](auto& Out) {
            AppendU64(Out, Evidence.AdditionalEvidence.size());
            for (const auto& Item : Evidence.AdditionalEvidence)
            {
                AppendString(Out, Item.Name.View());
                AppendDigest(Out, Item.Digest);
            }
        });
    }
    return EAssetResult::Success;
}

EAssetResult FAssetDerivedKeyBuilder::Build(
    const FAssetDerivedKeyEvidence& Evidence,
    FAssetDerivedKey& OutKey)
{
    OutKey = {};
    Core::TArray<Core::uint8> Bytes;
    const EAssetResult Result = BuildCanonicalStreamForTesting(Evidence, Bytes);
    if (Result != EAssetResult::Success)
    {
        return Result;
    }
    OutKey = FAssetDerivedKey(FAssetDigest::FromBytes(Bytes));
    return EAssetResult::Success;
}

} // namespace Stoner::Asset
