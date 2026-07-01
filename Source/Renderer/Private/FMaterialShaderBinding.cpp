#include "Renderer/FMaterialShaderBinding.h"

#include <sstream>

namespace Stoner::Renderer
{

namespace
{

EMaterialResult ResolveBindingForParameters(const Stoner::Core::FString& MaterialName,
    const Stoner::Core::FString& ShaderReference,
    const FShaderPermutation& Permutation,
    const FMaterialParameterSet& Parameters,
    const FShaderLibrary& Library,
    FMaterialShaderBinding& OutBinding,
    FMaterialDiagnosticLog* Diagnostics)
{
    const FShaderVariant* Variant = nullptr;
    const EMaterialResult VariantResult = Library.ResolveVariant(ShaderReference, Permutation, Variant, Diagnostics);
    if (VariantResult != EMaterialResult::Success)
    {
        return VariantResult;
    }

    const FShaderRecord* Record = Library.FindRecord(ShaderReference);
    if (Record == nullptr)
    {
        return EMaterialResult::NotFound;
    }

    const EMaterialResult RequiredResult = Library.ValidateRequiredParameters(*Record, Parameters, Diagnostics);
    if (RequiredResult != EMaterialResult::Success)
    {
        return RequiredResult;
    }

    OutBinding.MaterialName = MaterialName;
    OutBinding.ShaderId = ShaderReference;
    OutBinding.VariantId = Variant->VariantId;
    OutBinding.PermutationKey = Permutation.GetCanonicalKey();
    OutBinding.ResolvedParameters = Parameters;
    return ExtractResourceRequirementsFromParameters(Parameters, MaterialName, OutBinding.ResourceRequirements, Diagnostics);
}

} // namespace

Stoner::Core::FString FMaterialShaderBinding::Dump() const
{
    std::ostringstream Stream;
    Stream << "MaterialShaderBinding " << MaterialName.CStr() << '\n'
        << "  Shader=" << ShaderId.CStr() << '\n'
        << "  Variant=" << VariantId.CStr() << '\n'
        << "  Permutation=" << PermutationKey.CStr() << '\n'
        << ResolvedParameters.Dump().CStr()
        << DumpResourceRequirements(ResourceRequirements).CStr();
    return Stoner::Core::FString(Stream.str());
}

EMaterialResult ResolveMaterialShaderBinding(const FMaterial& Material, const FShaderLibrary& Library,
    FMaterialShaderBinding& OutBinding, FMaterialDiagnosticLog* Diagnostics)
{
    if (Material.GetValidationState() == EMaterialValidationState::Invalidated)
    {
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::Invalidation,
                EMaterialResult::Invalidated, "MAT-BINDING-MATERIAL-INVALIDATED", Material.GetName(),
                "invalidated material cannot be bound");
        }
        return EMaterialResult::Invalidated;
    }
    return ResolveBindingForParameters(Material.GetName(), Material.GetShaderReference(), Material.GetPermutationRequest(),
        Material.GetParameters(), Library, OutBinding, Diagnostics);
}

EMaterialResult ResolveMaterialShaderBinding(const FMaterialInstance& Instance, const FShaderLibrary& Library,
    FMaterialShaderBinding& OutBinding, FMaterialDiagnosticLog* Diagnostics)
{
    const FMaterial* Root = Instance.FindRootMaterial(Diagnostics);
    if (Root == nullptr)
    {
        return EMaterialResult::NotFound;
    }

    FMaterialParameterSet Effective;
    const EMaterialResult ResolveResult = Instance.ResolveEffectiveParameters(Effective, Diagnostics);
    if (ResolveResult != EMaterialResult::Success)
    {
        return ResolveResult;
    }

    return ResolveBindingForParameters(Instance.GetName(), Root->GetShaderReference(), Root->GetPermutationRequest(),
        Effective, Library, OutBinding, Diagnostics);
}

} // namespace Stoner::Renderer
