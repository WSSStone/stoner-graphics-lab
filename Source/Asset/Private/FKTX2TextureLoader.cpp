#include "Asset/FKTX2TextureLoader.h"

#include <new>
#include <stdexcept>

namespace Stoner::Asset
{
namespace
{

FAssetLoadResult Failure(
    EAssetResult Result,
    const char* Code,
    const char* Field,
    const char* Reason)
{
    FAssetDiagnostic Diagnostic;
    Diagnostic.Stage = EAssetStage::Load;
    Diagnostic.Result = Result;
    Diagnostic.Severity = EAssetDiagnosticSeverity::Error;
    Diagnostic.Code = Core::FString(Code);
    Diagnostic.Participant = Core::FString("loader.ktx2");
    Diagnostic.Field = Core::FString(Field);
    Diagnostic.Reason = Core::FString(Reason);
    return {Result, {}, {std::move(Diagnostic)}};
}

} // namespace

FAssetExtensionCapability
FKTX2TextureLoader::GetCapability() const
{
    FAssetExtensionCapability Capability;
    Capability.Kind = EAssetExtensionKind::Loader;
    (void)FAssetParticipantId::Create(
        Core::FString("loader.ktx2"),
        Capability.Participant);
    (void)FAssetProducerVersion::Create(
        Core::FString("022-v1"),
        Capability.ProducerVersion);
    Capability.Priority = 100;
    Capability.FormatHints = {Core::FString("ktx2")};
    return Capability;
}

FAssetLoadResult FKTX2TextureLoader::Load(
    const FAssetLoadRequest& Request)
{
    const auto Parameters =
        std::dynamic_pointer_cast<const FKTX2LoadParameters>(
            Request.Parameters);
    if (!Parameters ||
        !Parameters->ExpectedId.IsValid() ||
        Parameters->ExpectedId != Request.Metadata.Id ||
        Request.Metadata.Validate() != EAssetResult::Success ||
        Parameters->Limits.Validate() != EAssetResult::Success)
    {
        return Failure(
            EAssetResult::InvalidInput,
            "asset.ktx2.load-request",
            "request",
            "typed identity metadata limits and source are required");
    }

    Core::TArray<Core::uint8> Bytes;
    EAssetResult Result = Request.Source.ReadBounded(
        Parameters->Limits.MaxArtifactBytes,
        std::nullopt,
        Bytes);
    if (Result != EAssetResult::Success)
    {
        if (Result == EAssetResult::ImageLimitExceeded)
        {
            Result = EAssetResult::KTX2LimitExceeded;
        }
        return Failure(
            Result,
            "asset.ktx2.load-source",
            "artifactBytes",
            "bounded source read failed");
    }

    FKTX2TextureArtifact Artifact;
    FAssetDiagnosticList Diagnostics;
    Result = FKTX2TextureCodec::Open(
        Parameters->ExpectedId,
        Bytes,
        Parameters->Limits,
        Artifact,
        &Diagnostics);
    if (Result != EAssetResult::Success)
    {
        return {Result, {}, std::move(Diagnostics)};
    }

    try
    {
        return {
            EAssetResult::Success,
            Core::MakeShared<FKTX2TextureArtifact>(
                std::move(Artifact)),
            std::move(Diagnostics)};
    }
    catch (const std::bad_alloc&)
    {
        return Failure(
            EAssetResult::CapacityExceeded,
            "asset.ktx2.load-publish",
            "payload",
            "artifact publication allocation failed");
    }
    catch (const std::length_error&)
    {
        return Failure(
            EAssetResult::CapacityExceeded,
            "asset.ktx2.load-publish",
            "payload",
            "artifact publication capacity was exceeded");
    }
}

} // namespace Stoner::Asset
