#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FTextureTargetProfile.h"

#include <optional>

namespace Stoner::Asset
{
class FKTX2TextureArtifact;
}

namespace Stoner::RHI
{
enum class ERHIResult;
class IRHIDevice;
class IRHITexture;
}

namespace Stoner::Renderer
{

enum class EKTX2TextureRealizationStage
{
    ValidateArtifact,
    Select,
    Transcode,
    Create,
    Upload,
    Finalize
};

struct FKTX2TextureRealizationDiagnostic
{
    EKTX2TextureRealizationStage Stage =
        EKTX2TextureRealizationStage::ValidateArtifact;
    Stoner::RHI::ERHIResult Result{};
    Stoner::Core::FString AssetIdentity;
    std::optional<Stoner::Core::uint32> MipLevel;
    Stoner::Core::FString Code;
    Stoner::Core::FString Reason;
};

struct FKTX2TextureRealizationRequest
{
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIDevice> Device;
    Stoner::Core::TSharedPtr<
        const Stoner::Asset::FKTX2TextureArtifact> Artifact;
    FTextureTargetProfile TargetProfile;
};

struct FKTX2TextureRealizationResult
{
    Stoner::RHI::ERHIResult Result{};
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture> Texture;
    FTextureTargetSelection Selection;
    FKTX2TextureRealizationDiagnostic Diagnostic;

    [[nodiscard]] bool Succeeded() const noexcept;
};

class FKTX2TextureRealizer
{
public:
    [[nodiscard]] static FKTX2TextureRealizationResult Realize(
        const FKTX2TextureRealizationRequest& Request);
};

[[nodiscard]] const char* ToString(
    EKTX2TextureRealizationStage Stage) noexcept;

} // namespace Stoner::Renderer
