#pragma once

#include "Asset/FAssetSourceVersionRecord.h"
#include "Renderer/FStaticMeshAssetConversion.h"
#include "RHI/ERHIResult.h"

#include <optional>

namespace Stoner::Asset
{
class FStaticMeshAsset;
}

namespace Stoner::RHI
{
class IRHIBuffer;
class IRHIDevice;
}

namespace Stoner::Renderer
{

enum class EStaticMeshRealizationStage
{
    ValidateAsset,
    Plan,
    Allocate,
    Upload,
    Finalize,
    Publish
};

struct FStaticMeshRealizationDiagnostic
{
    EStaticMeshRealizationStage Stage =
        EStaticMeshRealizationStage::ValidateAsset;
    Stoner::RHI::ERHIResult Result = Stoner::RHI::ERHIResult::Failed;
    Stoner::Core::FString AssetIdentity;
    std::optional<Stoner::Core::FString> PrimitiveKey;
    Stoner::Core::FString Code;
    Stoner::Core::FString Reason;
};

struct FStaticMeshAssetSnapshot
{
    Stoner::Core::TArray<Stoner::Asset::FAssetSourceVersionRecord>
        SourceManifest;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer> VertexBuffer;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer> IndexBuffer;
    Stoner::RHI::FRHIVertexInputDesc VertexInput;
    Stoner::RHI::ERHIIndexType IndexType =
        Stoner::RHI::ERHIIndexType::UInt16;
    Stoner::Core::TArray<FStaticMeshSection> Sections;
    Stoner::Asset::FStaticMeshBounds Bounds;
    Stoner::Asset::FAssetDigest RealizationProfileDigest;
};

struct FStaticMeshRealizationRequest
{
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIDevice> Device;
    Stoner::Core::TSharedPtr<const Stoner::Asset::FStaticMeshAsset> Asset;
    FStaticMeshRealizationProfile Profile;
};

struct FStaticMeshRealizationResult
{
    Stoner::RHI::ERHIResult Result = Stoner::RHI::ERHIResult::Failed;
    Stoner::Core::TSharedPtr<const FStaticMeshAssetSnapshot> Snapshot;
    FStaticMeshRealizationDiagnostic Diagnostic;

    [[nodiscard]] bool Succeeded() const noexcept;
};

class FStaticMeshRealizer
{
public:
    [[nodiscard]] static FStaticMeshRealizationResult Realize(
        const FStaticMeshRealizationRequest& Request);
};

[[nodiscard]] const char* ToString(
    EStaticMeshRealizationStage Stage) noexcept;

} // namespace Stoner::Renderer
