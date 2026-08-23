#include "Renderer/FStaticModelRealization.h"

#include "FStaticModelRealizationPlan.h"
#include "FStaticModelRealizationTransaction.h"
#include "Renderer/FKTX2TextureRealization.h"
#include "RHI/FRHIBufferDesc.h"
#include "RHI/FRHIBufferUploadDesc.h"
#include "RHI/FRHIPipelineLayoutDesc.h"
#include "RHI/FRHISamplerDesc.h"
#include "RHI/FRHITextureDesc.h"
#include "RHI/FRHITextureUploadDesc.h"
#include "RHI/IRHIBuffer.h"
#include "RHI/IRHIDescriptorSet.h"
#include "RHI/IRHIDevice.h"
#include "RHI/IRHIGraphicsPipeline.h"
#include "RHI/IRHIPipelineLayout.h"
#include "RHI/IRHISampler.h"
#include "RHI/IRHIShaderModule.h"
#include "RHI/IRHITexture.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <map>
#include <new>
#include <set>
#include <sstream>
#include <utility>

namespace Stoner::Renderer
{
namespace
{

using namespace Stoner::RHI;

std::atomic<Core::uint64> GSnapshotGeneration{1};
constexpr Core::uint64 StaticModelBindingBufferBytes = 512;

ERHIResult Failure(
    FStaticModelRealizationInspection& Inspection,
    EStaticModelRealizationStage Stage,
    ERHIResult Result,
    const char* Code,
    const Core::FString& Subject,
    const char* Reason)
{
    Inspection.Stage = Stage;
    if (Inspection.FirstFailure.Code.IsEmpty())
        Inspection.FirstFailure = {
            Stage, Result, Core::FString(Code), Subject,
            Core::FString(Reason)};
    return Result;
}

template <typename T>
void Track(
    Private::FStaticModelRealizationTransaction& Transaction,
    Core::FString StableId,
    const Core::TSharedPtr<T>& Resource)
{
    Transaction.Add({
        std::move(StableId),
        [Resource]()
        {
            if (Resource) (void)Resource->Invalidate();
        }});
}

bool Cancelled(const FStaticModelRealizationRequest& Request)
{
    return Request.RuntimeContext && Request.RuntimeContext->ShouldStop();
}

bool BuildLayout(
    const Core::TArray<FRHIShaderModuleDesc>& Modules,
    FRHIPipelineLayoutDesc& OutLayout)
{
    FRHIPipelineLayoutDesc Result;
    for (const auto& Module : Modules)
    {
        for (const auto& Required : Module.InterfaceMetadata.Bindings)
        {
            auto Found = std::find_if(
                Result.Bindings.begin(), Result.Bindings.end(),
                [&Required](const auto& Existing)
                {
                    return Existing.SetIndex == Required.SetIndex &&
                        Existing.BindingSlot == Required.BindingSlot;
                });
            if (Found == Result.Bindings.end())
                Result.Bindings.push_back({
                    Required.SetIndex, Required.BindingSlot,
                    Required.DescriptorType, Required.ArrayCount,
                    Required.Visibility});
            else if (Found->DescriptorType == Required.DescriptorType &&
                     Found->ArrayCount == Required.ArrayCount)
                Found->Visibility |= Required.Visibility;
            else
            {
                return false;
            }
        }
        for (const auto& Required : Module.InterfaceMetadata.ConstantRanges)
        {
            auto Found = std::find_if(
                Result.ConstantRanges.begin(), Result.ConstantRanges.end(),
                [&Required](const auto& Existing)
                {
                    return Existing.OffsetBytes == Required.OffsetBytes &&
                        Existing.SizeBytes == Required.SizeBytes;
                });
            if (Found == Result.ConstantRanges.end())
                Result.ConstantRanges.push_back(Required);
            else
                Found->Visibility |= Required.Visibility;
        }
    }
    if (Result.Bindings.empty())
        Result.Bindings.push_back({
            0, 0, ERHIDescriptorType::UniformBuffer, 1,
            ERHIShaderStageFlags::Vertex |
                ERHIShaderStageFlags::Fragment});
    std::sort(
        Result.Bindings.begin(), Result.Bindings.end(),
        [](const auto& Left, const auto& Right)
        {
            if (Left.SetIndex != Right.SetIndex)
                return Left.SetIndex < Right.SetIndex;
            return Left.BindingSlot < Right.BindingSlot;
        });
    if (!IsValidRHIPipelineLayoutDesc(Result)) return false;
    OutLayout = std::move(Result);
    return true;
}

const Asset::FShaderInterfaceBinding* FindNamedBinding(
    const Asset::FSelectedShaderProgram& Shader,
    const FRHIDescriptorBinding& Binding)
{
    const auto Found = std::find_if(
        Shader.InterfaceBindings.begin(), Shader.InterfaceBindings.end(),
        [&Binding](const auto& Candidate)
        {
            return Candidate.SetIndex == Binding.SetIndex &&
                Candidate.BindingIndex == Binding.BindingSlot;
        });
    return Found == Shader.InterfaceBindings.end() ? nullptr : &*Found;
}

const FMaterialAssetSnapshot::FTextureBinding* FindTextureBinding(
    const FMaterialAssetSnapshot& Material,
    const Core::FString& Name)
{
    const auto Found = std::lower_bound(
        Material.TextureBindings.begin(), Material.TextureBindings.end(), Name,
        [](const auto& Candidate, const Core::FString& Key)
        {
            return Candidate.ParameterName < Key;
        });
    return Found == Material.TextureBindings.end() ||
            Found->ParameterName != Name
        ? nullptr : &*Found;
}

bool TryGetMaterialFallbackPixel(
    const Core::FString& Name,
    std::array<Core::uint8, 4>& OutPixel) noexcept
{
    if (Name == Core::FString("BaseColorTexture") ||
        Name == Core::FString("MetallicRoughnessTexture") ||
        Name == Core::FString("OcclusionTexture") ||
        Name == Core::FString("EmissiveTexture"))
    {
        OutPixel = {255, 255, 255, 255};
        return true;
    }
    if (Name == Core::FString("NormalTexture"))
    {
        OutPixel = {128, 128, 255, 255};
        return true;
    }
    return false;
}

const Private::FStaticModelPlannedMaterial* FindShaderMaterial(
    const Private::FStaticModelRealizationPlan& Plan,
    const Asset::FAssetId& ShaderId)
{
    const auto Found = std::find_if(
        Plan.Materials.begin(), Plan.Materials.end(),
        [&ShaderId](const auto& Material)
        {
            return Material.SelectedShader.ShaderId == ShaderId;
        });
    return Found == Plan.Materials.end() ? nullptr : &*Found;
}

const FTextureTargetProfile* FindTextureTargetProfile(
    const FStaticModelRealizationRequest& Request,
    const Asset::FAssetId& TextureId)
{
    if (Request.TextureTargetProfiles.empty())
        return &Request.TextureTargetProfile;
    const auto Found = std::find_if(
        Request.TextureTargetProfiles.begin(),
        Request.TextureTargetProfiles.end(),
        [&TextureId](const auto& Record)
        { return Record.AssetId == TextureId; });
    return Found == Request.TextureTargetProfiles.end()
        ? nullptr : &Found->Profile;
}

} // namespace

struct FStaticModelRenderSnapshot::FImpl
{
    Asset::FAssetId RootAssetId;
    Asset::FAssetVersion RootVersion;
    Core::uint64 SnapshotGeneration = 0;
    Core::TArray<FStaticModelRenderNode> Nodes;
    Core::TArray<FStaticModelRenderDraw> Draws;
    Core::TArray<FStaticMeshAssetSnapshot> Meshes;
    Core::TArray<FMaterialAssetSnapshot> Materials;
    Core::TArray<FShaderAssetSnapshot> Shaders;
    Core::TArray<FStaticModelTextureResource> Textures;
    Core::TArray<FStaticModelMaterialResources> MaterialResources;
    Core::TArray<FStaticModelDrawResources> DrawResources;
    Core::TArray<Core::TSharedPtr<RHI::IRHIShaderModule>> ShaderModules;
    Core::TArray<Core::TSharedPtr<RHI::IRHIBuffer>> UniformBuffers;
    Core::TArray<Core::TSharedPtr<RHI::IRHISampler>> Samplers;
    Core::TArray<Core::TSharedPtr<RHI::IRHITexture>> FallbackTextures;
    Core::TArray<Core::TSharedPtr<RHI::IRHIPipelineLayout>> Layouts;
    Core::TArray<Core::TSharedPtr<RHI::IRHIDescriptorSet>> DescriptorSets;
    Core::TArray<Core::TSharedPtr<RHI::IRHIGraphicsPipeline>> Pipelines;
    Core::TArray<Private::FStaticModelOwnedResource> OwnedResources;
    FStaticModelRealizationInspection Inspection;
};

bool FStaticModelRealizationLimits::IsValid() const noexcept
{
    return MaxNodes > 0 && MaxDraws > 0 && MaxMeshes > 0 &&
        MaxMaterials > 0 && MaxShaders > 0 && MaxTextures > 0 &&
        MaxDescriptors > 0 && MaxPipelines > 0 &&
        MaxAggregateResourceBytes > 0;
}

Core::FString FStaticModelRealizationInspection::Dump() const
{
    std::ostringstream Out;
    Out << "stage=" << ToString(Stage)
        << " generation=" << SnapshotGeneration
        << " nodes=" << NodeCount
        << " draws=" << DrawCount
        << " meshes=" << UniqueMeshCount
        << " materials=" << UniqueMaterialCount
        << " shaders=" << UniqueShaderCount
        << " textures=" << UniqueTextureCount
        << " descriptors=" << DescriptorSetCount
        << " pipelines=" << PipelineCount
        << " created=" << CreatedResourceCount
        << " released=" << ReleasedResourceCount
        << " committed=" << (bCommitted ? 1 : 0)
        << " rolledBack=" << (bRolledBack ? 1 : 0);
    if (!FirstFailure.Code.IsEmpty())
        Out << " failure=" << FirstFailure.Code.CStr()
            << " subject=" << FirstFailure.Subject.CStr();
    return Core::FString(Out.str());
}

FStaticModelRenderSnapshot::FStaticModelRenderSnapshot(
    Core::TUniquePtr<FImpl> Impl)
    : Impl_(std::move(Impl))
{
}

FStaticModelRenderSnapshot::~FStaticModelRenderSnapshot()
{
    if (!Impl_) return;
    Private::ReleaseStaticModelResources(
        Impl_->OwnedResources, Impl_->Inspection);
    Impl_->Inspection.Stage = EStaticModelRealizationStage::Released;
}

const Asset::FAssetId& FStaticModelRenderSnapshot::GetRootAssetId() const noexcept
{
    return Impl_->RootAssetId;
}

const Asset::FAssetVersion&
FStaticModelRenderSnapshot::GetRootVersion() const noexcept
{
    return Impl_->RootVersion;
}

Core::uint64 FStaticModelRenderSnapshot::GetSnapshotGeneration() const noexcept
{
    return Impl_->SnapshotGeneration;
}

const Core::TArray<FStaticModelRenderNode>&
FStaticModelRenderSnapshot::GetNodes() const noexcept
{
    return Impl_->Nodes;
}

const Core::TArray<FStaticModelRenderDraw>&
FStaticModelRenderSnapshot::GetDraws() const noexcept
{
    return Impl_->Draws;
}

const Core::TArray<FStaticMeshAssetSnapshot>&
FStaticModelRenderSnapshot::GetMeshes() const noexcept
{
    return Impl_->Meshes;
}

const Core::TArray<FMaterialAssetSnapshot>&
FStaticModelRenderSnapshot::GetMaterials() const noexcept
{
    return Impl_->Materials;
}

const Core::TArray<FShaderAssetSnapshot>&
FStaticModelRenderSnapshot::GetShaders() const noexcept
{
    return Impl_->Shaders;
}

const Core::TArray<FStaticModelTextureResource>&
FStaticModelRenderSnapshot::GetTextures() const noexcept
{
    return Impl_->Textures;
}

const Core::TArray<FStaticModelMaterialResources>&
FStaticModelRenderSnapshot::GetMaterialResources() const noexcept
{
    return Impl_->MaterialResources;
}

const Core::TArray<FStaticModelDrawResources>&
FStaticModelRenderSnapshot::GetDrawResources() const noexcept
{
    return Impl_->DrawResources;
}

const FStaticModelRealizationInspection&
FStaticModelRenderSnapshot::Inspect() const noexcept
{
    return Impl_->Inspection;
}

RHI::ERHIResult FStaticModelRealizer::Realize(
    const FStaticModelRealizationRequest& Request,
    Core::TSharedPtr<const FStaticModelRenderSnapshot>& OutSnapshot,
    FStaticModelRealizationInspection& OutInspection)
{
    OutSnapshot.reset();
    OutInspection = {};
    Private::FStaticModelRealizationPlan Plan;
    FStaticModelRealizationDiagnostic PlanFailure;
    ERHIResult Result = Private::BuildStaticModelRealizationPlan(
        Request, Plan, PlanFailure);
    if (Result != ERHIResult::Success)
    {
        OutInspection.FirstFailure = std::move(PlanFailure);
        OutInspection.Stage = OutInspection.FirstFailure.Stage;
        return Result;
    }
    OutInspection.Stage = EStaticModelRealizationStage::Plan;
    OutInspection.NodeCount = static_cast<Core::uint32>(Plan.Nodes.size());
    OutInspection.DrawCount = static_cast<Core::uint32>(Plan.Draws.size());
    OutInspection.UniqueMeshCount = static_cast<Core::uint32>(Plan.Meshes.size());
    OutInspection.UniqueMaterialCount =
        static_cast<Core::uint32>(Plan.Materials.size());
    OutInspection.UniqueTextureCount =
        static_cast<Core::uint32>(Plan.Textures.size());

    std::set<Asset::FAssetId> ShaderIds;
    for (const auto& Material : Plan.Materials)
        ShaderIds.insert(Material.SelectedShader.ShaderId);
    std::map<Asset::FAssetId, Core::uint32> ShaderIndices;
    std::map<Asset::FAssetId, FRHIPipelineLayoutDesc> LayoutDescs;
    Core::TArray<FShaderAssetSnapshot> CpuShaderSnapshots;
    for (const Asset::FAssetId& ShaderId : ShaderIds)
    {
        const auto* Material = FindShaderMaterial(Plan, ShaderId);
        FShaderAssetSnapshot ShaderSnapshot;
        if (!Material || ConvertShaderAsset(
                {Material ? &Material->SelectedShader : nullptr},
                ShaderSnapshot) != EMaterialResult::Success)
            return Failure(OutInspection, EStaticModelRealizationStage::Shader,
                ERHIResult::InvalidState, "renderer.static-model.shader-convert",
                ShaderId.ToString(), "shader snapshot conversion failed");
        FRHIPipelineLayoutDesc LayoutDesc;
        if (!BuildLayout(ShaderSnapshot.ModuleDescriptions, LayoutDesc))
            return Failure(OutInspection, EStaticModelRealizationStage::Shader,
                ERHIResult::InvalidState, "renderer.static-model.layout-interface",
                ShaderId.ToString(),
                "shader stages declare an incompatible descriptor layout");
        ShaderIndices.emplace(
            ShaderId, static_cast<Core::uint32>(CpuShaderSnapshots.size()));
        LayoutDescs.emplace(ShaderId, std::move(LayoutDesc));
        CpuShaderSnapshots.push_back(std::move(ShaderSnapshot));
    }

    Core::TArray<FMaterialAssetSnapshot> CpuMaterialSnapshots;
    CpuMaterialSnapshots.reserve(Plan.Materials.size());
    Core::uint64 DescriptorCount = 0;
    for (const auto& Planned : Plan.Materials)
    {
        const auto ShaderIndex = ShaderIndices.at(
            Planned.SelectedShader.ShaderId);
        FMaterialAssetSnapshot Snapshot;
        if (ConvertMaterialAsset(
                {&Planned.Resolved, &CpuShaderSnapshots[ShaderIndex]},
                Snapshot) != EMaterialResult::Success)
            return Failure(OutInspection, EStaticModelRealizationStage::Material,
                ERHIResult::InvalidState, "renderer.static-model.material-convert",
                Planned.AssetId.ToString(),
                "material snapshot conversion failed");
        std::set<Core::uint32> Sets;
        for (const auto& Binding :
             LayoutDescs.at(Planned.SelectedShader.ShaderId).Bindings)
        {
            Sets.insert(Binding.SetIndex);
            if (Binding.DescriptorType == ERHIDescriptorType::SampledTexture ||
                Binding.DescriptorType == ERHIDescriptorType::Sampler ||
                Binding.DescriptorType ==
                    ERHIDescriptorType::CombinedTextureSampler)
            {
                const auto* Named = FindNamedBinding(
                    Planned.SelectedShader, Binding);
                std::array<Core::uint8, 4> FallbackPixel{};
                if (!Named || Named->Name.IsEmpty() ||
                    Binding.ArrayCount != 1 ||
                    (!FindTextureBinding(Snapshot, Named->Name) &&
                     !TryGetMaterialFallbackPixel(
                         Named->Name, FallbackPixel)))
                    return Failure(OutInspection,
                        EStaticModelRealizationStage::Descriptor,
                        ERHIResult::InvalidState,
                        "renderer.static-model.texture-interface",
                        Planned.AssetId.ToString(),
                        "shader texture binding does not map to one material parameter");
            }
        }
        DescriptorCount += Sets.size();
        if (DescriptorCount > Request.Limits.MaxDescriptors)
            return Failure(OutInspection,
                EStaticModelRealizationStage::Descriptor,
                ERHIResult::Unavailable,
                "renderer.static-model.descriptor-limit",
                Request.Model->GetDesc().Id.ToString(),
                "descriptor count exceeds realization limit");
        CpuMaterialSnapshots.push_back(std::move(Snapshot));
    }
    if (Plan.Materials.size() > Request.Limits.MaxPipelines)
        return Failure(OutInspection, EStaticModelRealizationStage::Pipeline,
            ERHIResult::Unavailable, "renderer.static-model.pipeline-limit",
            Request.Model->GetDesc().Id.ToString(),
            "pipeline count exceeds realization limit");

    Private::FStaticModelRealizationTransaction Transaction(OutInspection);
    Core::TUniquePtr<FStaticModelRenderSnapshot::FImpl> Impl;
    try
    {
        Impl = Core::MakeUnique<FStaticModelRenderSnapshot::FImpl>();
        Impl->RootAssetId = Request.Model->GetDesc().Id;
        Impl->RootVersion = Request.Model->GetDesc().Version;
        Impl->Nodes = Plan.Nodes;
        Impl->Meshes.reserve(Plan.Meshes.size());
        Impl->Textures.reserve(Plan.Textures.size());
        Impl->Materials.reserve(Plan.Materials.size());
        Impl->MaterialResources.reserve(Plan.Materials.size());
    }
    catch (const std::bad_alloc&)
    {
        return Failure(OutInspection, EStaticModelRealizationStage::Plan,
            ERHIResult::Unavailable, "renderer.static-model.allocation",
            Request.Model->GetDesc().Id.ToString(),
            "snapshot planning allocation failed");
    }

    for (const auto& Mesh : Plan.Meshes)
    {
        if (Cancelled(Request))
            return Failure(OutInspection, EStaticModelRealizationStage::Mesh,
                ERHIResult::Failed, "renderer.static-model.cancelled",
                Mesh->GetDesc().Id.ToString(),
                "realization was cancelled during mesh creation");
        const auto Realized = FStaticMeshRealizer::Realize(
            {Request.Device, Mesh, Request.MeshProfile});
        if (!Realized.Succeeded())
            return Failure(OutInspection, EStaticModelRealizationStage::Mesh,
                Realized.Result, "renderer.static-model.mesh-realization",
                Mesh->GetDesc().Id.ToString(),
                "static mesh realization failed");
        Track(Transaction,
            Core::FString("mesh.vertex:" + Mesh->GetDesc().Id.ToString().ToStdString()),
            Realized.Snapshot->VertexBuffer);
        Track(Transaction,
            Core::FString("mesh.index:" + Mesh->GetDesc().Id.ToString().ToStdString()),
            Realized.Snapshot->IndexBuffer);
        Impl->Meshes.push_back(*Realized.Snapshot);
    }

    for (const auto& Texture : Plan.Textures)
    {
        if (Cancelled(Request))
            return Failure(OutInspection, EStaticModelRealizationStage::Texture,
                ERHIResult::Failed, "renderer.static-model.cancelled",
                Texture->GetId().ToString(),
                "realization was cancelled during texture creation");
        const FTextureTargetProfile* TextureProfile =
            FindTextureTargetProfile(Request, Texture->GetId());
        if (!TextureProfile)
            return Failure(OutInspection, EStaticModelRealizationStage::Texture,
                ERHIResult::InvalidState,
                "renderer.static-model.texture-profile",
                Texture->GetId().ToString(),
                "texture target profile is missing");
        const auto Realized = FKTX2TextureRealizer::Realize({
            Request.Device, Texture, *TextureProfile});
        if (!Realized.Succeeded())
            return Failure(OutInspection, EStaticModelRealizationStage::Texture,
                Realized.Result, Realized.Diagnostic.Code.CStr(),
                Texture->GetId().ToString(), Realized.Diagnostic.Reason.CStr());
        Track(Transaction,
            Core::FString("texture:" + Texture->GetId().ToString().ToStdString()),
            Realized.Texture);
        const auto Version = std::find_if(
            Request.Dependencies.Versions.begin(),
            Request.Dependencies.Versions.end(),
            [&Texture](const auto& Candidate)
            {
                return Candidate.AssetId == Texture->GetId();
            });
        Impl->Textures.push_back({
            Texture->GetId(), Version->Version, Realized.Texture});
    }

    std::map<Asset::FAssetId, Core::TArray<Core::TSharedPtr<IRHIShaderModule>>>
        ShaderModules;
    std::map<Asset::FAssetId, Core::TSharedPtr<IRHIPipelineLayout>> Layouts;
    for (const Asset::FAssetId& ShaderId : ShaderIds)
    {
        const auto* Material = FindShaderMaterial(Plan, ShaderId);
        if (!Material)
            return Failure(OutInspection, EStaticModelRealizationStage::Shader,
                ERHIResult::InvalidState, "renderer.static-model.shader-plan",
                ShaderId.ToString(), "planned shader has no material owner");
        FShaderAssetSnapshot ShaderSnapshot =
            CpuShaderSnapshots[ShaderIndices.at(ShaderId)];
        Core::TArray<Core::TSharedPtr<IRHIShaderModule>> Modules;
        for (const auto& Desc : ShaderSnapshot.ModuleDescriptions)
        {
            auto Created = Request.Device->CreateShaderModule(Desc);
            if (!Created.Succeeded())
                return Failure(OutInspection, EStaticModelRealizationStage::Shader,
                    Created.Result, "renderer.static-model.shader-create",
                    ShaderId.ToString(), "RHI shader module creation failed");
            Track(Transaction,
                Core::FString("shader:" + Desc.Payload.PayloadIdentity.ToStdString()),
                Created.Object);
            Impl->ShaderModules.push_back(Created.Object);
            Modules.push_back(std::move(Created.Object));
        }
        auto Layout = Request.Device->CreatePipelineLayout(
            LayoutDescs.at(ShaderId));
        if (!Layout.Succeeded())
            return Failure(OutInspection, EStaticModelRealizationStage::Shader,
                Layout.Result, "renderer.static-model.layout-create",
                ShaderId.ToString(), "RHI pipeline layout creation failed");
        Track(Transaction,
            Core::FString("layout:" + ShaderId.ToString().ToStdString()),
            Layout.Object);
        Impl->Layouts.push_back(Layout.Object);
        ShaderModules.emplace(ShaderId, std::move(Modules));
        Layouts.emplace(ShaderId, Layout.Object);
        Impl->Shaders.push_back(std::move(ShaderSnapshot));
    }
    OutInspection.UniqueShaderCount =
        static_cast<Core::uint32>(Impl->Shaders.size());

    std::map<Asset::FAssetId, Core::uint32> TextureIndices;
    for (Core::uint32 Index = 0; Index < Plan.Textures.size(); ++Index)
        TextureIndices.emplace(Plan.Textures[Index]->GetId(), Index);

    std::map<Core::FString, Core::TSharedPtr<IRHITexture>> FallbackTextures;
    const auto GetFallbackTexture = [&](const Core::FString& Name)
        -> Core::TSharedPtr<IRHITexture>
    {
        const auto Existing = FallbackTextures.find(Name);
        if (Existing != FallbackTextures.end()) return Existing->second;
        std::array<Core::uint8, 4> Pixel{};
        if (!TryGetMaterialFallbackPixel(Name, Pixel)) return nullptr;
        FRHITextureDesc Desc;
        Desc.Format = ERHIFormat::R8G8B8A8_UNorm;
        Desc.Usage = ERHITextureUsage::Sampled |
            ERHITextureUsage::CopyDestination;
        auto Created = Request.Device->CreateTexture(Desc);
        if (!Created.Succeeded()) return nullptr;
        FRHITextureUploadDesc Upload;
        Upload.Width = 1;
        Upload.Height = 1;
        Upload.RowPitchBytes = Pixel.size();
        Upload.Data = Pixel.data();
        Upload.DataSizeBytes = Pixel.size();
        if (Request.Device->UploadTexture(Created.Object, Upload) !=
            ERHIResult::Success)
        {
            (void)Created.Object->Invalidate();
            return nullptr;
        }
        Track(Transaction,
            Core::FString("texture-fallback:" + Name.ToStdString()),
            Created.Object);
        Impl->FallbackTextures.push_back(Created.Object);
        FallbackTextures.emplace(Name, Created.Object);
        return Created.Object;
    };

    for (Core::uint32 MaterialIndex = 0;
         MaterialIndex < Plan.Materials.size(); ++MaterialIndex)
    {
        if (OutInspection.PipelineCount >= Request.Limits.MaxPipelines)
            return Failure(OutInspection,
                EStaticModelRealizationStage::Pipeline,
                ERHIResult::Unavailable,
                "renderer.static-model.pipeline-limit",
                Request.Model->GetDesc().Id.ToString(),
                "pipeline count exceeds realization limit");
        const auto& Planned = Plan.Materials[MaterialIndex];
        FMaterialAssetSnapshot MaterialSnapshot =
            CpuMaterialSnapshots[MaterialIndex];

        const auto Layout = Layouts.at(Planned.SelectedShader.ShaderId);
        std::set<Core::uint32> Sets;
        for (const auto& Binding : Layout->GetDesc().Bindings)
            Sets.insert(Binding.SetIndex);
        std::map<Core::uint32, Core::TSharedPtr<IRHIDescriptorSet>> DescriptorSets;
        Core::TArray<FStaticModelBufferBindingResource> BufferBindings;
        for (Core::uint32 Set : Sets)
        {
            if (OutInspection.DescriptorSetCount >= Request.Limits.MaxDescriptors)
                return Failure(OutInspection,
                    EStaticModelRealizationStage::Descriptor,
                    ERHIResult::Unavailable,
                    "renderer.static-model.descriptor-limit",
                    Planned.AssetId.ToString(),
                    "descriptor count exceeds realization limit");
            auto Descriptor = Request.Device->CreateDescriptorSet(Layout, Set);
            if (!Descriptor.Succeeded())
                return Failure(OutInspection,
                    EStaticModelRealizationStage::Descriptor,
                    Descriptor.Result,
                    "renderer.static-model.descriptor-create",
                    Planned.AssetId.ToString(),
                    "RHI descriptor set creation failed");
            Track(Transaction,
                Core::FString("descriptor:" + Planned.AssetId.ToString().ToStdString() +
                    ":" + std::to_string(Set)), Descriptor.Object);
            DescriptorSets.emplace(Set, Descriptor.Object);
            Impl->DescriptorSets.push_back(Descriptor.Object);
            ++OutInspection.DescriptorSetCount;
        }

        for (const auto& Binding : Layout->GetDesc().Bindings)
        {
            auto Descriptor = DescriptorSets.at(Binding.SetIndex);
            for (Core::uint32 Element = 0;
                 Element < Binding.ArrayCount; ++Element)
            {
                if (Binding.DescriptorType == ERHIDescriptorType::UniformBuffer ||
                    Binding.DescriptorType == ERHIDescriptorType::StorageBuffer)
                {
                    FRHIBufferDesc BufferDesc;
                    BufferDesc.SizeInBytes = StaticModelBindingBufferBytes;
                    BufferDesc.Usage = Binding.DescriptorType ==
                            ERHIDescriptorType::UniformBuffer
                        ? ERHIBufferUsage::Uniform |
                            ERHIBufferUsage::CopyDestination
                        : ERHIBufferUsage::Storage |
                            ERHIBufferUsage::CopyDestination;
                    BufferDesc.MemoryAccess = ERHIMemoryAccess::DeviceLocal;
                    auto Buffer = Request.Device->CreateBuffer(BufferDesc);
                    if (!Buffer.Succeeded())
                        return Failure(OutInspection,
                            EStaticModelRealizationStage::Descriptor,
                            Buffer.Result,
                            "renderer.static-model.buffer-binding",
                            Planned.AssetId.ToString(),
                            "material buffer allocation failed");
                    Track(Transaction,
                        Core::FString("material-buffer:" +
                            Planned.AssetId.ToString().ToStdString() + ":" +
                            std::to_string(Binding.SetIndex) + ":" +
                            std::to_string(Binding.BindingSlot)), Buffer.Object);
                    Impl->UniformBuffers.push_back(Buffer.Object);
                    BufferBindings.push_back({
                        Binding.SetIndex, Binding.BindingSlot, Buffer.Object});
                    Core::TArray<Core::uint8> Zeroes(
                        StaticModelBindingBufferBytes, 0);
                    if (Request.Device->UploadBuffer(
                            Buffer.Object,
                            {0, Zeroes.data(), Zeroes.size()}) !=
                            ERHIResult::Success ||
                        Descriptor->UpdateBuffer(
                            Binding.BindingSlot, Element,
                            Buffer.Object) != ERHIResult::Success)
                        return Failure(OutInspection,
                            EStaticModelRealizationStage::Descriptor,
                            ERHIResult::Failed,
                            "renderer.static-model.buffer-binding",
                            Planned.AssetId.ToString(),
                            "material buffer upload or binding failed");
                }
                else if (Binding.DescriptorType ==
                             ERHIDescriptorType::SampledTexture ||
                         Binding.DescriptorType ==
                             ERHIDescriptorType::Sampler ||
                         Binding.DescriptorType ==
                             ERHIDescriptorType::CombinedTextureSampler)
                {
                    const auto* Named = FindNamedBinding(
                        Planned.SelectedShader, Binding);
                    const auto* MaterialBinding = Named
                        ? FindTextureBinding(MaterialSnapshot, Named->Name)
                        : nullptr;
                    const auto Texture = MaterialBinding
                        ? Impl->Textures[
                            TextureIndices.at(MaterialBinding->TextureId)].Texture
                        : (Named ? GetFallbackTexture(Named->Name) : nullptr);
                    if (!Texture)
                        return Failure(OutInspection,
                            EStaticModelRealizationStage::Descriptor,
                            ERHIResult::InvalidState,
                            "renderer.static-model.texture-binding-missing",
                            Planned.AssetId.ToString(),
                            "validated texture binding or semantic fallback became unavailable");
                    if (Binding.DescriptorType ==
                        ERHIDescriptorType::SampledTexture)
                    {
                        if (Descriptor->UpdateTexture(
                                Binding.BindingSlot, Element,
                                Texture) !=
                            ERHIResult::Success)
                            return Failure(OutInspection,
                                EStaticModelRealizationStage::Descriptor,
                                ERHIResult::Failed,
                                "renderer.static-model.texture-binding",
                                Planned.AssetId.ToString(),
                                "sampled texture descriptor update failed");
                    }
                    else
                    {
                        auto Sampler = Request.Device->CreateSampler(
                            MaterialBinding
                                ? MaterialBinding->Sampler
                                : FRHISamplerDesc{});
                        if (!Sampler.Succeeded())
                            return Failure(OutInspection,
                                EStaticModelRealizationStage::Descriptor,
                                Sampler.Result,
                                "renderer.static-model.combined-binding",
                                Planned.AssetId.ToString(),
                                "material sampler allocation failed");
                        Track(Transaction,
                            Core::FString("sampler:" +
                                Planned.AssetId.ToString().ToStdString() + ":" +
                                std::to_string(Binding.BindingSlot)), Sampler.Object);
                        Impl->Samplers.push_back(Sampler.Object);
                        const ERHIResult UpdateResult =
                            Binding.DescriptorType == ERHIDescriptorType::Sampler
                            ? Descriptor->UpdateSampler(
                                Binding.BindingSlot, Element, Sampler.Object)
                            : Descriptor->UpdateCombinedTextureSampler(
                                Binding.BindingSlot, Element,
                                Texture,
                                Sampler.Object);
                        if (UpdateResult != ERHIResult::Success)
                            return Failure(OutInspection,
                                EStaticModelRealizationStage::Descriptor,
                                ERHIResult::Failed,
                                "renderer.static-model.combined-binding",
                                Planned.AssetId.ToString(),
                                "combined texture sampler update failed");
                    }
                }
            }
        }

        FRHIGraphicsPipelineDesc PipelineDesc;
        PipelineDesc.ShaderModules =
            ShaderModules.at(Planned.SelectedShader.ShaderId);
        PipelineDesc.PipelineLayout = Layout;
        PipelineDesc.VertexInput = Impl->Meshes.front().VertexInput;
        PipelineDesc.Rasterizer.CullMode =
            MaterialSnapshot.Material.GetDesc().RenderState.bTwoSided
            ? ERHICullMode::None : ERHICullMode::Back;
        PipelineDesc.DepthStencil.bDepthTestEnabled =
            MaterialSnapshot.Material.GetDesc().RenderState.bDepthTest;
        PipelineDesc.DepthStencil.bDepthWriteEnabled =
            MaterialSnapshot.Material.GetDesc().RenderState.bDepthWrite;
        PipelineDesc.DepthStencil.DepthCompare = ERHICompareOp::LessEqual;
        const auto BlendMode =
            MaterialSnapshot.Material.GetDesc().BlendMode;
        PipelineDesc.Blend.bEnabled =
            BlendMode == EMaterialBlendMode::Translucent ||
            BlendMode == EMaterialBlendMode::Additive;
        if (BlendMode == EMaterialBlendMode::Translucent)
        {
            PipelineDesc.Blend.SourceColor = ERHIBlendFactor::SourceAlpha;
            PipelineDesc.Blend.DestinationColor =
                ERHIBlendFactor::OneMinusSourceAlpha;
        }
        else if (BlendMode == EMaterialBlendMode::Additive)
        {
            PipelineDesc.Blend.SourceColor = ERHIBlendFactor::One;
            PipelineDesc.Blend.DestinationColor = ERHIBlendFactor::One;
        }
        PipelineDesc.RenderTargets = Request.RenderTargets;
        PipelineDesc.RuntimeMode = Request.Device->GetRuntimeSnapshot().ObjectMode;
        PipelineDesc.CompatibilitySummary = Core::FString(
            "static-model:" + Planned.AssetId.ToString().ToStdString());
        auto Pipeline = Request.Device->CreateGraphicsPipeline(PipelineDesc);
        if (!Pipeline.Succeeded())
            return Failure(OutInspection, EStaticModelRealizationStage::Pipeline,
                Pipeline.Result, "renderer.static-model.pipeline-create",
                Planned.AssetId.ToString(),
                "RHI graphics pipeline creation failed");
        Track(Transaction,
            Core::FString("pipeline:" + Planned.AssetId.ToString().ToStdString()),
            Pipeline.Object);
        Impl->Pipelines.push_back(Pipeline.Object);
        Impl->Materials.push_back(std::move(MaterialSnapshot));
        FStaticModelMaterialResources Resources;
        Resources.PipelineLayout = Layout;
        for (const auto& [Set, Descriptor] : DescriptorSets)
        {
            (void)Set;
            Resources.DescriptorSets.push_back(Descriptor);
        }
        Resources.BufferBindings = std::move(BufferBindings);
        Resources.Pipeline = Pipeline.Object;
        Impl->MaterialResources.push_back(std::move(Resources));
        ++OutInspection.PipelineCount;
    }

    Impl->Draws.reserve(Plan.Draws.size());
    for (const auto& Draw : Plan.Draws)
        Impl->Draws.push_back({
            Draw.NodePlanIndex, Draw.MeshPlanIndex, Draw.SectionIndex,
            Draw.MaterialPlanIndex, Draw.MaterialPlanIndex,
            Draw.StableKey, Draw.Bounds});

    Impl->DrawResources.reserve(Impl->Draws.size());
    Core::TArray<Core::uint32> MaterialUseCounts(
        Impl->MaterialResources.size(), 0);
    for (const auto& Draw : Impl->Draws)
    {
        auto& UseCount = MaterialUseCounts[Draw.MaterialIndex];
        const auto& Material = Impl->MaterialResources[Draw.MaterialIndex];
        FStaticModelDrawResources DrawResources;
        DrawResources.DescriptorSets = Material.DescriptorSets;
        DrawResources.BufferBindings = Material.BufferBindings;
        if (UseCount++ != 0)
        {
            const auto& Planned = Plan.Materials[Draw.MaterialIndex];
            const auto& MaterialSnapshot = Impl->Materials[Draw.MaterialIndex];
            const auto Layout = Material.PipelineLayout;
            const Core::uint32 DrawSet = 1;
            const bool bHasDrawSet = std::any_of(
                Layout->GetDesc().Bindings.begin(),
                Layout->GetDesc().Bindings.end(),
                [DrawSet](const auto& Binding)
                { return Binding.SetIndex == DrawSet; });
            if (bHasDrawSet)
            {
                if (OutInspection.DescriptorSetCount >=
                    Request.Limits.MaxDescriptors)
                    return Failure(OutInspection,
                        EStaticModelRealizationStage::Descriptor,
                        ERHIResult::Unavailable,
                        "renderer.static-model.descriptor-limit",
                        Draw.StableKey,
                        "per-draw descriptor count exceeds realization limit");
                auto Descriptor = Request.Device->CreateDescriptorSet(
                    Layout, DrawSet);
                if (!Descriptor.Succeeded())
                    return Failure(OutInspection,
                        EStaticModelRealizationStage::Descriptor,
                        Descriptor.Result,
                        "renderer.static-model.draw-descriptor-create",
                        Draw.StableKey,
                        "per-draw descriptor set creation failed");
                Track(Transaction,
                    Core::FString("draw-descriptor:" +
                        Draw.StableKey.ToStdString()), Descriptor.Object);
                Impl->DescriptorSets.push_back(Descriptor.Object);
                ++OutInspection.DescriptorSetCount;

                std::erase_if(DrawResources.BufferBindings,
                    [DrawSet](const auto& Binding)
                    { return Binding.SetIndex == DrawSet; });
                for (const auto& Binding : Layout->GetDesc().Bindings)
                {
                    if (Binding.SetIndex != DrawSet) continue;
                    for (Core::uint32 Element = 0;
                         Element < Binding.ArrayCount; ++Element)
                    {
                        if (Binding.DescriptorType ==
                                ERHIDescriptorType::UniformBuffer ||
                            Binding.DescriptorType ==
                                ERHIDescriptorType::StorageBuffer)
                        {
                            FRHIBufferDesc BufferDesc;
                            BufferDesc.SizeInBytes =
                                StaticModelBindingBufferBytes;
                            BufferDesc.Usage = Binding.DescriptorType ==
                                    ERHIDescriptorType::UniformBuffer
                                ? ERHIBufferUsage::Uniform |
                                    ERHIBufferUsage::CopyDestination
                                : ERHIBufferUsage::Storage |
                                    ERHIBufferUsage::CopyDestination;
                            BufferDesc.MemoryAccess =
                                ERHIMemoryAccess::DeviceLocal;
                            auto Buffer = Request.Device->CreateBuffer(BufferDesc);
                            if (!Buffer.Succeeded())
                                return Failure(OutInspection,
                                    EStaticModelRealizationStage::Descriptor,
                                    Buffer.Result,
                                    "renderer.static-model.draw-buffer-create",
                                    Draw.StableKey,
                                    "per-draw buffer allocation failed");
                            Track(Transaction,
                                Core::FString("draw-buffer:" +
                                    Draw.StableKey.ToStdString() + ":" +
                                    std::to_string(Binding.BindingSlot)),
                                Buffer.Object);
                            Impl->UniformBuffers.push_back(Buffer.Object);
                            Core::TArray<Core::uint8> Zeroes(
                                StaticModelBindingBufferBytes, 0);
                            if (Request.Device->UploadBuffer(
                                    Buffer.Object,
                                    {0, Zeroes.data(), Zeroes.size()}) !=
                                    ERHIResult::Success ||
                                Descriptor.Object->UpdateBuffer(
                                    Binding.BindingSlot, Element,
                                    Buffer.Object) != ERHIResult::Success)
                                return Failure(OutInspection,
                                    EStaticModelRealizationStage::Descriptor,
                                    ERHIResult::Failed,
                                    "renderer.static-model.draw-buffer-bind",
                                    Draw.StableKey,
                                    "per-draw buffer upload or binding failed");
                            DrawResources.BufferBindings.push_back({
                                Binding.SetIndex, Binding.BindingSlot,
                                Buffer.Object});
                        }
                        else if (Binding.DescriptorType ==
                                     ERHIDescriptorType::SampledTexture ||
                                 Binding.DescriptorType ==
                                     ERHIDescriptorType::Sampler ||
                                 Binding.DescriptorType ==
                                     ERHIDescriptorType::CombinedTextureSampler)
                        {
                            const auto* Named = FindNamedBinding(
                                Planned.SelectedShader, Binding);
                            const auto* MaterialBinding = Named
                                ? FindTextureBinding(
                                    MaterialSnapshot, Named->Name)
                                : nullptr;
                            const auto Texture = MaterialBinding
                                ? Impl->Textures[TextureIndices.at(
                                    MaterialBinding->TextureId)].Texture
                                : (Named
                                    ? GetFallbackTexture(Named->Name)
                                    : nullptr);
                            if (!Texture)
                                return Failure(OutInspection,
                                    EStaticModelRealizationStage::Descriptor,
                                    ERHIResult::InvalidState,
                                    "renderer.static-model.draw-texture-missing",
                                    Draw.StableKey,
                                    "per-draw texture binding or semantic fallback is unavailable");
                            if (Binding.DescriptorType ==
                                ERHIDescriptorType::SampledTexture)
                            {
                                if (Descriptor.Object->UpdateTexture(
                                        Binding.BindingSlot, Element,
                                        Texture) !=
                                    ERHIResult::Success)
                                    return Failure(OutInspection,
                                        EStaticModelRealizationStage::Descriptor,
                                        ERHIResult::Failed,
                                        "renderer.static-model.draw-texture-bind",
                                        Draw.StableKey,
                                        "per-draw texture update failed");
                            }
                            else
                            {
                                auto Sampler = Request.Device->CreateSampler(
                                    MaterialBinding
                                        ? MaterialBinding->Sampler
                                        : FRHISamplerDesc{});
                                if (!Sampler.Succeeded())
                                    return Failure(OutInspection,
                                        EStaticModelRealizationStage::Descriptor,
                                        Sampler.Result,
                                        "renderer.static-model.draw-sampler-create",
                                        Draw.StableKey,
                                        "per-draw sampler allocation failed");
                                Track(Transaction,
                                    Core::FString("draw-sampler:" +
                                        Draw.StableKey.ToStdString() + ":" +
                                        std::to_string(Binding.BindingSlot)),
                                    Sampler.Object);
                                Impl->Samplers.push_back(Sampler.Object);
                                const ERHIResult UpdateResult =
                                    Binding.DescriptorType ==
                                            ERHIDescriptorType::Sampler
                                    ? Descriptor.Object->UpdateSampler(
                                        Binding.BindingSlot, Element,
                                        Sampler.Object)
                                    : Descriptor.Object
                                        ->UpdateCombinedTextureSampler(
                                            Binding.BindingSlot, Element,
                                            Texture,
                                            Sampler.Object);
                                if (UpdateResult != ERHIResult::Success)
                                    return Failure(OutInspection,
                                        EStaticModelRealizationStage::Descriptor,
                                        ERHIResult::Failed,
                                        "renderer.static-model.draw-sampler-bind",
                                        Draw.StableKey,
                                        "per-draw sampler update failed");
                            }
                        }
                    }
                }
                const auto Existing = std::find_if(
                    DrawResources.DescriptorSets.begin(),
                    DrawResources.DescriptorSets.end(),
                    [DrawSet](const auto& Candidate)
                    { return Candidate->GetSetIndex() == DrawSet; });
                if (Existing == DrawResources.DescriptorSets.end())
                    DrawResources.DescriptorSets.push_back(Descriptor.Object);
                else
                    *Existing = Descriptor.Object;
            }
        }
        Impl->DrawResources.push_back(std::move(DrawResources));
    }

    const Core::uint64 Generation =
        GSnapshotGeneration.fetch_add(1, std::memory_order_relaxed);
    if (Generation == 0)
        return Failure(OutInspection, EStaticModelRealizationStage::Commit,
            ERHIResult::Unavailable, "renderer.static-model.generation",
            Request.Model->GetDesc().Id.ToString(),
            "snapshot generation space is exhausted");
    OutInspection.Stage = EStaticModelRealizationStage::Published;
    OutInspection.SnapshotGeneration = Generation;
    Impl->SnapshotGeneration = Generation;
    Impl->OwnedResources = Transaction.Commit();
    Impl->Inspection = OutInspection;
    try
    {
        OutSnapshot = Core::TSharedPtr<const FStaticModelRenderSnapshot>(
            new FStaticModelRenderSnapshot(std::move(Impl)));
    }
    catch (const std::bad_alloc&)
    {
        OutInspection.bCommitted = false;
        OutInspection.bRolledBack = true;
        if (Impl)
            Private::ReleaseStaticModelResources(
                Impl->OwnedResources, OutInspection);
        return Failure(OutInspection, EStaticModelRealizationStage::Commit,
            ERHIResult::Unavailable, "renderer.static-model.publish-allocation",
            Request.Model->GetDesc().Id.ToString(),
            "snapshot publication allocation failed");
    }
    return ERHIResult::Success;
}

const char* ToString(EStaticModelRealizationStage Stage) noexcept
{
    switch (Stage)
    {
    case EStaticModelRealizationStage::Validate: return "Validate";
    case EStaticModelRealizationStage::Plan: return "Plan";
    case EStaticModelRealizationStage::Mesh: return "Mesh";
    case EStaticModelRealizationStage::Texture: return "Texture";
    case EStaticModelRealizationStage::Shader: return "Shader";
    case EStaticModelRealizationStage::Material: return "Material";
    case EStaticModelRealizationStage::Descriptor: return "Descriptor";
    case EStaticModelRealizationStage::Pipeline: return "Pipeline";
    case EStaticModelRealizationStage::Commit: return "Commit";
    case EStaticModelRealizationStage::Rollback: return "Rollback";
    case EStaticModelRealizationStage::Published: return "Published";
    case EStaticModelRealizationStage::Released: return "Released";
    }
    return "Unknown";
}

} // namespace Stoner::Renderer
