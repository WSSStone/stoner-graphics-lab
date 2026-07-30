#include "AssetKTX2Tests.h"

#include "Asset/AssetMinimal.h"
#include "FImageMipGenerator.h"
#include "FKTX2Preflight.h"
#include "FTextureCookPolicy.h"
#include "IKTX2Encoder.h"
#include "ktx.h"

#include <array>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>

namespace
{

using namespace Stoner::Asset;
using namespace Stoner::Asset::Private;
using namespace Stoner::Core;

void Record(
    FAssetKTX2TestResult& Result,
    bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

class FMemoryAssetSource final : public IAssetSource
{
public:
    explicit FMemoryAssetSource(
        TArray<uint8> InBytes,
        EAssetResult InResult = EAssetResult::Success)
        : Bytes_(std::move(InBytes)),
          Result_(InResult)
    {
    }

    EAssetResult Read(
        uint64 Offset,
        usize MaximumBytes,
        TArray<uint8>& OutBytes) const override
    {
        OutBytes.clear();
        if (Result_ != EAssetResult::Success)
        {
            return Result_;
        }
        if (Offset > Bytes_.size())
        {
            return EAssetResult::TruncatedSource;
        }
        const usize Begin = static_cast<usize>(Offset);
        const usize Count =
            std::min(MaximumBytes, Bytes_.size() - Begin);
        OutBytes.assign(
            Bytes_.begin() + static_cast<std::ptrdiff_t>(Begin),
            Bytes_.begin() +
                static_cast<std::ptrdiff_t>(Begin + Count));
        return EAssetResult::Success;
    }

private:
    TArray<uint8> Bytes_;
    EAssetResult Result_ = EAssetResult::Success;
};

void PutU32(TArray<uint8>& Bytes, usize Offset, uint32 Value)
{
    if (Offset + sizeof(Value) <= Bytes.size())
    {
        Bytes[Offset] = static_cast<uint8>(Value);
        Bytes[Offset + 1] =
            static_cast<uint8>(Value >> 8U);
        Bytes[Offset + 2] =
            static_cast<uint8>(Value >> 16U);
        Bytes[Offset + 3] =
            static_cast<uint8>(Value >> 24U);
    }
}

void PutU64(TArray<uint8>& Bytes, usize Offset, uint64 Value)
{
    PutU32(Bytes, Offset, static_cast<uint32>(Value));
    PutU32(
        Bytes,
        Offset + sizeof(uint32),
        static_cast<uint32>(Value >> 32U));
}

TArray<uint8> MakePixels()
{
    TArray<uint8> Pixels(4 * 4 * 4);
    for (uint32 Pixel = 0; Pixel < 16; ++Pixel)
    {
        Pixels[Pixel * 4] = static_cast<uint8>(Pixel * 13);
        Pixels[Pixel * 4 + 1] =
            static_cast<uint8>(255 - Pixel * 11);
        Pixels[Pixel * 4 + 2] = static_cast<uint8>(Pixel * 7);
        Pixels[Pixel * 4 + 3] = 255;
    }
    return Pixels;
}

FAssetId MakeId(
    const char* Type,
    const std::string& Name,
    const char* Subresource)
{
    FAssetId Id;
    (void)FAssetId::Create(
        FString(Type),
        FString("Tests/KTX2/" + Name),
        std::optional<FString>(FString(Subresource)),
        Id);
    return Id;
}

FAssetSourceLocator MakeSource(const std::string& Name)
{
    FAssetSourceLocator Source;
    (void)FAssetSourceLocator::Create(
        FString("fixture"),
        FString("KTX2/" + Name),
        Source);
    return Source;
}

FAssetParticipantId MakeParticipant(const char* Value)
{
    FAssetParticipantId Participant;
    (void)FAssetParticipantId::Create(FString(Value), Participant);
    return Participant;
}

FAssetProducerVersion MakeProducerVersion(const char* Value)
{
    FAssetProducerVersion Version;
    (void)FAssetProducerVersion::Create(FString(Value), Version);
    return Version;
}

TSharedPtr<const FTextureAsset> MakeTexture(
    const std::string& Name,
    FImageExtent2D Extent,
    EImageTexelFormat Format,
    TArray<uint8> Bytes,
    ETextureSemantic Semantic,
    EImageColorSpace ColorSpace,
    EImageAlphaMode AlphaMode = EImageAlphaMode::None,
    EImageMipPolicy MipPolicy = EImageMipPolicy::BaseOnly)
{
    FImageMip BaseMip;
    if (FImageMip::Create(
            Extent, Format, std::move(Bytes), BaseMip) !=
        EAssetResult::Success)
    {
        return {};
    }
    FImageAsset Image;
    if (FImageAsset::Create(
            MakeId("Image", Name, "image"),
            MakeSource(Name),
            BaseMip,
            ColorSpace,
            AlphaMode,
            FAssetDigest::FromBytes(BaseMip.GetBytes()),
            Image) != EAssetResult::Success)
    {
        return {};
    }
    const auto SharedImage = MakeShared<FImageAsset>(std::move(Image));
    FImageImportSettings Settings;
    Settings.Semantic = Semantic;
    Settings.ColorSpace = ColorSpace;
    Settings.MipPolicy = MipPolicy;
    TArray<FImageMip> Mips;
    if (GenerateImageMips(BaseMip, Settings, Mips) !=
        EAssetResult::Success)
    {
        return {};
    }
    FTextureAsset Texture;
    if (FTextureAsset::Create(
            MakeId("Texture", Name, "texture"),
            SharedImage,
            Settings,
            std::move(Mips),
            Texture) != EAssetResult::Success)
    {
        return {};
    }
    return MakeShared<FTextureAsset>(std::move(Texture));
}

TArray<uint8> MakePattern(
    FImageExtent2D Extent,
    EImageTexelFormat Format,
    ETextureSemantic Semantic,
    EImageAlphaMode Alpha)
{
    const uint32 BytesPerTexel = GetImageBytesPerTexel(Format);
    const uint32 Channels = GetImageChannelCount(Format);
    TArray<uint8> Bytes(
        static_cast<usize>(Extent.Width) * Extent.Height *
            BytesPerTexel,
        0);
    if (IsImageFloatFormat(Format))
    {
        return Bytes;
    }
    for (uint32 Y = 0; Y < Extent.Height; ++Y)
    {
        for (uint32 X = 0; X < Extent.Width; ++X)
        {
            const usize Offset =
                (static_cast<usize>(Y) * Extent.Width + X) *
                Channels;
            for (uint32 Channel = 0; Channel < Channels; ++Channel)
            {
                uint8 Value = static_cast<uint8>(
                    (X * 17U + Y * 29U + Channel * 61U) & 255U);
                if (Semantic == ETextureSemantic::Color)
                {
                    const uint32 TileX = X / 16U;
                    const uint32 TileY = Y / 16U;
                    if (Channel == 0)
                    {
                        Value = static_cast<uint8>(
                            32U + (TileX % 4U) * 48U);
                    }
                    else if (Channel == 1)
                    {
                        Value = static_cast<uint8>(
                            48U + (TileY % 4U) * 40U);
                    }
                    else
                    {
                        Value = static_cast<uint8>(
                            80U + ((TileX + TileY) % 4U) * 32U);
                    }
                }
                else if (Semantic == ETextureSemantic::Normal)
                {
                    const int TileX =
                        static_cast<int>((X / 16U) % 4U) - 1;
                    const int TileY =
                        static_cast<int>((Y / 16U) % 4U) - 1;
                    const double NormalX =
                        static_cast<double>(TileX) * 0.12;
                    const double NormalY =
                        static_cast<double>(TileY) * 0.12;
                    const double NormalZ = std::sqrt(std::max(
                        0.0,
                        1.0 - NormalX * NormalX -
                            NormalY * NormalY));
                    const std::array<double, 3> Normal = {
                        NormalX, NormalY, NormalZ};
                    Value = static_cast<uint8>(std::clamp(
                        std::lround(
                            (Normal[Channel] * 0.5 + 0.5) *
                            255.0),
                        0L,
                        255L));
                }
                if (Alpha == EImageAlphaMode::Straight &&
                    Channel + 1 == Channels)
                {
                    Value = static_cast<uint8>(
                        32U +
                        (((X / 4U) + (Y / 4U)) % 8U) * 28U);
                }
                Bytes[Offset + Channel] = Value;
            }
        }
    }
    return Bytes;
}

FAssetMetadata MakeMetadata(const FTextureAsset& Texture)
{
    FAssetMetadata Metadata;
    Metadata.Id = Texture.GetId();
    Metadata.Source = Texture.GetImage()->GetSource();
    Metadata.Producer = MakeParticipant("importer.image");
    Metadata.ProducerVersion = MakeProducerVersion("021-v1");
    Metadata.Version.SourceDigest =
        Texture.GetImage()->GetSourceDigest();
    Metadata.Version.ContentDigest = Texture.GetContentDigest();
    return Metadata;
}

FAssetCookResult CookTexture(
    const TSharedPtr<const FTextureAsset>& Texture,
    FTextureCookSettings Settings = {},
    FString TargetProfile = {})
{
    FAssetCookRequest Request;
    if (Texture)
    {
        Request.Metadata = MakeMetadata(*Texture);
        Request.Payload = Texture;
        auto Parameters = MakeShared<FTextureCookParameters>();
        Parameters->TextureId = Texture->GetId();
        Parameters->Settings = std::move(Settings);
        Request.Parameters = std::move(Parameters);
    }
    Request.TargetProfile = std::move(TargetProfile);

    FAssetExtensionRegistry Registry;
    FAssetRegistrationToken Token;
    FAssetParticipantId Participant =
        MakeParticipant("cooker.ktx2");
    if (RegisterKTX2TextureCooker(Registry, Token) !=
        EAssetResult::Success)
    {
        return {};
    }
    return FAssetDispatch::Cook(Registry, Participant, Request);
}

struct FKTX2CorpusCase
{
    std::string Name;
    TSharedPtr<const FTextureAsset> Texture;
    FTextureCookSettings Settings;
};

TArray<FKTX2CorpusCase> BuildCorpus()
{
    TArray<FKTX2CorpusCase> Corpus;
    const auto Add = [&Corpus](
        const char* Name,
        FImageExtent2D Extent,
        EImageTexelFormat Format,
        ETextureSemantic Semantic,
        EImageColorSpace ColorSpace,
        EImageAlphaMode Alpha,
        EImageMipPolicy Mips,
        ETextureCompressionPolicy Policy,
        ETextureCookQuality Quality,
        bool AllowLossyData = false)
    {
        FTextureCookSettings Settings;
        Settings.CompressionPolicy = Policy;
        Settings.Quality = Quality;
        Settings.bAllowLossyData = AllowLossyData;
        Corpus.push_back({
            Name,
            MakeTexture(
                Name,
                Extent,
                Format,
                MakePattern(Extent, Format, Semantic, Alpha),
                Semantic,
                ColorSpace,
                Alpha,
                Mips),
            Settings});
    };

    Add("etc1s-color-balanced-full", {64, 64},
        EImageTexelFormat::R8G8B8A8_UNorm, ETextureSemantic::Color,
        EImageColorSpace::SRGB, EImageAlphaMode::None,
        EImageMipPolicy::FullChain, ETextureCompressionPolicy::ETC1S,
        ETextureCookQuality::Balanced);
    Add("etc1s-color-high-full", {64, 64},
        EImageTexelFormat::R8G8B8A8_UNorm, ETextureSemantic::Color,
        EImageColorSpace::SRGB, EImageAlphaMode::None,
        EImageMipPolicy::FullChain, ETextureCompressionPolicy::ETC1S,
        ETextureCookQuality::High);
    Add("etc1s-alpha-balanced-odd", {17, 9},
        EImageTexelFormat::R8G8B8A8_UNorm, ETextureSemantic::Color,
        EImageColorSpace::SRGB, EImageAlphaMode::Straight,
        EImageMipPolicy::FullChain, ETextureCompressionPolicy::ETC1S,
        ETextureCookQuality::Balanced);
    Add("etc1s-alpha-high-base", {17, 9},
        EImageTexelFormat::R8G8B8A8_UNorm, ETextureSemantic::Color,
        EImageColorSpace::SRGB, EImageAlphaMode::Straight,
        EImageMipPolicy::BaseOnly, ETextureCompressionPolicy::ETC1S,
        ETextureCookQuality::High);
    Add("etc1s-terminal-1x1", {1, 1},
        EImageTexelFormat::R8G8B8_UNorm, ETextureSemantic::Color,
        EImageColorSpace::Linear, EImageAlphaMode::None,
        EImageMipPolicy::FullChain, ETextureCompressionPolicy::ETC1S,
        ETextureCookQuality::Balanced);
    Add("uastc-color-balanced-full", {64, 64},
        EImageTexelFormat::R8G8B8A8_UNorm, ETextureSemantic::Color,
        EImageColorSpace::SRGB, EImageAlphaMode::None,
        EImageMipPolicy::FullChain, ETextureCompressionPolicy::UASTC,
        ETextureCookQuality::Balanced);
    Add("uastc-color-high-full", {64, 64},
        EImageTexelFormat::R8G8B8A8_UNorm, ETextureSemantic::Color,
        EImageColorSpace::SRGB, EImageAlphaMode::None,
        EImageMipPolicy::FullChain, ETextureCompressionPolicy::UASTC,
        ETextureCookQuality::High);
    Add("uastc-normal-balanced-full", {64, 64},
        EImageTexelFormat::R8G8B8_UNorm, ETextureSemantic::Normal,
        EImageColorSpace::Linear, EImageAlphaMode::None,
        EImageMipPolicy::FullChain, ETextureCompressionPolicy::UASTC,
        ETextureCookQuality::Balanced);
    Add("uastc-normal-high-full", {64, 64},
        EImageTexelFormat::R8G8B8_UNorm, ETextureSemantic::Normal,
        EImageColorSpace::Linear, EImageAlphaMode::None,
        EImageMipPolicy::FullChain, ETextureCompressionPolicy::UASTC,
        ETextureCookQuality::High);
    Add("uastc-data-optin-odd", {7, 5},
        EImageTexelFormat::R8G8_UNorm, ETextureSemantic::Data,
        EImageColorSpace::Linear, EImageAlphaMode::None,
        EImageMipPolicy::FullChain, ETextureCompressionPolicy::UASTC,
        ETextureCookQuality::Balanced, true);
    Add("uncompressed-r8", {5, 3},
        EImageTexelFormat::R8_UNorm, ETextureSemantic::Data,
        EImageColorSpace::Linear, EImageAlphaMode::None,
        EImageMipPolicy::BaseOnly,
        ETextureCompressionPolicy::Uncompressed,
        ETextureCookQuality::Balanced);
    Add("uncompressed-rg8", {5, 3},
        EImageTexelFormat::R8G8_UNorm, ETextureSemantic::Data,
        EImageColorSpace::Linear, EImageAlphaMode::None,
        EImageMipPolicy::BaseOnly,
        ETextureCompressionPolicy::Uncompressed,
        ETextureCookQuality::Balanced);
    Add("uncompressed-rgb8-srgb", {7, 5},
        EImageTexelFormat::R8G8B8_UNorm, ETextureSemantic::Color,
        EImageColorSpace::SRGB, EImageAlphaMode::None,
        EImageMipPolicy::FullChain,
        ETextureCompressionPolicy::Uncompressed,
        ETextureCookQuality::Balanced);
    Add("uncompressed-rgba8-alpha", {5, 3},
        EImageTexelFormat::R8G8B8A8_UNorm, ETextureSemantic::Color,
        EImageColorSpace::SRGB, EImageAlphaMode::Straight,
        EImageMipPolicy::BaseOnly,
        ETextureCompressionPolicy::Uncompressed,
        ETextureCookQuality::Balanced);
    Add("uncompressed-rgb32f", {3, 2},
        EImageTexelFormat::R32G32B32_Float, ETextureSemantic::Color,
        EImageColorSpace::Linear, EImageAlphaMode::None,
        EImageMipPolicy::BaseOnly,
        ETextureCompressionPolicy::Uncompressed,
        ETextureCookQuality::Balanced);
    Add("uncompressed-rgba16f", {3, 2},
        EImageTexelFormat::R16G16B16A16_Float,
        ETextureSemantic::Color, EImageColorSpace::Linear,
        EImageAlphaMode::Straight, EImageMipPolicy::BaseOnly,
        ETextureCompressionPolicy::Uncompressed,
        ETextureCookQuality::Balanced);
    Add("uncompressed-rgba32f", {3, 2},
        EImageTexelFormat::R32G32B32A32_Float,
        ETextureSemantic::Color, EImageColorSpace::Linear,
        EImageAlphaMode::Straight, EImageMipPolicy::BaseOnly,
        ETextureCompressionPolicy::Uncompressed,
        ETextureCookQuality::Balanced);
    Add("uncompressed-data-full", {3, 1},
        EImageTexelFormat::R8G8_UNorm, ETextureSemantic::Data,
        EImageColorSpace::Linear, EImageAlphaMode::None,
        EImageMipPolicy::FullChain,
        ETextureCompressionPolicy::Uncompressed,
        ETextureCookQuality::Balanced);
    Add("uncompressed-terminal-1x1", {1, 1},
        EImageTexelFormat::R8G8B8A8_UNorm, ETextureSemantic::Color,
        EImageColorSpace::Linear, EImageAlphaMode::Straight,
        EImageMipPolicy::FullChain,
        ETextureCompressionPolicy::Uncompressed,
        ETextureCookQuality::Balanced);
    return Corpus;
}

TArray<uint8> ReadBytes(const std::filesystem::path& Path)
{
    std::ifstream Stream(Path, std::ios::binary);
    return {
        std::istreambuf_iterator<char>(Stream),
        std::istreambuf_iterator<char>()};
}

bool WriteBytes(
    const std::filesystem::path& Path,
    std::span<const uint8> Bytes)
{
    std::error_code Error;
    std::filesystem::create_directories(Path.parent_path(), Error);
    if (Error)
    {
        return false;
    }
    std::ofstream Stream(Path, std::ios::binary | std::ios::trunc);
    Stream.write(
        reinterpret_cast<const char*>(Bytes.data()),
        static_cast<std::streamsize>(Bytes.size()));
    return Stream.good();
}

bool Reopens(
    std::span<const uint8> Bytes,
    ETextureCompressionPolicy Policy)
{
    ktxTexture2* Texture = nullptr;
    const KTX_error_code Result = ktxTexture2_CreateFromMemory(
        Bytes.data(),
        Bytes.size(),
        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
        &Texture);
    if (Result != KTX_SUCCESS || Texture == nullptr)
    {
        return false;
    }
    const bool Valid =
        Texture->baseWidth == 4 &&
        Texture->baseHeight == 4 &&
        Texture->numLevels == 1 &&
        Texture->vkFormat == 0 &&
        Texture->supercompressionScheme ==
            (Policy == ETextureCompressionPolicy::ETC1S
                 ? KTX_SS_BASIS_LZ
                 : KTX_SS_NONE);
    ktxTexture_Destroy(ktxTexture(Texture));
    return Valid;
}

uint32 ReadU32(std::span<const uint8> Bytes, usize Offset)
{
    return static_cast<uint32>(Bytes[Offset]) |
        (static_cast<uint32>(Bytes[Offset + 1]) << 8U) |
        (static_cast<uint32>(Bytes[Offset + 2]) << 16U) |
        (static_cast<uint32>(Bytes[Offset + 3]) << 24U);
}

uint64 ReadU64(std::span<const uint8> Bytes, usize Offset)
{
    if (Offset + sizeof(uint64) > Bytes.size())
    {
        return 0;
    }
    return static_cast<uint64>(ReadU32(Bytes, Offset)) |
        (static_cast<uint64>(
             ReadU32(Bytes, Offset + sizeof(uint32)))
         << 32U);
}

TArray<FString> ReadKvdKeys(std::span<const uint8> Bytes)
{
    TArray<FString> Keys;
    if (Bytes.size() < 64)
    {
        return Keys;
    }
    const usize Begin = ReadU32(Bytes, 56);
    const usize Length = ReadU32(Bytes, 60);
    if (Begin > Bytes.size() || Length > Bytes.size() - Begin)
    {
        return {};
    }
    usize Cursor = Begin;
    const usize End = Begin + Length;
    while (Cursor < End)
    {
        if (End - Cursor < 4)
        {
            return {};
        }
        const usize EntryLength = ReadU32(Bytes, Cursor);
        Cursor += 4;
        if (EntryLength == 0 || EntryLength > End - Cursor)
        {
            return {};
        }
        const auto EntryBegin = Bytes.begin() +
            static_cast<std::ptrdiff_t>(Cursor);
        const auto EntryEnd = EntryBegin +
            static_cast<std::ptrdiff_t>(EntryLength);
        const auto Terminator =
            std::find(EntryBegin, EntryEnd, static_cast<uint8>(0));
        if (Terminator == EntryEnd || Terminator == EntryBegin)
        {
            return {};
        }
        Keys.emplace_back(std::string(
            EntryBegin, Terminator));
        Cursor += (EntryLength + 3U) & ~usize(3U);
    }
    return Cursor == End ? Keys : TArray<FString>{};
}

bool DecodeRGBA8(
    std::span<const uint8> Bytes,
    TArray<uint8>& OutBase)
{
    OutBase.clear();
    ktxTexture2* Texture = nullptr;
    if (ktxTexture2_CreateFromMemory(
            Bytes.data(),
            Bytes.size(),
            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
            &Texture) != KTX_SUCCESS ||
        Texture == nullptr)
    {
        return false;
    }
    const KTX_error_code Transcode = ktxTexture2_TranscodeBasis(
        Texture, KTX_TTF_RGBA32, 0);
    ktx_size_t Offset = 0;
    const KTX_error_code OffsetResult =
        Transcode == KTX_SUCCESS
        ? ktxTexture_GetImageOffset(
              ktxTexture(Texture), 0, 0, 0, &Offset)
        : KTX_TRANSCODE_FAILED;
    const ktx_size_t Size = OffsetResult == KTX_SUCCESS
        ? ktxTexture_GetImageSize(ktxTexture(Texture), 0)
        : 0;
    if (OffsetResult == KTX_SUCCESS &&
        Texture->pData != nullptr &&
        Offset <= Texture->dataSize &&
        Size <= Texture->dataSize - Offset)
    {
        OutBase.assign(
            Texture->pData + Offset,
            Texture->pData + Offset + Size);
    }
    ktxTexture2_Destroy(Texture);
    return !OutBase.empty();
}

double ColorPsnr(
    std::span<const uint8> Reference,
    std::span<const uint8> Decoded)
{
    if (Reference.size() != Decoded.size() ||
        Reference.size() % 4 != 0)
    {
        return 0.0;
    }
    double SquaredError = 0.0;
    usize Samples = 0;
    for (usize Offset = 0; Offset < Reference.size(); Offset += 4)
    {
        for (usize Channel = 0; Channel < 3; ++Channel)
        {
            const double Difference =
                static_cast<double>(Reference[Offset + Channel]) -
                static_cast<double>(Decoded[Offset + Channel]);
            SquaredError += Difference * Difference;
            ++Samples;
        }
    }
    if (SquaredError == 0.0)
    {
        return std::numeric_limits<double>::infinity();
    }
    const double Mse = SquaredError / static_cast<double>(Samples);
    return 10.0 * std::log10((255.0 * 255.0) / Mse);
}

std::pair<double, double> NormalAngularError(
    std::span<const uint8> ReferenceRGB,
    std::span<const uint8> DecodedRGBA)
{
    if (ReferenceRGB.size() / 3 != DecodedRGBA.size() / 4)
    {
        return {
            std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::infinity()};
    }
    TArray<double> Angles;
    Angles.reserve(ReferenceRGB.size() / 3);
    for (usize Pixel = 0; Pixel < ReferenceRGB.size() / 3; ++Pixel)
    {
        std::array<double, 3> Reference{};
        std::array<double, 3> Decoded{};
        double ReferenceLength = 0.0;
        double DecodedLength = 0.0;
        for (usize Channel = 0; Channel < 3; ++Channel)
        {
            Reference[Channel] =
                static_cast<double>(ReferenceRGB[Pixel * 3 + Channel]) /
                    127.5 -
                1.0;
            Decoded[Channel] =
                static_cast<double>(DecodedRGBA[Pixel * 4 + Channel]) /
                    127.5 -
                1.0;
            ReferenceLength +=
                Reference[Channel] * Reference[Channel];
            DecodedLength += Decoded[Channel] * Decoded[Channel];
        }
        ReferenceLength = std::sqrt(ReferenceLength);
        DecodedLength = std::sqrt(DecodedLength);
        double Dot = 0.0;
        for (usize Channel = 0; Channel < 3; ++Channel)
        {
            Dot += Reference[Channel] * Decoded[Channel] /
                (ReferenceLength * DecodedLength);
        }
        Dot = std::clamp(Dot, -1.0, 1.0);
        Angles.push_back(
            std::acos(Dot) * 180.0 / std::acos(-1.0));
    }
    std::sort(Angles.begin(), Angles.end());
    double Sum = 0.0;
    for (double Angle : Angles)
    {
        Sum += Angle;
    }
    const usize P99Index = std::min(
        Angles.size() - 1,
        static_cast<usize>(
            std::ceil(Angles.size() * 0.99)) - 1);
    return {
        Sum / static_cast<double>(Angles.size()),
        Angles[P99Index]};
}

struct FBasisQualityEvidence
{
    double Etc1sColorPsnr = 0.0;
    double UastcColorPsnr = 0.0;
    double NormalMeanDegrees =
        std::numeric_limits<double>::infinity();
    double NormalP99Degrees =
        std::numeric_limits<double>::infinity();
    uint64 Etc1sPayloadBytes = 0;
    uint64 Etc1sReferenceBytes = 0;
    uint64 UastcPayloadBytes = 0;
    uint64 UastcReferenceBytes = 0;
    bool bPassed = false;
};

FBasisQualityEvidence MeasureBasisQuality(
    const TArray<FKTX2CorpusCase>& Corpus)
{
    const auto Find = [&Corpus](const char* Name)
        -> const FKTX2CorpusCase*
    {
        const auto Found = std::find_if(
            Corpus.begin(),
            Corpus.end(),
            [Name](const FKTX2CorpusCase& Case)
            {
                return Case.Name == Name;
            });
        return Found == Corpus.end() ? nullptr : &*Found;
    };
    const FKTX2CorpusCase* Etc =
        Find("etc1s-color-balanced-full");
    const FKTX2CorpusCase* Uastc =
        Find("uastc-color-balanced-full");
    const FKTX2CorpusCase* Normal =
        Find("uastc-normal-balanced-full");
    if (Etc == nullptr || Uastc == nullptr || Normal == nullptr)
    {
        return {};
    }

    const FAssetCookResult EtcCook =
        CookTexture(Etc->Texture, Etc->Settings);
    const FAssetCookResult UastcCook =
        CookTexture(Uastc->Texture, Uastc->Settings);
    const FAssetCookResult NormalCook =
        CookTexture(Normal->Texture, Normal->Settings);
    const auto EtcArtifact =
        std::dynamic_pointer_cast<const FKTX2TextureArtifact>(
            EtcCook.Payload);
    const auto UastcArtifact =
        std::dynamic_pointer_cast<const FKTX2TextureArtifact>(
            UastcCook.Payload);
    const auto NormalArtifact =
        std::dynamic_pointer_cast<const FKTX2TextureArtifact>(
            NormalCook.Payload);

    TArray<uint8> EtcDecoded;
    TArray<uint8> UastcDecoded;
    TArray<uint8> NormalDecoded;
    const bool Decoded =
        DecodeRGBA8(EtcCook.Artifact, EtcDecoded) &&
        DecodeRGBA8(UastcCook.Artifact, UastcDecoded) &&
        DecodeRGBA8(NormalCook.Artifact, NormalDecoded);

    const auto PayloadBytes =
        [](const FKTX2TextureArtifact& Artifact)
        {
            uint64 Total = 0;
            for (const FKTX2Level& Level :
                 Artifact.GetInfo().Levels)
            {
                Total += Level.ByteLength;
            }
            return Total;
        };
    const auto RgbaMipBytes =
        [](const FTextureAsset& Texture)
        {
            uint64 Total = 0;
            for (const FImageMip& Mip : Texture.GetMips())
            {
                Total += static_cast<uint64>(
                    Mip.GetExtent().Width) *
                    Mip.GetExtent().Height * 4ULL;
            }
            return Total;
        };

    FBasisQualityEvidence Evidence;
    if (Decoded)
    {
        Evidence.Etc1sColorPsnr = ColorPsnr(
            Etc->Texture->GetMips().front().GetBytes(),
            EtcDecoded);
        Evidence.UastcColorPsnr = ColorPsnr(
            Uastc->Texture->GetMips().front().GetBytes(),
            UastcDecoded);
        const auto [MeanAngle, P99Angle] =
            NormalAngularError(
                Normal->Texture->GetMips().front().GetBytes(),
                NormalDecoded);
        Evidence.NormalMeanDegrees = MeanAngle;
        Evidence.NormalP99Degrees = P99Angle;
    }
    if (EtcArtifact && UastcArtifact && NormalArtifact)
    {
        Evidence.Etc1sPayloadBytes =
            PayloadBytes(*EtcArtifact);
        Evidence.UastcPayloadBytes =
            PayloadBytes(*UastcArtifact);
        Evidence.Etc1sReferenceBytes =
            RgbaMipBytes(*Etc->Texture);
        Evidence.UastcReferenceBytes =
            RgbaMipBytes(*Uastc->Texture);
    }
    Evidence.bPassed =
        Decoded && EtcArtifact && UastcArtifact &&
        NormalArtifact &&
        Evidence.Etc1sColorPsnr >= 35.0 &&
        Evidence.UastcColorPsnr >= 40.0 &&
        Evidence.NormalMeanDegrees <= 3.0 &&
        Evidence.NormalP99Degrees <= 10.0 &&
        Evidence.Etc1sPayloadBytes * 100ULL <=
            Evidence.Etc1sReferenceBytes * 35ULL &&
        Evidence.UastcPayloadBytes * 100ULL <=
            Evidence.UastcReferenceBytes * 40ULL;
    return Evidence;
}

void TestCanonicalEncoder(FAssetKTX2TestResult& Result)
{
    const TArray<uint8> Pixels = MakePixels();
    FCanonicalBasisEncoder Encoder;
    bool AllPass = true;
    for (const auto& Case : std::array{
             std::pair{
                 ETextureCompressionPolicy::ETC1S,
                 "fe3ffc586356f3591deced7e3b59aa79e5d7a0fb40d1959eda3f5f125239a438"},
             std::pair{
                 ETextureCompressionPolicy::UASTC,
                 "b59c23533c8c6ba91e1a34d82bc3cfe4ff739dd3d662492b85e86fca0eb68ba7"}})
    {
        FKTX2EncoderRequest Request;
        Request.Policy = Case.first;
        Request.bSRGB = true;
        Request.Mips.push_back({
            {4, 4},
            std::span<const uint8>(Pixels)});
        Request.MaxOutputBytes = 1024 * 1024;

        TArray<uint8> Golden;
        for (int Iteration = 0; Iteration < 20; ++Iteration)
        {
            const FKTX2EncoderResult Encoded =
                Encoder.Encode(Request);
            const FString Digest =
                FAssetDigest::FromBytes(Encoded.Bytes).ToLowerHex();
            FKTX2PreflightResult Preflight;
            FAssetDiagnosticList PreflightDiagnostics;
            const EAssetResult PreflightResult = PreflightKTX2(
                Encoded.Bytes, {}, Preflight, &PreflightDiagnostics);
            const bool IterationPass =
                Encoded.Result == EAssetResult::Success &&
                Encoded.Diagnostics.empty() &&
                Digest == FString(Case.second) &&
                PreflightResult == EAssetResult::Success &&
                Preflight.Width == 4 &&
                Preflight.Height == 4 &&
                Preflight.Levels.size() == 1 &&
                Reopens(Encoded.Bytes, Case.first) &&
                (Iteration == 0 || Encoded.Bytes == Golden);
            if (!IterationPass)
            {
                std::cout
                    << "  encoder failure policy="
                    << static_cast<int>(Case.first)
                    << " iteration=" << Iteration
                    << " result=" << static_cast<int>(Encoded.Result)
                    << " preflight="
                    << static_cast<int>(PreflightResult)
                    << " bytes=" << Encoded.Bytes.size()
                    << " digest=" << Digest.ToStdString()
                    << " diagnostics="
                    << FAssetDiagnostics::FormatNormalized(
                           Encoded.Diagnostics).ToStdString()
                    << " preflightDiagnostics="
                    << FAssetDiagnostics::FormatNormalized(
                           PreflightDiagnostics).ToStdString()
                    << '\n';
            }
            AllPass = AllPass && IterationPass;
            if (Iteration == 0)
            {
                Golden = Encoded.Bytes;
            }
        }
    }
    Record(
        Result,
        AllPass,
        "canonical ETC1S and UASTC module output is exact over 20 runs");
}

void TestPolicy(FAssetKTX2TestResult& Result)
{
    const auto Color = MakeTexture(
        "policy-color",
        {4, 4},
        EImageTexelFormat::R8G8B8A8_UNorm,
        MakePixels(),
        ETextureSemantic::Color,
        EImageColorSpace::SRGB,
        EImageAlphaMode::Straight);
    const auto Normal = MakeTexture(
        "policy-normal",
        {1, 1},
        EImageTexelFormat::R8G8B8_UNorm,
        {128, 128, 255},
        ETextureSemantic::Normal,
        EImageColorSpace::Linear);
    const auto Data = MakeTexture(
        "policy-data",
        {1, 1},
        EImageTexelFormat::R8G8_UNorm,
        {7, 11},
        ETextureSemantic::Data,
        EImageColorSpace::Linear);
    const auto Hdr = MakeTexture(
        "policy-hdr",
        {1, 1},
        EImageTexelFormat::R16G16B16A16_Float,
        {0, 0, 0, 0, 0, 0, 0, 0},
        ETextureSemantic::Color,
        EImageColorSpace::Linear,
        EImageAlphaMode::Straight);

    ETextureCompressionPolicy ColorPolicy{};
    ETextureCompressionPolicy NormalPolicy{};
    ETextureCompressionPolicy DataPolicy{};
    ETextureCompressionPolicy HdrPolicy{};
    FTextureCookSettings Defaults;
    const bool DefaultsPass =
        Color && Normal && Data && Hdr &&
        ResolveTextureCookPolicy(
            *Color, Defaults, ColorPolicy) ==
            EAssetResult::Success &&
        ResolveTextureCookPolicy(
            *Normal, Defaults, NormalPolicy) ==
            EAssetResult::Success &&
        ResolveTextureCookPolicy(
            *Data, Defaults, DataPolicy) ==
            EAssetResult::Success &&
        ResolveTextureCookPolicy(
            *Hdr, Defaults, HdrPolicy) ==
            EAssetResult::Success &&
        ColorPolicy == ETextureCompressionPolicy::ETC1S &&
        NormalPolicy == ETextureCompressionPolicy::UASTC &&
        DataPolicy == ETextureCompressionPolicy::Uncompressed &&
        HdrPolicy == ETextureCompressionPolicy::Uncompressed;
    Record(
        Result,
        DefaultsPass,
        "semantic defaults resolve color normal data and HDR policies");

    FTextureCookSettings ETC1S;
    ETC1S.CompressionPolicy = ETextureCompressionPolicy::ETC1S;
    FTextureCookSettings UastcData;
    UastcData.CompressionPolicy = ETextureCompressionPolicy::UASTC;
    ETextureCompressionPolicy Ignored{};
    const bool RejectionsPass =
        ResolveTextureCookPolicy(
            *Normal, ETC1S, Ignored) ==
            EAssetResult::UnsupportedCompression &&
        ResolveTextureCookPolicy(
            *Hdr, ETC1S, Ignored) ==
            EAssetResult::UnsupportedCompression &&
        ResolveTextureCookPolicy(
            *Data, UastcData, Ignored) ==
            EAssetResult::UnsupportedCompression;
    UastcData.bAllowLossyData = true;
    const bool OptInPass =
        ResolveTextureCookPolicy(
            *Data, UastcData, Ignored) ==
            EAssetResult::Success &&
        Ignored == ETextureCompressionPolicy::UASTC;
    Record(
        Result,
        RejectionsPass && OptInPass,
        "lossy semantic policies reject by default and honor explicit data opt-in");

    FTextureCookSettings UnknownProfile;
    UnknownProfile.PortableProfile = FString("local-machine-defaults");
    FTextureCookSettings UnknownProducer;
    UnknownProducer.ProducerVersion = FString("022-v2");
    Record(
        Result,
        UnknownProfile.Validate() == EAssetResult::InvalidInput &&
            UnknownProducer.Validate() == EAssetResult::InvalidInput &&
            FTextureCookSettings{}.Validate() ==
                EAssetResult::Success &&
            FTextureCookLimits{}.Validate() ==
                EAssetResult::Success,
        "portable profile and producer defaults are fixed and validated");
}

void TestCooker(FAssetKTX2TestResult& Result)
{
    const auto Color = MakeTexture(
        "cook-color",
        {4, 4},
        EImageTexelFormat::R8G8B8A8_UNorm,
        MakePixels(),
        ETextureSemantic::Color,
        EImageColorSpace::SRGB,
        EImageAlphaMode::Straight);
    const FAssetCookResult Cooked = CookTexture(
        Color, {}, FString("desktop-bc"));
    const auto Artifact =
        std::dynamic_pointer_cast<const FKTX2TextureArtifact>(
            Cooked.Payload);
    const bool CompressedPass =
        Cooked.Result == EAssetResult::Success &&
        Cooked.TargetProfile == FString("desktop-bc") &&
        Artifact &&
        Cooked.Artifact.size() == Artifact->GetBytes().size() &&
        Cooked.CookDigest == Artifact->GetArtifactDigest() &&
        Artifact->GetInfo().CompressionPolicy ==
            ETextureCompressionPolicy::ETC1S &&
        Artifact->GetInfo().BasisModel == EKTX2BasisModel::ETC1S &&
        Artifact->GetInfo().Supercompression ==
            EKTX2Supercompression::BasisLZ &&
        Artifact->GetInfo().TextureId == Color->GetId() &&
        Artifact->GetInfo().SourceDigest ==
            Color->GetImage()->GetSourceDigest() &&
        Artifact->GetInfo().ContentDigest ==
            Color->GetContentDigest() &&
        Artifact->GetInfo().ColorSpace == EImageColorSpace::SRGB &&
        Artifact->GetInfo().Semantic == ETextureSemantic::Color &&
        Artifact->GetInfo().AlphaMode == EImageAlphaMode::Straight &&
        Artifact->GetInfo().Writer ==
            FString("StonerGraphicsLab/022-v1") &&
        Cooked.Diagnostics.empty();
    if (!CompressedPass)
    {
        std::cout << "  cooker failure result="
                  << static_cast<int>(Cooked.Result)
                  << " diagnostics="
                  << FAssetDiagnostics::FormatNormalized(
                         Cooked.Diagnostics).ToStdString()
                  << '\n';
    }
    Record(
        Result,
        CompressedPass,
        "registered KTX2 cooker publishes one reopened compressed artifact");

    const FAssetCookResult OtherTarget = CookTexture(
        Color, {}, FString("mobile-astc"));
    const auto OtherArtifact =
        std::dynamic_pointer_cast<const FKTX2TextureArtifact>(
            OtherTarget.Payload);
    Record(
        Result,
        Artifact && OtherArtifact &&
            Cooked.Artifact == OtherTarget.Artifact &&
            Artifact->GetInfo().CookRevision ==
                OtherArtifact->GetInfo().CookRevision &&
            Cooked.TargetProfile != OtherTarget.TargetProfile,
        "runtime target profile does not recook or reidentify the artifact");

    FTextureCookSettings High;
    High.Quality = ETextureCookQuality::High;
    const FAssetCookResult HighCook = CookTexture(Color, High);
    const auto HighArtifact =
        std::dynamic_pointer_cast<const FKTX2TextureArtifact>(
            HighCook.Payload);
    Record(
        Result,
        Artifact && HighArtifact &&
            Artifact->GetId() == HighArtifact->GetId() &&
            Artifact->GetInfo().CookRevision !=
                HighArtifact->GetInfo().CookRevision &&
            Artifact->GetArtifactDigest() !=
                HighArtifact->GetArtifactDigest(),
        "quality changes cook revision while preserving logical identity");

    FAssetCookRequest Invalid;
    FKTX2TextureCooker Direct;
    const FAssetCookResult InvalidResult = Direct.Cook(Invalid);
    Record(
        Result,
        InvalidResult.Result == EAssetResult::InvalidInput &&
            InvalidResult.Artifact.empty() &&
            !InvalidResult.CookDigest.IsAvailable() &&
            !InvalidResult.Payload &&
            !InvalidResult.Diagnostics.empty(),
        "invalid typed cook request publishes no partial output");
}

void TestUncompressedLayouts(FAssetKTX2TestResult& Result)
{
    struct FCase
    {
        const char* Name;
        EImageTexelFormat Format;
        usize ByteCount;
        ETextureSemantic Semantic;
        EImageColorSpace ColorSpace;
        EImageAlphaMode Alpha;
    };
    const std::array Cases = {
        FCase{"r8", EImageTexelFormat::R8_UNorm, 1,
              ETextureSemantic::Data, EImageColorSpace::Linear,
              EImageAlphaMode::None},
        FCase{"rg8", EImageTexelFormat::R8G8_UNorm, 2,
              ETextureSemantic::Data, EImageColorSpace::Linear,
              EImageAlphaMode::None},
        FCase{"rgb8", EImageTexelFormat::R8G8B8_UNorm, 3,
              ETextureSemantic::Color, EImageColorSpace::SRGB,
              EImageAlphaMode::None},
        FCase{"rgba8", EImageTexelFormat::R8G8B8A8_UNorm, 4,
              ETextureSemantic::Color, EImageColorSpace::SRGB,
              EImageAlphaMode::Straight},
        FCase{"rgb32f", EImageTexelFormat::R32G32B32_Float, 12,
              ETextureSemantic::Color, EImageColorSpace::Linear,
              EImageAlphaMode::None},
        FCase{"rgba16f", EImageTexelFormat::R16G16B16A16_Float, 8,
              ETextureSemantic::Color, EImageColorSpace::Linear,
              EImageAlphaMode::Straight},
        FCase{"rgba32f", EImageTexelFormat::R32G32B32A32_Float, 16,
              ETextureSemantic::Color, EImageColorSpace::Linear,
              EImageAlphaMode::Straight}};

    bool AllPass = true;
    for (const FCase& Case : Cases)
    {
        TArray<uint8> Bytes(Case.ByteCount, 0);
        const auto Texture = MakeTexture(
            std::string("uncompressed-") + Case.Name,
            {1, 1},
            Case.Format,
            std::move(Bytes),
            Case.Semantic,
            Case.ColorSpace,
            Case.Alpha);
        FTextureCookSettings Settings;
        Settings.CompressionPolicy =
            ETextureCompressionPolicy::Uncompressed;
        const FAssetCookResult Cooked =
            CookTexture(Texture, Settings);
        const auto Artifact =
            std::dynamic_pointer_cast<const FKTX2TextureArtifact>(
                Cooked.Payload);
        const bool CasePass =
            Cooked.Result == EAssetResult::Success &&
            Artifact &&
            Artifact->GetInfo().StoredTexelFormat.has_value() &&
            *Artifact->GetInfo().StoredTexelFormat == Case.Format &&
            Artifact->GetInfo().Levels.size() == 1 &&
            Artifact->GetInfo().Levels[0].Extent ==
                FImageExtent2D{1, 1};
        if (!CasePass)
        {
            std::cout << "  uncompressed failure case=" << Case.Name
                      << " result=" << static_cast<int>(Cooked.Result)
                      << " diagnostics="
                      << FAssetDiagnostics::FormatNormalized(
                             Cooked.Diagnostics).ToStdString()
                      << '\n';
        }
        AllPass = AllPass && CasePass;
    }
    Record(
        Result,
        AllPass,
        "all seven uncompressed LDR and HDR layouts cook and reopen exactly");
}

void TestGoldenCorpus(
    FAssetKTX2TestResult& Result,
    const FAssetKTX2TestOptions& Options)
{
    const TArray<FKTX2CorpusCase> Corpus = BuildCorpus();
    const FBasisQualityEvidence Quality =
        MeasureBasisQuality(Corpus);
    const std::filesystem::path GoldenRoot =
        "Tests/Fixtures/KTX2/Golden";
    const std::filesystem::path SourceRoot =
        "Tests/Fixtures/KTX2/Valid";
    bool AllPass = Corpus.size() >= 18;
    std::string Report = "{\n  \"schema\": \"stoner.ktx2.report.v1\",\n";
    Report += "  \"runs\": " +
        std::to_string(Options.DeterminismRuns) + ",\n";
    Report += "  \"artifacts\": [\n";

    for (usize CaseIndex = 0;
         CaseIndex < Corpus.size();
         ++CaseIndex)
    {
        const FKTX2CorpusCase& Case = Corpus[CaseIndex];
        const FAssetCookResult First =
            CookTexture(Case.Texture, Case.Settings);
        const auto FirstArtifact =
            std::dynamic_pointer_cast<const FKTX2TextureArtifact>(
                First.Payload);
        bool CasePass =
            Case.Texture && First.Result == EAssetResult::Success &&
            FirstArtifact && First.Diagnostics.empty();

        if (CasePass && !Options.EmitDirectory.empty())
        {
            CasePass = WriteBytes(
                std::filesystem::path(Options.EmitDirectory) /
                    (Case.Name + ".ktx2"),
                First.Artifact);
        }
        if (CasePass && !Options.EmitSourceDirectory.empty())
        {
            CasePass = WriteBytes(
                std::filesystem::path(Options.EmitSourceDirectory) /
                    (Case.Name + ".source.bin"),
                Case.Texture->GetMips().front().GetBytes());
        }

        const TArray<uint8> Golden = ReadBytes(
            GoldenRoot / (Case.Name + ".ktx2"));
        const TArray<uint8> Source = ReadBytes(
            SourceRoot / (Case.Name + ".source.bin"));
        CasePass = CasePass &&
            !Golden.empty() &&
            Golden == First.Artifact &&
            Source.size() ==
                Case.Texture->GetMips().front().GetBytes().size() &&
            std::equal(
                Source.begin(),
                Source.end(),
                Case.Texture->GetMips().front().GetBytes().begin());

        const FString Diagnostics =
            FAssetDiagnostics::FormatNormalized(First.Diagnostics);
        for (int Iteration = 1;
             Iteration < Options.DeterminismRuns;
             ++Iteration)
        {
            const FAssetCookResult Repeated =
                CookTexture(Case.Texture, Case.Settings);
            const auto RepeatedArtifact =
                std::dynamic_pointer_cast<const FKTX2TextureArtifact>(
                    Repeated.Payload);
            CasePass = CasePass &&
                Repeated.Result == EAssetResult::Success &&
                RepeatedArtifact &&
                Repeated.Artifact == First.Artifact &&
                Repeated.CookDigest == First.CookDigest &&
                RepeatedArtifact->GetInfo() ==
                    FirstArtifact->GetInfo() &&
                FAssetDiagnostics::FormatNormalized(
                    Repeated.Diagnostics) == Diagnostics;
        }
        if (!CasePass)
        {
            std::cout << "  corpus failure case=" << Case.Name
                      << " result=" << static_cast<int>(First.Result)
                      << " bytes=" << First.Artifact.size()
                      << " diagnostics="
                      << Diagnostics.ToStdString() << '\n';
        }
        AllPass = AllPass && CasePass;

        const FString Digest = First.CookDigest.ToLowerHex();
        const FString SourceDigest = Case.Texture
            ? FAssetDigest::FromBytes(
                  Case.Texture->GetMips().front().GetBytes())
                  .ToLowerHex()
            : FString();
        Report += "    {\"name\":\"" + Case.Name +
            "\",\"artifactBytes\":" +
            std::to_string(First.Artifact.size()) +
            ",\"artifactDigest\":\"" + Digest.ToStdString() +
            "\",\"sourceDigest\":\"" +
            SourceDigest.ToStdString() + "\"}";
        Report += CaseIndex + 1 == Corpus.size() ? "\n" : ",\n";
    }
    Report += "  ],\n";
    Report += "  \"quality\": {\n";
    const auto JsonMetric = [](double Value)
    {
        return std::isfinite(Value)
            ? std::to_string(Value)
            : std::string("null");
    };
    Report += "    \"passed\": " +
        std::string(Quality.bPassed ? "true" : "false") + ",\n";
    Report += "    \"etc1sColorPsnr\": " +
        JsonMetric(Quality.Etc1sColorPsnr) + ",\n";
    Report += "    \"etc1sColorExact\": " +
        std::string(
            std::isinf(Quality.Etc1sColorPsnr)
                ? "true"
                : "false") + ",\n";
    Report += "    \"uastcColorPsnr\": " +
        JsonMetric(Quality.UastcColorPsnr) + ",\n";
    Report += "    \"uastcColorExact\": " +
        std::string(
            std::isinf(Quality.UastcColorPsnr)
                ? "true"
                : "false") + ",\n";
    Report += "    \"normalMeanDegrees\": " +
        JsonMetric(Quality.NormalMeanDegrees) + ",\n";
    Report += "    \"normalP99Degrees\": " +
        JsonMetric(Quality.NormalP99Degrees) + ",\n";
    Report += "    \"etc1sPayloadBytes\": " +
        std::to_string(Quality.Etc1sPayloadBytes) + ",\n";
    Report += "    \"etc1sReferenceBytes\": " +
        std::to_string(Quality.Etc1sReferenceBytes) + ",\n";
    Report += "    \"uastcPayloadBytes\": " +
        std::to_string(Quality.UastcPayloadBytes) + ",\n";
    Report += "    \"uastcReferenceBytes\": " +
        std::to_string(Quality.UastcReferenceBytes) + ",\n";
    Report += "    \"thresholds\": {\"etc1sPsnrMin\":35.0,"
        "\"uastcPsnrMin\":40.0,\"normalMeanMax\":3.0,"
        "\"normalP99Max\":10.0,\"etc1sSizePercentMax\":35,"
        "\"uastcSizePercentMax\":40}\n";
    Report += "  }\n}\n";
    if (!Options.ReportPath.empty())
    {
        const auto* Data =
            reinterpret_cast<const uint8*>(Report.data());
        AllPass = WriteBytes(
            Options.ReportPath,
            std::span<const uint8>(Data, Report.size())) &&
            AllPass;
    }
    Record(
        Result,
        AllPass,
        "19-artifact corpus matches sources golden bytes metadata and repeated cooks");
}

void TestCanonicalProfile(FAssetKTX2TestResult& Result)
{
    const TArray<FKTX2CorpusCase> Corpus = BuildCorpus();
    const TArray<FString> ExpectedKeys = {
        FString("KTXorientation"),
        FString("KTXwriter"),
        FString("stoner.alphaMode"),
        FString("stoner.assetId"),
        FString("stoner.channelCount"),
        FString("stoner.contentDigest"),
        FString("stoner.cookRevision"),
        FString("stoner.mipPolicy"),
        FString("stoner.portableProfile"),
        FString("stoner.semantic"),
        FString("stoner.sourceDigest")};
    bool AllPass = true;
    for (const FKTX2CorpusCase& Case : Corpus)
    {
        const FAssetCookResult Cooked =
            CookTexture(Case.Texture, Case.Settings);
        const auto Artifact =
            std::dynamic_pointer_cast<const FKTX2TextureArtifact>(
                Cooked.Payload);
        FKTX2PreflightResult Preflight;
        const TArray<FString> Keys = ReadKvdKeys(Cooked.Artifact);
        const EAssetResult PreflightResult = PreflightKTX2(
            Cooked.Artifact, {}, Preflight);
        const bool CasePass =
            Artifact &&
            Keys == ExpectedKeys &&
            PreflightResult == EAssetResult::Success &&
            Preflight.Levels.size() ==
                Case.Texture->GetMips().size() &&
            Artifact->GetInfo().ArtifactDigest ==
                FAssetDigest::FromBytes(Cooked.Artifact) &&
            Artifact->GetInfo().Writer ==
                FString("StonerGraphicsLab/022-v1") &&
            Artifact->GetInfo().PortableProfile ==
                FString("stoner.ktx2.portable.v1") &&
            Artifact->GetInfo().Origin == EImageOrigin::TopLeft;
        if (!CasePass)
        {
            std::cout << "  profile failure case=" << Case.Name
                      << " keys=" << Keys.size()
                      << " preflight="
                      << static_cast<int>(PreflightResult)
                      << " levels=" << Preflight.Levels.size()
                      << "/"
                      << Case.Texture->GetMips().size()
                      << " writer="
                      << (Artifact
                              ? Artifact->GetInfo()
                                    .Writer.ToStdString()
                              : "<none>")
                      << " profile="
                      << (Artifact
                              ? Artifact->GetInfo()
                                    .PortableProfile.ToStdString()
                              : "<none>")
                      << " diagnostics="
                      << FAssetDiagnostics::FormatNormalized(
                             Cooked.Diagnostics).ToStdString()
                      << '\n';
            for (const FString& Key : Keys)
            {
                std::cout << "    key=" << Key.ToStdString()
                          << '\n';
            }
        }
        AllPass = AllPass && CasePass;
    }
    Record(
        Result,
        AllPass,
        "canonical KVD order profile identity levels and artifact digests are exact");
}

void TestBasisQualityAndSize(FAssetKTX2TestResult& Result)
{
    const TArray<FKTX2CorpusCase> Corpus = BuildCorpus();
    const FBasisQualityEvidence Evidence =
        MeasureBasisQuality(Corpus);
    if (!Evidence.bPassed)
    {
        std::cout
            << "  quality etc1sPsnr="
            << Evidence.Etc1sColorPsnr
            << " uastcPsnr="
            << Evidence.UastcColorPsnr
            << " normalMean="
            << Evidence.NormalMeanDegrees
            << " normalP99="
            << Evidence.NormalP99Degrees << '\n';
    }
    Record(
        Result,
        Evidence.bPassed,
        "Basis quality and payload size meet portable thresholds");
}

struct FExpectedTranscodeFormat
{
    ETextureTranscodeFormat Format =
        ETextureTranscodeFormat::Unknown;
    uint32 BlockWidth = 0;
    uint32 BlockHeight = 0;
    uint32 BytesPerBlock = 0;
};

FExpectedTranscodeFormat ExpectedTranscodeFormat(
    ETextureTranscodeFormat Format)
{
    switch (Format)
    {
    case ETextureTranscodeFormat::BC1_RGBA_UNorm:
    case ETextureTranscodeFormat::BC1_RGBA_SRGB:
    case ETextureTranscodeFormat::BC4_R_UNorm:
    case ETextureTranscodeFormat::ETC2_RGB8_UNorm:
    case ETextureTranscodeFormat::ETC2_RGB8_SRGB:
    case ETextureTranscodeFormat::EAC_R11_UNorm:
        return {Format, 4, 4, 8};
    case ETextureTranscodeFormat::BC3_RGBA_UNorm:
    case ETextureTranscodeFormat::BC3_RGBA_SRGB:
    case ETextureTranscodeFormat::BC5_RG_UNorm:
    case ETextureTranscodeFormat::BC7_RGBA_UNorm:
    case ETextureTranscodeFormat::BC7_RGBA_SRGB:
    case ETextureTranscodeFormat::ETC2_RGBA8_UNorm:
    case ETextureTranscodeFormat::ETC2_RGBA8_SRGB:
    case ETextureTranscodeFormat::EAC_RG11_UNorm:
    case ETextureTranscodeFormat::ASTC_4x4_RGBA_UNorm:
    case ETextureTranscodeFormat::ASTC_4x4_RGBA_SRGB:
        return {Format, 4, 4, 16};
    case ETextureTranscodeFormat::R8_UNorm:
        return {Format, 1, 1, 1};
    case ETextureTranscodeFormat::R8G8_UNorm:
        return {Format, 1, 1, 2};
    case ETextureTranscodeFormat::R8G8B8A8_UNorm:
    case ETextureTranscodeFormat::R8G8B8A8_SRGB:
        return {Format, 1, 1, 4};
    case ETextureTranscodeFormat::Unknown:
        return {};
    }
    return {};
}

bool HasExactTranscodeFootprints(
    const FTextureTranscodeResult& Transcoded,
    const FKTX2TextureArtifact& Artifact)
{
    if (Transcoded.Result != EAssetResult::Success ||
        !Transcoded.Payload ||
        Transcoded.Payload->Mips.size() !=
            Artifact.GetInfo().Levels.size())
    {
        return false;
    }
    const FExpectedTranscodeFormat Expected =
        ExpectedTranscodeFormat(Transcoded.Payload->Format);
    if (Expected.BlockWidth == 0)
    {
        return false;
    }
    for (usize Index = 0;
         Index < Transcoded.Payload->Mips.size();
         ++Index)
    {
        const FTranscodedTextureMip& Mip =
            Transcoded.Payload->Mips[Index];
        const FKTX2Level& Source = Artifact.GetInfo().Levels[Index];
        const uint64 BlocksX =
            (static_cast<uint64>(Source.Extent.Width) +
             Expected.BlockWidth - 1) /
            Expected.BlockWidth;
        const uint64 BlocksY =
            (static_cast<uint64>(Source.Extent.Height) +
             Expected.BlockHeight - 1) /
            Expected.BlockHeight;
        const uint64 RowPitch =
            BlocksX * Expected.BytesPerBlock;
        if (Mip.MipLevel != Index ||
            Mip.Extent != Source.Extent ||
            Mip.BlockWidth != Expected.BlockWidth ||
            Mip.BlockHeight != Expected.BlockHeight ||
            Mip.BytesPerBlock != Expected.BytesPerBlock ||
            Mip.RowPitchBytes != RowPitch ||
            Mip.Bytes.size() != RowPitch * BlocksY)
        {
            return false;
        }
    }
    return true;
}

TSharedPtr<const FKTX2TextureArtifact> CookArtifact(
    const FKTX2CorpusCase& Case)
{
    const FAssetCookResult Cooked =
        CookTexture(Case.Texture, Case.Settings);
    return std::dynamic_pointer_cast<const FKTX2TextureArtifact>(
        Cooked.Payload);
}

void TestBasisTranscodeTargets(FAssetKTX2TestResult& Result)
{
    const TArray<FKTX2CorpusCase> Corpus = BuildCorpus();
    const auto Find = [&Corpus](const char* Name)
        -> const FKTX2CorpusCase*
    {
        const auto Found = std::find_if(
            Corpus.begin(),
            Corpus.end(),
            [Name](const FKTX2CorpusCase& Case)
            {
                return Case.Name == Name;
            });
        return Found == Corpus.end() ? nullptr : &*Found;
    };
    const FKTX2CorpusCase* SrgbCase =
        Find("etc1s-color-balanced-full");
    const FKTX2CorpusCase* LinearCase =
        Find("etc1s-terminal-1x1");
    const FKTX2CorpusCase* AlphaCase =
        Find("etc1s-alpha-balanced-odd");
    const FKTX2CorpusCase* NormalCase =
        Find("uastc-normal-balanced-full");
    const FKTX2CorpusCase* DataCase =
        Find("uastc-data-optin-odd");
    if (!SrgbCase || !LinearCase || !AlphaCase ||
        !NormalCase || !DataCase)
    {
        Record(Result, false, "all Asset transcode targets publish exact complete mip footprints");
        return;
    }

    const auto Srgb = CookArtifact(*SrgbCase);
    const auto Linear = CookArtifact(*LinearCase);
    const auto Alpha = CookArtifact(*AlphaCase);
    const auto Normal = CookArtifact(*NormalCase);
    const auto Data = CookArtifact(*DataCase);
    const auto DataRTexture = MakeTexture(
        "uastc-data-r",
        {7, 5},
        EImageTexelFormat::R8_UNorm,
        MakePattern(
            {7, 5},
            EImageTexelFormat::R8_UNorm,
            ETextureSemantic::Data,
            EImageAlphaMode::None),
        ETextureSemantic::Data,
        EImageColorSpace::Linear,
        EImageAlphaMode::None,
        EImageMipPolicy::FullChain);
    FTextureCookSettings DataRSettings;
    DataRSettings.CompressionPolicy =
        ETextureCompressionPolicy::UASTC;
    DataRSettings.bAllowLossyData = true;
    const FAssetCookResult DataRCooked =
        CookTexture(DataRTexture, DataRSettings);
    const auto DataR =
        std::dynamic_pointer_cast<const FKTX2TextureArtifact>(
            DataRCooked.Payload);

    struct FCase
    {
        TSharedPtr<const FKTX2TextureArtifact> Artifact;
        ETextureTranscodeFormat Format;
    };
    const TArray<FCase> Cases = {
        {Linear, ETextureTranscodeFormat::BC1_RGBA_UNorm},
        {Srgb, ETextureTranscodeFormat::BC1_RGBA_SRGB},
        {Linear, ETextureTranscodeFormat::BC3_RGBA_UNorm},
        {Srgb, ETextureTranscodeFormat::BC3_RGBA_SRGB},
        {DataR, ETextureTranscodeFormat::BC4_R_UNorm},
        {Normal, ETextureTranscodeFormat::BC5_RG_UNorm},
        {Linear, ETextureTranscodeFormat::BC7_RGBA_UNorm},
        {Srgb, ETextureTranscodeFormat::BC7_RGBA_SRGB},
        {Linear, ETextureTranscodeFormat::ETC2_RGB8_UNorm},
        {Srgb, ETextureTranscodeFormat::ETC2_RGB8_SRGB},
        {Linear, ETextureTranscodeFormat::ETC2_RGBA8_UNorm},
        {Srgb, ETextureTranscodeFormat::ETC2_RGBA8_SRGB},
        {DataR, ETextureTranscodeFormat::EAC_R11_UNorm},
        {Data, ETextureTranscodeFormat::EAC_RG11_UNorm},
        {Normal, ETextureTranscodeFormat::ASTC_4x4_RGBA_UNorm},
        {Srgb, ETextureTranscodeFormat::ASTC_4x4_RGBA_SRGB},
        {DataR, ETextureTranscodeFormat::R8_UNorm},
        {Data, ETextureTranscodeFormat::R8G8_UNorm},
        {Normal, ETextureTranscodeFormat::R8G8B8A8_UNorm},
        {Alpha, ETextureTranscodeFormat::R8G8B8A8_SRGB}};

    bool AllPass = Srgb && Linear && Alpha && Normal && Data && DataR;
    for (const FCase& Case : Cases)
    {
        FTextureTranscodeRequest Request;
        Request.Artifact = Case.Artifact;
        Request.TargetFormat = Case.Format;
        const FTextureTranscodeResult Transcoded =
            FTextureTranscoder::Transcode(Request);
        const bool CasePass =
            Case.Artifact &&
            HasExactTranscodeFootprints(
                Transcoded, *Case.Artifact) &&
            Transcoded.Payload->Semantic ==
                Case.Artifact->GetInfo().Semantic &&
            Transcoded.Payload->ColorSpace ==
                Case.Artifact->GetInfo().ColorSpace &&
            Transcoded.Payload->AlphaMode ==
                Case.Artifact->GetInfo().AlphaMode &&
            Transcoded.Payload->Origin ==
                Case.Artifact->GetInfo().Origin;
        if (!CasePass)
        {
            std::cout << "  transcode failure format="
                      << static_cast<int>(Case.Format)
                      << " result="
                      << static_cast<int>(Transcoded.Result)
                      << " diagnostics="
                      << FAssetDiagnostics::FormatNormalized(
                             Transcoded.Diagnostics).ToStdString()
                      << '\n';
        }
        AllPass = AllPass && CasePass;
    }
    Record(
        Result,
        AllPass,
        "all Asset transcode targets publish exact complete mip footprints");

    const auto RejectsWithoutPayload =
        [](TSharedPtr<const FKTX2TextureArtifact> Artifact,
           ETextureTranscodeFormat Format,
           FTextureCookLimits Limits = {})
        {
            const FTextureTranscodeResult Rejected =
                FTextureTranscoder::Transcode(
                    {std::move(Artifact), Format, Limits});
            return Rejected.Result != EAssetResult::Success &&
                !Rejected.Payload &&
                !Rejected.Diagnostics.empty();
        };
    FTextureCookLimits TinyLimit;
    TinyLimit.MaxTargetPayloadBytes = 1;
    const bool RejectionsPass =
        RejectsWithoutPayload(
            {}, ETextureTranscodeFormat::BC1_RGBA_UNorm) &&
        RejectsWithoutPayload(
            Srgb, ETextureTranscodeFormat::Unknown) &&
        RejectsWithoutPayload(
            Srgb, ETextureTranscodeFormat::BC1_RGBA_UNorm) &&
        RejectsWithoutPayload(
            Normal, ETextureTranscodeFormat::BC7_RGBA_SRGB) &&
        RejectsWithoutPayload(
            Alpha, ETextureTranscodeFormat::BC1_RGBA_SRGB) &&
        RejectsWithoutPayload(
            Alpha, ETextureTranscodeFormat::ETC2_RGB8_SRGB) &&
        RejectsWithoutPayload(
            Data, ETextureTranscodeFormat::BC4_R_UNorm) &&
        RejectsWithoutPayload(
            Data, ETextureTranscodeFormat::R8_UNorm) &&
        RejectsWithoutPayload(
            Srgb, ETextureTranscodeFormat::BC1_RGBA_SRGB, TinyLimit);
    Record(
        Result,
        RejectionsPass,
        "invalid transfer alpha channel and budget targets publish no payload");
}

void TestMalformedMatrix(FAssetKTX2TestResult& Result)
{
    struct FCase
    {
        std::string Name;
        TArray<uint8> Bytes;
        FTextureCookLimits Limits;
        EAssetResult Expected = EAssetResult::MalformedContainer;
        FString Code;
    };

    const TArray<uint8> Etc = ReadBytes(
        "Tests/Fixtures/KTX2/Golden/"
        "etc1s-color-balanced-full.ktx2");
    const TArray<uint8> Uastc = ReadBytes(
        "Tests/Fixtures/KTX2/Golden/"
        "uastc-color-balanced-full.ktx2");
    TArray<FCase> Cases;
    const auto Add = [&Cases](
        std::string Name,
        TArray<uint8> Bytes,
        EAssetResult Expected,
        const char* Code,
        FTextureCookLimits Limits = {})
    {
        Cases.push_back({
            std::move(Name),
            std::move(Bytes),
            Limits,
            Expected,
            FString(Code)});
    };
    for (const usize Size : {
             usize{0}, usize{1}, usize{2}, usize{4}, usize{8},
             usize{11}, usize{12}, usize{31}, usize{63},
             usize{79}})
    {
        TArray<uint8> Truncated(
            Etc.begin(),
            Etc.begin() +
                static_cast<std::ptrdiff_t>(
                    std::min(Size, Etc.size())));
        Add(
            "truncated-" + std::to_string(Size),
            std::move(Truncated),
            EAssetResult::MalformedContainer,
            "asset.ktx2.header");
    }
    for (usize Byte = 0; Byte < 12; ++Byte)
    {
        TArray<uint8> Mutated = Etc;
        Mutated[Byte] ^= 0x5aU;
        Add(
            "identifier-" + std::to_string(Byte),
            std::move(Mutated),
            EAssetResult::MalformedContainer,
            "asset.ktx2.header");
    }
    const auto AddU32 = [&](
        const char* Name,
        usize Offset,
        uint32 Value,
        EAssetResult Expected,
        const char* Code,
        FTextureCookLimits Limits = {})
    {
        TArray<uint8> Mutated = Etc;
        PutU32(Mutated, Offset, Value);
        Add(Name, std::move(Mutated), Expected, Code, Limits);
    };
    AddU32("width-zero", 20, 0,
        EAssetResult::MalformedContainer, "asset.ktx2.scope");
    AddU32("height-zero", 24, 0,
        EAssetResult::MalformedContainer, "asset.ktx2.scope");
    AddU32("depth-nonzero", 28, 1,
        EAssetResult::MalformedContainer, "asset.ktx2.scope");
    AddU32("layers-nonzero", 32, 1,
        EAssetResult::MalformedContainer, "asset.ktx2.scope");
    AddU32("faces-zero", 36, 0,
        EAssetResult::MalformedContainer, "asset.ktx2.scope");
    AddU32("faces-two", 36, 2,
        EAssetResult::MalformedContainer, "asset.ktx2.scope");
    AddU32("levels-zero", 40, 0,
        EAssetResult::MalformedContainer, "asset.ktx2.scope");
    AddU32("width-limit", 20, 16385,
        EAssetResult::KTX2LimitExceeded, "asset.ktx2.scope");
    AddU32("height-limit", 24, 16385,
        EAssetResult::KTX2LimitExceeded, "asset.ktx2.scope");
    AddU32("mip-limit", 40, 16,
        EAssetResult::KTX2LimitExceeded, "asset.ktx2.scope");
    AddU32("type-size-zero", 16, 0,
        EAssetResult::MalformedContainer,
        "asset.ktx2.header-contract");
    AddU32("dfd-missing", 52, 0,
        EAssetResult::MalformedContainer,
        "asset.ktx2.header-contract");
    AddU32("sgd-missing", 72, 0,
        EAssetResult::MalformedContainer,
        "asset.ktx2.header-contract");
    AddU32("unknown-supercompression", 44, 99,
        EAssetResult::UnsupportedCompression,
        "asset.ktx2.supercompression");
    AddU32("dfd-misaligned", 48, 249,
        EAssetResult::MalformedContainer,
        "asset.ktx2.metadata-range");
    AddU32("dfd-out-of-range", 48,
        static_cast<uint32>(Etc.size() - 4),
        EAssetResult::MalformedContainer,
        "asset.ktx2.metadata-range");
    AddU32("kvd-overlap", 56, 80,
        EAssetResult::MalformedContainer, "asset.ktx2.overlap");
    AddU32("kvd-misaligned", 56, 309,
        EAssetResult::MalformedContainer,
        "asset.ktx2.metadata-range");

    {
        TArray<uint8> Mutated = Etc;
        PutU64(Mutated, 64, 889);
        Add(
            "sgd-misaligned",
            std::move(Mutated),
            EAssetResult::MalformedContainer,
            "asset.ktx2.metadata-range");
    }
    {
        TArray<uint8> Mutated = Etc;
        PutU64(Mutated, 80, 0);
        Add(
            "level-overlap",
            std::move(Mutated),
            EAssetResult::MalformedContainer,
            "asset.ktx2.overlap");
    }
    {
        TArray<uint8> Mutated = Etc;
        PutU64(Mutated, 88, 0);
        Add(
            "level-empty",
            std::move(Mutated),
            EAssetResult::MalformedContainer,
            "asset.ktx2.level-range");
    }
    {
        TArray<uint8> Mutated = Etc;
        PutU64(Mutated, 80, Etc.size() - 4);
        PutU64(Mutated, 88, 64);
        Add(
            "level-out-of-range",
            std::move(Mutated),
            EAssetResult::MalformedContainer,
            "asset.ktx2.level-range");
    }
    {
        TArray<uint8> Mutated = Uastc;
        PutU64(Mutated, 96, 4095);
        Add(
            "uncompressed-length-contradiction",
            std::move(Mutated),
            EAssetResult::MalformedContainer,
            "asset.ktx2.level-range");
    }

    bool AllPass = Cases.size() >= 40;
    for (const FCase& Case : Cases)
    {
        FKTX2TextureInfo Info;
        FAssetDiagnosticList Diagnostics;
        const EAssetResult Actual =
            FKTX2TextureCodec::Inspect(
                Case.Bytes,
                Case.Limits,
                Info,
                &Diagnostics);
        const bool HasExpectedDiagnostic =
            std::any_of(
                Diagnostics.begin(),
                Diagnostics.end(),
                [&Case](const FAssetDiagnostic& Diagnostic)
                {
                    return Diagnostic.Stage ==
                            EAssetStage::Container &&
                        Diagnostic.Result == Case.Expected &&
                        Diagnostic.Code == Case.Code;
                });
        const bool CasePass =
            Actual == Case.Expected &&
            HasExpectedDiagnostic &&
            !Info.TextureId.IsValid();
        if (!CasePass)
        {
            std::cout << "  malformed failure case="
                      << Case.Name
                      << " expected="
                      << static_cast<int>(Case.Expected)
                      << " actual=" << static_cast<int>(Actual)
                      << " diagnostics="
                      << FAssetDiagnostics::FormatNormalized(
                             Diagnostics).ToStdString()
                      << '\n';
        }
        AllPass = AllPass && CasePass;
    }
    Record(
        Result,
        AllPass,
        "40+ malformed KTX2 matrix returns stable container evidence without publication");
}

void TestLimitsAndLoader(FAssetKTX2TestResult& Result)
{
    const TArray<FKTX2CorpusCase> Corpus = BuildCorpus();
    const auto Found = std::find_if(
        Corpus.begin(),
        Corpus.end(),
        [](const FKTX2CorpusCase& Case)
        {
            return Case.Name ==
                "etc1s-color-balanced-full";
        });
    if (Found == Corpus.end())
    {
        Record(Result, false,
            "KTX2 exact limits and loader publication are atomic");
        return;
    }
    const FAssetCookResult Cooked =
        CookTexture(Found->Texture, Found->Settings);
    const auto Artifact =
        std::dynamic_pointer_cast<const FKTX2TextureArtifact>(
            Cooked.Payload);
    FKTX2PreflightResult Preflight;
    const EAssetResult PreflightResult = PreflightKTX2(
        Cooked.Artifact, {}, Preflight);
    uint64 LargestLevel = 0;
    for (const FKTX2Level& Level : Preflight.Levels)
    {
        LargestLevel = std::max(
            LargestLevel,
            std::max(
                Level.ByteLength,
                Level.UncompressedByteLength));
    }
    FTextureCookLimits Exact;
    Exact.MaxDimension = Preflight.Width;
    Exact.MaxArtifactBytes = Cooked.Artifact.size();
    Exact.MaxMetadataBytes =
        Preflight.DfdLength +
        Preflight.KvdLength +
        Preflight.SgdLength;
    Exact.MaxKeyValuePairs = 11;
    Exact.MaxLevelBytes = LargestLevel;
    Exact.MaxMipLevels = Preflight.LevelCount;
    FKTX2TextureInfo ExactInfo;
    FAssetDiagnosticList ExactDiagnostics;
    const bool ExactAccepted =
        Artifact &&
        PreflightResult == EAssetResult::Success &&
        FKTX2TextureCodec::Inspect(
            Cooked.Artifact,
            Exact,
            ExactInfo,
            &ExactDiagnostics) == EAssetResult::Success;

    const auto RejectsLimit =
        [&Cooked](FTextureCookLimits Limits)
        {
            FKTX2TextureInfo Info;
            FAssetDiagnosticList Diagnostics;
            return FKTX2TextureCodec::Inspect(
                       Cooked.Artifact,
                       Limits,
                       Info,
                       &Diagnostics) ==
                    EAssetResult::KTX2LimitExceeded &&
                !Info.TextureId.IsValid() &&
                std::any_of(
                    Diagnostics.begin(),
                    Diagnostics.end(),
                    [](const FAssetDiagnostic& Diagnostic)
                    {
                        return !Diagnostic.Field.IsEmpty() &&
                            !Diagnostic.Limit.IsEmpty();
                    });
        };
    FTextureCookLimits DimensionAbove = Exact;
    --DimensionAbove.MaxDimension;
    FTextureCookLimits ArtifactAbove = Exact;
    --ArtifactAbove.MaxArtifactBytes;
    FTextureCookLimits MetadataAbove = Exact;
    --MetadataAbove.MaxMetadataBytes;
    FTextureCookLimits KeysAbove = Exact;
    --KeysAbove.MaxKeyValuePairs;
    FTextureCookLimits LevelAbove = Exact;
    --LevelAbove.MaxLevelBytes;
    FTextureCookLimits MipsAbove = Exact;
    --MipsAbove.MaxMipLevels;

    const FTextureTranscodeResult BaselineTranscode =
        FTextureTranscoder::Transcode({
            Artifact,
            ETextureTranscodeFormat::BC7_RGBA_SRGB,
            {}});
    uint64 TargetBytes = 0;
    if (BaselineTranscode.Payload)
    {
        for (const FTranscodedTextureMip& Mip :
             BaselineTranscode.Payload->Mips)
        {
            TargetBytes += Mip.Bytes.size();
        }
    }
    FTextureCookLimits ExactTarget;
    ExactTarget.MaxTargetPayloadBytes = TargetBytes;
    FTextureCookLimits TargetAbove = ExactTarget;
    if (TargetAbove.MaxTargetPayloadBytes > 0)
    {
        --TargetAbove.MaxTargetPayloadBytes;
    }
    const FTextureTranscodeResult ExactTargetResult =
        FTextureTranscoder::Transcode({
            Artifact,
            ETextureTranscodeFormat::BC7_RGBA_SRGB,
            ExactTarget});
    const FTextureTranscodeResult RejectedTarget =
        FTextureTranscoder::Transcode({
            Artifact,
            ETextureTranscodeFormat::BC7_RGBA_SRGB,
            TargetAbove});
    const bool LimitsPass =
        ExactAccepted &&
        RejectsLimit(DimensionAbove) &&
        RejectsLimit(ArtifactAbove) &&
        RejectsLimit(MetadataAbove) &&
        RejectsLimit(KeysAbove) &&
        RejectsLimit(LevelAbove) &&
        RejectsLimit(MipsAbove) &&
        TargetBytes > 0 &&
        ExactTargetResult.Result == EAssetResult::Success &&
        RejectedTarget.Result ==
            EAssetResult::KTX2LimitExceeded &&
        !RejectedTarget.Payload;

    FAssetExtensionRegistry Registry;
    FAssetRegistrationToken Token;
    const bool Registered =
        RegisterKTX2TextureLoader(Registry, Token) ==
        EAssetResult::Success;
    FAssetLoadRequest Request;
    Request.Metadata = MakeMetadata(*Found->Texture);
    Request.Source = FAssetSourceLease(
        MakeShared<FMemoryAssetSource>(Cooked.Artifact));
    auto Parameters = MakeShared<FKTX2LoadParameters>();
    Parameters->ExpectedId = Found->Texture->GetId();
    Parameters->Limits = Exact;
    Request.Parameters = Parameters;
    const FAssetParticipantId LoaderId =
        MakeParticipant("loader.ktx2");
    const FAssetLoadResult Loaded =
        FAssetDispatch::Load(Registry, LoaderId, Request);
    const auto LoadedArtifact =
        std::dynamic_pointer_cast<const FKTX2TextureArtifact>(
            Loaded.Payload);

    FAssetLoadRequest Missing = Request;
    Missing.Source = {};
    const FAssetLoadResult MissingResult =
        FAssetDispatch::Load(Registry, LoaderId, Missing);
    FAssetLoadRequest Denied = Request;
    Denied.Source = FAssetSourceLease(
        MakeShared<FMemoryAssetSource>(
            TArray<uint8>{},
            EAssetResult::AccessDenied));
    const FAssetLoadResult DeniedResult =
        FAssetDispatch::Load(Registry, LoaderId, Denied);
    FAssetLoadRequest WrongIdentity = Request;
    WrongIdentity.Metadata.Id = MakeId(
        "Texture", "loader-wrong", "texture");
    auto WrongParameters = MakeShared<FKTX2LoadParameters>();
    WrongParameters->ExpectedId = WrongIdentity.Metadata.Id;
    WrongParameters->Limits = Exact;
    WrongIdentity.Parameters = WrongParameters;
    const FAssetLoadResult ConflictResult =
        FAssetDispatch::Load(
            Registry, LoaderId, WrongIdentity);

    const FAssetExecutionLease ActiveLease =
        Registry.Acquire(
            EAssetExtensionKind::Loader,
            LoaderId);
    const auto LeasedLoader =
        ActiveLease.Get<IAssetLoader>();
    Token.Reset();
    const FAssetLoadResult LeasedResult =
        LeasedLoader
        ? LeasedLoader->Load(Request)
        : FAssetLoadResult{};
    const FAssetLoadResult Inactive =
        FAssetDispatch::Load(Registry, LoaderId, Request);
    const bool LoaderPass =
        Registered &&
        Loaded.Result == EAssetResult::Success &&
        LoadedArtifact &&
        LoadedArtifact->GetBytes().size() ==
            Artifact->GetBytes().size() &&
        std::equal(
            LoadedArtifact->GetBytes().begin(),
            LoadedArtifact->GetBytes().end(),
            Artifact->GetBytes().begin()) &&
        MissingResult.Result == EAssetResult::NotFound &&
        !MissingResult.Payload &&
        DeniedResult.Result == EAssetResult::AccessDenied &&
        !DeniedResult.Payload &&
        ConflictResult.Result == EAssetResult::Conflict &&
        !ConflictResult.Payload &&
        LeasedResult.Result == EAssetResult::Success &&
        LeasedResult.Payload &&
        Inactive.Result ==
            EAssetResult::RegistrationInactive &&
        !Inactive.Payload;
    Record(
        Result,
        LimitsPass && LoaderPass,
        "KTX2 exact limits and loader publication are atomic");
}

void TestCorruptAndConcurrentRequests(
    FAssetKTX2TestResult& Result)
{
    const TArray<FKTX2CorpusCase> Corpus = BuildCorpus();
    const auto Found = std::find_if(
        Corpus.begin(),
        Corpus.end(),
        [](const FKTX2CorpusCase& Case)
        {
            return Case.Name ==
                "uastc-color-balanced-full";
        });
    const auto EtcFound = std::find_if(
        Corpus.begin(),
        Corpus.end(),
        [](const FKTX2CorpusCase& Case)
        {
            return Case.Name ==
                "etc1s-color-balanced-full";
        });
    if (Found == Corpus.end() || EtcFound == Corpus.end())
    {
        Record(Result, false,
            "corrupt and eight-way immutable requests publish no partial output");
        return;
    }
    const FAssetCookResult BaselineCook =
        CookTexture(Found->Texture, Found->Settings);
    const auto BaselineArtifact =
        std::dynamic_pointer_cast<const FKTX2TextureArtifact>(
            BaselineCook.Payload);
    const FTextureTranscodeResult BaselineTranscode =
        FTextureTranscoder::Transcode({
            BaselineArtifact,
            ETextureTranscodeFormat::BC7_RGBA_SRGB,
            {}});
    const FAssetCookResult EtcCook =
        CookTexture(EtcFound->Texture, EtcFound->Settings);
    const auto EtcArtifact =
        std::dynamic_pointer_cast<const FKTX2TextureArtifact>(
            EtcCook.Payload);
    if (!BaselineArtifact ||
        !BaselineTranscode.Payload ||
        BaselineCook.Artifact.size() < 2 ||
        !EtcArtifact)
    {
        Record(Result, false,
            "corrupt and eight-way immutable requests publish no partial output");
        return;
    }

    TArray<uint8> Truncated = BaselineCook.Artifact;
    Truncated.pop_back();
    FKTX2TextureInfo CorruptInfo =
        BaselineArtifact->GetInfo();
    CorruptInfo.ArtifactDigest = {};
    FKTX2TextureArtifact CorruptArtifact;
    const EAssetResult CorruptCreate =
        FKTX2TextureArtifact::Create(
            BaselineArtifact->GetId(),
            std::move(CorruptInfo),
            std::move(Truncated),
            CorruptArtifact);
    const FTextureTranscodeResult CorruptResult =
        FTextureTranscoder::Transcode({
            MakeShared<FKTX2TextureArtifact>(
                std::move(CorruptArtifact)),
            ETextureTranscodeFormat::BC7_RGBA_SRGB,
            {}});
    const bool CorruptPass =
        CorruptCreate == EAssetResult::Success &&
        CorruptResult.Result == EAssetResult::CorruptPayload &&
        !CorruptResult.Payload &&
        std::any_of(
            CorruptResult.Diagnostics.begin(),
            CorruptResult.Diagnostics.end(),
            [](const FAssetDiagnostic& Diagnostic)
            {
                return Diagnostic.Stage ==
                        EAssetStage::Transcode &&
                    Diagnostic.Result ==
                        EAssetResult::CorruptPayload &&
                    Diagnostic.Level.has_value();
            });

    TArray<uint8> CorruptSgdBytes = EtcCook.Artifact;
    const uint64 SgdOffset = ReadU64(CorruptSgdBytes, 64);
    const uint64 SgdLength = ReadU64(CorruptSgdBytes, 72);
    bool CorruptSgdPass =
        SgdOffset < CorruptSgdBytes.size() &&
        SgdLength >= 20 &&
        SgdLength <= CorruptSgdBytes.size() - SgdOffset;
    if (CorruptSgdPass)
    {
        std::fill_n(
            CorruptSgdBytes.begin() +
                static_cast<std::ptrdiff_t>(SgdOffset),
            20,
            uint8{0});
    }
    FKTX2TextureInfo CorruptSgdInfo =
        EtcArtifact->GetInfo();
    CorruptSgdInfo.ArtifactDigest = {};
    FKTX2TextureArtifact CorruptSgdArtifact;
    const EAssetResult CorruptSgdCreate =
        CorruptSgdPass
        ? FKTX2TextureArtifact::Create(
              EtcArtifact->GetId(),
              std::move(CorruptSgdInfo),
              std::move(CorruptSgdBytes),
              CorruptSgdArtifact)
        : EAssetResult::InvalidInput;
    const FTextureTranscodeResult CorruptSgdResult =
        CorruptSgdCreate == EAssetResult::Success
        ? FTextureTranscoder::Transcode({
              MakeShared<FKTX2TextureArtifact>(
                  std::move(CorruptSgdArtifact)),
              ETextureTranscodeFormat::BC7_RGBA_SRGB,
              {}})
        : FTextureTranscodeResult{};
    CorruptSgdPass =
        CorruptSgdPass &&
        CorruptSgdCreate == EAssetResult::Success &&
        CorruptSgdResult.Result ==
            EAssetResult::CorruptPayload &&
        !CorruptSgdResult.Payload &&
        std::any_of(
            CorruptSgdResult.Diagnostics.begin(),
            CorruptSgdResult.Diagnostics.end(),
            [](const FAssetDiagnostic& Diagnostic)
            {
                return Diagnostic.Stage ==
                        EAssetStage::Transcode &&
                    Diagnostic.Result ==
                        EAssetResult::CorruptPayload;
            });

    FAssetExtensionRegistry Registry;
    FAssetRegistrationToken LoaderToken;
    const bool Registered =
        RegisterKTX2TextureLoader(
            Registry, LoaderToken) ==
        EAssetResult::Success;
    FAssetLoadRequest LoadRequest;
    LoadRequest.Metadata = MakeMetadata(*Found->Texture);
    LoadRequest.Source = FAssetSourceLease(
        MakeShared<FMemoryAssetSource>(
            BaselineCook.Artifact));
    auto LoadParameters = MakeShared<FKTX2LoadParameters>();
    LoadParameters->ExpectedId = Found->Texture->GetId();
    LoadRequest.Parameters = LoadParameters;
    const FAssetParticipantId LoaderId =
        MakeParticipant("loader.ktx2");

    const auto PayloadMatches =
        [&BaselineTranscode](
            const FTextureTranscodeResult& Candidate)
        {
            if (!Candidate.Payload ||
                !BaselineTranscode.Payload ||
                Candidate.Payload->Format !=
                    BaselineTranscode.Payload->Format ||
                Candidate.Payload->Mips.size() !=
                    BaselineTranscode.Payload->Mips.size())
            {
                return false;
            }
            for (usize Index = 0;
                 Index < Candidate.Payload->Mips.size();
                 ++Index)
            {
                const auto& Left =
                    Candidate.Payload->Mips[Index];
                const auto& Right =
                    BaselineTranscode.Payload->Mips[Index];
                if (Left.MipLevel != Right.MipLevel ||
                    Left.Extent != Right.Extent ||
                    Left.RowPitchBytes !=
                        Right.RowPitchBytes ||
                    Left.Bytes != Right.Bytes)
                {
                    return false;
                }
            }
            return true;
        };

    struct FWorkerResult
    {
        uint32 Failures = 0;
        FString CookDiagnostics;
    };
    TArray<std::future<FWorkerResult>> Workers;
    for (int Worker = 0; Worker < 8; ++Worker)
    {
        Workers.push_back(std::async(
            std::launch::async,
            [&, Worker]
            {
                (void)Worker;
                const FAssetCookResult Cooked =
                    CookTexture(
                        Found->Texture,
                        Found->Settings);
                FKTX2TextureInfo Inspected;
                FAssetDiagnosticList Diagnostics;
                const EAssetResult InspectResult =
                    FKTX2TextureCodec::Inspect(
                        BaselineArtifact->GetBytes(),
                        {},
                        Inspected,
                        &Diagnostics);
                const FAssetLoadResult Loaded =
                    FAssetDispatch::Load(
                        Registry,
                        LoaderId,
                        LoadRequest);
                const FTextureTranscodeResult Transcoded =
                    FTextureTranscoder::Transcode({
                        BaselineArtifact,
                        ETextureTranscodeFormat::BC7_RGBA_SRGB,
                        {}});
                const auto LoadedArtifact =
                    std::dynamic_pointer_cast<
                        const FKTX2TextureArtifact>(
                        Loaded.Payload);
                uint32 Failures = 0;
                if (Cooked.Result != EAssetResult::Success)
                    Failures |= 1U;
                if (Cooked.Artifact != BaselineCook.Artifact)
                    Failures |= 16U;
                if (InspectResult != EAssetResult::Success ||
                    Inspected != BaselineArtifact->GetInfo())
                    Failures |= 2U;
                if (Loaded.Result != EAssetResult::Success ||
                    !LoadedArtifact ||
                    LoadedArtifact->GetBytes().size() !=
                        BaselineArtifact->GetBytes().size() ||
                    !std::equal(
                        LoadedArtifact->GetBytes().begin(),
                        LoadedArtifact->GetBytes().end(),
                        BaselineArtifact->GetBytes().begin()))
                    Failures |= 4U;
                if (!PayloadMatches(Transcoded))
                    Failures |= 8U;
                return FWorkerResult{
                    Failures,
                    FAssetDiagnostics::FormatNormalized(
                        Cooked.Diagnostics)};
            }));
    }
    bool ConcurrentPass = Registered;
    uint32 ConcurrentFailures = 0;
    FString ConcurrentDiagnostics;
    for (auto& Worker : Workers)
    {
        const FWorkerResult WorkerResult = Worker.get();
        ConcurrentFailures |= WorkerResult.Failures;
        if (ConcurrentDiagnostics.IsEmpty() &&
            !WorkerResult.CookDiagnostics.IsEmpty())
        {
            ConcurrentDiagnostics =
                WorkerResult.CookDiagnostics;
        }
    }
    ConcurrentPass =
        ConcurrentPass && ConcurrentFailures == 0;
    if (!CorruptPass || !CorruptSgdPass || !ConcurrentPass)
    {
        std::cout << "  corrupt/concurrent failure corruptCreate="
                  << static_cast<int>(CorruptCreate)
                  << " corruptResult="
                  << static_cast<int>(CorruptResult.Result)
                  << " corruptPayload="
                  << static_cast<bool>(CorruptResult.Payload)
                  << " corruptSgdCreate="
                  << static_cast<int>(CorruptSgdCreate)
                  << " corruptSgdResult="
                  << static_cast<int>(CorruptSgdResult.Result)
                  << " concurrent=" << ConcurrentPass
                  << " concurrentMask=" << ConcurrentFailures
                  << " concurrentDiagnostics="
                  << ConcurrentDiagnostics.ToStdString()
                  << " diagnostics="
                  << FAssetDiagnostics::FormatNormalized(
                         CorruptResult.Diagnostics).ToStdString()
                  << '\n';
    }
    Record(
        Result,
        CorruptPass && CorruptSgdPass && ConcurrentPass,
        "corrupt and eight-way immutable requests publish no partial output");
}

} // namespace

FAssetKTX2TestResult RunAssetKTX2Tests(
    const FAssetKTX2TestOptions& Options)
{
    FAssetKTX2TestResult Result;
    TestCanonicalEncoder(Result);
    TestPolicy(Result);
    TestCooker(Result);
    TestUncompressedLayouts(Result);
    TestGoldenCorpus(Result, Options);
    TestCanonicalProfile(Result);
    TestBasisQualityAndSize(Result);
    TestBasisTranscodeTargets(Result);
    TestMalformedMatrix(Result);
    TestLimitsAndLoader(Result);
    TestCorruptAndConcurrentRequests(Result);
    return Result;
}

FAssetKTX2TestResult RunAssetKTX2EncoderTests()
{
    FAssetKTX2TestResult Result;
    TestCanonicalEncoder(Result);
    return Result;
}
