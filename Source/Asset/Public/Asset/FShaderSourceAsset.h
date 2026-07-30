#pragma once

#include "Asset/FAssetId.h"
#include "Asset/FAssetPayload.h"
#include "Asset/FAssetVersion.h"
#include "Asset/FMaterialShaderTypes.h"
#include "Core/TArray.h"

namespace Stoner::Asset
{

class FShaderSourceAsset final : public FAssetPayload
{
public:
    [[nodiscard]] static EAssetResult Create(
        FAssetId Id,
        FAssetVersion Version,
        EShaderSourceLanguage Language,
        Core::TArray<Core::uint8> Bytes,
        FShaderSourceAsset& OutAsset);

    [[nodiscard]] Core::FString GetAssetType() const override;
    [[nodiscard]] const FAssetId& GetId() const noexcept;
    [[nodiscard]] const FAssetVersion& GetVersion() const noexcept;
    [[nodiscard]] EShaderSourceLanguage GetLanguage() const noexcept;
    [[nodiscard]] const Core::TArray<Core::uint8>& GetBytes() const noexcept;

private:
    FAssetId Id_;
    FAssetVersion Version_;
    EShaderSourceLanguage Language_ = EShaderSourceLanguage::GLSL;
    Core::TArray<Core::uint8> Bytes_;
};

} // namespace Stoner::Asset
