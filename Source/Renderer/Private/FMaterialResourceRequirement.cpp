#include "Renderer/FMaterialResourceRequirement.h"

#include "Renderer/FMaterial.h"
#include "Renderer/FMaterialInstance.h"
#include "Renderer/FMaterialParameterSet.h"

#include <algorithm>
#include <sstream>

namespace Stoner::Renderer
{

FMaterialResourceReference FMaterialResourceReference::Texture(Stoner::Core::FString InReferenceId,
    EMaterialResourceAccess InAccess)
{
    return {std::move(InReferenceId), EMaterialResourceKind::Texture, InAccess, false, false};
}

bool FMaterialResourceReference::IsAbstract() const noexcept
{
    return !ReferenceId.IsEmpty() && !bLiveResource && !bGraphLocalHandle;
}

const char* ToString(EMaterialResourceKind Kind) noexcept
{
    switch (Kind)
    {
    case EMaterialResourceKind::Unspecified: return "Unspecified";
    case EMaterialResourceKind::Texture: return "Texture";
    case EMaterialResourceKind::Buffer: return "Buffer";
    }
    return "Unknown";
}

const char* ToString(EMaterialResourceAccess Access) noexcept
{
    switch (Access)
    {
    case EMaterialResourceAccess::Read: return "Read";
    case EMaterialResourceAccess::SampledRead: return "SampledRead";
    }
    return "Unknown";
}

Stoner::Core::FString FormatResourceReference(const FMaterialResourceReference& Reference)
{
    std::ostringstream Stream;
    Stream << Reference.ReferenceId.CStr() << "{kind=" << ToString(Reference.Kind)
        << ",access=" << ToString(Reference.Access) << '}';
    return Stoner::Core::FString(Stream.str());
}

Stoner::Core::FString DumpResourceRequirements(const Stoner::Core::TArray<FMaterialResourceRequirement>& Requirements)
{
    std::ostringstream Stream;
    Stream << "ResourceRequirements\n";
    for (const FMaterialResourceRequirement& Requirement : Requirements)
    {
        Stream << "  " << Requirement.ParameterName.CStr() << '='
            << FormatResourceReference(Requirement.Reference).CStr()
            << ",source=" << Requirement.SourceName.CStr() << '\n';
    }
    return Stoner::Core::FString(Stream.str());
}

EMaterialResult ExtractResourceRequirementsFromParameters(const FMaterialParameterSet& Parameters,
    const Stoner::Core::FString& SourceName,
    Stoner::Core::TArray<FMaterialResourceRequirement>& OutRequirements,
    FMaterialDiagnosticLog* Diagnostics)
{
    OutRequirements.clear();
    for (const FMaterialParameter& Parameter : Parameters.GetParameters())
    {
        if (Parameter.Value.Type != EMaterialParameterValueType::ResourceReference)
        {
            continue;
        }

        if (!Parameter.Value.ResourceReference.IsAbstract())
        {
            if (Diagnostics != nullptr)
            {
                Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::ResourceRequirement,
                    EMaterialResult::ValidationFailed, "MAT-RESOURCE-NOT-ABSTRACT", Parameter.Name,
                    "resource parameter must be an abstract Renderer reference");
            }
            return EMaterialResult::ValidationFailed;
        }

        OutRequirements.push_back({Parameter.Name, Parameter.Value.ResourceReference, SourceName});
    }

    std::sort(OutRequirements.begin(), OutRequirements.end(), [](const FMaterialResourceRequirement& Left, const FMaterialResourceRequirement& Right) {
        return Left.ParameterName < Right.ParameterName;
    });
    return EMaterialResult::Success;
}

EMaterialResult ExtractMaterialResourceRequirements(const FMaterial& Material,
    Stoner::Core::TArray<FMaterialResourceRequirement>& OutRequirements,
    FMaterialDiagnosticLog* Diagnostics)
{
    if (Material.GetValidationState() == EMaterialValidationState::Invalidated)
    {
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::Invalidation,
                EMaterialResult::Invalidated, "MAT-RESOURCE-INVALIDATED", Material.GetName(),
                "invalidated material cannot report resource requirements");
        }
        return EMaterialResult::Invalidated;
    }
    return ExtractResourceRequirementsFromParameters(Material.GetParameters(), Material.GetName(), OutRequirements, Diagnostics);
}

EMaterialResult ExtractMaterialResourceRequirements(const FMaterialInstance& Instance,
    Stoner::Core::TArray<FMaterialResourceRequirement>& OutRequirements,
    FMaterialDiagnosticLog* Diagnostics)
{
    FMaterialParameterSet Effective;
    const EMaterialResult ResolveResult = Instance.ResolveEffectiveParameters(Effective, Diagnostics);
    if (ResolveResult != EMaterialResult::Success)
    {
        return ResolveResult;
    }
    return ExtractResourceRequirementsFromParameters(Effective, Instance.GetName(), OutRequirements, Diagnostics);
}

} // namespace Stoner::Renderer
