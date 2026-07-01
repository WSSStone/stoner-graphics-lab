#include "Renderer/FMaterial.h"

#include <sstream>

namespace Stoner::Renderer
{

FMaterial::FMaterial(FMaterialDesc InDesc)
    : Desc(std::move(InDesc))
{
}

EMaterialResult FMaterial::Validate(FMaterialDiagnosticLog* Diagnostics)
{
    if (ValidationState == EMaterialValidationState::Invalidated)
    {
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::Invalidation,
                EMaterialResult::Invalidated, "MAT-MATERIAL-INVALIDATED", Desc.Name,
                "invalidated material cannot be validated");
        }
        return EMaterialResult::Invalidated;
    }

    if (Desc.Name.IsEmpty())
    {
        ValidationState = EMaterialValidationState::Invalid;
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::Material,
                EMaterialResult::ValidationFailed, "MAT-MATERIAL-NAME-EMPTY", "<empty>", "material name is required");
        }
        return EMaterialResult::ValidationFailed;
    }

    if (Desc.ShaderReference.IsEmpty())
    {
        ValidationState = EMaterialValidationState::Invalid;
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::Material,
                EMaterialResult::ValidationFailed, "MAT-MATERIAL-SHADER-EMPTY", Desc.Name, "shader reference is required");
        }
        return EMaterialResult::ValidationFailed;
    }

    if (!IsSupportedMaterialDomainBlend(Desc.Domain, Desc.BlendMode))
    {
        ValidationState = EMaterialValidationState::Invalid;
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::Material,
                EMaterialResult::UnsupportedCombination, "MAT-MATERIAL-DOMAIN-BLEND", Desc.Name,
                "unsupported material domain and blend mode combination");
        }
        return EMaterialResult::UnsupportedCombination;
    }

    Stoner::Core::TArray<FMaterialResourceRequirement> Requirements;
    const EMaterialResult ResourceResult = ExtractResourceRequirementsFromParameters(Desc.Parameters, Desc.Name, Requirements, Diagnostics);
    if (ResourceResult != EMaterialResult::Success)
    {
        ValidationState = EMaterialValidationState::Invalid;
        return ResourceResult;
    }

    ValidationState = EMaterialValidationState::Valid;
    return EMaterialResult::Success;
}

void FMaterial::Reset(FMaterialDesc InDesc)
{
    Desc = std::move(InDesc);
    ValidationState = EMaterialValidationState::Draft;
}

void FMaterial::Invalidate()
{
    ValidationState = EMaterialValidationState::Invalidated;
}

const FMaterialDesc& FMaterial::GetDesc() const noexcept
{
    return Desc;
}

const Stoner::Core::FString& FMaterial::GetName() const noexcept
{
    return Desc.Name;
}

const Stoner::Core::FString& FMaterial::GetShaderReference() const noexcept
{
    return Desc.ShaderReference;
}

const FShaderPermutation& FMaterial::GetPermutationRequest() const noexcept
{
    return Desc.PermutationRequest;
}

const FMaterialParameterSet& FMaterial::GetParameters() const noexcept
{
    return Desc.Parameters;
}

EMaterialValidationState FMaterial::GetValidationState() const noexcept
{
    return ValidationState;
}

bool FMaterial::IsValid() const noexcept
{
    return ValidationState == EMaterialValidationState::Valid;
}

Stoner::Core::FString FMaterial::Dump() const
{
    std::ostringstream Stream;
    Stream << "Material " << Desc.Name.CStr() << '\n'
        << "  State=" << ToString(ValidationState) << '\n'
        << "  Shader=" << Desc.ShaderReference.CStr() << '\n'
        << "  Domain=" << ToString(Desc.Domain) << '\n'
        << "  BlendMode=" << ToString(Desc.BlendMode) << '\n'
        << "  RenderState=DepthTest:" << (Desc.RenderState.bDepthTest ? "true" : "false")
        << ",DepthWrite:" << (Desc.RenderState.bDepthWrite ? "true" : "false")
        << ",TwoSided:" << (Desc.RenderState.bTwoSided ? "true" : "false") << '\n'
        << "  Permutation=" << Desc.PermutationRequest.GetCanonicalKey().CStr() << '\n'
        << Desc.Parameters.Dump().CStr();
    return Stoner::Core::FString(Stream.str());
}

const char* ToString(EMaterialDomain Domain) noexcept
{
    switch (Domain)
    {
    case EMaterialDomain::Surface: return "Surface";
    case EMaterialDomain::PostProcess: return "PostProcess";
    case EMaterialDomain::UI: return "UI";
    case EMaterialDomain::Decal: return "Decal";
    }
    return "Unknown";
}

const char* ToString(EMaterialBlendMode BlendMode) noexcept
{
    switch (BlendMode)
    {
    case EMaterialBlendMode::Opaque: return "Opaque";
    case EMaterialBlendMode::Translucent: return "Translucent";
    case EMaterialBlendMode::Additive: return "Additive";
    case EMaterialBlendMode::Masked: return "Masked";
    }
    return "Unknown";
}

bool IsSupportedMaterialDomainBlend(EMaterialDomain Domain, EMaterialBlendMode BlendMode) noexcept
{
    switch (Domain)
    {
    case EMaterialDomain::Surface:
        return true;
    case EMaterialDomain::PostProcess:
        return BlendMode == EMaterialBlendMode::Opaque || BlendMode == EMaterialBlendMode::Additive;
    case EMaterialDomain::UI:
        return BlendMode == EMaterialBlendMode::Translucent || BlendMode == EMaterialBlendMode::Masked;
    case EMaterialDomain::Decal:
        return BlendMode == EMaterialBlendMode::Translucent || BlendMode == EMaterialBlendMode::Masked;
    }
    return false;
}

} // namespace Stoner::Renderer
