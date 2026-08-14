#pragma once

#include "Asset/FAssetDigest.h"
#include "Asset/FAssetImportRequest.h"
#include "Asset/FAssetExtensionRegistry.h"
#include "Asset/FAssetDiagnostics.h"
#include "Asset/IAssetImporter.h"
#include "Asset/IAssetResolver.h"
#include "Core/FCoordinateConvention.h"

namespace Stoner::Asset
{

enum class EStaticMeshNormalPolicy : Core::uint8
{
    GenerateFlat,
    RequireSource
};

enum class EStaticMeshTangentPolicy : Core::uint8
{
    GenerateWhenRequired,
    RequireSource
};

struct FStaticModelImportLimits
{
    static constexpr Core::uint64 DefaultMainSourceBytes = 256ULL * 1024ULL * 1024ULL;
    static constexpr Core::uint64 DefaultSingleDependencyBytes = 512ULL * 1024ULL * 1024ULL;
    static constexpr Core::uint64 DefaultAggregateDependencyBytes = 1024ULL * 1024ULL * 1024ULL;
    static constexpr Core::uint64 DefaultParserAllocationBytes = 512ULL * 1024ULL * 1024ULL;
    static constexpr Core::uint32 DefaultSceneCount = 1024;
    static constexpr Core::uint32 DefaultNodeCount = 1000000;
    static constexpr Core::uint32 DefaultHierarchyDepth = 1024;
    static constexpr Core::uint32 DefaultMeshCount = 262144;
    static constexpr Core::uint32 DefaultPrimitiveCount = 1000000;
    static constexpr Core::uint32 DefaultMaterialTextureImageCount = 262144;
    static constexpr Core::uint32 DefaultVertexCount = 16777216;
    static constexpr Core::uint32 DefaultIndexCount = 50331648;
    static constexpr Core::uint64 DefaultDecodedGeometryBytes = 2ULL * 1024ULL * 1024ULL * 1024ULL;
    static constexpr Core::uint32 DefaultDiagnosticCount = 4096;

    Core::uint64 MaxMainSourceBytes = DefaultMainSourceBytes;
    Core::uint64 MaxSingleDependencyBytes = DefaultSingleDependencyBytes;
    Core::uint64 MaxAggregateDependencyBytes = DefaultAggregateDependencyBytes;
    Core::uint64 MaxParserAllocationBytes = DefaultParserAllocationBytes;
    Core::uint32 MaxScenes = DefaultSceneCount;
    Core::uint32 MaxNodes = DefaultNodeCount;
    Core::uint32 MaxHierarchyDepth = DefaultHierarchyDepth;
    Core::uint32 MaxMeshes = DefaultMeshCount;
    Core::uint32 MaxPrimitives = DefaultPrimitiveCount;
    Core::uint32 MaxMaterials = DefaultMaterialTextureImageCount;
    Core::uint32 MaxTextures = DefaultMaterialTextureImageCount;
    Core::uint32 MaxImages = DefaultMaterialTextureImageCount;
    Core::uint32 MaxVerticesPerPrimitive = DefaultVertexCount;
    Core::uint32 MaxIndicesPerPrimitive = DefaultIndexCount;
    Core::uint64 MaxDecodedGeometryBytes = DefaultDecodedGeometryBytes;
    Core::uint32 MaxDiagnostics = DefaultDiagnosticCount;

    [[nodiscard]] EAssetResult Validate() const noexcept;
    [[nodiscard]] bool operator==(const FStaticModelImportLimits&) const = default;
};

class FStaticModelImportProfile final : public FAssetImportParameters
{
public:
    Core::uint32 SchemaVersion = 1;
    Core::FString ProfileName = Core::FString("static-model-v1");
    EStaticMeshNormalPolicy NormalPolicy = EStaticMeshNormalPolicy::GenerateFlat;
    EStaticMeshTangentPolicy TangentPolicy =
        EStaticMeshTangentPolicy::GenerateWhenRequired;
    Core::uint32 MaximumTexCoordSets = 2;
    Core::FString MaterialMappingProfile = Core::FString("gltf-metallic-roughness-v1");
    Core::FString CoordinateConvention =
        Core::FString(Core::FCoordinateConvention::Name);
    FStaticModelImportLimits Limits;

    [[nodiscard]] EAssetResult Validate() const noexcept;
    [[nodiscard]] FAssetDigest GetDigest() const;
};

struct FStaticModelImportRequest
{
    FAssetImportRequest AssetRequest;
    Core::TSharedPtr<IAssetResolver> DependencyResolver;
    Core::TSharedPtr<const FStaticModelImportProfile> Profile;
};

[[nodiscard]] bool IsValidStaticMeshNormalPolicy(
    EStaticMeshNormalPolicy Value) noexcept;
[[nodiscard]] bool IsValidStaticMeshTangentPolicy(
    EStaticMeshTangentPolicy Value) noexcept;

[[nodiscard]] EAssetResult RegisterStaticModelImporter(
    FAssetExtensionRegistry& Registry,
    FAssetRegistrationToken& OutToken);

[[nodiscard]] EAssetResult ImportStaticModel(
    const FStaticModelImportRequest& Request,
    Core::TArray<FAssetImportOutput>& OutOutputs,
    FAssetDiagnosticList* Diagnostics = nullptr);

} // namespace Stoner::Asset
