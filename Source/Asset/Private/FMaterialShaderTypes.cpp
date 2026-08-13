#include "Asset/FMaterialShaderTypes.h"

#include <algorithm>
#include <string>

namespace Stoner::Asset
{

EAssetResult FMaterialTextureBinding::Create(
    const FAssetId& TextureId,
    Core::uint32 InTexCoordSet,
    FMaterialSamplerIntent InSampler,
    FMaterialTextureBinding& OutBinding)
{
    OutBinding = {};
    if (InTexCoordSet > 1 ||
        !IsValidAssetSamplerFilter(InSampler.MinFilter) ||
        !IsValidAssetSamplerFilter(InSampler.MagFilter) ||
        !IsValidAssetSamplerMipFilter(InSampler.MipFilter) ||
        !IsValidAssetSamplerAddressMode(InSampler.AddressU) ||
        !IsValidAssetSamplerAddressMode(InSampler.AddressV))
    {
        return EAssetResult::InvalidMaterialAsset;
    }
    const EAssetResult ReferenceResult =
        TSoftAssetRef<FTextureAsset>::Create(TextureId, OutBinding.Texture);
    if (ReferenceResult != EAssetResult::Success)
    {
        return ReferenceResult;
    }
    OutBinding.TexCoordSet = InTexCoordSet;
    OutBinding.Sampler = InSampler;
    return EAssetResult::Success;
}

Core::FString FShaderPermutationKey::ToString() const
{
    Core::TArray<Core::FString> Sorted = Flags;
    std::sort(Sorted.begin(), Sorted.end());
    std::string Result;
    for (const Core::FString& Flag : Sorted)
    {
        Result += std::to_string(Flag.Len());
        Result.push_back(':');
        Result += Flag.ToStdString();
        Result.push_back(';');
    }
    return Core::FString(std::move(Result));
}

FMaterialAssetParameterValue FMaterialAssetParameterValue::FromScalar(float Value)
{
    return {EMaterialAssetParameterType::Scalar, Value};
}

FMaterialAssetParameterValue FMaterialAssetParameterValue::FromVector(
    Core::FVector4 Value)
{
    return {EMaterialAssetParameterType::Vector, Value};
}

FMaterialAssetParameterValue FMaterialAssetParameterValue::FromColor(
    Core::FColor Value)
{
    return {EMaterialAssetParameterType::Color, Value};
}

FMaterialAssetParameterValue FMaterialAssetParameterValue::FromTexture(
    FAssetId Value)
{
    return {EMaterialAssetParameterType::TextureReference, std::move(Value)};
}

FMaterialAssetParameterValue FMaterialAssetParameterValue::FromTextureBinding(
    FMaterialTextureBinding Value)
{
    return {
        EMaterialAssetParameterType::TextureBinding,
        std::move(Value)};
}

} // namespace Stoner::Asset
