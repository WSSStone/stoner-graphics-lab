#include "FMaterialDependencyExtractor.h"

#include <algorithm>

namespace Stoner::Asset::Private
{
namespace
{

void AddUnique(
    Core::TArray<FAssetDependency>& Dependencies,
    const FAssetId& Id,
    EAssetDependencyStrength Strength)
{
    FAssetDependency Dependency{
        Id,
        EAssetDependencyRole::Runtime,
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
        EAssetDependencyStrength::Required);
    for (const FMaterialAssetParameter& Parameter : Desc.Parameters)
    {
        if (const FAssetId* Texture = TextureId(Parameter.Value))
        {
            AddUnique(
                Desc.Dependencies,
                *Texture,
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
        EAssetDependencyStrength::Required);
    for (const FMaterialAssetParameter& Override : Desc.Overrides)
    {
        if (const FAssetId* Texture = TextureId(Override.Value))
        {
            AddUnique(
                Desc.Dependencies,
                *Texture,
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
