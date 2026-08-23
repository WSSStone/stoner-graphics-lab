#pragma once

#include "Asset/FAssetRuntimeExecutionContext.h"
#include "Asset/FAssetTargetProfile.h"
#include "Asset/FKTX2TextureArtifact.h"
#include "Asset/FMaterialAsset.h"
#include "Asset/FMaterialInstanceAsset.h"
#include "Asset/FShaderAsset.h"
#include "Asset/FShaderPayloadAsset.h"
#include "Asset/FStaticMeshAsset.h"
#include "Asset/FStaticModelAsset.h"
#include "Renderer/FMaterialAssetConversion.h"
#include "Renderer/FShaderAssetConversion.h"
#include "Renderer/FStaticMeshRealization.h"
#include "Renderer/FTextureTargetProfile.h"
#include "RHI/FRHIGraphicsPipelineDesc.h"

#include <optional>

namespace Stoner::RHI
{
class IRHIBuffer;
class IRHIDescriptorSet;
class IRHIDevice;
class IRHIGraphicsPipeline;
class IRHIPipelineLayout;
class IRHISampler;
class IRHIShaderModule;
class IRHITexture;
}

namespace Stoner::Renderer
{

enum class EStaticModelRealizationStage : Core::uint8
{
    Validate,
    Plan,
    Mesh,
    Texture,
    Shader,
    Material,
    Descriptor,
    Pipeline,
    Commit,
    Rollback,
    Published,
    Released
};

struct FStaticModelRealizationLimits
{
    Core::uint32 MaxNodes = 4096;
    Core::uint32 MaxDraws = 65536;
    Core::uint32 MaxMeshes = 4096;
    Core::uint32 MaxMaterials = 4096;
    Core::uint32 MaxShaders = 1024;
    Core::uint32 MaxTextures = 4096;
    Core::uint32 MaxDescriptors = 16384;
    Core::uint32 MaxPipelines = 4096;
    Core::uint64 MaxAggregateResourceBytes =
        8ULL * 1024ULL * 1024ULL * 1024ULL;

    [[nodiscard]] bool IsValid() const noexcept;
};

struct FStaticModelRealizationDependencies
{
    struct FVersionRecord
    {
        Asset::FAssetId AssetId;
        Asset::FAssetVersion Version;
    };

    Core::TArray<Core::TSharedPtr<const Asset::FStaticMeshAsset>> Meshes;
    Core::TArray<Core::TSharedPtr<const Asset::FMaterialAsset>> Materials;
    Core::TArray<Core::TSharedPtr<const Asset::FMaterialInstanceAsset>>
        MaterialInstances;
    Core::TArray<Core::TSharedPtr<const Asset::FShaderAsset>> Shaders;
    Core::TArray<Core::TSharedPtr<const Asset::FShaderPayloadAsset>>
        ShaderPayloads;
    Core::TArray<Core::TSharedPtr<const Asset::FKTX2TextureArtifact>> Textures;
    Core::TArray<FVersionRecord> Versions;
};

struct FStaticModelRealizationRequest
{
    Core::TSharedPtr<RHI::IRHIDevice> Device;
    Core::TSharedPtr<const Asset::FStaticModelAsset> Model;
    FStaticModelRealizationDependencies Dependencies;
    Core::TSharedPtr<const Asset::FAssetTargetProfileEvidence> TargetEvidence;
    struct FTextureProfileRecord
    {
        Asset::FAssetId AssetId;
        FTextureTargetProfile Profile;
    };
    Core::TArray<FTextureProfileRecord> TextureTargetProfiles;
    FTextureTargetProfile TextureTargetProfile;
    FStaticMeshRealizationProfile MeshProfile;
    RHI::FRHIRenderTargetCompatibility RenderTargets;
    FStaticModelRealizationLimits Limits;
    Core::TSharedPtr<const Asset::FAssetRuntimeExecutionContext>
        RuntimeContext;
};

struct FStaticModelRealizationDiagnostic
{
    EStaticModelRealizationStage Stage =
        EStaticModelRealizationStage::Validate;
    RHI::ERHIResult Result = RHI::ERHIResult::Failed;
    Core::FString Code;
    Core::FString Subject;
    Core::FString Reason;
};

struct FStaticModelRealizationInspection
{
    EStaticModelRealizationStage Stage =
        EStaticModelRealizationStage::Validate;
    Core::uint64 SnapshotGeneration = 0;
    Core::uint32 NodeCount = 0;
    Core::uint32 DrawCount = 0;
    Core::uint32 UniqueMeshCount = 0;
    Core::uint32 UniqueMaterialCount = 0;
    Core::uint32 UniqueShaderCount = 0;
    Core::uint32 UniqueTextureCount = 0;
    Core::uint32 DescriptorSetCount = 0;
    Core::uint32 PipelineCount = 0;
    Core::uint32 CreatedResourceCount = 0;
    Core::uint32 ReleasedResourceCount = 0;
    bool bCommitted = false;
    bool bRolledBack = false;
    Core::TArray<Core::FString> OrderedResourceIds;
    Core::TArray<Core::FString> ReverseReleaseIds;
    FStaticModelRealizationDiagnostic FirstFailure;

