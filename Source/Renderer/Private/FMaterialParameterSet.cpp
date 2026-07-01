#include "Renderer/FMaterialParameterSet.h"

#include <algorithm>
#include <sstream>

namespace Stoner::Renderer
{

namespace
{

void SortParameters(Stoner::Core::TArray<FMaterialParameter>& Parameters)
{
    std::sort(Parameters.begin(), Parameters.end(), [](const FMaterialParameter& Left, const FMaterialParameter& Right) {
        return Left.Name < Right.Name;
    });
}

} // namespace

FMaterialParameterValue FMaterialParameterValue::FromScalar(float Value) noexcept
{
    FMaterialParameterValue Out;
    Out.Type = EMaterialParameterValueType::Scalar;
    Out.Scalar = Value;
    return Out;
}

FMaterialParameterValue FMaterialParameterValue::FromVector(Stoner::Core::FVector4 Value) noexcept
{
    FMaterialParameterValue Out;
    Out.Type = EMaterialParameterValueType::Vector;
    Out.Vector = Value;
    return Out;
}

FMaterialParameterValue FMaterialParameterValue::FromColor(Stoner::Core::FColor Value) noexcept
{
    FMaterialParameterValue Out;
    Out.Type = EMaterialParameterValueType::Color;
    Out.Color = Value;
    return Out;
}

FMaterialParameterValue FMaterialParameterValue::FromResourceReference(FMaterialResourceReference Value) noexcept
{
    FMaterialParameterValue Out;
    Out.Type = EMaterialParameterValueType::ResourceReference;
    Out.ResourceReference = std::move(Value);
    return Out;
}

EMaterialResult FMaterialParameterSet::AddParameter(Stoner::Core::FString Name, FMaterialParameterValue Value,
    FMaterialDiagnosticLog* Diagnostics)
{
    if (Name.IsEmpty())
    {
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::Parameter,
                EMaterialResult::ValidationFailed, "MAT-PARAM-NAME-EMPTY", "<empty>", "parameter name is required");
        }
        return EMaterialResult::ValidationFailed;
    }

    if (Contains(Name))
    {
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::Parameter,
                EMaterialResult::DuplicateName, "MAT-PARAM-DUPLICATE", Name, "duplicate material parameter name");
        }
        return EMaterialResult::DuplicateName;
    }

    Parameters.push_back({std::move(Name), std::move(Value)});
    SortParameters(Parameters);
    return EMaterialResult::Success;
}

EMaterialResult FMaterialParameterSet::SetParameter(Stoner::Core::FString Name, FMaterialParameterValue Value,
    FMaterialDiagnosticLog* Diagnostics)
{
    for (FMaterialParameter& Parameter : Parameters)
    {
        if (Parameter.Name == Name)
        {
            Parameter.Value = std::move(Value);
            return EMaterialResult::Success;
        }
    }
    return AddParameter(std::move(Name), std::move(Value), Diagnostics);
}

const FMaterialParameter* FMaterialParameterSet::FindParameter(const Stoner::Core::FString& Name) const noexcept
{
    for (const FMaterialParameter& Parameter : Parameters)
    {
        if (Parameter.Name == Name)
        {
            return &Parameter;
        }
    }
    return nullptr;
}

bool FMaterialParameterSet::Contains(const Stoner::Core::FString& Name) const noexcept
{
    return FindParameter(Name) != nullptr;
}

bool FMaterialParameterSet::IsEmpty() const noexcept
{
    return Parameters.empty();
}

void FMaterialParameterSet::Clear()
{
    Parameters.clear();
}

const Stoner::Core::TArray<FMaterialParameter>& FMaterialParameterSet::GetParameters() const noexcept
{
    return Parameters;
}

Stoner::Core::TArray<FMaterialParameter>& FMaterialParameterSet::GetMutableParameters() noexcept
{
    return Parameters;
}

Stoner::Core::FString FMaterialParameterSet::Dump() const
{
    std::ostringstream Stream;
    Stream << "Parameters\n";
    for (const FMaterialParameter& Parameter : Parameters)
    {
        Stream << "  " << Parameter.Name.CStr() << ':' << ToString(Parameter.Value.Type)
            << '=' << FormatParameterValue(Parameter.Value).CStr() << '\n';
    }
    return Stoner::Core::FString(Stream.str());
}

const char* ToString(EMaterialParameterValueType Type) noexcept
{
    switch (Type)
    {
    case EMaterialParameterValueType::Scalar: return "Scalar";
    case EMaterialParameterValueType::Vector: return "Vector";
    case EMaterialParameterValueType::Color: return "Color";
    case EMaterialParameterValueType::ResourceReference: return "ResourceReference";
    }
    return "Unknown";
}

Stoner::Core::FString FormatParameterValue(const FMaterialParameterValue& Value)
{
    std::ostringstream Stream;
    switch (Value.Type)
    {
    case EMaterialParameterValueType::Scalar:
        Stream << Value.Scalar;
        break;
    case EMaterialParameterValueType::Vector:
        Stream << '(' << Value.Vector.X << ',' << Value.Vector.Y << ',' << Value.Vector.Z << ',' << Value.Vector.W << ')';
        break;
    case EMaterialParameterValueType::Color:
        Stream << '(' << Value.Color.R << ',' << Value.Color.G << ',' << Value.Color.B << ',' << Value.Color.A << ')';
        break;
    case EMaterialParameterValueType::ResourceReference:
        Stream << FormatResourceReference(Value.ResourceReference).CStr();
        break;
    }
    return Stoner::Core::FString(Stream.str());
}

bool AreParameterValuesEqual(const FMaterialParameterValue& Left, const FMaterialParameterValue& Right) noexcept
{
    if (Left.Type != Right.Type)
    {
        return false;
    }

    switch (Left.Type)
    {
    case EMaterialParameterValueType::Scalar:
        return Left.Scalar == Right.Scalar;
    case EMaterialParameterValueType::Vector:
        return Left.Vector == Right.Vector;
    case EMaterialParameterValueType::Color:
        return Left.Color == Right.Color;
    case EMaterialParameterValueType::ResourceReference:
        return Left.ResourceReference.ReferenceId == Right.ResourceReference.ReferenceId &&
            Left.ResourceReference.Kind == Right.ResourceReference.Kind &&
            Left.ResourceReference.Access == Right.ResourceReference.Access &&
            Left.ResourceReference.bLiveResource == Right.ResourceReference.bLiveResource &&
            Left.ResourceReference.bGraphLocalHandle == Right.ResourceReference.bGraphLocalHandle;
    }
    return false;
}

bool AreParameterTypesEqual(const FMaterialParameterValue& Left, const FMaterialParameterValue& Right) noexcept
{
    return Left.Type == Right.Type;
}

} // namespace Stoner::Renderer
