#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FMaterialDiagnostics.h"
#include "Renderer/FMaterialParameterSet.h"
#include "Renderer/FShaderPermutation.h"

namespace Stoner::Renderer
{

enum class EMaterialDomain
{
    Surface,
    PostProcess,
    UI,
    Decal
};

enum class EMaterialBlendMode
{
    Opaque,
    Translucent,
    Additive,
    Masked
};

struct FMaterialRenderStateSummary
{
    bool bDepthTest = true;
    bool bDepthWrite = true;
    bool bTwoSided = false;
};

struct FMaterialDesc
{
    Stoner::Core::FString Name;
    Stoner::Core::FString ShaderReference;
    EMaterialDomain Domain = EMaterialDomain::Surface;
    EMaterialBlendMode BlendMode = EMaterialBlendMode::Opaque;
    FMaterialRenderStateSummary RenderState;
    FShaderPermutation PermutationRequest;
    FMaterialParameterSet Parameters;
};

class FMaterial
{
public:
    FMaterial() = default;
    explicit FMaterial(FMaterialDesc InDesc);

    [[nodiscard]] EMaterialResult Validate(FMaterialDiagnosticLog* Diagnostics = nullptr);
    void Reset(FMaterialDesc InDesc = {});
    void Invalidate();

    [[nodiscard]] const FMaterialDesc& GetDesc() const noexcept;
    [[nodiscard]] const Stoner::Core::FString& GetName() const noexcept;
    [[nodiscard]] const Stoner::Core::FString& GetShaderReference() const noexcept;
    [[nodiscard]] const FShaderPermutation& GetPermutationRequest() const noexcept;
    [[nodiscard]] const FMaterialParameterSet& GetParameters() const noexcept;
    [[nodiscard]] EMaterialValidationState GetValidationState() const noexcept;
    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] Stoner::Core::FString Dump() const;

private:
    FMaterialDesc Desc;
    EMaterialValidationState ValidationState = EMaterialValidationState::Draft;
};

[[nodiscard]] const char* ToString(EMaterialDomain Domain) noexcept;
[[nodiscard]] const char* ToString(EMaterialBlendMode BlendMode) noexcept;
[[nodiscard]] bool IsSupportedMaterialDomainBlend(EMaterialDomain Domain, EMaterialBlendMode BlendMode) noexcept;

} // namespace Stoner::Renderer
