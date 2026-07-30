#pragma once

#include "Asset/FAssetDigest.h"
#include "Asset/FAssetId.h"
#include "Asset/FAssetPayload.h"
#include "Asset/FImageTypes.h"
#include "Asset/FTextureCook.h"
#include "Core/TArray.h"

#include <optional>
#include <span>

namespace Stoner::Asset
{

enum class EKTX2BasisModel : Core::uint8
{
    None,
    ETC1S,
    UASTC
};

enum class EKTX2Supercompression : Core::uint8
{
    None,
    BasisLZ
};

struct FKTX2Level
{
    Core::uint32 MipLevel = 0;
    FImageExtent2D Extent;
    Core::uint64 ByteOffset = 0;
    Core::uint64 ByteLength = 0;
    Core::uint64 UncompressedByteLength = 0;

    [[nodiscard]] bool operator==(const FKTX2Level&) const = default;
};

struct FKTX2TextureInfo
{
    FAssetId TextureId;
    FAssetDigest SourceDigest;
    FAssetDigest ContentDigest;
    FAssetDigest CookRevision;
    FAssetDigest ArtifactDigest;
    Core::FString ProducerVersion;
    Core::FString PortableProfile;
    ETextureCompressionPolicy CompressionPolicy =
        ETextureCompressionPolicy::Uncompressed;
    EKTX2BasisModel BasisModel = EKTX2BasisModel::None;
    EKTX2Supercompression Supercompression =
        EKTX2Supercompression::None;
    ETextureSemantic Semantic = ETextureSemantic::Unspecified;
    EImageColorSpace ColorSpace = EImageColorSpace::Linear;
    EImageAlphaMode AlphaMode = EImageAlphaMode::None;
    EImageOrigin Origin = EImageOrigin::TopLeft;
    EImageMipPolicy MipPolicy = EImageMipPolicy::FullChain;
    Core::uint32 SourceChannelCount = 0;
    FImageExtent2D BaseExtent;
    Core::TArray<FKTX2Level> Levels;
    std::optional<EImageTexelFormat> StoredTexelFormat;
    Core::FString Writer;

    [[nodiscard]] bool operator==(const FKTX2TextureInfo&) const = default;
};

class FKTX2TextureArtifact final : public FAssetPayload
{
public:
    [[nodiscard]] static EAssetResult Create(
        FAssetId Id,
        FKTX2TextureInfo Info,
        Core::TArray<Core::uint8> Bytes,
        FKTX2TextureArtifact& OutArtifact);

    [[nodiscard]] Core::FString GetAssetType() const override;
    [[nodiscard]] const FAssetId& GetId() const noexcept;
    [[nodiscard]] const FKTX2TextureInfo& GetInfo() const noexcept;
    [[nodiscard]] std::span<const Core::uint8> GetBytes() const noexcept;
    [[nodiscard]] const FAssetDigest& GetArtifactDigest() const noexcept;

private:
    FAssetId Id_;
    FKTX2TextureInfo Info_;
    Core::TArray<Core::uint8> Bytes_;
};

template <>
struct TAssetTypeTraits<FKTX2TextureArtifact>
{
    static Core::FString GetAssetType()
    {
        return Core::FString("Texture");
    }
};

} // namespace Stoner::Asset
