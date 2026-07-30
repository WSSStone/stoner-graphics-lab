#include "FMaterialAssetValidator.h"

#include "Core/FMath.h"

#include <algorithm>
#include <set>

namespace Stoner::Asset
{
namespace
{

bool IsFinite(const FMaterialAssetParameterValue& Value)
{
    using Core::FMath;
    switch (Value.Type)
    {
    case EMaterialAssetParameterType::Scalar:
        return std::holds_alternative<float>(Value.Value) &&
            FMath::IsFinite(std::get<float>(Value.Value));
    case EMaterialAssetParameterType::Vector:
        if (!std::holds_alternative<Core::FVector4>(Value.Value)) return false;
        {
            const auto V = std::get<Core::FVector4>(Value.Value);
            return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) &&
                FMath::IsFinite(V.Z) && FMath::IsFinite(V.W);
        }
    case EMaterialAssetParameterType::Color:
        if (!std::holds_alternative<Core::FColor>(Value.Value)) return false;
        {
            const auto C = std::get<Core::FColor>(Value.Value);
            return FMath::IsFinite(C.R) && FMath::IsFinite(C.G) &&
                FMath::IsFinite(C.B) && FMath::IsFinite(C.A);
        }
    case EMaterialAssetParameterType::TextureReference:
        return std::holds_alternative<FAssetId>(Value.Value) &&
            std::get<FAssetId>(Value.Value).IsValid() &&
            std::get<FAssetId>(Value.Value).GetAssetType() == "Texture";
    }
    return false;
}

bool IsDomainBlendValid(
    EMaterialAssetDomain Domain,
    EMaterialAssetBlendMode Blend)
{
    if (Domain == EMaterialAssetDomain::UI)
    {
        return Blend == EMaterialAssetBlendMode::Translucent ||
            Blend == EMaterialAssetBlendMode::Additive;
    }
    if (Domain == EMaterialAssetDomain::PostProcess)
    {
        return Blend == EMaterialAssetBlendMode::Opaque ||
            Blend == EMaterialAssetBlendMode::Translucent ||
            Blend == EMaterialAssetBlendMode::Additive;
    }
    return true;
}

EAssetResult NormalizeParameters(
    Core::TArray<FMaterialAssetParameter>& Parameters)
{
    std::sort(
        Parameters.begin(),
        Parameters.end(),
        [](const auto& Left, const auto& Right)
        {
            return Left.Name < Right.Name;
        });
    for (std::size_t Index = 0; Index < Parameters.size(); ++Index)
    {
        if (Parameters[Index].Name.IsEmpty() ||
            !IsFinite(Parameters[Index].Value) ||
            (Index > 0 && Parameters[Index - 1].Name == Parameters[Index].Name))
        {
            return EAssetResult::InvalidMaterialAsset;
        }
    }
    return EAssetResult::Success;
}

} // namespace

namespace Private
{

EAssetResult ValidateMaterialAsset(
    FMaterialAssetDesc& Desc,
    FAssetDiagnosticList*)
{
    if (!Desc.Id.IsValid() ||
        Desc.Id.GetAssetType() != TAssetTypeTraits<FMaterialAsset>::GetAssetType() ||
        Desc.Version.Validate() != EAssetResult::Success ||
        Desc.SchemaVersion != 1 ||
        Desc.Shader.IsEmpty() ||
        !IsDomainBlendValid(Desc.Domain, Desc.BlendMode))
    {
        return EAssetResult::InvalidMaterialAsset;
    }
    std::sort(
        Desc.PermutationRequest.Flags.begin(),
        Desc.PermutationRequest.Flags.end());
    if (std::adjacent_find(
            Desc.PermutationRequest.Flags.begin(),
            Desc.PermutationRequest.Flags.end()) !=
            Desc.PermutationRequest.Flags.end())
    {
        return EAssetResult::InvalidMaterialAsset;
    }
    return NormalizeParameters(Desc.Parameters);
}

EAssetResult ValidateMaterialInstanceAsset(
    FMaterialInstanceAssetDesc& Desc,
    FAssetDiagnosticList*)
{
    if (!Desc.Id.IsValid() ||
        Desc.Id.GetAssetType() !=
            TAssetTypeTraits<FMaterialInstanceAsset>::GetAssetType() ||
        Desc.Version.Validate() != EAssetResult::Success ||
        Desc.SchemaVersion != 1)
    {
        return EAssetResult::InvalidInstanceChain;
    }
    const bool bValidParent = std::visit(
        [](const auto& Parent) { return !Parent.IsEmpty(); },
        Desc.Parent.Reference);
    if (!bValidParent ||
        NormalizeParameters(Desc.Overrides) != EAssetResult::Success)
    {
        return EAssetResult::InvalidInstanceChain;
    }
    return EAssetResult::Success;
}

} // namespace Private

EAssetResult FMaterialAsset::CreateValidated(
    FMaterialAssetDesc Desc,
    FMaterialAsset& OutAsset,
    FAssetDiagnosticList* Diagnostics)
{
    OutAsset = {};
    const EAssetResult Result =
        Private::ValidateMaterialAsset(Desc, Diagnostics);
    if (Result != EAssetResult::Success)
    {
        return Result;
    }
    OutAsset.Desc_ = std::move(Desc);
    return EAssetResult::Success;
}

Core::FString FMaterialAsset::GetAssetType() const
{
    return TAssetTypeTraits<FMaterialAsset>::GetAssetType();
}
const FMaterialAssetDesc& FMaterialAsset::GetDesc() const noexcept { return Desc_; }

EAssetResult FMaterialInstanceAsset::CreateValidated(
    FMaterialInstanceAssetDesc Desc,
    FMaterialInstanceAsset& OutAsset,
    FAssetDiagnosticList* Diagnostics)
{
    OutAsset = {};
    const EAssetResult Result =
        Private::ValidateMaterialInstanceAsset(Desc, Diagnostics);
    if (Result != EAssetResult::Success)
    {
        return Result;
    }
    OutAsset.Desc_ = std::move(Desc);
    return EAssetResult::Success;
}

Core::FString FMaterialInstanceAsset::GetAssetType() const
{
    return TAssetTypeTraits<FMaterialInstanceAsset>::GetAssetType();
}
const FMaterialInstanceAssetDesc&
FMaterialInstanceAsset::GetDesc() const noexcept { return Desc_; }

} // namespace Stoner::Asset
