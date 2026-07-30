#include "Asset/FTextureTranscode.h"

#include "FBasisTextureTranscoder.h"

#include <new>
#include <stdexcept>

namespace Stoner::Asset
{
namespace
{

void AddDiagnostic(
    FAssetDiagnosticList& Diagnostics,
    EAssetResult Result,
    const char* Code,
    const char* Field,
    const char* Reason)
{
    FAssetDiagnostic Diagnostic;
    Diagnostic.Stage = EAssetStage::Transcode;
    Diagnostic.Result = Result;
    Diagnostic.Severity = EAssetDiagnosticSeverity::Error;
    Diagnostic.Code = Core::FString(Code);
    Diagnostic.Participant = Core::FString("transcoder.texture");
    Diagnostic.Field = Core::FString(Field);
    Diagnostic.Reason = Core::FString(Reason);
    Diagnostics.push_back(std::move(Diagnostic));
}

FTextureTranscodeResult Fail(
    EAssetResult Result,
    FAssetDiagnosticList Diagnostics)
{
    FTextureTranscodeResult Failed;
    Failed.Result = Result;
    Failed.Diagnostics = std::move(Diagnostics);
    return Failed;
}

} // namespace

FTextureTranscodeResult FTextureTranscoder::Transcode(
    const FTextureTranscodeRequest& Request)
{
    FAssetDiagnosticList Diagnostics;
    if (!Request.Artifact ||
        Request.TargetFormat == ETextureTranscodeFormat::Unknown ||
        Request.Limits.Validate() != EAssetResult::Success)
    {
        AddDiagnostic(
            Diagnostics,
            EAssetResult::InvalidInput,
            "asset.ktx2.transcode-request",
            "request",
            "artifact, target format, and limits must be valid");
        return Fail(
            EAssetResult::InvalidInput, std::move(Diagnostics));
    }
    if (Request.Artifact->GetBytes().empty() ||
        Request.Artifact->GetArtifactDigest() !=
            FAssetDigest::FromBytes(
                Request.Artifact->GetBytes()) ||
        Request.Artifact->GetInfo().ArtifactDigest !=
            Request.Artifact->GetArtifactDigest())
    {
        AddDiagnostic(
            Diagnostics,
            EAssetResult::CorruptPayload,
            "asset.ktx2.transcode-artifact",
            "artifactDigest",
            "artifact bytes and immutable digest evidence disagree");
        return Fail(
            EAssetResult::CorruptPayload, std::move(Diagnostics));
    }

    try
    {
        FTranscodedTexturePayload Payload;
        const EAssetResult Result =
            Private::TranscodeBasisTexture(
                *Request.Artifact,
                Request.TargetFormat,
                Request.Limits,
                Payload,
                Diagnostics);
        if (Result != EAssetResult::Success)
        {
            return Fail(Result, std::move(Diagnostics));
        }
        FTextureTranscodeResult Success;
        Success.Result = EAssetResult::Success;
        Success.Payload =
            Core::MakeShared<FTranscodedTexturePayload>(
                std::move(Payload));
        return Success;
    }
    catch (const std::bad_alloc&)
    {
        AddDiagnostic(
            Diagnostics,
            EAssetResult::TranscodeFailure,
            "asset.ktx2.transcode-allocation",
            "targetPayload",
            "target payload allocation failed");
    }
    catch (const std::length_error&)
    {
        AddDiagnostic(
            Diagnostics,
            EAssetResult::KTX2LimitExceeded,
            "asset.ktx2.transcode-length",
            "targetPayload",
            "target payload length exceeds the host container limit");
    }
    return Fail(
        Diagnostics.back().Result, std::move(Diagnostics));
}

} // namespace Stoner::Asset
