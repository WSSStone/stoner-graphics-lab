#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FMaterialDiagnostics.h"

namespace Stoner::Renderer
{

class FMaterial;
class FMaterialInstance;
class FMaterialParameterSet;

enum class EMaterialResourceKind
{
    Unspecified,
    Texture,
    Buffer
};

enum class EMaterialResourceAccess
{
    Read,
    SampledRead
};

struct FMaterialResourceReference
{
    Stoner::Core::FString ReferenceId;
    EMaterialResourceKind Kind = EMaterialResourceKind::Unspecified;
    EMaterialResourceAccess Access = EMaterialResourceAccess::SampledRead;
    bool bLiveResource = false;
    bool bGraphLocalHandle = false;

    [[nodiscard]] static FMaterialResourceReference Texture(Stoner::Core::FString InReferenceId,
        EMaterialResourceAccess InAccess = EMaterialResourceAccess::SampledRead);
    [[nodiscard]] bool IsAbstract() const noexcept;
};

struct FMaterialResourceRequirement
{
    Stoner::Core::FString ParameterName;
    FMaterialResourceReference Reference;
    Stoner::Core::FString SourceName;
};

[[nodiscard]] const char* ToString(EMaterialResourceKind Kind) noexcept;
[[nodiscard]] const char* ToString(EMaterialResourceAccess Access) noexcept;
[[nodiscard]] Stoner::Core::FString FormatResourceReference(const FMaterialResourceReference& Reference);
[[nodiscard]] Stoner::Core::FString DumpResourceRequirements(const Stoner::Core::TArray<FMaterialResourceRequirement>& Requirements);
[[nodiscard]] EMaterialResult ExtractMaterialResourceRequirements(const FMaterial& Material,
    Stoner::Core::TArray<FMaterialResourceRequirement>& OutRequirements,
    FMaterialDiagnosticLog* Diagnostics = nullptr);
[[nodiscard]] EMaterialResult ExtractMaterialResourceRequirements(const FMaterialInstance& Instance,
    Stoner::Core::TArray<FMaterialResourceRequirement>& OutRequirements,
    FMaterialDiagnosticLog* Diagnostics = nullptr);
[[nodiscard]] EMaterialResult ExtractResourceRequirementsFromParameters(const FMaterialParameterSet& Parameters,
    const Stoner::Core::FString& SourceName,
    Stoner::Core::TArray<FMaterialResourceRequirement>& OutRequirements,
    FMaterialDiagnosticLog* Diagnostics = nullptr);

} // namespace Stoner::Renderer
