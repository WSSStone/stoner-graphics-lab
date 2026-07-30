#pragma once

#include "Asset/FAssetId.h"
#include "Asset/FImageTypes.h"
#include "Asset/IAssetCooker.h"

namespace Stoner::Asset
{

enum class ETextureCompressionPolicy : Core::uint8
{
    DefaultBySemantic,
    ETC1S,
    UASTC,
    Uncompressed
};

enum class ETextureCookQuality : Core::uint8
{
    Balanced,
    High
};

struct FTextureCookLimits
{
    static constexpr Core::uint32 DefaultMaxDimension = 16384;
    static constexpr Core::uint64 DefaultMaxArtifactBytes =
        512ULL * 1024ULL * 1024ULL;
    static constexpr Core::uint64 DefaultMaxMetadataBytes =
        1ULL * 1024ULL * 1024ULL;
    static constexpr Core::uint32 DefaultMaxKeyValuePairs = 64;
    static constexpr Core::uint64 DefaultMaxLevelBytes =
        512ULL * 1024ULL * 1024ULL;
    static constexpr Core::uint64 DefaultMaxTargetPayloadBytes =
        1024ULL * 1024ULL * 1024ULL;
    static constexpr Core::uint32 DefaultMaxMipLevels = 15;

    Core::uint32 MaxDimension = DefaultMaxDimension;
    Core::uint64 MaxArtifactBytes = DefaultMaxArtifactBytes;
    Core::uint64 MaxMetadataBytes = DefaultMaxMetadataBytes;
    Core::uint32 MaxKeyValuePairs = DefaultMaxKeyValuePairs;
    Core::uint64 MaxLevelBytes = DefaultMaxLevelBytes;
    Core::uint64 MaxTargetPayloadBytes = DefaultMaxTargetPayloadBytes;
    Core::uint32 MaxMipLevels = DefaultMaxMipLevels;

    [[nodiscard]] EAssetResult Validate() const noexcept;
    [[nodiscard]] bool operator==(const FTextureCookLimits&) const = default;
};

struct FTextureCookSettings
{
    ETextureCompressionPolicy CompressionPolicy =
        ETextureCompressionPolicy::DefaultBySemantic;
    ETextureCookQuality Quality = ETextureCookQuality::Balanced;
    bool bAllowLossyData = false;
    Core::FString PortableProfile =
        Core::FString("stoner.ktx2.portable.v1");
    Core::FString ProducerVersion = Core::FString("022-v1");

    [[nodiscard]] EAssetResult Validate() const noexcept;
    [[nodiscard]] bool operator==(const FTextureCookSettings&) const = default;
};

struct FTextureCookParameters final : FAssetCookParameters
{
    FAssetId TextureId;
    FTextureCookSettings Settings;
    FTextureCookLimits Limits;
};

class FKTX2TextureCooker final : public IAssetCooker
{
public:
    [[nodiscard]] FAssetExtensionCapability GetCapability() const override;
    [[nodiscard]] FAssetCookResult Cook(
        const FAssetCookRequest& Request) override;
};

[[nodiscard]] EAssetResult RegisterKTX2TextureCooker(
    FAssetExtensionRegistry& Registry,
    FAssetRegistrationToken& OutToken);

} // namespace Stoner::Asset
