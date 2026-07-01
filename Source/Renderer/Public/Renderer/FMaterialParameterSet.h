#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FMaterialDiagnostics.h"
#include "Renderer/FMaterialResourceRequirement.h"

namespace Stoner::Renderer
{

enum class EMaterialParameterValueType
{
    Scalar,
    Vector,
    Color,
    ResourceReference
};

struct FMaterialParameterValue
{
    EMaterialParameterValueType Type = EMaterialParameterValueType::Scalar;
    float Scalar = 0.0f;
    Stoner::Core::FVector4 Vector;
    Stoner::Core::FColor Color;
    FMaterialResourceReference ResourceReference;

    [[nodiscard]] static FMaterialParameterValue FromScalar(float Value) noexcept;
    [[nodiscard]] static FMaterialParameterValue FromVector(Stoner::Core::FVector4 Value) noexcept;
    [[nodiscard]] static FMaterialParameterValue FromColor(Stoner::Core::FColor Value) noexcept;
    [[nodiscard]] static FMaterialParameterValue FromResourceReference(FMaterialResourceReference Value) noexcept;
};

struct FMaterialParameter
{
    Stoner::Core::FString Name;
    FMaterialParameterValue Value;
};

class FMaterialParameterSet
{
public:
    [[nodiscard]] EMaterialResult AddParameter(Stoner::Core::FString Name, FMaterialParameterValue Value,
        FMaterialDiagnosticLog* Diagnostics = nullptr);
    [[nodiscard]] EMaterialResult SetParameter(Stoner::Core::FString Name, FMaterialParameterValue Value,
        FMaterialDiagnosticLog* Diagnostics = nullptr);
    [[nodiscard]] const FMaterialParameter* FindParameter(const Stoner::Core::FString& Name) const noexcept;
    [[nodiscard]] bool Contains(const Stoner::Core::FString& Name) const noexcept;
    [[nodiscard]] bool IsEmpty() const noexcept;
    void Clear();

    [[nodiscard]] const Stoner::Core::TArray<FMaterialParameter>& GetParameters() const noexcept;
    [[nodiscard]] Stoner::Core::TArray<FMaterialParameter>& GetMutableParameters() noexcept;
    [[nodiscard]] Stoner::Core::FString Dump() const;

private:
    Stoner::Core::TArray<FMaterialParameter> Parameters;
};

[[nodiscard]] const char* ToString(EMaterialParameterValueType Type) noexcept;
[[nodiscard]] Stoner::Core::FString FormatParameterValue(const FMaterialParameterValue& Value);
[[nodiscard]] bool AreParameterValuesEqual(const FMaterialParameterValue& Left, const FMaterialParameterValue& Right) noexcept;
[[nodiscard]] bool AreParameterTypesEqual(const FMaterialParameterValue& Left, const FMaterialParameterValue& Right) noexcept;

} // namespace Stoner::Renderer
