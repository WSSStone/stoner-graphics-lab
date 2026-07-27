#include "Renderer/FMeshDrawCommand.h"

#include <algorithm>
#include <sstream>

namespace Stoner::Renderer
{

namespace
{

void AddMaterialDiagnostic(FForwardDiagnosticLog* Diagnostics,
    const char* Code,
    const Stoner::Core::FString& Subject,
    const char* Message)
{
    if (Diagnostics != nullptr)
    {
        Diagnostics->Add(EForwardDiagnosticSeverity::Error, EForwardDiagnosticCategory::Material,
            EForwardResult::InvalidMaterial, Code, Subject, Message);
    }
}

} // namespace

bool FForwardPBRSurfaceInputs::IsComplete(FForwardDiagnosticLog* Diagnostics, const Stoner::Core::FString& SubjectName) const
{
    const struct
    {
        bool bPresent;
        const char* Code;
        const char* Message;
    } Requirements[] = {
        {bHasBaseColor, "FWD-PBR-BASECOLOR", "base color input is required"},
        {bHasMetallic, "FWD-PBR-METALLIC", "metallic input is required"},
        {bHasRoughness, "FWD-PBR-ROUGHNESS", "roughness input is required"},
        {bHasNormal, "FWD-PBR-NORMAL", "normal input is required"},
        {bHasOcclusion, "FWD-PBR-OCCLUSION", "occlusion input is required"},
        {bHasEmissive, "FWD-PBR-EMISSIVE", "emissive input is required"},
        {bHasAlpha, "FWD-PBR-ALPHA", "alpha input is required"},
    };

    bool bComplete = true;
    for (const auto& Requirement : Requirements)
    {
        if (!Requirement.bPresent)
        {
            bComplete = false;
            AddMaterialDiagnostic(Diagnostics, Requirement.Code, SubjectName, Requirement.Message);
        }
    }
    return bComplete;
}

FMeshDrawCommand::FMeshDrawCommand(FMeshDrawCandidate InCandidate, EForwardDrawPass InPass, const FForwardViewData& View)
    : Candidate(std::move(InCandidate))
    , Pass(InPass)
{
    SortKey.MaterialId = Candidate.MaterialBinding.MaterialId;
    SortKey.MeshId = Candidate.MeshId;
    SortKey.ObjectId = Candidate.ObjectId;
    SortKey.CameraSpaceDepth = View.ComputeCameraSpaceDepth(Candidate.WorldPosition);
}

const FMeshDrawCandidate& FMeshDrawCommand::GetCandidate() const noexcept
{
    return Candidate;
}

const FForwardMaterialBinding& FMeshDrawCommand::GetMaterialBinding() const noexcept
{
    return Candidate.MaterialBinding;
}

EForwardDrawPass FMeshDrawCommand::GetPass() const noexcept
{
    return Pass;
}

const FMeshDrawSortKey& FMeshDrawCommand::GetSortKey() const noexcept
{
    return SortKey;
}

EForwardValidationState FMeshDrawCommand::GetValidationState() const noexcept
{
    return ValidationState;
}

Stoner::Core::uint32 FMeshDrawCommand::GetObjectId() const noexcept
{
    return Candidate.ObjectId;
}

Stoner::Core::uint32 FMeshDrawCommand::GetMeshId() const noexcept
{
    return Candidate.MeshId;
}

Stoner::Core::uint32 FMeshDrawCommand::GetMaterialId() const noexcept
{
    return Candidate.MaterialBinding.MaterialId;
}

float FMeshDrawCommand::GetCameraSpaceDepth() const noexcept
{
    return SortKey.CameraSpaceDepth;
}

Stoner::Core::FString FMeshDrawCommand::GetStableIdentity() const
{
    std::ostringstream Stream;
    Stream << Candidate.ObjectId << ':' << Candidate.MeshId << ':' << Candidate.MaterialBinding.MaterialId << ':' << ToString(Pass);
    return Stoner::Core::FString(Stream.str());
}

Stoner::Core::FString FMeshDrawCommand::Dump() const
{
    std::ostringstream Stream;
    Stream << GetStableIdentity().CStr() << " state=" << ToString(ValidationState)
        << " name=" << Candidate.DebugName.CStr();
    return Stoner::Core::FString(Stream.str());
}

void FMeshDrawCommand::SetValidationState(EForwardValidationState NewState) noexcept
{
    ValidationState = NewState;
}

bool IsForwardOpaqueCompatible(const FForwardMaterialBinding& Binding, FForwardDiagnosticLog* Diagnostics)
{
    if (!Binding.bHasMaterialBinding)
    {
        AddMaterialDiagnostic(Diagnostics, "FWD-MAT-MISSING", Binding.MaterialName, "material binding is required");
        return false;
    }
    if (!Binding.bHasShaderBinding)
    {
        AddMaterialDiagnostic(Diagnostics, "FWD-MAT-SHADER", Binding.MaterialName, "shader binding summary is required");
        return false;
    }
    if (Binding.Domain != EMaterialDomain::Surface)
    {
        AddMaterialDiagnostic(Diagnostics, "FWD-MAT-DOMAIN", Binding.MaterialName, "forward opaque rendering requires surface domain");
        return false;
    }
    if (Binding.BlendMode != EMaterialBlendMode::Opaque && Binding.BlendMode != EMaterialBlendMode::Masked)
    {
        AddMaterialDiagnostic(Diagnostics, "FWD-MAT-OPAQUE-BLEND", Binding.MaterialName, "opaque pass requires opaque or masked blend mode");
        return false;
    }
    return Binding.SurfaceInputs.IsComplete(Diagnostics, Binding.MaterialName);
}

bool IsForwardTransparentCompatible(const FForwardMaterialBinding& Binding, FForwardDiagnosticLog* Diagnostics)
{
    if (!Binding.bHasMaterialBinding)
    {
        AddMaterialDiagnostic(Diagnostics, "FWD-MAT-MISSING", Binding.MaterialName, "material binding is required");
        return false;
    }
    if (!Binding.bHasShaderBinding)
    {
        AddMaterialDiagnostic(Diagnostics, "FWD-MAT-SHADER", Binding.MaterialName, "shader binding summary is required");
        return false;
    }
    if (Binding.Domain != EMaterialDomain::Surface || Binding.BlendMode != EMaterialBlendMode::Translucent)
    {
        AddMaterialDiagnostic(Diagnostics, "FWD-MAT-TRANSPARENT-BLEND", Binding.MaterialName, "transparent pass requires translucent surface material");
        return false;
    }
    return Binding.SurfaceInputs.IsComplete(Diagnostics, Binding.MaterialName);
}

bool ValidateForwardMeshDrawCandidate(const FMeshDrawCandidate& Candidate, EForwardDrawPass Pass, FForwardDiagnosticLog* Diagnostics)
{
    bool bValid = true;
    if (!IsStableForwardId(Candidate.ObjectId) || !IsStableForwardId(Candidate.MeshId))
    {
        bValid = false;
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EForwardDiagnosticSeverity::Error, EForwardDiagnosticCategory::Draw,
                EForwardResult::ValidationFailed, "FWD-DRAW-ID", Candidate.DebugName,
                "draw candidate requires stable object and mesh identifiers");
        }
    }
    if (!IsForwardFinite(Candidate.WorldPosition))
    {
        bValid = false;
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EForwardDiagnosticSeverity::Error, EForwardDiagnosticCategory::Draw,
                EForwardResult::ValidationFailed, "FWD-DRAW-POSITION", Candidate.DebugName,
                "draw candidate position must be finite");
        }
    }
    if (!IsStableForwardId(Candidate.MaterialBinding.MaterialId))
    {
        bValid = false;
        AddMaterialDiagnostic(Diagnostics, "FWD-MAT-ID", Candidate.MaterialBinding.MaterialName,
            "material binding requires stable material identity");
    }

    const bool bMaterialValid = Pass == EForwardDrawPass::Opaque
        ? IsForwardOpaqueCompatible(Candidate.MaterialBinding, Diagnostics)
        : IsForwardTransparentCompatible(Candidate.MaterialBinding, Diagnostics);
    return bValid && bMaterialValid;
}

