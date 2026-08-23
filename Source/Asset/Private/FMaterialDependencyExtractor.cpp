#include "FMaterialDependencyExtractor.h"

#include <algorithm>

namespace Stoner::Asset::Private
{
namespace
{

void AddUnique(
    Core::TArray<FAssetDependency>& Dependencies,
    const FAssetId& Id,
    EAssetDependencyRole Role,
    EAssetDependencyStrength Strength)
{
    FAssetDependency Dependency{
        Id,
        Role,
        Strength,
        EAssetDependencyResolution::Unresolved};
    if (std::none_of(
            Dependencies.begin(),
            Dependencies.end(),
            [&Dependency](const auto& Existing)
            {
                return Existing.SameDeclaration(Dependency);
            }))
    {
        Dependencies.push_back(std::move(Dependency));
    }
}

const FAssetId* TextureId(const FMaterialAssetParameterValue& Value)
{
    if (Value.Type == EMaterialAssetParameterType::TextureReference &&
        std::holds_alternative<FAssetId>(Value.Value))
    {
        return &std::get<FAssetId>(Value.Value);
    }
    if (Value.Type == EMaterialAssetParameterType::TextureBinding &&
        std::holds_alternative<FMaterialTextureBinding>(Value.Value))
    {
        const auto& Binding =
            std::get<FMaterialTextureBinding>(Value.Value);
        if (Binding.Texture.GetId())
        {
            return &*Binding.Texture.GetId();
        }
    }
    return nullptr;
}

} // namespace

EAssetResult ExtractMaterialDependencies(FMaterialAssetDesc& Desc)
{
    Desc.Dependencies.clear();
    if (!Desc.Shader.GetId())
    {
        return EAssetResult::InvalidMaterialAsset;
    }
    AddUnique(
        Desc.Dependencies,
        *Desc.Shader.GetId(),
        EAssetDependencyRole::Runtime,
        EAssetDependencyStrength::Required);
    for (const FMaterialAssetParameter& Parameter : Desc.Parameters)
    {
        if (const FAssetId* Texture = TextureId(Parameter.Value))
        {
            AddUnique(
                Desc.Dependencies,
                *Texture,
                EAssetDependencyRole::Runtime,
                EAssetDependencyStrength::Required);
        }
    }
    std::sort(
        Desc.Dependencies.begin(),
        Desc.Dependencies.end(),
        [](const auto& Left, const auto& Right)
        {
            return Left.TargetId < Right.TargetId;
        });
    return EAssetResult::Success;
}

EAssetResult ExtractMaterialInstanceDependencies(
    FMaterialInstanceAssetDesc& Desc)
{
    Desc.Dependencies.clear();
    const auto ParentId = std::visit(
        [](const auto& Parent) -> std::optional<FAssetId>
        {
            return Parent.GetId();
        },
        Desc.Parent.Reference);
    if (!ParentId)
    {
        return EAssetResult::InvalidInstanceChain;
    }
    AddUnique(
        Desc.Dependencies,
        *ParentId,
        EAssetDependencyRole::Runtime,
        EAssetDependencyStrength::Required);
    for (const FMaterialAssetParameter& Override : Desc.Overrides)
    {
        if (const FAssetId* Texture = TextureId(Override.Value))
        {
            AddUnique(
                Desc.Dependencies,
                *Texture,
                EAssetDependencyRole::Runtime,
                EAssetDependencyStrength::Required);
        }
    }
    std::sort(
        Desc.Dependencies.begin(),
        Desc.Dependencies.end(),
        [](const auto& Left, const auto& Right)
        {
            return Left.TargetId < Right.TargetId;
        });
    return EAssetResult::Success;
}

EAssetResult ExtractShaderDependencies(FShaderAssetDesc& Desc)
{
    Desc.Dependencies.clear();
    for (const FShaderSourceReference& Stage : Desc.Stages)
    {
        if (Stage.Source.GetId())
            AddUnique(
                Desc.Dependencies,
                *Stage.Source.GetId(),
                EAssetDependencyRole::Source,
                EAssetDependencyStrength::Required);
    }
    for (const FShaderVariantDefinition& Variant : Desc.Variants)
    {
        for (const FShaderPayloadReference& Payload : Variant.Payloads)
        {
            if (Payload.Payload.GetId())
                AddUnique(
                    Desc.Dependencies,
                    *Payload.Payload.GetId(),
                    EAssetDependencyRole::Runtime,
                    EAssetDependencyStrength::Required);
        }
    }
    std::sort(
        Desc.Dependencies.begin(),
        Desc.Dependencies.end(),
        [](const auto& Left, const auto& Right)
        {
            return Left.TargetId < Right.TargetId;
        });
    return EAssetResult::Success;
}

} // namespace Stoner::Asset::Private
