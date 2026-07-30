#include "Asset/FMaterialShaderTypes.h"

#include <algorithm>
#include <string>

namespace Stoner::Asset
{

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

} // namespace Stoner::Asset