void SortForwardOpaqueDraws(Stoner::Core::TArray<FMeshDrawCommand>& Draws)
{
    std::sort(Draws.begin(), Draws.end(), [](const FMeshDrawCommand& Left, const FMeshDrawCommand& Right) {
        if (Left.GetMaterialId() != Right.GetMaterialId())
        {
            return Left.GetMaterialId() < Right.GetMaterialId();
        }
        if (Left.GetMeshId() != Right.GetMeshId())
        {
            return Left.GetMeshId() < Right.GetMeshId();
        }
        return Left.GetObjectId() < Right.GetObjectId();
    });
}

void SortForwardTransparentDraws(Stoner::Core::TArray<FMeshDrawCommand>& Draws)
{
    std::sort(Draws.begin(), Draws.end(), [](const FMeshDrawCommand& Left, const FMeshDrawCommand& Right) {
        if (!Stoner::Core::FMath::IsNearlyEqual(Left.GetCameraSpaceDepth(), Right.GetCameraSpaceDepth()))
        {
            return Left.GetCameraSpaceDepth() > Right.GetCameraSpaceDepth();
        }
        if (Left.GetMaterialId() != Right.GetMaterialId())
        {
            return Left.GetMaterialId() < Right.GetMaterialId();
        }
        if (Left.GetObjectId() != Right.GetObjectId())
        {
            return Left.GetObjectId() < Right.GetObjectId();
        }
        return Left.GetMeshId() < Right.GetMeshId();
    });
}

const char* ToString(EForwardDrawPass Pass) noexcept
{
    switch (Pass)
    {
    case EForwardDrawPass::Opaque: return "Opaque";
    case EForwardDrawPass::Transparent: return "Transparent";
    }
    return "Unknown";
}

} // namespace Stoner::Renderer
