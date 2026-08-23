#include "FStaticModelRealizationPlan.h"

#include "RHI/IRHIDevice.h"

#include <algorithm>
#include <limits>
#include <set>
#include <variant>

namespace Stoner::Renderer::Private
{
namespace
{

using namespace Stoner::Asset;
using namespace Stoner::RHI;

ERHIResult Fail(
    FStaticModelRealizationDiagnostic& Out,
    ERHIResult Result,
    const char* Code,
    const Core::FString& Subject,
    const char* Reason)
{
    Out = {
        EStaticModelRealizationStage::Plan,
        Result,
        Core::FString(Code),
        Subject,
        Core::FString(Reason)};
    return Result;
}

template <typename T, typename F>
bool SortUnique(
    const Core::TArray<Core::TSharedPtr<const T>>& Input,
    Core::TArray<Core::TSharedPtr<const T>>& Out,
    F GetId)
{
    Out = Input;
    if (std::any_of(
            Out.begin(), Out.end(),
            [](const auto& Value) { return !Value; }))
        return false;
    std::sort(
        Out.begin(), Out.end(),
        [&GetId](const auto& Left, const auto& Right)
        {
            return GetId(*Left) < GetId(*Right);
        });
    return std::adjacent_find(
        Out.begin(), Out.end(),
        [&GetId](const auto& Left, const auto& Right)
        {
            return GetId(*Left) == GetId(*Right);
        }) == Out.end();
}

template <typename T, typename F>
const T* Find(
    const Core::TArray<Core::TSharedPtr<const T>>& Values,
    const FAssetId& Id,
    F GetId)
{
    const auto Found = std::lower_bound(
        Values.begin(), Values.end(), Id,
        [&GetId](const auto& Value, const FAssetId& Key)
        {
            return GetId(*Value) < Key;
        });
    return Found == Values.end() || GetId(**Found) != Id
        ? nullptr : Found->get();
}

template <typename T, typename F>
Core::TSharedPtr<const T> FindShared(
    const Core::TArray<Core::TSharedPtr<const T>>& Values,
    const FAssetId& Id,
    F GetId)
{
    const auto Found = std::lower_bound(
        Values.begin(), Values.end(), Id,
        [&GetId](const auto& Value, const FAssetId& Key)
        {
            return GetId(*Value) < Key;
        });
    return Found == Values.end() || GetId(**Found) != Id
        ? Core::TSharedPtr<const T>{} : *Found;
}

class FMaterialLookup final : public IMaterialAssetLookup
{
public:
    FMaterialLookup(
        const Core::TArray<Core::TSharedPtr<const FMaterialAsset>>& Materials,
        const Core::TArray<Core::TSharedPtr<const FMaterialInstanceAsset>>& Instances,
        const Core::TArray<FStaticModelRealizationDependencies::FVersionRecord>&
            Versions)
        : Materials_(Materials), Instances_(Instances), Versions_(Versions)
    {
    }

    Core::TSharedPtr<const FMaterialAsset> FindMaterial(
        const FAssetId& Id) const override
    {
        const auto Found = std::lower_bound(
            Materials_.begin(), Materials_.end(), Id,
            [](const auto& Value, const FAssetId& Key)
            {
                return Value->GetDesc().Id < Key;
            });
        return Found == Materials_.end() || (*Found)->GetDesc().Id != Id
            ? nullptr : *Found;
    }

    Core::TSharedPtr<const FMaterialInstanceAsset> FindInstance(
        const FAssetId& Id) const override
    {
        const auto Found = std::lower_bound(
            Instances_.begin(), Instances_.end(), Id,
            [](const auto& Value, const FAssetId& Key)
            {
                return Value->GetDesc().Id < Key;
            });
        return Found == Instances_.end() || (*Found)->GetDesc().Id != Id
            ? nullptr : *Found;
    }

