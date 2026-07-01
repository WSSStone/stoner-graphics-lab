#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FMaterialInstance.h"
#include "Renderer/FMaterialResourceRequirement.h"
#include "Renderer/FShaderLibrary.h"

namespace Stoner::Renderer
{

struct FMaterialShaderBinding
{
    Stoner::Core::FString MaterialName;
    Stoner::Core::FString ShaderId;
    Stoner::Core::FString VariantId;
    Stoner::Core::FString PermutationKey;
    FMaterialParameterSet ResolvedParameters;
    Stoner::Core::TArray<FMaterialResourceRequirement> ResourceRequirements;

    [[nodiscard]] Stoner::Core::FString Dump() const;
};

[[nodiscard]] EMaterialResult ResolveMaterialShaderBinding(const FMaterial& Material, const FShaderLibrary& Library,
    FMaterialShaderBinding& OutBinding, FMaterialDiagnosticLog* Diagnostics = nullptr);
[[nodiscard]] EMaterialResult ResolveMaterialShaderBinding(const FMaterialInstance& Instance, const FShaderLibrary& Library,
    FMaterialShaderBinding& OutBinding, FMaterialDiagnosticLog* Diagnostics = nullptr);

} // namespace Stoner::Renderer
