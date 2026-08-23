#include "FMaterialShaderSchemaValidator.h"

#include "FMaterialAssetValidator.h"
#include "FMaterialDependencyExtractor.h"
#include "FShaderProgramValidator.h"

namespace Stoner::Asset::Private
{

EAssetResult ValidateMaterialShaderDefinition(
    FMaterialShaderDefinition& Definition,
    FAssetDiagnosticList* Diagnostics)
{
    switch (Definition.Kind)
    {
    case EMaterialShaderDefinitionKind::Shader:
    {
        auto& Desc = std::get<FShaderAssetDesc>(Definition.Value);
        EAssetResult Result = ExtractShaderDependencies(Desc);
        return Result == EAssetResult::Success
            ? ValidateShaderProgram(Desc, Diagnostics)
            : Result;
    }
    case EMaterialShaderDefinitionKind::Material:
    {
        auto& Desc = std::get<FMaterialAssetDesc>(Definition.Value);
        EAssetResult Result = ExtractMaterialDependencies(Desc);
        return Result == EAssetResult::Success
            ? ValidateMaterialAsset(Desc, Diagnostics)
            : Result;
    }
    case EMaterialShaderDefinitionKind::MaterialInstance:
    {
        auto& Desc = std::get<FMaterialInstanceAssetDesc>(Definition.Value);
        EAssetResult Result = ExtractMaterialInstanceDependencies(Desc);
        return Result == EAssetResult::Success
            ? ValidateMaterialInstanceAsset(Desc, Diagnostics)
            : Result;
    }
    }
    return EAssetResult::InvalidDefinition;
}

} // namespace Stoner::Asset::Private