    std::optional<FAssetVersion> FindDependencyVersion(
        const FAssetId& Id) const override
    {
        if (auto Value = FindMaterial(Id)) return Value->GetDesc().Version;
        if (auto Value = FindInstance(Id)) return Value->GetDesc().Version;
        const auto Found = std::lower_bound(
            Versions_.begin(), Versions_.end(), Id,
            [](const auto& Value, const FAssetId& Key)
            {
                return Value.AssetId < Key;
            });
        return Found == Versions_.end() || Found->AssetId != Id
            ? std::nullopt
            : std::optional<FAssetVersion>(Found->Version);
    }

private:
    const Core::TArray<Core::TSharedPtr<const FMaterialAsset>>& Materials_;
    const Core::TArray<Core::TSharedPtr<const FMaterialInstanceAsset>>& Instances_;
    const Core::TArray<FStaticModelRealizationDependencies::FVersionRecord>&
        Versions_;
};

class FPayloadLookup final : public IShaderPayloadLookup
{
public:
    explicit FPayloadLookup(
        const Core::TArray<Core::TSharedPtr<const FShaderPayloadAsset>>& Payloads)
        : Payloads_(Payloads)
    {
    }

    Core::TSharedPtr<const FShaderPayloadAsset> Find(
        const FAssetId& Id) const override
    {
        const auto Found = std::lower_bound(
            Payloads_.begin(), Payloads_.end(), Id,
            [](const auto& Value, const FAssetId& Key)
            {
                return Value->GetId() < Key;
            });
        return Found == Payloads_.end() || (*Found)->GetId() != Id
            ? nullptr : *Found;
    }

private:
    const Core::TArray<Core::TSharedPtr<const FShaderPayloadAsset>>& Payloads_;
};

EShaderBackendFamily Backend(EAssetGraphicsBackend Value)
{
    switch (Value)
    {
    case EAssetGraphicsBackend::Vulkan: return EShaderBackendFamily::Vulkan;
    case EAssetGraphicsBackend::Metal: return EShaderBackendFamily::Metal;
    case EAssetGraphicsBackend::DirectX12: return EShaderBackendFamily::DirectX12;
    case EAssetGraphicsBackend::OpenGL: return EShaderBackendFamily::OpenGL;
    case EAssetGraphicsBackend::GLES: return EShaderBackendFamily::GLES;
    }
    return EShaderBackendFamily::Vulkan;
}

bool AddBytes(Core::uint64 Value, Core::uint64& Total)
{
    if (Value > std::numeric_limits<Core::uint64>::max() - Total)
        return false;
    Total += Value;
    return true;
}

bool SupportsRenderTargets(
    const FRHIDeviceCapabilities& Capabilities,
    const FRHIRenderTargetCompatibility& Targets)
{
    if (!Capabilities.SupportsSampleCount(Targets.SampleCount)) return false;
    for (const ERHIFormat Format : Targets.ColorFormats)
        if (!Capabilities.SupportsFormatUsage(
                Format, ERHIFormatCapability::ColorAttachment))
            return false;
    return Targets.DepthStencilFormat == ERHIFormat::Unknown ||
        Capabilities.SupportsFormatUsage(
            Targets.DepthStencilFormat,
            ERHIFormatCapability::DepthStencilAttachment);
}

bool HasValidTextureProfiles(const FStaticModelRealizationRequest& Request)
{
    if (Request.TextureTargetProfiles.empty())
        return Request.TextureTargetProfile.Validate() == ERHIResult::Success;
    if (Request.TextureTargetProfiles.size() !=
        Request.Dependencies.Textures.size())
        return false;
    std::set<FAssetId> ProfileIds;
    for (const auto& Record : Request.TextureTargetProfiles)
        if (!Record.AssetId.IsValid() ||
            Record.Profile.Validate() != ERHIResult::Success ||
            !ProfileIds.insert(Record.AssetId).second)
            return false;
    for (const auto& Texture : Request.Dependencies.Textures)
        if (!Texture || !ProfileIds.contains(Texture->GetId()))
            return false;
    return true;
}

} // namespace

RHI::ERHIResult BuildStaticModelRealizationPlan(
    const FStaticModelRealizationRequest& Request,
    FStaticModelRealizationPlan& OutPlan,
    FStaticModelRealizationDiagnostic& OutFailure)
{
    OutPlan = {};
    OutFailure = {};
    if (!Request.Device || !Request.Device->IsActive() || !Request.Model ||
        !Request.TargetEvidence ||
        Request.TargetEvidence->Validate() != EAssetResult::Success ||
        !Request.Limits.IsValid() ||
        !HasValidTextureProfiles(Request) ||
        !IsValidRHIRenderTargetCompatibility(Request.RenderTargets) ||
        !SupportsRenderTargets(
            Request.Device->GetCapabilities(), Request.RenderTargets))
        return Fail(OutFailure, ERHIResult::InvalidState,
            "renderer.static-model.request-invalid", {},
            "device, model, target, limits, or render targets are invalid");
    if (Request.RuntimeContext && Request.RuntimeContext->ShouldStop())
        return Fail(OutFailure, ERHIResult::Failed,
            "renderer.static-model.cancelled", Request.Model->GetDesc().Id.ToString(),
            "realization was cancelled before planning");

    const auto MeshId = [](const FStaticMeshAsset& Value)
        -> const FAssetId& { return Value.GetDesc().Id; };
    const auto MaterialId = [](const FMaterialAsset& Value)
        -> const FAssetId& { return Value.GetDesc().Id; };
    const auto InstanceId = [](const FMaterialInstanceAsset& Value)
        -> const FAssetId& { return Value.GetDesc().Id; };
    const auto ShaderId = [](const FShaderAsset& Value)
        -> const FAssetId& { return Value.GetDesc().Id; };
    const auto PayloadId = [](const FShaderPayloadAsset& Value)
        -> const FAssetId& { return Value.GetId(); };
    const auto TextureId = [](const FKTX2TextureArtifact& Value)
        -> const FAssetId& { return Value.GetId(); };

    Core::TArray<Core::TSharedPtr<const FStaticMeshAsset>> Meshes;
    Core::TArray<Core::TSharedPtr<const FMaterialAsset>> Materials;
    Core::TArray<Core::TSharedPtr<const FMaterialInstanceAsset>> Instances;
    Core::TArray<Core::TSharedPtr<const FShaderAsset>> Shaders;
    Core::TArray<Core::TSharedPtr<const FShaderPayloadAsset>> Payloads;
    Core::TArray<Core::TSharedPtr<const FKTX2TextureArtifact>> Textures;
    auto Versions = Request.Dependencies.Versions;
    if (!SortUnique(Request.Dependencies.Meshes, Meshes, MeshId) ||
        !SortUnique(Request.Dependencies.Materials, Materials, MaterialId) ||
        !SortUnique(Request.Dependencies.MaterialInstances, Instances, InstanceId) ||
        !SortUnique(Request.Dependencies.Shaders, Shaders, ShaderId) ||
        !SortUnique(Request.Dependencies.ShaderPayloads, Payloads, PayloadId) ||
        !SortUnique(Request.Dependencies.Textures, Textures, TextureId))
        return Fail(OutFailure, ERHIResult::InvalidState,
            "renderer.static-model.dependency-duplicate",
            Request.Model->GetDesc().Id.ToString(),
            "typed dependencies contain null or duplicate identities");

    std::sort(
        Versions.begin(), Versions.end(),
        [](const auto& Left, const auto& Right)
        {
            return Left.AssetId < Right.AssetId;
        });
    std::set<FAssetId> TypedIds;
    const auto AddTyped = [&TypedIds](const auto& Values, auto GetId)
    {
        for (const auto& Value : Values) TypedIds.insert(GetId(*Value));
    };
    AddTyped(Meshes, MeshId);
    AddTyped(Materials, MaterialId);
    AddTyped(Instances, InstanceId);
    AddTyped(Shaders, ShaderId);
    AddTyped(Payloads, PayloadId);
    AddTyped(Textures, TextureId);
    if (Versions.size() != TypedIds.size() ||
        std::adjacent_find(
            Versions.begin(), Versions.end(),
            [](const auto& Left, const auto& Right)
            { return Left.AssetId == Right.AssetId; }) != Versions.end() ||
        !std::equal(
            Versions.begin(), Versions.end(), TypedIds.begin(),
            [](const auto& Version, const FAssetId& Id)
            {
                return Version.AssetId == Id && Version.AssetId.IsValid() &&
                    Version.Version.Validate() == EAssetResult::Success;
            }))
        return Fail(OutFailure, ERHIResult::InvalidState,
            "renderer.static-model.version-set",
            Request.Model->GetDesc().Id.ToString(),
            "dependency versions must map one-to-one to the typed payload set");
    const auto VersionMatches = [&Versions](
        const FAssetId& Id, const FAssetVersion& Version)
    {
        const auto Found = std::lower_bound(
            Versions.begin(), Versions.end(), Id,
            [](const auto& Value, const FAssetId& Key)
            { return Value.AssetId < Key; });
        return Found != Versions.end() && Found->AssetId == Id &&
            Found->Version == Version;
    };
    const auto IntrinsicVersionsMatch = [&VersionMatches](
        const auto& Values, auto GetId, auto GetVersion)
    {
        return std::all_of(
            Values.begin(), Values.end(),
            [&VersionMatches, &GetId, &GetVersion](const auto& Value)
            { return VersionMatches(GetId(*Value), GetVersion(*Value)); });
    };
    if (!IntrinsicVersionsMatch(Meshes, MeshId,
            [](const auto& Value) -> const FAssetVersion&
            { return Value.GetDesc().Version; }) ||
        !IntrinsicVersionsMatch(Materials, MaterialId,
            [](const auto& Value) -> const FAssetVersion&
            { return Value.GetDesc().Version; }) ||
        !IntrinsicVersionsMatch(Instances, InstanceId,
            [](const auto& Value) -> const FAssetVersion&
            { return Value.GetDesc().Version; }) ||
        !IntrinsicVersionsMatch(Shaders, ShaderId,
            [](const auto& Value) -> const FAssetVersion&
            { return Value.GetDesc().Version; }) ||
        !IntrinsicVersionsMatch(Payloads, PayloadId,
            [](const auto& Value) -> const FAssetVersion&
            { return Value.GetVersion(); }))
        return Fail(OutFailure, ERHIResult::InvalidState,
            "renderer.static-model.version-mismatch",
            Request.Model->GetDesc().Id.ToString(),
            "typed payload version differs from explicit dependency evidence");

    const auto& Model = Request.Model->GetDesc();
    if (Model.Nodes.empty() || Model.Nodes.size() > Request.Limits.MaxNodes ||
        Meshes.size() > Request.Limits.MaxMeshes ||
        Materials.size() + Instances.size() > Request.Limits.MaxMaterials ||
        Shaders.size() > Request.Limits.MaxShaders ||
        Textures.size() > Request.Limits.MaxTextures)
        return Fail(OutFailure, ERHIResult::Unavailable,
            "renderer.static-model.limit", Model.Id.ToString(),
            "CPU dependency counts exceed realization limits");

    Core::TArray<Core::uint8> Visit(Model.Nodes.size(), 0);
    const auto VisitNode = [&](const auto& Self, Core::uint32 NodeIndex,
                               const Core::FMatrix4x4& Parent) -> bool
    {
        if (NodeIndex >= Model.Nodes.size() || Visit[NodeIndex] != 0)
            return false;
        Visit[NodeIndex] = 1;
        const auto& Node = Model.Nodes[NodeIndex];
        FStaticModelRenderNode Planned;
        Planned.SourceNodeIndex = NodeIndex;
        Planned.StableKey = Node.StableKey;
        const Core::FMatrix4x4 World =
            Parent * Node.LocalTransform.ToMatrix();
        Planned.WorldTransform = World;
        OutPlan.Nodes.push_back(std::move(Planned));
        for (Core::uint32 Child : Node.Children)
            if (!Self(Self, Child, World))
                return false;
        Visit[NodeIndex] = 2;
        return true;
    };
    for (Core::uint32 Root : Model.RootNodeIndices)
        if (!VisitNode(VisitNode, Root, Core::FMatrix4x4::Identity()))
            return Fail(OutFailure, ERHIResult::InvalidState,
                "renderer.static-model.hierarchy", Model.Id.ToString(),
                "model hierarchy is cyclic, duplicated, or out of range");
    if (std::any_of(Visit.begin(), Visit.end(),
            [](Core::uint8 Value) { return Value != 2; }))
        return Fail(OutFailure, ERHIResult::InvalidState,
            "renderer.static-model.hierarchy-incomplete", Model.Id.ToString(),
            "model hierarchy does not cover every node exactly once");

    std::set<FAssetId> RequiredMeshIds;
    std::set<FAssetId> DeclaredMaterialIds;
    std::set<FAssetId> RequiredMaterialIds;
    for (const auto& Node : Model.Nodes)
        if (Node.Mesh && Node.Mesh->GetId())
            RequiredMeshIds.insert(*Node.Mesh->GetId());
    for (const FAssetId& Id : RequiredMeshIds)
    {
        const auto Mesh = FindShared(Meshes, Id, MeshId);
        if (!Mesh)
            return Fail(OutFailure, ERHIResult::InvalidState,
                "renderer.static-model.mesh-missing", Id.ToString(),
                "a referenced mesh is absent from the typed dependency set");
        OutPlan.Meshes.push_back(Mesh);
        for (const auto& Slot : Mesh->GetDesc().MaterialSlots)
        {
            if (!Slot.Material.GetId())
                return Fail(OutFailure, ERHIResult::InvalidState,
                    "renderer.static-model.material-slot", Id.ToString(),
                    "mesh declares a material slot without an identity");
            DeclaredMaterialIds.insert(*Slot.Material.GetId());
        }
        for (const auto& Primitive : Mesh->GetDesc().Primitives)
        {
            if (Primitive.MaterialSlotIndex >=
                Mesh->GetDesc().MaterialSlots.size())
                return Fail(OutFailure, ERHIResult::InvalidState,
                    "renderer.static-model.material-slot", Id.ToString(),
                    "primitive material slot is out of range");
            RequiredMaterialIds.insert(*Mesh->GetDesc()
                .MaterialSlots[Primitive.MaterialSlotIndex].Material.GetId());
            Core::uint64 Bytes = 0;
            Bytes += Primitive.Vertices.Positions.size() * sizeof(Core::FVector3);
            Bytes += Primitive.Vertices.Normals.size() * sizeof(Core::FVector3);
            Bytes += Primitive.Vertices.Tangents.size() * sizeof(Core::FVector4);
            Bytes += Primitive.Vertices.TexCoords[0].size() * sizeof(Core::FVector2);
            Bytes += Primitive.Vertices.TexCoords[1].size() * sizeof(Core::FVector2);
            Bytes += Primitive.Indices.GetIndexCount() * sizeof(Core::uint32);
            if (!AddBytes(Bytes, OutPlan.EstimatedResourceBytes))
                return Fail(OutFailure, ERHIResult::Unavailable,
                    "renderer.static-model.byte-overflow", Id.ToString(),
                    "mesh resource byte estimate overflowed");
        }
    }
    if (OutPlan.Meshes.size() != Meshes.size())
        return Fail(OutFailure, ERHIResult::InvalidState,
            "renderer.static-model.mesh-extra", Model.Id.ToString(),
            "the typed mesh set contains an unreferenced asset");

    std::set<FAssetId> SuppliedMaterialIds;
    for (const auto& Material : Materials)
        SuppliedMaterialIds.insert(Material->GetDesc().Id);
    for (const auto& Instance : Instances)
        SuppliedMaterialIds.insert(Instance->GetDesc().Id);
    if (SuppliedMaterialIds != DeclaredMaterialIds ||
        SuppliedMaterialIds.size() != Materials.size() + Instances.size())
        return Fail(OutFailure, ERHIResult::InvalidState,
            "renderer.static-model.material-extra", Model.Id.ToString(),
            "typed materials must exactly match the mesh-declared slots");

    FMaterialLookup MaterialLookup(Materials, Instances, Versions);
    FPayloadLookup PayloadLookup(Payloads);
    std::set<FAssetId> RequiredShaderIds;
    std::set<FAssetId> SelectedPayloadIds;
    std::set<FAssetId> RequiredTextureIds;
    const auto& Profile = Request.TargetEvidence->Profile;
    FShaderTargetRequest ShaderRequest;
    ShaderRequest.Backend = Backend(Profile.GraphicsBackend);
    if (Profile.GraphicsBackend == EAssetGraphicsBackend::Metal)
        ShaderRequest.CpuArchitecture = Profile.CpuArchitecture;
    for (const auto& Choice : Profile.ShaderPayloadChoices)
        if (Choice.Backend == Profile.GraphicsBackend)
            ShaderRequest.AcceptableProfiles.push_back(Choice.Profile);

    for (const FAssetId& Id : DeclaredMaterialIds)
    {
        FStaticModelPlannedMaterial Planned;
        Planned.AssetId = Id;
        if (ResolveMaterial(
                Id, MaterialLookup, {}, Planned.Resolved) !=
            EAssetResult::Success || !Planned.Resolved.Shader.GetId())
            return Fail(OutFailure, ERHIResult::InvalidState,
                "renderer.static-model.material-invalid", Id.ToString(),
                "material or instance chain cannot be resolved");
        const FAssetId ShaderAssetId = *Planned.Resolved.Shader.GetId();
        RequiredShaderIds.insert(ShaderAssetId);
        const FShaderAsset* Shader = Find(Shaders, ShaderAssetId, ShaderId);
        ShaderRequest.Permutation = Planned.Resolved.PermutationRequest;
        if (!Shader || SelectShaderProgram(
                *Shader, ShaderRequest, PayloadLookup,
                Planned.SelectedShader) != EAssetResult::Success)
            return Fail(OutFailure, ERHIResult::InvalidState,
                "renderer.static-model.shader-invalid", ShaderAssetId.ToString(),
                "material shader target selection failed");
        for (const auto& Stage : Planned.SelectedShader.Stages)
            if (Stage.Payload)
                SelectedPayloadIds.insert(Stage.Payload->GetId());
        for (const auto& Parameter : Planned.Resolved.EffectiveParameters)
        {
            if (Parameter.Value.Type ==
                EMaterialAssetParameterType::TextureReference)
                Planned.TextureIds.push_back(
                    std::get<FAssetId>(Parameter.Value.Value));
            else if (Parameter.Value.Type ==
                     EMaterialAssetParameterType::TextureBinding)
            {
                const auto& Binding = std::get<FMaterialTextureBinding>(
                    Parameter.Value.Value);
                if (!Binding.Texture.GetId())
                    return Fail(OutFailure, ERHIResult::InvalidState,
                        "renderer.static-model.texture-reference", Id.ToString(),
                        "material texture binding has no identity");
                Planned.TextureIds.push_back(*Binding.Texture.GetId());
            }
        }
        std::sort(Planned.TextureIds.begin(), Planned.TextureIds.end());
        Planned.TextureIds.erase(
            std::unique(Planned.TextureIds.begin(), Planned.TextureIds.end()),
            Planned.TextureIds.end());
        RequiredTextureIds.insert(
            Planned.TextureIds.begin(), Planned.TextureIds.end());
        if (RequiredMaterialIds.contains(Id))
            OutPlan.Materials.push_back(std::move(Planned));
    }
    if (RequiredShaderIds.size() != Shaders.size())
        return Fail(OutFailure, ERHIResult::InvalidState,
            "renderer.static-model.shader-extra", Model.Id.ToString(),
            "the typed shader set is incomplete or contains unrelated assets");

    if (SelectedPayloadIds.size() != Payloads.size())
        return Fail(OutFailure, ERHIResult::InvalidState,
            "renderer.static-model.shader-payload-extra", Model.Id.ToString(),
            "selected shader payload set is incomplete or contains unrelated assets");

    for (const FAssetId& Id : RequiredTextureIds)
    {
        const auto Texture = FindShared(Textures, Id, TextureId);
        if (!Texture)
            return Fail(OutFailure, ERHIResult::InvalidState,
                "renderer.static-model.texture-missing", Id.ToString(),
                "a material texture is absent from the typed dependency set");
        OutPlan.Textures.push_back(Texture);
        if (!AddBytes(Texture->GetBytes().size(),
                OutPlan.EstimatedResourceBytes))
            return Fail(OutFailure, ERHIResult::Unavailable,
                "renderer.static-model.byte-overflow", Id.ToString(),
                "texture resource byte estimate overflowed");
    }
    if (OutPlan.Textures.size() != Textures.size() ||
        OutPlan.EstimatedResourceBytes >
            Request.Limits.MaxAggregateResourceBytes)
        return Fail(OutFailure, ERHIResult::Unavailable,
            "renderer.static-model.texture-extra-or-limit", Model.Id.ToString(),
            "texture set is unrelated or aggregate bytes exceed the limit");

    for (Core::uint32 NodePlanIndex = 0;
         NodePlanIndex < OutPlan.Nodes.size(); ++NodePlanIndex)
    {
        const auto& Node = Model.Nodes[
            OutPlan.Nodes[NodePlanIndex].SourceNodeIndex];
        if (!Node.Mesh || !Node.Mesh->GetId()) continue;
        const auto MeshIt = std::lower_bound(
            OutPlan.Meshes.begin(), OutPlan.Meshes.end(), *Node.Mesh->GetId(),
            [](const auto& Value, const FAssetId& Id)
            {
                return Value->GetDesc().Id < Id;
            });
        if (MeshIt == OutPlan.Meshes.end() ||
            (*MeshIt)->GetDesc().Id != *Node.Mesh->GetId())
            return Fail(OutFailure, ERHIResult::InvalidState,
                "renderer.static-model.mesh-plan", Node.StableKey,
                "planned node mesh cannot be indexed");
        const Core::uint32 MeshIndex = static_cast<Core::uint32>(
            std::distance(OutPlan.Meshes.begin(), MeshIt));
        const auto& Mesh = (*MeshIt)->GetDesc();
        for (Core::uint32 Section = 0;
             Section < Mesh.Primitives.size(); ++Section)
        {
            const auto Slot = Mesh.Primitives[Section].MaterialSlotIndex;
            if (Slot >= Mesh.MaterialSlots.size() ||
                !Mesh.MaterialSlots[Slot].Material.GetId())
                return Fail(OutFailure, ERHIResult::InvalidState,
                    "renderer.static-model.material-slot", Node.StableKey,
                    "primitive material slot is invalid");
            const FAssetId& MaterialIdValue =
                *Mesh.MaterialSlots[Slot].Material.GetId();
            const auto MaterialIt = std::lower_bound(
                OutPlan.Materials.begin(), OutPlan.Materials.end(),
                MaterialIdValue,
                [](const auto& Value, const FAssetId& Id)
                {
                    return Value.AssetId < Id;
                });
            if (MaterialIt == OutPlan.Materials.end() ||
                MaterialIt->AssetId != MaterialIdValue)
                return Fail(OutFailure, ERHIResult::InvalidState,
                    "renderer.static-model.material-plan", Node.StableKey,
                    "primitive material cannot be indexed");
            OutPlan.Draws.push_back({
                NodePlanIndex,
                MeshIndex,
                Section,
                static_cast<Core::uint32>(std::distance(
                    OutPlan.Materials.begin(), MaterialIt)),
                Node.StableKey,
                Mesh.Primitives[Section].LocalBounds});
        }
    }
    if (OutPlan.Draws.empty() ||
        OutPlan.Draws.size() > Request.Limits.MaxDraws)
        return Fail(OutFailure, ERHIResult::Unavailable,
            "renderer.static-model.draw-limit", Model.Id.ToString(),
            "draw count is zero or exceeds the configured limit");
    return ERHIResult::Success;
}

} // namespace Stoner::Renderer::Private
