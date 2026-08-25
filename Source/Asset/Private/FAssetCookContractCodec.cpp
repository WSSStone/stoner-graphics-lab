#include "Asset/FAssetCookContractCodec.h"

#include "FAssetCookedPayloadCodec.h"
#include "FAssetDerivedKeyBuilder.h"
#include "FAssetDerivedDataEntryCodec.h"
#include "FAssetCookManifestCodec.h"
#include "FAssetTargetProfileCodec.h"
#include "FImageTextureCookedCodec.h"
#include "FMaterialShaderCookedCodec.h"
#include "FStaticModelCookedCodec.h"
#include "FCurrentGenerationPointerCodec.h"

#include "Asset/FImageAsset.h"
#include "Asset/FKTX2TextureArtifact.h"
#include "Asset/FMaterialAsset.h"
#include "Asset/FMaterialInstanceAsset.h"
#include "Asset/FShaderAsset.h"
#include "Asset/FShaderPayloadAsset.h"
#include "Asset/FShaderSourceAsset.h"
#include "Asset/FStaticMeshAsset.h"
#include "Asset/FStaticModelAsset.h"
#include "Asset/FTextureAsset.h"

namespace Stoner::Asset
{
namespace
{

EAssetResult DecodeTypedPayload(
    FAssetCookedPayloadEnvelope Envelope,
    Core::TSharedPtr<const FAssetPayload>& OutPayload,
    FAssetCookedPayloadEnvelope* OutEnvelope)
{
    EAssetResult Result = Private::DecodeImageTextureCookedBody(
        Envelope.Header, Envelope.Body, OutPayload);
    if (Result == EAssetResult::Unsupported)
        Result = Private::DecodeMaterialShaderCookedBody(
            Envelope.Header, Envelope.Body, OutPayload);
    if (Result == EAssetResult::Unsupported)
        Result = Private::DecodeStaticModelCookedBody(
            Envelope.Header, Envelope.Body, OutPayload);
    if (Result != EAssetResult::Success)
    {
        OutPayload.reset();
        return Result;
    }
    if (OutEnvelope) *OutEnvelope = std::move(Envelope);
    return EAssetResult::Success;
}

} // namespace

EAssetResult FAssetCookContractCodec::ParseTargetProfile(
    std::span<const Core::uint8> Bytes,
    FAssetTargetProfileEvidence& OutEvidence)
{
    return Private::ParseAssetTargetProfile(Bytes, OutEvidence);
}

EAssetResult FAssetCookContractCodec::WriteTargetProfile(
    const FAssetTargetProfile& Profile,
    Core::FString& OutCanonical,
    FAssetTargetProfileEvidence* OutEvidence)
{
    return Private::WriteAssetTargetProfile(
        Profile, OutCanonical, OutEvidence);
}

EAssetResult FAssetCookContractCodec::WriteCookedPayload(
    const FAssetCookedPayloadHeader& Header,
    std::span<const Core::uint8> ReservedHeaderExtensions,
    std::span<const Core::uint8> Body,
    const FAssetCookedPayloadLimits& Limits,
    Core::TArray<Core::uint8>& OutBytes,
    FAssetCookedPayloadEnvelope* OutEnvelope)
{
    return Private::WriteAssetCookedPayload(
        Header, ReservedHeaderExtensions, Body, Limits,
        OutBytes, OutEnvelope);
}

EAssetResult FAssetCookContractCodec::ParseCookedPayload(
    std::span<const Core::uint8> Bytes,
    const FAssetCookedPayloadLimits& Limits,
    FAssetCookedPayloadEnvelope& OutEnvelope)
{
    return Private::ParseAssetCookedPayload(Bytes, Limits, OutEnvelope);
}

EAssetResult FAssetCookContractCodec::WriteTypedPayload(
    const FAssetPayload& Payload,
    const FAssetCookedPayloadLimits& Limits,
    Core::TArray<Core::uint8>& OutBytes,
    FAssetCookedPayloadEnvelope* OutEnvelope)
{
    OutBytes.clear();
    if (OutEnvelope) *OutEnvelope = {};
    FAssetCookedPayloadHeader Header;
    Core::TArray<Core::uint8> Body;
    EAssetResult Result = Private::EncodeImageTextureCookedBody(
        Payload, Limits, Header, Body);
    if (Result == EAssetResult::TypeMismatch)
        Result = Private::EncodeMaterialShaderCookedBody(
            Payload, Limits, Header, Body);
    if (Result == EAssetResult::TypeMismatch)
        Result = Private::EncodeStaticModelCookedBody(
            Payload, Limits, Header, Body);
    if (Result != EAssetResult::Success) return Result;
    return Private::WriteAssetCookedPayload(
        Header, {}, Body, Limits, OutBytes, OutEnvelope);
}

EAssetResult FAssetCookContractCodec::DescribeTypedPayload(
    const FAssetPayload& Payload,
    FAssetCookedPayloadHeader& OutHeader)
{
    OutHeader = {};
    const auto Set = [&OutHeader](const FAssetId& Id, const char* Codec)
    {
        OutHeader.AssetId = Id;
        OutHeader.AssetType = Id.GetAssetType();
        OutHeader.CodecId = Core::FString(Codec);
        OutHeader.CodecVersion = 1;
        OutHeader.PayloadSchemaVersion = 1;
        return EAssetResult::Success;
    };
    if (const auto* Value = dynamic_cast<const FImageAsset*>(&Payload))
        return Set(Value->GetId(), "stoner.image");
    if (const auto* Value = dynamic_cast<const FTextureAsset*>(&Payload))
        return Set(Value->GetId(), "stoner.texture");
    if (const auto* Value = dynamic_cast<const FKTX2TextureArtifact*>(&Payload))
        return Set(Value->GetId(), "stoner.ktx2");
    if (const auto* Value = dynamic_cast<const FShaderAsset*>(&Payload))
        return Set(Value->GetDesc().Id, "stoner.shader-program");
    if (const auto* Value = dynamic_cast<const FMaterialAsset*>(&Payload))
        return Set(Value->GetDesc().Id, "stoner.material");
    if (const auto* Value = dynamic_cast<const FMaterialInstanceAsset*>(&Payload))
        return Set(Value->GetDesc().Id, "stoner.material-instance");
    if (const auto* Value = dynamic_cast<const FShaderSourceAsset*>(&Payload))
        return Set(Value->GetId(), "stoner.shader-source");
    if (const auto* Value = dynamic_cast<const FShaderPayloadAsset*>(&Payload))
        return Set(Value->GetId(), "stoner.shader-payload");
    if (const auto* Value = dynamic_cast<const FStaticMeshAsset*>(&Payload))
        return Set(Value->GetDesc().Id, "stoner.static-mesh");
    if (const auto* Value = dynamic_cast<const FStaticModelAsset*>(&Payload))
        return Set(Value->GetDesc().Id, "stoner.static-model");
    return EAssetResult::TypeMismatch;
}

EAssetResult FAssetCookContractCodec::LoadTypedPayload(
    std::span<const Core::uint8> Bytes,
    const FAssetCookedPayloadLimits& Limits,
    Core::TSharedPtr<const FAssetPayload>& OutPayload,
    FAssetCookedPayloadEnvelope* OutEnvelope)
{
    OutPayload.reset();
    if (OutEnvelope) *OutEnvelope = {};
    FAssetCookedPayloadEnvelope Envelope;
    const EAssetResult Parse = Private::ParseAssetCookedPayload(
        Bytes, Limits, Envelope);
    if (Parse != EAssetResult::Success) return Parse;
    return DecodeTypedPayload(std::move(Envelope), OutPayload, OutEnvelope);
}

EAssetResult FAssetCookContractCodec::LoadManifestAuthenticatedTypedPayload(
    std::span<const Core::uint8> Bytes,
    const FAssetDigest& ExpectedEnvelopeDigest,
    const FAssetCookedPayloadLimits& Limits,
    Core::TSharedPtr<const FAssetPayload>& OutPayload,
    FAssetCookedPayloadEnvelope* OutEnvelope)
{
    OutPayload.reset();
    if (OutEnvelope) *OutEnvelope = {};
    if (!ExpectedEnvelopeDigest.IsAvailable())
        return EAssetResult::InvalidInput;
    FAssetCookedPayloadEnvelope Envelope;
    const EAssetResult Parse =
        Private::ParseManifestAuthenticatedAssetCookedPayload(
            Bytes, ExpectedEnvelopeDigest, Limits, Envelope);
    if (Parse != EAssetResult::Success) return Parse;
    return DecodeTypedPayload(std::move(Envelope), OutPayload, OutEnvelope);
}

EAssetResult FAssetCookContractCodec::WriteManifest(
    FAssetCookManifest& InOutManifest,
    const FAssetCookManifestLimits& Limits,
    Core::FString& OutCanonical)
{
    return Private::WriteAssetCookManifest(
        InOutManifest, Limits, OutCanonical);
}

EAssetResult FAssetCookContractCodec::ParseManifest(
    std::span<const Core::uint8> Bytes,
    const FAssetCookManifestLimits& Limits,
    FAssetCookManifest& OutManifest)
{
    return Private::ParseAssetCookManifest(Bytes, Limits, OutManifest);
}

EAssetResult FAssetCookContractCodec::ComputeManifestGenerationId(
    const FAssetCookManifest& Manifest,
    FAssetDigest& OutGenerationId)
{
    return Private::ComputeAssetCookManifestGenerationId(
        Manifest, OutGenerationId);
}

EAssetResult FAssetCookContractCodec::BuildDerivedKey(
    const FAssetDerivedKeyEvidence& Evidence,
    FAssetDerivedKey& OutKey)
{
    return FAssetDerivedKeyBuilder::Build(Evidence, OutKey);
}

EAssetResult FAssetCookContractCodec::BuildProfileProjection(
    const FAssetTargetProfileEvidence& Profile,
    const FAssetParticipantId& Producer,
    Core::uint32 ExpectedSchemaVersion,
    std::span<const Core::FString> RelevantTargetFields,
    FAssetProfileProjectionEvidence& OutProjection)
{
    return Private::BuildAssetProfileProjection(
        Profile, Producer, ExpectedSchemaVersion,
        RelevantTargetFields, OutProjection);
}

EAssetResult FAssetCookContractCodec::WriteDerivedDataEntry(
    const FAssetDerivedDataEntry& Entry,
    Core::FString& OutCanonical)
{
    return Private::WriteAssetDerivedDataEntry(Entry, OutCanonical);
}

EAssetResult FAssetCookContractCodec::ParseDerivedDataEntry(
    std::span<const Core::uint8> Bytes,
    const FAssetDerivedDataEntryLimits& Limits,
    FAssetDerivedDataEntry& OutEntry)
{
    return Private::ParseAssetDerivedDataEntry(Bytes, Limits, OutEntry);
}

EAssetResult FAssetCookContractCodec::WriteCurrentPointer(
    const FCurrentGenerationPointer& Pointer,
    Core::FString& OutCanonical)
{
    return Private::WriteCurrentGenerationPointer(Pointer, OutCanonical);
}

EAssetResult FAssetCookContractCodec::ParseCurrentPointer(
    std::span<const Core::uint8> Bytes,
    FCurrentGenerationPointer& OutPointer)
{
    return Private::ParseCurrentGenerationPointer(Bytes, OutPointer);
}

} // namespace Stoner::Asset
