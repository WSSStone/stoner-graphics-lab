#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FMaterial.h"

namespace Stoner::Renderer
{

struct FMaterialInstanceDesc
{
    Stoner::Core::FString Name;
    const FMaterial* ParentMaterial = nullptr;
    const class FMaterialInstance* ParentInstance = nullptr;
    FMaterialParameterSet Overrides;
};

class FMaterialInstance
{
public:
    FMaterialInstance() = default;
    explicit FMaterialInstance(FMaterialInstanceDesc InDesc);

    [[nodiscard]] EMaterialResult Validate(FMaterialDiagnosticLog* Diagnostics = nullptr);
    [[nodiscard]] EMaterialResult ResolveEffectiveParameters(FMaterialParameterSet& OutParameters,
        FMaterialDiagnosticLog* Diagnostics = nullptr) const;
    [[nodiscard]] const FMaterial* FindRootMaterial(FMaterialDiagnosticLog* Diagnostics = nullptr) const;

    void Reset(FMaterialInstanceDesc InDesc = {});
    void Invalidate();
    void SetParentMaterial(const FMaterial* Parent) noexcept;
    void SetParentInstance(const FMaterialInstance* Parent) noexcept;

    [[nodiscard]] const Stoner::Core::FString& GetName() const noexcept;
    [[nodiscard]] const FMaterialParameterSet& GetOverrides() const noexcept;
    [[nodiscard]] EMaterialValidationState GetValidationState() const noexcept;
    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] Stoner::Core::FString Dump() const;

private:
    [[nodiscard]] EMaterialResult ValidateAcyclic(FMaterialDiagnosticLog* Diagnostics) const;

    FMaterialInstanceDesc Desc;
    EMaterialValidationState ValidationState = EMaterialValidationState::Draft;
};

} // namespace Stoner::Renderer