    [[nodiscard]] Core::FString Dump() const;
};

struct FStaticModelRenderNode
{
    Core::uint32 SourceNodeIndex = 0;
    Core::FString StableKey;
    Core::FMatrix4x4 WorldTransform;
};

struct FStaticModelRenderDraw
{
    Core::uint32 NodeIndex = 0;
    Core::uint32 MeshIndex = 0;
    Core::uint32 SectionIndex = 0;
    Core::uint32 MaterialIndex = 0;
    Core::uint32 PipelineIndex = 0;
    Core::FString StableKey;
    Asset::FStaticMeshBounds Bounds;
};

struct FStaticModelTextureResource
{
    Asset::FAssetId AssetId;
    Asset::FAssetVersion Version;
    Core::TSharedPtr<RHI::IRHITexture> Texture;
};

struct FStaticModelBufferBindingResource
{
    Core::uint32 SetIndex = 0;
    Core::uint32 BindingSlot = 0;
    Core::TSharedPtr<RHI::IRHIBuffer> Buffer;
};

struct FStaticModelMaterialResources
{
    Core::TSharedPtr<RHI::IRHIPipelineLayout> PipelineLayout;
    Core::TArray<Core::TSharedPtr<RHI::IRHIDescriptorSet>> DescriptorSets;
    Core::TArray<FStaticModelBufferBindingResource> BufferBindings;
    Core::TSharedPtr<RHI::IRHIGraphicsPipeline> Pipeline;
};

struct FStaticModelDrawResources
{
    Core::TArray<Core::TSharedPtr<RHI::IRHIDescriptorSet>> DescriptorSets;
    Core::TArray<FStaticModelBufferBindingResource> BufferBindings;
};

class FStaticModelRenderSnapshot
{
public:
    ~FStaticModelRenderSnapshot();
    FStaticModelRenderSnapshot(const FStaticModelRenderSnapshot&) = delete;
    FStaticModelRenderSnapshot& operator=(
        const FStaticModelRenderSnapshot&) = delete;

    [[nodiscard]] const Asset::FAssetId& GetRootAssetId() const noexcept;
    [[nodiscard]] const Asset::FAssetVersion& GetRootVersion() const noexcept;
    [[nodiscard]] Core::uint64 GetSnapshotGeneration() const noexcept;
    [[nodiscard]] const Core::TArray<FStaticModelRenderNode>&
        GetNodes() const noexcept;
    [[nodiscard]] const Core::TArray<FStaticModelRenderDraw>&
        GetDraws() const noexcept;
    [[nodiscard]] const Core::TArray<FStaticMeshAssetSnapshot>&
        GetMeshes() const noexcept;
    [[nodiscard]] const Core::TArray<FMaterialAssetSnapshot>&
        GetMaterials() const noexcept;
    [[nodiscard]] const Core::TArray<FShaderAssetSnapshot>&
        GetShaders() const noexcept;
    [[nodiscard]] const Core::TArray<FStaticModelTextureResource>&
        GetTextures() const noexcept;
    [[nodiscard]] const Core::TArray<FStaticModelMaterialResources>&
        GetMaterialResources() const noexcept;
    [[nodiscard]] const Core::TArray<FStaticModelDrawResources>&
        GetDrawResources() const noexcept;
    [[nodiscard]] const FStaticModelRealizationInspection&
        Inspect() const noexcept;

private:
    friend class FStaticModelRealizer;
    struct FImpl;
    explicit FStaticModelRenderSnapshot(Core::TUniquePtr<FImpl> Impl);
    Core::TUniquePtr<FImpl> Impl_;
};

class FStaticModelRealizer
{
public:
    [[nodiscard]] static RHI::ERHIResult Realize(
        const FStaticModelRealizationRequest& Request,
        Core::TSharedPtr<const FStaticModelRenderSnapshot>& OutSnapshot,
        FStaticModelRealizationInspection& OutInspection);
};

[[nodiscard]] const char* ToString(
    EStaticModelRealizationStage Stage) noexcept;

} // namespace Stoner::Renderer
