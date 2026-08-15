#include "Renderer/FKTX2TextureRealization.h"

#include "Asset/FKTX2TextureArtifact.h"
#include "Asset/FTextureTranscode.h"
#include "RHI/FRHIFormatInfo.h"
#include "RHI/FRHITextureDesc.h"
#include "RHI/FRHITextureUploadDesc.h"
#include "RHI/IRHIDevice.h"
#include "RHI/IRHITexture.h"

#include <limits>
#include <new>
#include <span>
#include <stdexcept>

namespace Stoner::Renderer
{
namespace
{

struct FRealizationMip
{
    Stoner::Core::uint32 MipLevel = 0;
    Stoner::Asset::FImageExtent2D Extent;
    Stoner::Core::uint64 RowPitchBytes = 0;
    std::span<const Stoner::Core::uint8> Bytes;
};

FKTX2TextureRealizationResult Failure(
    EKTX2TextureRealizationStage Stage,
    Stoner::RHI::ERHIResult Result,
    const Stoner::Core::FString& Identity,
    const char* Code,
    const char* Reason,
    FTextureTargetSelection Selection = {},
    std::optional<Stoner::Core::uint32> MipLevel =
        std::nullopt)
{
    FKTX2TextureRealizationResult Failed;
    Failed.Result = Result;
    Failed.Selection = std::move(Selection);
    Failed.Diagnostic = {
        Stage,
        Result,
        Identity,
        MipLevel,
        Stoner::Core::FString(Code),
        Stoner::Core::FString(Reason)};
    return Failed;
}

[[nodiscard]] Stoner::RHI::ERHIResult MapAssetResult(
    Stoner::Asset::EAssetResult Result) noexcept
{
    using Stoner::Asset::EAssetResult;
    using Stoner::RHI::ERHIResult;
    switch (Result)
    {
    case EAssetResult::Success:
        return ERHIResult::Success;
    case EAssetResult::Unsupported:
    case EAssetResult::UnsupportedCompression:
        return ERHIResult::Unsupported;
    case EAssetResult::CapacityExceeded:
    case EAssetResult::ImageLimitExceeded:
    case EAssetResult::KTX2LimitExceeded:
        return ERHIResult::Unavailable;
    case EAssetResult::InvalidIdentity:
    case EAssetResult::InvalidUtf8:
    case EAssetResult::IdentityTooLong:
    case EAssetResult::TypeMismatch:
    case EAssetResult::InvalidInput:
        return ERHIResult::InvalidState;
    case EAssetResult::NotFound:
    case EAssetResult::AccessDenied:
    case EAssetResult::MalformedSource:
    case EAssetResult::TransientFailure:
    case EAssetResult::AlreadyExists:
    case EAssetResult::Conflict:
    case EAssetResult::UnresolvedDependency:
    case EAssetResult::DependencyCycle:
    case EAssetResult::IncompleteRegistry:
    case EAssetResult::NoMatchingResolver:
    case EAssetResult::AmbiguousResolver:
    case EAssetResult::NoMatchingImporter:
    case EAssetResult::AmbiguousImporter:
    case EAssetResult::DependencyFailure:
    case EAssetResult::ProcessingFailure:
    case EAssetResult::RegistrationInactive:
    case EAssetResult::TruncatedSource:
    case EAssetResult::UnsupportedColorProfile:
    case EAssetResult::NonFiniteImageData:
    case EAssetResult::HDRPrecisionRangeExceeded:
    case EAssetResult::MalformedContainer:
    case EAssetResult::CorruptPayload:
    case EAssetResult::CookFailure:
    case EAssetResult::TranscodeFailure:
    case EAssetResult::InvalidDefinition:
    case EAssetResult::UnsupportedSchema:
    case EAssetResult::UnknownRequiredExtension:
    case EAssetResult::DefinitionLimitExceeded:
    case EAssetResult::DependencyMismatch:
    case EAssetResult::InvalidShaderProgram:
    case EAssetResult::TargetUnavailable:
    case EAssetResult::AmbiguousTarget:
    case EAssetResult::InvalidMaterialAsset:
    case EAssetResult::InvalidInstanceChain:
    case EAssetResult::InvalidHandle:
    case EAssetResult::NotReady:
    case EAssetResult::Cancelled:
    case EAssetResult::ShuttingDown:
    case EAssetResult::ReentrantPump:
    case EAssetResult::DeadlineExceeded:
    case EAssetResult::SourceChanged:
        return ERHIResult::Failed;
    }
    return ERHIResult::Failed;
}

[[nodiscard]] bool IsArtifactValid(
    const Stoner::Asset::FKTX2TextureArtifact& Artifact) noexcept
{
    const auto& Info = Artifact.GetInfo();
    const auto Bytes = Artifact.GetBytes();
    if (!Info.TextureId.IsValid() ||
        !Info.BaseExtent.IsValid() ||
        Info.Levels.empty() ||
        Info.Levels.size() >
            std::numeric_limits<Stoner::Core::uint32>::max() ||
        Bytes.empty() ||
        Artifact.GetArtifactDigest() !=
            Stoner::Asset::FAssetDigest::FromBytes(Bytes) ||
        Info.ArtifactDigest != Artifact.GetArtifactDigest())
    {
        return false;
    }
    for (Stoner::Core::usize Index = 0;
         Index < Info.Levels.size();
         ++Index)
    {
        const auto& Level = Info.Levels[Index];
        if (Level.MipLevel != Index ||
            !Level.Extent.IsValid() ||
            Level.Extent.Width !=
                Stoner::RHI::GetRHIMipExtent(
                    Info.BaseExtent.Width,
                    static_cast<Stoner::Core::uint32>(Index)) ||
            Level.Extent.Height !=
                Stoner::RHI::GetRHIMipExtent(
                    Info.BaseExtent.Height,
                    static_cast<Stoner::Core::uint32>(Index)))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool BuildBasisMips(
    const Stoner::Asset::FTranscodedTexturePayload& Payload,
    Stoner::RHI::ERHIFormat Format,
    Stoner::Core::TArray<FRealizationMip>& OutMips) noexcept
{
    OutMips.clear();
    try
    {
        OutMips.reserve(Payload.Mips.size());
        const Stoner::RHI::FRHIFormatInfo Info =
            Stoner::RHI::GetRHIFormatInfo(Format);
        for (Stoner::Core::usize Index = 0;
             Index < Payload.Mips.size();
             ++Index)
        {
            const auto& Mip = Payload.Mips[Index];
            Stoner::RHI::FRHITextureFootprint Footprint;
            if (Mip.MipLevel != Index ||
                Mip.BlockWidth != Info.BlockWidth ||
                Mip.BlockHeight != Info.BlockHeight ||
                Mip.BytesPerBlock != Info.BytesPerBlock ||
                !Stoner::RHI::TryGetRHITextureFootprint(
                    Format,
                    Mip.Extent.Width,
                    Mip.Extent.Height,
                    1,
                    Footprint) ||
                Mip.RowPitchBytes !=
                    Footprint.TightRowBytes ||
                Mip.Bytes.size() != Footprint.TotalBytes)
            {
                OutMips.clear();
                return false;
            }
            OutMips.push_back({
                Mip.MipLevel,
                Mip.Extent,
                Mip.RowPitchBytes,
                Mip.Bytes});
        }
    }
    catch (const std::bad_alloc&)
    {
        OutMips.clear();
        return false;
    }
    catch (const std::length_error&)
    {
        OutMips.clear();
        return false;
    }
    return !OutMips.empty();
}

[[nodiscard]] bool BuildUncompressedMips(
    const Stoner::Asset::FKTX2TextureArtifact& Artifact,
    Stoner::RHI::ERHIFormat Format,
    Stoner::Core::TArray<FRealizationMip>& OutMips) noexcept
{
    OutMips.clear();
    const auto Bytes = Artifact.GetBytes();
    try
    {
        OutMips.reserve(Artifact.GetInfo().Levels.size());
        for (const auto& Level : Artifact.GetInfo().Levels)
        {
            Stoner::RHI::FRHITextureFootprint Footprint;
            if (!Stoner::RHI::TryGetRHITextureFootprint(
                    Format,
                    Level.Extent.Width,
                    Level.Extent.Height,
                    1,
                    Footprint) ||
                Level.ByteOffset > Bytes.size() ||
                Level.ByteLength >
                    Bytes.size() - Level.ByteOffset ||
                Level.ByteLength != Footprint.TotalBytes)
            {
                OutMips.clear();
                return false;
            }
            OutMips.push_back({
                Level.MipLevel,
                Level.Extent,
                Footprint.TightRowBytes,
                Bytes.subspan(
                    static_cast<Stoner::Core::usize>(
                        Level.ByteOffset),
                    static_cast<Stoner::Core::usize>(
                        Level.ByteLength))});
        }
    }
    catch (const std::bad_alloc&)
    {
        OutMips.clear();
        return false;
    }
    catch (const std::length_error&)
    {
        OutMips.clear();
        return false;
    }
    return !OutMips.empty();
}

} // namespace

FKTX2TextureRealizationResult FKTX2TextureRealizer::Realize(
    const FKTX2TextureRealizationRequest& Request)
{
    using namespace Stoner::Asset;
    using namespace Stoner::RHI;

    const Stoner::Core::FString Identity = Request.Artifact
        ? Request.Artifact->GetId().ToString()
        : Stoner::Core::FString();
    if (!Request.Device || !Request.Device->IsActive())
    {
        return Failure(
            EKTX2TextureRealizationStage::ValidateArtifact,
            ERHIResult::InvalidState,
            Identity,
            "renderer.ktx2-realization.device",
            "device is missing or inactive");
    }
    if (!Request.Artifact ||
        !IsArtifactValid(*Request.Artifact))
    {
        return Failure(
            EKTX2TextureRealizationStage::ValidateArtifact,
            ERHIResult::InvalidState,
            Identity,
            "renderer.ktx2-realization.artifact",
            "artifact identity metadata bytes or mip chain is invalid");
    }

    FTextureTargetSelection Selection =
        SelectTextureTarget(
            Request.Artifact->GetInfo(),
            Request.TargetProfile,
            Request.Device->GetCapabilities());
    if (Selection.Result != ERHIResult::Success)
    {
        return Failure(
            EKTX2TextureRealizationStage::Select,
            Selection.Result,
            Identity,
            "renderer.ktx2-realization.select",
            "no compatible target satisfies profile and device usage",
            std::move(Selection));
    }

    FTextureTranscodeResult Transcoded;
    Stoner::Core::TArray<FRealizationMip> Mips;
    if (Request.Artifact->GetInfo().CompressionPolicy ==
        ETextureCompressionPolicy::Uncompressed)
    {
        if (!BuildUncompressedMips(
                *Request.Artifact,
                Selection.SelectedFormat,
                Mips))
        {
            return Failure(
                EKTX2TextureRealizationStage::Transcode,
                ERHIResult::Failed,
                Identity,
                "renderer.ktx2-realization.footprint",
                "stored mip bytes disagree with RHI footprints",
                std::move(Selection));
        }
    }
    else
    {
        FTextureTranscodeRequest TranscodeRequest;
        TranscodeRequest.Artifact = Request.Artifact;
        TranscodeRequest.TargetFormat =
            Selection.TranscodeFormat;
        Transcoded =
            FTextureTranscoder::Transcode(TranscodeRequest);
        if (Transcoded.Result != EAssetResult::Success ||
            !Transcoded.Payload)
        {
            return Failure(
                EKTX2TextureRealizationStage::Transcode,
                MapAssetResult(Transcoded.Result),
                Identity,
                "renderer.ktx2-realization.transcode",
                "artifact could not produce a complete target payload",
                std::move(Selection));
        }
        if (!BuildBasisMips(
                *Transcoded.Payload,
                Selection.SelectedFormat,
                Mips))
        {
            return Failure(
                EKTX2TextureRealizationStage::Transcode,
                ERHIResult::Failed,
                Identity,
                "renderer.ktx2-realization.footprint",
                "transcoded mips disagree with RHI footprints",
                std::move(Selection));
        }
    }

    FRHITextureDesc TextureDesc;
    TextureDesc.Dimension = ERHITextureDimension::Texture2D;
    TextureDesc.Width =
        Request.Artifact->GetInfo().BaseExtent.Width;
    TextureDesc.Height =
        Request.Artifact->GetInfo().BaseExtent.Height;
    TextureDesc.Depth = 1;
    TextureDesc.MipLevels =
        static_cast<Stoner::Core::uint32>(Mips.size());
    TextureDesc.ArrayLayers = 1;
    TextureDesc.SampleCount = ERHISampleCount::One;
    TextureDesc.Format = Selection.SelectedFormat;
    TextureDesc.Usage =
        ERHITextureUsage::Sampled |
        ERHITextureUsage::CopyDestination;

    auto Created = Request.Device->CreateTexture(TextureDesc);
    if (!Created.Succeeded())
    {
        return Failure(
            EKTX2TextureRealizationStage::Create,
            Created.Result,
            Identity,
            "renderer.ktx2-realization.create",
            "RHI texture creation failed",
            std::move(Selection));
    }

    for (const FRealizationMip& Mip : Mips)
    {
        FRHITextureUploadDesc Upload;
        Upload.MipLevel = Mip.MipLevel;
        Upload.Width = Mip.Extent.Width;
        Upload.Height = Mip.Extent.Height;
        Upload.Depth = 1;
        Upload.RowPitchBytes = Mip.RowPitchBytes;
        Upload.Data = Mip.Bytes.data();
        Upload.DataSizeBytes = Mip.Bytes.size();
        const ERHIResult UploadResult =
            Request.Device->UploadTexture(
                Created.Object, Upload);
        if (UploadResult != ERHIResult::Success)
        {
            (void)Created.Object->Invalidate();
            return Failure(
                EKTX2TextureRealizationStage::Upload,
                UploadResult,
                Identity,
                "renderer.ktx2-realization.upload",
                "synchronous mip upload failed",
                std::move(Selection),
                Mip.MipLevel);
        }
    }

    if (Created.Object->GetLifecycleState() !=
        ERHIResourceLifecycleState::Valid)
    {
        (void)Created.Object->Invalidate();
        return Failure(
            EKTX2TextureRealizationStage::Finalize,
            ERHIResult::Failed,
            Identity,
            "renderer.ktx2-realization.finalize",
            "uploaded texture is not sample-ready",
            std::move(Selection));
    }

    FKTX2TextureRealizationResult Result;
    Result.Result = ERHIResult::Success;
    Result.Texture = std::move(Created.Object);
    Result.Selection = std::move(Selection);
    Result.Diagnostic = {
        EKTX2TextureRealizationStage::Finalize,
        ERHIResult::Success,
        Identity,
        std::nullopt,
        Stoner::Core::FString(
            "renderer.ktx2-realization.success"),
        Stoner::Core::FString(
            "all target mips are sample-ready")};
    return Result;
}

bool FKTX2TextureRealizationResult::Succeeded() const noexcept
{
    return Result == Stoner::RHI::ERHIResult::Success &&
        Texture != nullptr;
}

const char* ToString(
    EKTX2TextureRealizationStage Stage) noexcept
{
    switch (Stage)
    {
    case EKTX2TextureRealizationStage::ValidateArtifact:
        return "ValidateArtifact";
    case EKTX2TextureRealizationStage::Select:
        return "Select";
    case EKTX2TextureRealizationStage::Transcode:
        return "Transcode";
    case EKTX2TextureRealizationStage::Create:
        return "Create";
    case EKTX2TextureRealizationStage::Upload:
        return "Upload";
    case EKTX2TextureRealizationStage::Finalize:
        return "Finalize";
    }
    return "Unknown";
}

} // namespace Stoner::Renderer
