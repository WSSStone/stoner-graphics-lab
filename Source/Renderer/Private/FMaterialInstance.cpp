#include "Renderer/FMaterialInstance.h"

#include <algorithm>
#include <sstream>

namespace Stoner::Renderer
{

FMaterialInstance::FMaterialInstance(FMaterialInstanceDesc InDesc)
    : Desc(std::move(InDesc))
{
}

EMaterialResult FMaterialInstance::Validate(FMaterialDiagnosticLog* Diagnostics)
{
    if (ValidationState == EMaterialValidationState::Invalidated)
    {
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::Invalidation,
                EMaterialResult::Invalidated, "MAT-INSTANCE-INVALIDATED", Desc.Name,
                "invalidated material instance cannot be validated");
        }
        return EMaterialResult::Invalidated;
    }

    if (Desc.Name.IsEmpty())
    {
        ValidationState = EMaterialValidationState::Invalid;
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::Instance,
                EMaterialResult::ValidationFailed, "MAT-INSTANCE-NAME-EMPTY", "<empty>", "material instance name is required");
        }
        return EMaterialResult::ValidationFailed;
    }

    const EMaterialResult CycleResult = ValidateAcyclic(Diagnostics);
    if (CycleResult != EMaterialResult::Success)
    {
        ValidationState = EMaterialValidationState::CycleDetected;
        return CycleResult;
    }

    const FMaterial* Root = FindRootMaterial(Diagnostics);
    if (Root == nullptr)
    {
        ValidationState = EMaterialValidationState::Invalid;
        return EMaterialResult::NotFound;
    }
    if (Root->GetValidationState() == EMaterialValidationState::Invalidated)
    {
        ValidationState = EMaterialValidationState::Invalid;
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::Invalidation,
                EMaterialResult::Invalidated, "MAT-INSTANCE-PARENT-INVALIDATED", Desc.Name,
                "parent material is invalidated");
        }
        return EMaterialResult::Invalidated;
    }

    for (const FMaterialParameter& Override : Desc.Overrides.GetParameters())
    {
        const FMaterialParameter* RootParameter = Root->GetParameters().FindParameter(Override.Name);
        if (RootParameter == nullptr)
        {
            ValidationState = EMaterialValidationState::Invalid;
            if (Diagnostics != nullptr)
            {
                Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::Parameter,
                    EMaterialResult::NotFound, "MAT-INSTANCE-UNKNOWN-PARAM", Override.Name,
                    "override parameter is not defined by the root material");
            }
            return EMaterialResult::NotFound;
        }
        if (!AreParameterTypesEqual(RootParameter->Value, Override.Value))
        {
            ValidationState = EMaterialValidationState::Invalid;
            if (Diagnostics != nullptr)
            {
                Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::Parameter,
                    EMaterialResult::TypeMismatch, "MAT-INSTANCE-PARAM-TYPE", Override.Name,
                    "override parameter type does not match root material parameter type");
            }
            return EMaterialResult::TypeMismatch;
        }
    }

    FMaterialParameterSet Effective;
    const EMaterialResult ResolveResult = ResolveEffectiveParameters(Effective, Diagnostics);
    if (ResolveResult != EMaterialResult::Success)
    {
        ValidationState = EMaterialValidationState::Invalid;
        return ResolveResult;
    }

    ValidationState = EMaterialValidationState::Valid;
    return EMaterialResult::Success;
}

EMaterialResult FMaterialInstance::ResolveEffectiveParameters(FMaterialParameterSet& OutParameters,
    FMaterialDiagnosticLog* Diagnostics) const
{
    const EMaterialResult CycleResult = ValidateAcyclic(Diagnostics);
    if (CycleResult != EMaterialResult::Success)
    {
        return CycleResult;
    }

    const FMaterial* Root = FindRootMaterial(Diagnostics);
    if (Root == nullptr)
    {
        return EMaterialResult::NotFound;
    }
    OutParameters = Root->GetParameters();

    Stoner::Core::TArray<const FMaterialInstance*> Chain;
    const FMaterialInstance* Current = this;
    while (Current != nullptr)
    {
        Chain.push_back(Current);
        Current = Current->Desc.ParentInstance;
    }
    std::reverse(Chain.begin(), Chain.end());

    for (const FMaterialInstance* Instance : Chain)
    {
        for (const FMaterialParameter& Override : Instance->Desc.Overrides.GetParameters())
        {
            const FMaterialParameter* Existing = OutParameters.FindParameter(Override.Name);
            if (Existing == nullptr)
            {
                if (Diagnostics != nullptr)
                {
                    Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::Parameter,
                        EMaterialResult::NotFound, "MAT-INSTANCE-UNKNOWN-PARAM", Override.Name,
                        "override parameter is not defined by the root material");
                }
                return EMaterialResult::NotFound;
            }
            if (!AreParameterTypesEqual(Existing->Value, Override.Value))
            {
                if (Diagnostics != nullptr)
                {
                    Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::Parameter,
                        EMaterialResult::TypeMismatch, "MAT-INSTANCE-PARAM-TYPE", Override.Name,
                        "override parameter type does not match root material parameter type");
                }
                return EMaterialResult::TypeMismatch;
            }
            (void)OutParameters.SetParameter(Override.Name, Override.Value, Diagnostics);
        }
    }

    return EMaterialResult::Success;
}

