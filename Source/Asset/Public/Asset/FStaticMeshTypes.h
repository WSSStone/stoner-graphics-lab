#pragma once

#include "Asset/FMaterialShaderTypes.h"
#include "Asset/TSoftAssetRef.h"
#include "Core/FBox.h"
#include "Core/FSphere.h"
#include "Core/FVector2.h"
#include "Core/FVector3.h"
#include "Core/FVector4.h"
#include "Core/TArray.h"

#include <array>
#include <variant>

namespace Stoner::Asset
{

struct FStaticMeshBounds
{
    Core::FBox Box;
    Core::FSphere Sphere;

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] bool Contains(const Core::FVector3& Position) const noexcept;
};

struct FStaticMeshVertexData
{
    Core::TArray<Core::FVector3> Positions;
    Core::TArray<Core::FVector3> Normals;
    Core::TArray<Core::FVector4> Tangents;
    std::array<Core::TArray<Core::FVector2>, 2> TexCoords;

    [[nodiscard]] bool IsValid(float Tolerance = Core::FMath::DefaultTolerance)
        const noexcept;
};

class FStaticMeshIndexData
{
public:
    [[nodiscard]] static EAssetResult Create(
        Core::TArray<Core::uint32> SourceIndices,
        FStaticMeshIndexData& OutIndices);

    [[nodiscard]] bool IsValid(Core::uint32 VertexCount) const noexcept;
    [[nodiscard]] bool Uses16BitIndices() const noexcept;
    [[nodiscard]] Core::uint32 GetIndexCount() const noexcept;
    [[nodiscard]] Core::uint32 GetIndex(Core::uint32 Index) const noexcept;

private:
    std::variant<Core::TArray<Core::uint16>, Core::TArray<Core::uint32>> Data_;
};

struct FStaticMeshPrimitive
{
    Core::FString StableKey;
    FStaticMeshVertexData Vertices;
    FStaticMeshIndexData Indices;
    Core::uint32 MaterialSlotIndex = 0;
    FStaticMeshBounds LocalBounds;
    Core::uint32 SourcePrimitiveIndex = 0;

    [[nodiscard]] bool IsValid(Core::uint32 MaterialSlotCount) const noexcept;
};

struct FStaticMeshMaterialSlot
{
    Core::FString StableKey;
    TSoftAssetRef<FMaterialAsset> Material;
};

} // namespace Stoner::Asset
