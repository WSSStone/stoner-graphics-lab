#include "FBasisTextureTranscoder.h"

#include "FKTX2ContainerCodec.h"
#include "ktx.h"

#include <algorithm>
#include <limits>
#include <string>

namespace Stoner::Asset::Private
{
namespace
{

struct FTranscodePlan
{
    ktx_transcode_fmt_e NativeFormat = KTX_TTF_NOSELECTION;
    Core::uint32 BlockWidth = 0;
    Core::uint32 BlockHeight = 0;
    Core::uint32 BytesPerBlock = 0;
    Core::uint32 TargetChannels = 0;
    bool bSRGB = false;
    bool bPreservesAlpha = false;
    bool bExtractFromRGBA = false;
};

void AddDiagnostic(
    FAssetDiagnosticList& Diagnostics,
    EAssetResult Result,
    const char* Code,
    const char* Field,
    const char* Reason,
    std::optional<Core::uint32> Level = std::nullopt,
    const char* Actual = nullptr,
    const char* Limit = nullptr)
{
    FAssetDiagnostic Diagnostic;
    Diagnostic.Stage = EAssetStage::Transcode;
    Diagnostic.Result = Result;
    Diagnostic.Severity = EAssetDiagnosticSeverity::Error;
    Diagnostic.Code = Core::FString(Code);
    Diagnostic.Participant =
        Core::FString("transcoder.basis-libktx");
    Diagnostic.Field = Core::FString(Field);
    Diagnostic.Reason = Core::FString(Reason);
    Diagnostic.Level = Level;
    if (Actual != nullptr)
    {
        Diagnostic.Actual = Core::FString(Actual);
    }
    if (Limit != nullptr)
    {
        Diagnostic.Limit = Core::FString(Limit);
    }
    Diagnostics.push_back(std::move(Diagnostic));
}

bool CheckedMultiply(
    Core::uint64 Left,
    Core::uint64 Right,
    Core::uint64& Out)
{
    if (Left != 0 &&
        Right > std::numeric_limits<Core::uint64>::max() / Left)
    {
        return false;
    }
    Out = Left * Right;
    return true;
}

bool CheckedAdd(
    Core::uint64 Left,
    Core::uint64 Right,
    Core::uint64& Out)
{
    if (Right > std::numeric_limits<Core::uint64>::max() - Left)
    {
        return false;
    }
    Out = Left + Right;
    return true;
}

FTranscodePlan GetPlan(ETextureTranscodeFormat Format)
{
    switch (Format)
    {
    case ETextureTranscodeFormat::BC1_RGBA_UNorm:
        return {KTX_TTF_BC1_RGB, 4, 4, 8, 3, false, false, false};
    case ETextureTranscodeFormat::BC1_RGBA_SRGB:
        return {KTX_TTF_BC1_RGB, 4, 4, 8, 3, true, false, false};
    case ETextureTranscodeFormat::BC3_RGBA_UNorm:
        return {KTX_TTF_BC3_RGBA, 4, 4, 16, 4, false, true, false};
    case ETextureTranscodeFormat::BC3_RGBA_SRGB:
        return {KTX_TTF_BC3_RGBA, 4, 4, 16, 4, true, true, false};
    case ETextureTranscodeFormat::BC4_R_UNorm:
        return {KTX_TTF_BC4_R, 4, 4, 8, 1, false, false, false};
    case ETextureTranscodeFormat::BC5_RG_UNorm:
        return {KTX_TTF_BC5_RG, 4, 4, 16, 2, false, false, false};
    case ETextureTranscodeFormat::BC7_RGBA_UNorm:
        return {KTX_TTF_BC7_RGBA, 4, 4, 16, 4, false, true, false};
    case ETextureTranscodeFormat::BC7_RGBA_SRGB:
        return {KTX_TTF_BC7_RGBA, 4, 4, 16, 4, true, true, false};
    case ETextureTranscodeFormat::ETC2_RGB8_UNorm:
        return {KTX_TTF_ETC1_RGB, 4, 4, 8, 3, false, false, false};
    case ETextureTranscodeFormat::ETC2_RGB8_SRGB:
        return {KTX_TTF_ETC1_RGB, 4, 4, 8, 3, true, false, false};
    case ETextureTranscodeFormat::ETC2_RGBA8_UNorm:
        return {KTX_TTF_ETC2_RGBA, 4, 4, 16, 4, false, true, false};
    case ETextureTranscodeFormat::ETC2_RGBA8_SRGB:
        return {KTX_TTF_ETC2_RGBA, 4, 4, 16, 4, true, true, false};
    case ETextureTranscodeFormat::EAC_R11_UNorm:
        return {KTX_TTF_ETC2_EAC_R11, 4, 4, 8, 1, false, false, false};
    case ETextureTranscodeFormat::EAC_RG11_UNorm:
        return {KTX_TTF_ETC2_EAC_RG11, 4, 4, 16, 2, false, false, false};
    case ETextureTranscodeFormat::ASTC_4x4_RGBA_UNorm:
        return {KTX_TTF_ASTC_4x4_RGBA, 4, 4, 16, 4, false, true, false};
    case ETextureTranscodeFormat::ASTC_4x4_RGBA_SRGB:
        return {KTX_TTF_ASTC_4x4_RGBA, 4, 4, 16, 4, true, true, false};
    case ETextureTranscodeFormat::R8_UNorm:
        return {KTX_TTF_RGBA32, 1, 1, 1, 1, false, false, true};
    case ETextureTranscodeFormat::R8G8_UNorm:
        return {KTX_TTF_RGBA32, 1, 1, 2, 2, false, false, true};
    case ETextureTranscodeFormat::R8G8B8A8_UNorm:
        return {KTX_TTF_RGBA32, 1, 1, 4, 4, false, true, false};
    case ETextureTranscodeFormat::R8G8B8A8_SRGB:
        return {KTX_TTF_RGBA32, 1, 1, 4, 4, true, true, false};
    case ETextureTranscodeFormat::Unknown:
        return {};
    }
    return {};
}

bool IsCompatible(
    const FKTX2TextureInfo& Info,
    const FTranscodePlan& Plan)
{
    if (Plan.NativeFormat == KTX_TTF_NOSELECTION ||
        Info.CompressionPolicy ==
            ETextureCompressionPolicy::Uncompressed ||
        Info.SourceChannelCount == 0 ||
        Info.SourceChannelCount > 4 ||
        Plan.bSRGB !=
            (Info.ColorSpace == EImageColorSpace::SRGB))
    {
        return false;
    }
    if (Info.Semantic != ETextureSemantic::Color && Plan.bSRGB)
    {
        return false;
    }
    if (Info.AlphaMode == EImageAlphaMode::Straight &&
        !Plan.bPreservesAlpha)
    {
        return false;
    }
    if (Info.Semantic == ETextureSemantic::Color)
    {
        return Plan.TargetChannels >= 3;
    }
    if (Info.Semantic == ETextureSemantic::Normal)
    {
        return Plan.TargetChannels >= 2;
    }
    if (Info.Semantic == ETextureSemantic::Data)
    {
        return Plan.TargetChannels >= Info.SourceChannelCount;
    }
    return false;
}

bool CalculateFootprint(
    FImageExtent2D Extent,
    const FTranscodePlan& Plan,
    Core::uint64& OutRowPitch,
    Core::uint64& OutByteCount)
{
    const Core::uint64 BlocksX =
        (static_cast<Core::uint64>(Extent.Width) +
         Plan.BlockWidth - 1) /
        Plan.BlockWidth;
    const Core::uint64 BlocksY =
        (static_cast<Core::uint64>(Extent.Height) +
         Plan.BlockHeight - 1) /
        Plan.BlockHeight;
    return CheckedMultiply(
               BlocksX, Plan.BytesPerBlock, OutRowPitch) &&
        CheckedMultiply(OutRowPitch, BlocksY, OutByteCount);
}

EAssetResult MapNativeFailure(KTX_error_code Result)
{
    switch (Result)
    {
    case KTX_FILE_DATA_ERROR:
    case KTX_FILE_UNEXPECTED_EOF:
        return EAssetResult::CorruptPayload;
    case KTX_INVALID_OPERATION:
    case KTX_INVALID_VALUE:
    case KTX_UNSUPPORTED_FEATURE:
        return EAssetResult::UnsupportedCompression;
    default:
        return EAssetResult::TranscodeFailure;
    }
}

} // namespace

EAssetResult TranscodeBasisTexture(
    const FKTX2TextureArtifact& Artifact,
    ETextureTranscodeFormat TargetFormat,
    const FTextureCookLimits& Limits,
    FTranscodedTexturePayload& OutPayload,
    FAssetDiagnosticList& OutDiagnostics)
{
    OutPayload = {};
    const FKTX2TextureInfo& Info = Artifact.GetInfo();
    const FTranscodePlan Plan = GetPlan(TargetFormat);
    if (!IsCompatible(Info, Plan))
    {
        AddDiagnostic(
            OutDiagnostics,
            EAssetResult::UnsupportedCompression,
            "asset.ktx2.transcode-target",
            "targetFormat",
            "target does not preserve artifact transfer, alpha, or semantic channels");
        return EAssetResult::UnsupportedCompression;
    }

    Core::uint64 AggregateBytes = 0;
    for (const FKTX2Level& Level : Info.Levels)
    {
        Core::uint64 RowPitch = 0;
        Core::uint64 ByteCount = 0;
        if (!CalculateFootprint(
                Level.Extent, Plan, RowPitch, ByteCount) ||
            ByteCount > Limits.MaxLevelBytes ||
            !CheckedAdd(
                AggregateBytes, ByteCount, AggregateBytes) ||
            AggregateBytes > Limits.MaxTargetPayloadBytes ||
            ByteCount > std::numeric_limits<Core::usize>::max())
        {
            AddDiagnostic(
                OutDiagnostics,
                EAssetResult::KTX2LimitExceeded,
                "asset.ktx2.transcode-limit",
                "targetPayloadBytes",
                "target footprint exceeds configured limits",
                Level.MipLevel,
                std::to_string(AggregateBytes).c_str(),
                std::to_string(
                    Limits.MaxTargetPayloadBytes).c_str());
            return EAssetResult::KTX2LimitExceeded;
        }
    }

    FKTX2TextureHandle Handle;
    EAssetResult Result = FKTX2ContainerCodec::Open(
        Artifact.GetBytes(), Handle, &OutDiagnostics);
    if (Result != EAssetResult::Success)
    {
        std::optional<Core::uint32> FailedLevel;
        const Core::uint64 ArtifactBytes =
            Artifact.GetBytes().size();
        for (const FKTX2Level& Level : Info.Levels)
        {
            if (Level.ByteOffset > ArtifactBytes ||
                Level.ByteLength >
                    ArtifactBytes - std::min(
                        Level.ByteOffset, ArtifactBytes))
            {
                FailedLevel = Level.MipLevel;
                break;
            }
        }
        AddDiagnostic(
            OutDiagnostics,
            EAssetResult::CorruptPayload,
            "asset.ktx2.transcode-container",
            "payload",
            "immutable artifact payload cannot be reopened for transcoding",
            FailedLevel);
        return EAssetResult::CorruptPayload;
    }
    const KTX_error_code NativeResult =
        ktxTexture2_TranscodeBasis(
            Handle.Get(), Plan.NativeFormat, 0);
    if (NativeResult != KTX_SUCCESS)
    {
        Result = MapNativeFailure(NativeResult);
        const std::string NativeCode =
            std::to_string(static_cast<int>(NativeResult));
        AddDiagnostic(
            OutDiagnostics,
            Result,
            "asset.ktx2.transcode-native",
            "payload",
            "Basis payload could not produce the selected target",
            std::nullopt,
            NativeCode.c_str());
        return Result;
    }

    FTranscodedTexturePayload Payload;
    Payload.Format = TargetFormat;
    Payload.Semantic = Info.Semantic;
    Payload.ColorSpace = Info.ColorSpace;
    Payload.AlphaMode = Info.AlphaMode;
    Payload.Origin = Info.Origin;
    Payload.Mips.reserve(Info.Levels.size());
    for (const FKTX2Level& Level : Info.Levels)
    {
        Core::uint64 RowPitch = 0;
        Core::uint64 ByteCount = 0;
        (void)CalculateFootprint(
            Level.Extent, Plan, RowPitch, ByteCount);
        ktx_size_t Offset = 0;
        const KTX_error_code OffsetResult =
            ktxTexture_GetImageOffset(
                ktxTexture(Handle.Get()),
                Level.MipLevel,
                0,
                0,
                &Offset);
        const ktx_size_t NativeBytes =
            OffsetResult == KTX_SUCCESS
            ? ktxTexture_GetImageSize(
                  ktxTexture(Handle.Get()), Level.MipLevel)
            : 0;
        const Core::uint64 RequiredNativeBytes =
            Plan.bExtractFromRGBA
            ? static_cast<Core::uint64>(Level.Extent.Width) *
                Level.Extent.Height * 4ULL
            : ByteCount;
        if (OffsetResult != KTX_SUCCESS ||
            NativeBytes != RequiredNativeBytes ||
            Handle.Get()->pData == nullptr ||
            Offset > Handle.Get()->dataSize ||
            NativeBytes > Handle.Get()->dataSize - Offset)
        {
            AddDiagnostic(
                OutDiagnostics,
                EAssetResult::TranscodeFailure,
                "asset.ktx2.transcode-footprint",
                "levelBytes",
                "native target bytes disagree with the checked footprint",
                Level.MipLevel);
            return EAssetResult::TranscodeFailure;
        }

        FTranscodedTextureMip Mip;
        Mip.MipLevel = Level.MipLevel;
        Mip.Extent = Level.Extent;
        Mip.BlockWidth = Plan.BlockWidth;
        Mip.BlockHeight = Plan.BlockHeight;
        Mip.BytesPerBlock = Plan.BytesPerBlock;
        Mip.RowPitchBytes = RowPitch;
        Mip.Bytes.resize(static_cast<Core::usize>(ByteCount));
        const Core::uint8* Source =
            Handle.Get()->pData + Offset;
        if (Plan.bExtractFromRGBA)
        {
            const Core::uint64 PixelCount =
                static_cast<Core::uint64>(Level.Extent.Width) *
                Level.Extent.Height;
            for (Core::uint64 Pixel = 0;
                 Pixel < PixelCount;
                 ++Pixel)
            {
                const Core::usize SourceOffset =
                    static_cast<Core::usize>(Pixel * 4ULL);
                const Core::usize TargetOffset =
                    static_cast<Core::usize>(
                        Pixel * Plan.TargetChannels);
                for (Core::uint32 Channel = 0;
                     Channel < Plan.TargetChannels;
                     ++Channel)
                {
                    Mip.Bytes[TargetOffset + Channel] =
                        Source[SourceOffset + Channel];
                }
            }
        }
        else
        {
            Mip.Bytes.assign(
                Source, Source + NativeBytes);
        }
        Payload.Mips.push_back(std::move(Mip));
    }
    OutPayload = std::move(Payload);
    return EAssetResult::Success;
}

} // namespace Stoner::Asset::Private