const FMaterial* FMaterialInstance::FindRootMaterial(FMaterialDiagnosticLog* Diagnostics) const
{
    const FMaterialInstance* Current = this;
    while (Current != nullptr)
    {
        if (Current->Desc.ParentMaterial != nullptr)
        {
            return Current->Desc.ParentMaterial;
        }
        Current = Current->Desc.ParentInstance;
    }

    if (Diagnostics != nullptr)
    {
        Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::Instance,
            EMaterialResult::NotFound, "MAT-INSTANCE-PARENT-MISSING", Desc.Name,
            "material instance requires a parent material or instance");
    }
    return nullptr;
}

void FMaterialInstance::Reset(FMaterialInstanceDesc InDesc)
{
    Desc = std::move(InDesc);
    ValidationState = EMaterialValidationState::Draft;
}

void FMaterialInstance::Invalidate()
{
    ValidationState = EMaterialValidationState::Invalidated;
}

void FMaterialInstance::SetParentMaterial(const FMaterial* Parent) noexcept
{
    Desc.ParentMaterial = Parent;
    Desc.ParentInstance = nullptr;
    ValidationState = EMaterialValidationState::Draft;
}

void FMaterialInstance::SetParentInstance(const FMaterialInstance* Parent) noexcept
{
    Desc.ParentInstance = Parent;
    Desc.ParentMaterial = nullptr;
    ValidationState = EMaterialValidationState::Draft;
}

const Stoner::Core::FString& FMaterialInstance::GetName() const noexcept
{
    return Desc.Name;
}

const FMaterialParameterSet& FMaterialInstance::GetOverrides() const noexcept
{
    return Desc.Overrides;
}

EMaterialValidationState FMaterialInstance::GetValidationState() const noexcept
{
    return ValidationState;
}

bool FMaterialInstance::IsValid() const noexcept
{
    return ValidationState == EMaterialValidationState::Valid;
}

Stoner::Core::FString FMaterialInstance::Dump() const
{
    std::ostringstream Stream;
    Stream << "MaterialInstance " << Desc.Name.CStr() << '\n'
        << "  State=" << ToString(ValidationState) << '\n'
        << "  ParentMaterial=" << (Desc.ParentMaterial != nullptr ? Desc.ParentMaterial->GetName().CStr() : "<none>") << '\n'
        << "  ParentInstance=" << (Desc.ParentInstance != nullptr ? Desc.ParentInstance->GetName().CStr() : "<none>") << '\n'
        << "Overrides\n";
    for (const FMaterialParameter& Override : Desc.Overrides.GetParameters())
    {
        Stream << "  " << Override.Name.CStr() << ':' << ToString(Override.Value.Type)
            << '=' << FormatParameterValue(Override.Value).CStr() << '\n';
    }
    return Stoner::Core::FString(Stream.str());
}

EMaterialResult FMaterialInstance::ValidateAcyclic(FMaterialDiagnosticLog* Diagnostics) const
{
    Stoner::Core::TArray<const FMaterialInstance*> Seen;
    const FMaterialInstance* Current = this;
    while (Current != nullptr)
    {
        if (std::find(Seen.begin(), Seen.end(), Current) != Seen.end())
        {
            if (Diagnostics != nullptr)
            {
                Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::Instance,
                    EMaterialResult::CycleDetected, "MAT-INSTANCE-CYCLE", Current->GetName(),
                    "material instance inheritance cycle detected");
            }
            return EMaterialResult::CycleDetected;
        }
        Seen.push_back(Current);
        Current = Current->Desc.ParentInstance;
    }
    return EMaterialResult::Success;
}

} // namespace Stoner::Renderer
