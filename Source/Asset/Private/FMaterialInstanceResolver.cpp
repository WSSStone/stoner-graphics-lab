#include "FMaterialInstanceResolver.h"

#include <algorithm>
#include <set>

namespace Stoner::Asset
{
namespace Private
{

EAssetResult AppendRuntimeDependencies(
    const IMaterialAssetLookup& Lookup,
    FResolvedMaterialAsset& Material)
{
    if (!Material.Shader.GetId())
    {
        return EAssetResult::InvalidInstanceChain;
    }
    const auto ShaderVersion =
        Lookup.FindDependencyVersion(*Material.Shader.GetId());
    if (!ShaderVersion)
    {
        return EAssetResult::UnresolvedDependency;
    }
    Material.SourceManifest.push_back({
        *Material.Shader.GetId(),
        *ShaderVersion,
        EAssetSourceRole::Program});
    for (const FMaterialAssetParameter& Parameter :
         Material.EffectiveParameters)
    {
        if (Parameter.Value.Type !=
                EMaterialAssetParameterType::TextureReference ||
            !std::holds_alternative<FAssetId>(Parameter.Value.Value))
        {
            continue;
        }
        const FAssetId& Texture =
            std::get<FAssetId>(Parameter.Value.Value);
        const auto TextureVersion =
            Lookup.FindDependencyVersion(Texture);
        if (!TextureVersion)
        {
            return EAssetResult::UnresolvedDependency;
        }
        Material.SourceManifest.push_back({
            Texture,
            *TextureVersion,
            EAssetSourceRole::Texture});
    }
    return NormalizeSourceManifest(Material.SourceManifest);
}

EAssetResult ResolveMaterialInternal(
    const FAssetId& MaterialOrInstance,
    const IMaterialAssetLookup& Lookup,
    const FMaterialShaderAssetLimits& Limits,
    FResolvedMaterialAsset& OutMaterial,
    FAssetDiagnosticList*)
{
    OutMaterial = {};
    if (!MaterialOrInstance.IsValid() ||
        Limits.Validate() != EAssetResult::Success)
    {
        return EAssetResult::InvalidInput;
    }
    if (MaterialOrInstance.GetAssetType() ==
        TAssetTypeTraits<FMaterialAsset>::GetAssetType())
    {
        const auto Material = Lookup.FindMaterial(MaterialOrInstance);
        if (!Material)
        {
            return EAssetResult::InvalidInstanceChain;
        }
        const auto& Desc = Material->GetDesc();
        OutMaterial.LeafId = Desc.Id;
        OutMaterial.LeafVersion = Desc.Version;
        OutMaterial.RootMaterialId = Desc.Id;
        OutMaterial.RootMaterialVersion = Desc.Version;
        OutMaterial.Domain = Desc.Domain;
        OutMaterial.BlendMode = Desc.BlendMode;
        OutMaterial.RenderState = Desc.RenderState;
        OutMaterial.Shader = Desc.Shader;
        OutMaterial.PermutationRequest = Desc.PermutationRequest;
        OutMaterial.EffectiveParameters = Desc.Parameters;
        OutMaterial.SourceManifest.push_back(
            {Desc.Id, Desc.Version, EAssetSourceRole::Material});
        return AppendRuntimeDependencies(Lookup, OutMaterial);
    }

    Core::TArray<Core::TSharedPtr<const FMaterialInstanceAsset>> Chain;
    std::set<FAssetId> Visited;
    FAssetId Current = MaterialOrInstance;
    Core::TSharedPtr<const FMaterialAsset> Root;
    for (Core::usize Depth = 0; Depth <= Limits.MaxInstanceDepth; ++Depth)
    {
        if (!Visited.insert(Current).second)
        {
            return EAssetResult::InvalidInstanceChain;
        }
        if (Current.GetAssetType() ==
            TAssetTypeTraits<FMaterialAsset>::GetAssetType())
        {
            Root = Lookup.FindMaterial(Current);
            break;
        }
        if (Current.GetAssetType() !=
            TAssetTypeTraits<FMaterialInstanceAsset>::GetAssetType() ||
            Depth == Limits.MaxInstanceDepth)
        {
            return EAssetResult::InvalidInstanceChain;
        }
        auto Instance = Lookup.FindInstance(Current);
        if (!Instance)
        {
            return EAssetResult::InvalidInstanceChain;
        }
        Chain.push_back(Instance);
        const auto ParentId = std::visit(
            [](const auto& Parent) -> std::optional<FAssetId>
            {
                return Parent.GetId();
            },
            Instance->GetDesc().Parent.Reference);
        if (!ParentId)
        {
            return EAssetResult::InvalidInstanceChain;
        }
        Current = *ParentId;
    }
    if (!Root)
    {
        return EAssetResult::InvalidInstanceChain;
    }

    const auto& RootDesc = Root->GetDesc();
    OutMaterial.LeafId = Chain.empty()
        ? RootDesc.Id
        : Chain.front()->GetDesc().Id;
    OutMaterial.LeafVersion = Chain.empty()
        ? RootDesc.Version
        : Chain.front()->GetDesc().Version;
    OutMaterial.RootMaterialId = RootDesc.Id;
    OutMaterial.RootMaterialVersion = RootDesc.Version;
    OutMaterial.Domain = RootDesc.Domain;
    OutMaterial.BlendMode = RootDesc.BlendMode;
    OutMaterial.RenderState = RootDesc.RenderState;
    OutMaterial.Shader = RootDesc.Shader;
    OutMaterial.PermutationRequest = RootDesc.PermutationRequest;
    OutMaterial.EffectiveParameters = RootDesc.Parameters;
    OutMaterial.SourceManifest.push_back(
        {RootDesc.Id, RootDesc.Version, EAssetSourceRole::Material});

    for (auto It = Chain.rbegin(); It != Chain.rend(); ++It)
    {
        const auto& Instance = (*It)->GetDesc();
        OutMaterial.SourceManifest.push_back(
            {Instance.Id, Instance.Version, EAssetSourceRole::Parent});
        for (const FMaterialAssetParameter& Override : Instance.Overrides)
        {
            const auto Parameter = std::find_if(
                OutMaterial.EffectiveParameters.begin(),
                OutMaterial.EffectiveParameters.end(),
                [&Override](const auto& Candidate)
                {
                    return Candidate.Name == Override.Name;
                });
            if (Parameter == OutMaterial.EffectiveParameters.end() ||
                Parameter->Value.Type != Override.Value.Type)
            {
                return EAssetResult::InvalidInstanceChain;
            }
            Parameter->Value = Override.Value;
        }
    }
    return AppendRuntimeDependencies(Lookup, OutMaterial);
}

} // namespace Private

EAssetResult ResolveMaterial(
    const FAssetId& MaterialOrInstance,
    const IMaterialAssetLookup& Lookup,
    const FMaterialShaderAssetLimits& Limits,
    FResolvedMaterialAsset& OutMaterial,
    FAssetDiagnosticList* Diagnostics)
{
    return Private::ResolveMaterialInternal(
        MaterialOrInstance,
        Lookup,
        Limits,
        OutMaterial,
        Diagnostics);
}

} // namespace Stoner::Asset
