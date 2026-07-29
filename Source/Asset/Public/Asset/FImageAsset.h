#pragma once

#include "Asset/FAssetDigest.h"
#include "Asset/FAssetId.h"
#include "Asset/FAssetPayload.h"
#include "Asset/FAssetSource.h"
#include "Asset/FImageMip.h"

namespace Stoner::Asset
{

class FImageAsset final : public FAssetPayload
{
public:
    [[nodiscard]] static EAssetResult Create(
        FAssetId Id,
        FAssetSourceLocator Source,
        FImageMip BaseMip,
        EImageColorSpace ColorSpace,
        EImageAlphaMode AlphaMode,
        FAssetDigest SourceDigest,
        FImageAsset& OutAsset);

    [[nodiscard]] Core::FString GetAssetType() const override;
    [[nodiscard]] const FAssetId& GetId() const noexcept;
    [[nodiscard]] const FAssetSourceLocator& GetSource() const noexcept;
    [[nodiscard]] const FImageMip& GetBaseMip() const noexcept;
    [[nodiscard]] EImageColorSpace GetColorSpace() const noexcept;
    [[nodiscard]] EImageAlphaMode GetAlphaMode() const noexcept;
    [[nodiscard]] EImageOrigin GetOrigin() const noexcept;
    [[nodiscard]] const FAssetDigest& GetSourceDigest() const noexcept;
    [[nodiscard]] const FAssetDigest& GetContentDigest() const noexcept;

private:
    FAssetId Id_;
    FAssetSourceLocator Source_;
    FImageMip BaseMip_;
    EImageColorSpace ColorSpace_ = EImageColorSpace::Linear;
    EImageAlphaMode AlphaMode_ = EImageAlphaMode::None;
    FAssetDigest SourceDigest_;
    FAssetDigest ContentDigest_;
};

template <>
struct TAssetTypeTraits<FImageAsset>
{
    static Core::FString GetAssetType() { return Core::FString("Image"); }
};

} // namespace Stoner::Asset
