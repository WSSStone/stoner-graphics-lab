#include "FCookedAssetLoadingStrategy.h"

#include "Asset/FAssetCookContractCodec.h"
#include "Core/FPlatformFileSystem.h"
#include "FShaderPayloadValidation.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>

namespace Stoner::Asset::Private
{

FCookedAssetLoadingStrategy::FCookedAssetLoadingStrategy(
    FAssetManagerConfig Config,
    const FBoundCookedGeneration& Generation,
    Core::TSharedPtr<FAssetManagerExecutionCounterState> Counters)
    : Config_(std::move(Config)), Generation_(Generation),
      Counters_(std::move(Counters))
{
}

FAssetLoadScratchResult FCookedAssetLoadingStrategy::Load(
    const FAssetLoadKey& Key,
    const FAssetRuntimeExecutionContext& Context)
{
    FAssetLoadScratchResult Out;
    FAssetManagerExecutionCounterState::Increment(
        Counters_->StrictLoaderExecutions);
    if (Context.ShouldStop())
    {
        Out.Result = EAssetResult::Cancelled;
        return Out;
    }
    const auto& Manifest = Generation_.GetManifest();
    const auto Found = std::lower_bound(
        Manifest.Records.begin(), Manifest.Records.end(), Key.AssetId,
        [](const FAssetCookManifestRecord& Record, const FAssetId& Id)
        {
            return Record.AssetId < Id;
        });
    if (Found == Manifest.Records.end() || Found->AssetId != Key.AssetId)
    {
        Out.Result = EAssetResult::NotFound;
        return Out;
    }
    if (Found->AssetType != Key.ExpectedType)
    {
        Out.Result = EAssetResult::TypeMismatch;
        return Out;
    }
    const auto PayloadPath = std::filesystem::path(
        Generation_.GetGenerationDirectory().ToStdString()) /
        Found->PayloadLocator.ToStdString();
    Core::FPlatformFileInfo Info;
    const Core::FString Path(PayloadPath.lexically_normal().generic_string());
    if (!Core::FPlatformFileSystem::QueryRegularFile(
            Path, Config_.Limits.MaxPayloadBytes, Info).IsSuccess() ||
        Info.ByteSize != Found->PayloadBytes)
    {
        Out.Result = EAssetResult::CorruptPayload;
        return Out;
    }
    Core::TArray<Core::uint8> Bytes;
    if (!Core::FPlatformFileSystem::ReadFile(Path, Bytes))
    {
        Out.Result = EAssetResult::CorruptPayload;
        return Out;
    }
    FAssetCookedPayloadLimits Limits;
    Limits.MaxEnvelopeBytes = Config_.Limits.MaxPayloadBytes;
    Limits.MaxBodyBytes = Config_.Limits.MaxPayloadBytes;
    Core::TSharedPtr<const FAssetPayload> Payload;
    FAssetCookedPayloadEnvelope Envelope;
    if (FAssetCookContractCodec::LoadTypedPayload(
            Bytes, Limits, Payload, &Envelope) != EAssetResult::Success ||
        !Payload || Envelope.EnvelopeDigest != Found->EnvelopeDigest ||
        Envelope.Header.AssetId != Found->AssetId ||
        Envelope.Header.AssetType != Found->AssetType ||
        Envelope.Header.CodecId != Found->Codec.Id.ToString() ||
        Found->Codec.Version.ToString() != Core::FString(
            std::to_string(Envelope.Header.CodecVersion)) ||
        Envelope.Header.PayloadSchemaVersion != Found->PayloadSchemaVersion ||
        (Found->AssetType == Core::FString("ShaderPayload") &&
         ValidateStrictCookedShaderPayload(
             Manifest.TargetProfile.Profile,
             Envelope.Header.CodecVersion,
             Envelope.Header.PayloadSchemaVersion,
             *Payload) != EAssetResult::Success))
    {
        Out.Result = EAssetResult::CorruptPayload;
        return Out;
    }
    FAssetMetadata Metadata;
    Metadata.Id = Found->AssetId;
    (void)FAssetSourceLocator::Create(
        Core::FString("cooked"), Found->PayloadLocator, Metadata.Source);
    Metadata.Producer = Found->Importer.Id;
    Metadata.ProducerVersion = Found->Importer.Version;
    Metadata.Version.SourceDigest = Found->SourceVersion;
    Metadata.Version.ContentDigest = Envelope.Header.BodyDigest;
    Metadata.Version.CookDigest = Found->EnvelopeDigest;
    Metadata.Version.Producer = Found->Cooker.Id;
    Metadata.Version.ProducerVersion = Found->Cooker.Version;
    Metadata.Version.TargetProfile =
        Manifest.TargetProfile.EffectiveProfileDigest.ToLowerHex();
    for (const auto& Dependency : Found->Dependencies)
    {
        const auto DependencyRecord = std::lower_bound(
            Manifest.Records.begin(), Manifest.Records.end(),
            Dependency.AssetId,
            [](const FAssetCookManifestRecord& Record, const FAssetId& Id)
            {
                return Record.AssetId < Id;
            });
        if (DependencyRecord == Manifest.Records.end() ||
            DependencyRecord->AssetId != Dependency.AssetId ||
            (Dependency.RequiredVersion &&
             DependencyRecord->SourceVersion != *Dependency.RequiredVersion))
        {
            Out.Metadata.clear();
            Out.Payloads.clear();
            Out.Result = EAssetResult::Conflict;
            return Out;
        }
        EAssetDependencyRole Role = EAssetDependencyRole::Runtime;
        if (Dependency.Role == Core::FString("source"))
            Role = EAssetDependencyRole::Source;
        else if (Dependency.Role == Core::FString("build"))
            Role = EAssetDependencyRole::Build;
        else if (Dependency.Role != Core::FString("runtime"))
        {
            Out.Metadata.clear();
            Out.Payloads.clear();
            Out.Result = EAssetResult::CorruptPayload;
            return Out;
        }
        Metadata.Dependencies.push_back({
            Dependency.AssetId,
            Role,
            EAssetDependencyStrength::Required,
            EAssetDependencyResolution::Resolved});
    }
    Out.Metadata.push_back(std::move(Metadata));
    Out.Payloads.push_back(std::move(Payload));
    Out.PayloadBytes.push_back(Found->PayloadBytes);
    Out.Result = EAssetResult::Success;
    return Out;
}

} // namespace Stoner::Asset::Private
