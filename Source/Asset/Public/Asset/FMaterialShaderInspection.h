#pragma once

#include "Asset/FAssetDiagnostics.h"
#include "Core/FString.h"

namespace Stoner::Asset
{

class FShaderAsset;
class FShaderSourceAsset;
class FShaderPayloadAsset;
class FMaterialAsset;
class FMaterialInstanceAsset;
struct FResolvedMaterialAsset;
struct FSelectedShaderProgram;

[[nodiscard]] Core::FString InspectShaderAsset(const FShaderAsset& Asset);
[[nodiscard]] Core::FString InspectShaderSourceAsset(
    const FShaderSourceAsset& Asset);
[[nodiscard]] Core::FString InspectShaderPayloadAsset(
    const FShaderPayloadAsset& Asset);
[[nodiscard]] Core::FString InspectMaterialAsset(const FMaterialAsset& Asset);
[[nodiscard]] Core::FString InspectMaterialInstanceAsset(
    const FMaterialInstanceAsset& Asset);
[[nodiscard]] Core::FString InspectResolvedMaterial(
    const FResolvedMaterialAsset& Material);
[[nodiscard]] Core::FString InspectSelectedShader(
    const FSelectedShaderProgram& Selection);

} // namespace Stoner::Asset
