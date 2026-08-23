#include "FImageTextureCookedCodec.h"

#include "Asset/FImageAsset.h"
#include "Asset/FKTX2TextureArtifact.h"
#include "Asset/FKTX2TextureCodec.h"
#include "Asset/FTextureAsset.h"
#include "FAssetCookedBinary.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <new>

namespace Stoner::Asset::Private
{
namespace
{

void WriteMip(FCookedBinaryWriter& Writer, const FImageMip& Mip)
{
    Writer.U32(Mip.GetExtent().Width);
    Writer.U32(Mip.GetExtent().Height);
    Writer.U8(static_cast<Core::uint8>(Mip.GetFormat()));
    Writer.Bytes(Mip.GetBytes());
}

bool ReadMip(FCookedBinaryReader& Reader, FImageMip& OutMip)
{
    FImageExtent2D Extent;
    Core::uint8 Format = 0;
    Core::TArray<Core::uint8> Bytes;
    return Reader.U32(Extent.Width) && Reader.U32(Extent.Height) &&
        Reader.U8(Format) && Reader.Bytes(Bytes) &&
        FImageMip::Create(
            Extent, static_cast<EImageTexelFormat>(Format),
            std::move(Bytes), OutMip) == EAssetResult::Success;
}

void WriteImage(FCookedBinaryWriter& Writer, const FImageAsset& Image)
{
    Writer.AssetId(Image.GetId());
    Writer.SourceLocator(Image.GetSource());
    WriteMip(Writer, Image.GetBaseMip());
    Writer.U8(static_cast<Core::uint8>(Image.GetColorSpace()));
    Writer.U8(static_cast<Core::uint8>(Image.GetAlphaMode()));
    Writer.Digest(Image.GetSourceDigest());
}

bool ReadImage(FCookedBinaryReader& Reader, FImageAsset& OutImage)
{
    FAssetId Id;
    FAssetSourceLocator Source;
    FImageMip Mip;
    Core::uint8 ColorSpace = 0;
    Core::uint8 AlphaMode = 0;
    FAssetDigest SourceDigest;
    return Reader.AssetId(Id) && Reader.SourceLocator(Source) &&
        ReadMip(Reader, Mip) && Reader.U8(ColorSpace) &&
        Reader.U8(AlphaMode) && Reader.Digest(SourceDigest) &&
        FImageAsset::Create(
            std::move(Id), std::move(Source), std::move(Mip),
            static_cast<EImageColorSpace>(ColorSpace),
            static_cast<EImageAlphaMode>(AlphaMode),
            std::move(SourceDigest), OutImage) == EAssetResult::Success;
}

void WriteSettings(
    FCookedBinaryWriter& Writer,
    const FImageImportSettings& Settings)
{
    Writer.U8(static_cast<Core::uint8>(Settings.Semantic));
    Writer.Bool(Settings.ColorSpace.has_value());
    if (Settings.ColorSpace)
        Writer.U8(static_cast<Core::uint8>(*Settings.ColorSpace));
    Writer.U8(static_cast<Core::uint8>(Settings.MipPolicy));
    Writer.U8(static_cast<Core::uint8>(Settings.HDRLayout));
    Writer.U32(Settings.Limits.MaxDimension);
    Writer.U64(Settings.Limits.MaxSourceBytes);
    Writer.U64(Settings.Limits.MaxMipBytes);
    Writer.U64(Settings.Limits.MaxDecodedChainBytes);
}

bool ReadSettings(
    FCookedBinaryReader& Reader,
    FImageImportSettings& OutSettings)
{
    Core::uint8 Semantic = 0;
    Core::uint8 ColorSpace = 0;
    Core::uint8 MipPolicy = 0;
    Core::uint8 HDRLayout = 0;
    bool HasColorSpace = false;
    if (!Reader.U8(Semantic) || !Reader.Bool(HasColorSpace) ||
        (HasColorSpace && !Reader.U8(ColorSpace)) ||
        !Reader.U8(MipPolicy) || !Reader.U8(HDRLayout) ||
        !Reader.U32(OutSettings.Limits.MaxDimension) ||
        !Reader.U64(OutSettings.Limits.MaxSourceBytes) ||
        !Reader.U64(OutSettings.Limits.MaxMipBytes) ||
        !Reader.U64(OutSettings.Limits.MaxDecodedChainBytes))
        return false;
    OutSettings.Semantic = static_cast<ETextureSemantic>(Semantic);
    OutSettings.ColorSpace = HasColorSpace
        ? std::optional<EImageColorSpace>(
              static_cast<EImageColorSpace>(ColorSpace))
        : std::nullopt;
    OutSettings.MipPolicy = static_cast<EImageMipPolicy>(MipPolicy);
    OutSettings.HDRLayout = static_cast<EHDRLayout>(HDRLayout);
    return OutSettings.Validate() == EAssetResult::Success;
}

void WriteKTXInfo(
    FCookedBinaryWriter& Writer,
    const FKTX2TextureInfo& Info)
{
    Writer.AssetId(Info.TextureId);
    Writer.Digest(Info.SourceDigest);
    Writer.Digest(Info.ContentDigest);
    Writer.Digest(Info.CookRevision);
    Writer.Digest(Info.ArtifactDigest);
    Writer.Text(Info.ProducerVersion);
    Writer.Text(Info.PortableProfile);
    Writer.U8(static_cast<Core::uint8>(Info.CompressionPolicy));
    Writer.U8(static_cast<Core::uint8>(Info.BasisModel));
    Writer.U8(static_cast<Core::uint8>(Info.Supercompression));
    Writer.U8(static_cast<Core::uint8>(Info.Semantic));
    Writer.U8(static_cast<Core::uint8>(Info.ColorSpace));
    Writer.U8(static_cast<Core::uint8>(Info.AlphaMode));
    Writer.U8(static_cast<Core::uint8>(Info.Origin));
    Writer.U8(static_cast<Core::uint8>(Info.MipPolicy));
    Writer.U32(Info.SourceChannelCount);
    Writer.U32(Info.BaseExtent.Width);
    Writer.U32(Info.BaseExtent.Height);
    Writer.U32(static_cast<Core::uint32>(Info.Levels.size()));
    for (const auto& Level : Info.Levels)
    {
        Writer.U32(Level.MipLevel);
        Writer.U32(Level.Extent.Width);
        Writer.U32(Level.Extent.Height);
        Writer.U64(Level.ByteOffset);
        Writer.U64(Level.ByteLength);
        Writer.U64(Level.UncompressedByteLength);
    }
    Writer.Bool(Info.StoredTexelFormat.has_value());
    if (Info.StoredTexelFormat)
        Writer.U8(static_cast<Core::uint8>(*Info.StoredTexelFormat));
    Writer.Text(Info.Writer);
}

bool ReadKTXInfo(FCookedBinaryReader& Reader, FKTX2TextureInfo& Out)
{
    Core::uint8 Compression = 0;
    Core::uint8 Basis = 0;
    Core::uint8 Super = 0;
    Core::uint8 Semantic = 0;
    Core::uint8 Color = 0;
    Core::uint8 Alpha = 0;
    Core::uint8 Origin = 0;
    Core::uint8 Mip = 0;
    Core::uint32 LevelCount = 0;
    if (!Reader.AssetId(Out.TextureId) || !Reader.Digest(Out.SourceDigest) ||
        !Reader.Digest(Out.ContentDigest) || !Reader.Digest(Out.CookRevision) ||
        !Reader.Digest(Out.ArtifactDigest) || !Reader.Text(Out.ProducerVersion) ||
        !Reader.Text(Out.PortableProfile) || !Reader.U8(Compression) ||
        !Reader.U8(Basis) || !Reader.U8(Super) || !Reader.U8(Semantic) ||
        !Reader.U8(Color) || !Reader.U8(Alpha) || !Reader.U8(Origin) ||
        !Reader.U8(Mip) || !Reader.U32(Out.SourceChannelCount) ||
        !Reader.U32(Out.BaseExtent.Width) ||
        !Reader.U32(Out.BaseExtent.Height) || !Reader.Count(LevelCount))
        return false;
    Out.CompressionPolicy = static_cast<ETextureCompressionPolicy>(Compression);
    Out.BasisModel = static_cast<EKTX2BasisModel>(Basis);
    Out.Supercompression = static_cast<EKTX2Supercompression>(Super);
    Out.Semantic = static_cast<ETextureSemantic>(Semantic);
    Out.ColorSpace = static_cast<EImageColorSpace>(Color);
    Out.AlphaMode = static_cast<EImageAlphaMode>(Alpha);
    Out.Origin = static_cast<EImageOrigin>(Origin);
    Out.MipPolicy = static_cast<EImageMipPolicy>(Mip);
    Out.Levels.reserve(LevelCount);
    for (Core::uint32 Index = 0; Index < LevelCount; ++Index)
    {
        FKTX2Level Level;
        if (!Reader.U32(Level.MipLevel) || !Reader.U32(Level.Extent.Width) ||
            !Reader.U32(Level.Extent.Height) || !Reader.U64(Level.ByteOffset) ||
            !Reader.U64(Level.ByteLength) ||
            !Reader.U64(Level.UncompressedByteLength))
            return false;
        Out.Levels.push_back(Level);
    }
    bool HasFormat = false;
    Core::uint8 Format = 0;
    if (!Reader.Bool(HasFormat) || (HasFormat && !Reader.U8(Format)) ||
        !Reader.Text(Out.Writer))
        return false;
    if (HasFormat) Out.StoredTexelFormat = static_cast<EImageTexelFormat>(Format);
    return true;
}

FAssetCookedPayloadHeader Header(
    const FAssetId& Id,
    const char* Codec)
{
    FAssetCookedPayloadHeader Value;
    Value.AssetId = Id;
    Value.AssetType = Id.GetAssetType();
    Value.CodecId = Core::FString(Codec);
    Value.CodecVersion = 1;
    Value.PayloadSchemaVersion = 1;
    return Value;
}

} // namespace

EAssetResult EncodeImageTextureCookedBody(
    const FAssetPayload& Payload,
    const FAssetCookedPayloadLimits& Limits,
    FAssetCookedPayloadHeader& OutHeader,
    Core::TArray<Core::uint8>& OutBody)
{
    OutHeader = {};
    OutBody.clear();
    if (Limits.Validate() != EAssetResult::Success)
        return EAssetResult::InvalidInput;
    try
    {
        FCookedBinaryWriter Writer(Limits.MaxBodyBytes);
        if (const auto* Image = dynamic_cast<const FImageAsset*>(&Payload))
        {
            WriteImage(Writer, *Image);
            OutHeader = Header(Image->GetId(), "stoner.image");
        }
        else if (const auto* Texture = dynamic_cast<const FTextureAsset*>(&Payload))
        {
            Writer.AssetId(Texture->GetId());
            if (!Texture->GetImage()) return EAssetResult::UnresolvedDependency;
            WriteImage(Writer, *Texture->GetImage());
            WriteSettings(Writer, Texture->GetSettings());
            if (Texture->GetMips().size() > std::numeric_limits<Core::uint32>::max())
                return EAssetResult::CapacityExceeded;
            Writer.U32(static_cast<Core::uint32>(Texture->GetMips().size()));
            for (const auto& Mip : Texture->GetMips()) WriteMip(Writer, Mip);
            OutHeader = Header(Texture->GetId(), "stoner.texture");
        }
        else if (const auto* KTX =
                     dynamic_cast<const FKTX2TextureArtifact*>(&Payload))
        {
            WriteKTXInfo(Writer, KTX->GetInfo());
            Writer.Bytes(KTX->GetBytes());
            OutHeader = Header(KTX->GetId(), "stoner.ktx2");
        }
        else return EAssetResult::TypeMismatch;
        OutBody = Writer.Take();
        if (OutBody.empty())
        {
            OutHeader = {};
            return EAssetResult::CapacityExceeded;
        }
        return EAssetResult::Success;
    }
    catch (const std::bad_alloc&)
    {
        OutHeader = {};
        OutBody.clear();
        return EAssetResult::CapacityExceeded;
    }
}

EAssetResult DecodeImageTextureCookedBody(
    const FAssetCookedPayloadHeader& HeaderValue,
    std::span<const Core::uint8> Body,
    Core::TSharedPtr<const FAssetPayload>& OutPayload)
{
    OutPayload.reset();
    try
    {
        FCookedBinaryReader Reader(Body);
        if (HeaderValue.CodecId == Core::FString("stoner.image"))
        {
            FImageAsset Image;
            if (!ReadImage(Reader, Image) || !Reader.AtEnd() ||
                Image.GetId() != HeaderValue.AssetId)
                return EAssetResult::CorruptPayload;
            OutPayload = Core::MakeShared<FImageAsset>(std::move(Image));
        }
        else if (HeaderValue.CodecId == Core::FString("stoner.texture"))
        {
            FAssetId Id;
            FImageAsset Image;
            FImageImportSettings Settings;
            Core::uint32 MipCount = 0;
            if (!Reader.AssetId(Id) || !ReadImage(Reader, Image) ||
                !ReadSettings(Reader, Settings) || !Reader.Count(MipCount) ||
                Id != HeaderValue.AssetId)
                return EAssetResult::CorruptPayload;
            Core::TArray<FImageMip> Mips;
            Mips.reserve(MipCount);
            for (Core::uint32 Index = 0; Index < MipCount; ++Index)
            {
                FImageMip Mip;
                if (!ReadMip(Reader, Mip)) return EAssetResult::CorruptPayload;
                Mips.push_back(std::move(Mip));
            }
            if (!Reader.AtEnd() || Mips.empty() ||
                Mips.front().GetExtent() != Image.GetBaseMip().GetExtent() ||
                Mips.front().GetFormat() != Image.GetBaseMip().GetFormat() ||
                !std::equal(
                    Mips.front().GetBytes().begin(),
                    Mips.front().GetBytes().end(),
                    Image.GetBaseMip().GetBytes().begin(),
                    Image.GetBaseMip().GetBytes().end()))
                return EAssetResult::CorruptPayload;
            Mips.front() = Image.GetBaseMip();
            auto ImagePtr = Core::MakeShared<FImageAsset>(std::move(Image));
            FTextureAsset Texture;
            if (FTextureAsset::Create(
                    std::move(Id), ImagePtr, std::move(Settings),
                    std::move(Mips), Texture) != EAssetResult::Success)
                return EAssetResult::CorruptPayload;
            OutPayload = Core::MakeShared<FTextureAsset>(std::move(Texture));
        }
        else if (HeaderValue.CodecId == Core::FString("stoner.ktx2"))
        {
            FKTX2TextureInfo Info;
            Core::TArray<Core::uint8> Bytes;
            if (!ReadKTXInfo(Reader, Info) || !Reader.Bytes(Bytes) ||
                !Reader.AtEnd() || Info.TextureId != HeaderValue.AssetId)
                return EAssetResult::CorruptPayload;
            FKTX2TextureArtifact Artifact;
            if (FKTX2TextureCodec::Open(
                    HeaderValue.AssetId, Bytes, {}, Artifact) !=
                    EAssetResult::Success || Artifact.GetInfo() != Info)
                return EAssetResult::CorruptPayload;
            OutPayload = Core::MakeShared<FKTX2TextureArtifact>(
                std::move(Artifact));
        }
        else return EAssetResult::Unsupported;
        return EAssetResult::Success;
    }
    catch (const std::bad_alloc&)
    {
        OutPayload.reset();
        return EAssetResult::CapacityExceeded;
    }
}

} // namespace Stoner::Asset::Private
