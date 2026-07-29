#pragma once

#include "Asset/FImageAsset.h"
#include "Asset/TSoftAssetRef.h"

namespace Stoner::Asset
{

class FTextureAsset final : public FAssetPayload
{
public:
    [[nodiscard]] static EAssetResult Create(
        FAssetId Id,
        Core::TSharedPtr<const FImageAsset> Image,
        FImageImportSettings Settings,
        Core::TArray<FImageMip> Mips,
        FTextureAsset& OutAsset);

    [[nodiscard]] Core::FString GetAssetType() const override;
    [[nodiscard]] const FAssetId& GetId() const noexcept;
    [[nodiscard]] const TSoftAssetRef<FImageAsset>& GetImageReference() const noexcept;
    [[nodiscard]] const Core::TSharedPtr<const FImageAsset>& GetImage() const noexcept;
    [[nodiscard]] const FImageImportSettings& GetSettings() const noexcept;
    [[nodiscard]] ETextureSemantic GetSemantic() const noexcept;
    [[nodiscard]] EImageColorSpace GetColorSpace() const noexcept;
    [[nodiscard]] EImageAlphaMode GetAlphaMode() const noexcept;
    [[nodiscard]] EImageOrigin GetOrigin() const noexcept;
    [[nodiscard]] EImageMipPolicy GetMipPolicy() const noexcept;
    [[nodiscard]] const Core::TArray<FImageMip>& GetMips() const noexcept;
    [[nodiscard]] const FAssetDigest& GetContentDigest() const noexcept;

private:
    FAssetId Id_;
    TSoftAssetRef<FImageAsset> ImageReference_;
    Core::TSharedPtr<const FImageAsset> Image_;
    FImageImportSettings Settings_;
    EImageColorSpace ColorSpace_ = EImageColorSpace::Linear;
    Core::TArray<FImageMip> Mips_;
    FAssetDigest ContentDigest_;
};

template <>
struct TAssetTypeTraits<FTextureAsset>
{
    static Core::FString GetAssetType() { return Core::FString("Texture"); }
};

} // namespace Stoner::Asset
