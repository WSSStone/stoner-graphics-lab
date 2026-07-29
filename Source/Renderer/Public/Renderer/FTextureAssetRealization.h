#pragma once

#include "Core/CoreMinimal.h"

#include <optional>

namespace Stoner::Asset
{
class FTextureAsset;
}

namespace Stoner::RHI
{
enum class ERHIResult;
class IRHIDevice;
class IRHITexture;
}

namespace Stoner::Renderer
{

enum class ETextureAssetRealizationStage
{
    ValidateAsset,
    Plan,
    Create,
    Expand,
    Upload,
    Finalize
};

struct FTextureAssetRealizationDiagnostic
{
    ETextureAssetRealizationStage Stage =
        ETextureAssetRealizationStage::ValidateAsset;
    Stoner::RHI::ERHIResult Result{};
    Stoner::Core::FString AssetIdentity;
    std::optional<Stoner::Core::uint32> MipLevel;
    Stoner::Core::FString Code;
    Stoner::Core::FString Reason;
};

struct FTextureAssetRealizationRequest
{
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIDevice> Device;
    Stoner::Core::TSharedPtr<const Stoner::Asset::FTextureAsset> Asset;
};

struct FTextureAssetRealizationResult
{
    Stoner::RHI::ERHIResult Result{};
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture> Texture;
    FTextureAssetRealizationDiagnostic Diagnostic;

    [[nodiscard]] bool Succeeded() const noexcept;
};

class FTextureAssetRealizer
{
public:
    [[nodiscard]] static FTextureAssetRealizationResult Realize(
        const FTextureAssetRealizationRequest& Request);
};

[[nodiscard]] const char* ToString(
    ETextureAssetRealizationStage Stage) noexcept;

} // namespace Stoner::Renderer
