#pragma once

#include "Asset/FMaterialInstanceAsset.h"
#include "Asset/FMaterialShaderAssetLimits.h"
#include "Asset/FShaderAsset.h"

#include <variant>

namespace Stoner::Asset::Private
{

enum class EMaterialShaderDefinitionKind
{
    Shader,
    Material,
    MaterialInstance
};

struct FMaterialShaderDefinition
{
    EMaterialShaderDefinitionKind Kind =
        EMaterialShaderDefinitionKind::Shader;
    std::variant<
        FShaderAssetDesc,
        FMaterialAssetDesc,
        FMaterialInstanceAssetDesc> Value;
};

[[nodiscard]] EAssetResult ParseMaterialShaderDefinition(
    std::span<const Core::uint8> Bytes,
    const FMaterialShaderAssetLimits& Limits,
    FMaterialShaderDefinition& OutDefinition,
    FAssetDiagnosticList* Diagnostics);

[[nodiscard]] EAssetResult WriteMaterialShaderDefinition(
    FMaterialShaderDefinition& Definition,
    Core::FString& OutCanonical,
    FAssetDiagnosticList* Diagnostics);

} // namespace Stoner::Asset::Private
