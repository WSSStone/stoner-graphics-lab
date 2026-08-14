#pragma once

#include "Asset/FAssetDigest.h"
#include "Asset/FAssetId.h"
#include "Asset/FStaticMeshTypes.h"
#include "Core/CoreMinimal.h"
#include "RHI/ERHIIndexType.h"
#include "RHI/ERHIResult.h"
#include "RHI/FRHIGraphicsPipelineDesc.h"

namespace Stoner::Asset
{
class FStaticMeshAsset;
}

namespace Stoner::Renderer
{

enum class EStaticMeshIndexPackingPolicy
{
    Smallest,
    UInt32
};

struct FStaticMeshRealizationProfile
{
    Stoner::Core::uint32 Version = 1;
    EStaticMeshIndexPackingPolicy IndexPacking =
        EStaticMeshIndexPackingPolicy::Smallest;
    Stoner::Core::FString CoordinateConvention =
        Stoner::Core::FString("LH-XForward-YRight-ZUp-Clockwise");

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] Stoner::Asset::FAssetDigest ComputeDigest() const;
};

struct FStaticMeshSection
{
    Stoner::Core::uint32 FirstIndex = 0;
    Stoner::Core::uint32 IndexCount = 0;
    Stoner::Core::int32 VertexOffset = 0;
    Stoner::Asset::FAssetId Material;
    Stoner::Asset::FStaticMeshBounds Bounds;
    Stoner::Core::FString SourcePrimitiveKey;
};

struct FStaticMeshPackingPlan
{
    Stoner::Core::TArray<Stoner::Core::uint8> VertexBytes;
    Stoner::Core::TArray<Stoner::Core::uint8> IndexBytes;
    Stoner::RHI::FRHIVertexInputDesc VertexInput;
    Stoner::RHI::ERHIIndexType IndexType =
        Stoner::RHI::ERHIIndexType::UInt16;
    Stoner::Core::TArray<FStaticMeshSection> Sections;
    Stoner::Asset::FAssetDigest ProfileDigest;
};

[[nodiscard]] Stoner::RHI::ERHIResult BuildStaticMeshPackingPlan(
    const Stoner::Asset::FStaticMeshAsset& Asset,
    const FStaticMeshRealizationProfile& Profile,
    FStaticMeshPackingPlan& OutPlan,
    Stoner::Core::FString* OutReason = nullptr);

} // namespace Stoner::Renderer
