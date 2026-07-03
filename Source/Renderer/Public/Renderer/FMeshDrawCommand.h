#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FForwardDiagnostics.h"
#include "Renderer/FForwardViewData.h"
#include "Renderer/FMaterial.h"
#include "Renderer/FMaterialResourceRequirement.h"

namespace Stoner::Renderer
{

enum class EForwardDrawPass
{
    Opaque,
    Transparent
};

struct FForwardSurfaceExtensionSlot
{
    Stoner::Core::FString Name;
    Stoner::Core::FString ValueSummary;
};

struct FForwardPBRSurfaceInputs
{
    bool bHasBaseColor = false;
    bool bHasMetallic = false;
    bool bHasRoughness = false;
    bool bHasNormal = false;
    bool bHasOcclusion = false;
    bool bHasEmissive = false;
    bool bHasAlpha = false;
    Stoner::Core::TArray<FForwardSurfaceExtensionSlot> ExtensionSlots;

    [[nodiscard]] bool IsComplete(FForwardDiagnosticLog* Diagnostics = nullptr,
        const Stoner::Core::FString& SubjectName = {}) const;
};

struct FForwardMaterialBinding
{
    Stoner::Core::uint32 MaterialId = 0;
    Stoner::Core::FString MaterialName;
    EMaterialDomain Domain = EMaterialDomain::Surface;
    EMaterialBlendMode BlendMode = EMaterialBlendMode::Opaque;
    bool bHasMaterialBinding = false;
    bool bHasShaderBinding = false;
    FForwardPBRSurfaceInputs SurfaceInputs;
    Stoner::Core::TArray<FMaterialResourceRequirement> ResourceRequirements;
};

struct FMeshDrawCandidate
{
    Stoner::Core::uint32 ObjectId = 0;
    Stoner::Core::uint32 MeshId = 0;
    Stoner::Core::FString DebugName;
    Stoner::Core::FVector3 WorldPosition = Stoner::Core::FVector3::Zero();
    bool bWantsOpaque = true;
    bool bWantsTransparent = false;
    FForwardMaterialBinding MaterialBinding;
};

struct FMeshDrawSortKey
{
    Stoner::Core::uint32 MaterialId = 0;
    Stoner::Core::uint32 MeshId = 0;
    Stoner::Core::uint32 ObjectId = 0;
    float CameraSpaceDepth = 0.0f;
};

class FMeshDrawCommand
{
public:
    FMeshDrawCommand() = default;
    FMeshDrawCommand(FMeshDrawCandidate InCandidate, EForwardDrawPass InPass, const FForwardViewData& View);

    [[nodiscard]] const FMeshDrawCandidate& GetCandidate() const noexcept;
    [[nodiscard]] const FForwardMaterialBinding& GetMaterialBinding() const noexcept;
    [[nodiscard]] EForwardDrawPass GetPass() const noexcept;
    [[nodiscard]] const FMeshDrawSortKey& GetSortKey() const noexcept;
    [[nodiscard]] EForwardValidationState GetValidationState() const noexcept;
    [[nodiscard]] Stoner::Core::uint32 GetObjectId() const noexcept;
    [[nodiscard]] Stoner::Core::uint32 GetMeshId() const noexcept;
    [[nodiscard]] Stoner::Core::uint32 GetMaterialId() const noexcept;
    [[nodiscard]] float GetCameraSpaceDepth() const noexcept;
    [[nodiscard]] Stoner::Core::FString GetStableIdentity() const;
    [[nodiscard]] Stoner::Core::FString Dump() const;

    void SetValidationState(EForwardValidationState NewState) noexcept;

private:
    FMeshDrawCandidate Candidate;
    EForwardDrawPass Pass = EForwardDrawPass::Opaque;
    FMeshDrawSortKey SortKey;
    EForwardValidationState ValidationState = EForwardValidationState::Draft;
};

[[nodiscard]] bool IsForwardOpaqueCompatible(const FForwardMaterialBinding& Binding,
    FForwardDiagnosticLog* Diagnostics = nullptr);
[[nodiscard]] bool IsForwardTransparentCompatible(const FForwardMaterialBinding& Binding,
    FForwardDiagnosticLog* Diagnostics = nullptr);
[[nodiscard]] bool ValidateForwardMeshDrawCandidate(const FMeshDrawCandidate& Candidate,
    EForwardDrawPass Pass,
    FForwardDiagnosticLog* Diagnostics = nullptr);
void SortForwardOpaqueDraws(Stoner::Core::TArray<FMeshDrawCommand>& Draws);
void SortForwardTransparentDraws(Stoner::Core::TArray<FMeshDrawCommand>& Draws);
[[nodiscard]] const char* ToString(EForwardDrawPass Pass) noexcept;

} // namespace Stoner::Renderer
