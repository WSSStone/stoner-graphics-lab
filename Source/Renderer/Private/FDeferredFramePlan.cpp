#include "Renderer/FDeferredFramePlan.h"

#include <algorithm>
#include <sstream>

namespace Stoner::Renderer
{

namespace
{

[[nodiscard]] bool IsFinite(float Value) noexcept
{
    return Stoner::Core::FMath::IsFinite(Value);
}

[[nodiscard]] bool IsFinite(const Stoner::Core::FVector3& Value) noexcept
{
    return IsFinite(Value.X) && IsFinite(Value.Y) && IsFinite(Value.Z);
}

[[nodiscard]] bool IsFinite(const Stoner::Core::FColor& Value) noexcept
{
    return IsFinite(Value.R) && IsFinite(Value.G) && IsFinite(Value.B) && IsFinite(Value.A);
}

[[nodiscard]] bool InUnitRange(float Value) noexcept
{
    return IsFinite(Value) && Value >= 0.0f && Value <= 1.0f;
}

void AddError(FDeferredDiagnosticLog* Diagnostics, EDeferredResult Result,
    const char* Code, const Stoner::Core::FString& Subject, const char* Reason)
{
    if (Diagnostics)
    {
        Diagnostics->Add(EDeferredDiagnosticSeverity::Error, EDeferredPassStage::SurfaceData,
            Result, Code, Subject, Reason);
    }
}

} // namespace

bool FDeferredViewData::IsValid(FDeferredDiagnosticLog* Diagnostics) const
{
    Stoner::Core::FMatrix4x4 ComputedInverse;
    const bool bMatricesFinite = IsDeferredFinite(View) && IsDeferredFinite(Projection) &&
        IsDeferredFinite(InverseViewProjection) && IsDeferredFinite(ViewProjection);
    const bool bInvertible = bMatricesFinite && ViewProjection.TryInverse(ComputedInverse);
    const bool bValid = !Name.IsEmpty() && bMatricesFinite && bInvertible &&
        ComputedInverse.NearlyEquals(InverseViewProjection, 1.0e-3f) &&
        IsFinite(CameraPosition) && Extent.IsPositive() && DepthPolicy.IsValid();
    if (!bValid)
    {
        AddError(Diagnostics, EDeferredResult::InvalidView, "DEF-VIEW", Name,
            "view requires finite convention-matched matrices camera and positive extent");
    }
    return bValid;
}

bool FDeferredOutputTarget::IsValid(const FDeferredViewData& View,
    FDeferredDiagnosticLog* Diagnostics) const
{
    const bool bValid = !Name.IsEmpty() &&
        Format == Stoner::RHI::ERHIFormat::R16G16B16A16_Float &&
        Extent.IsPositive() &&
        Extent.Width == View.Extent.Width && Extent.Height == View.Extent.Height;
    if (!bValid)
    {
        AddError(Diagnostics, EDeferredResult::InvalidOutput, "DEF-OUTPUT", Name,
            "SceneColor requires stable identity matching view extent and RGBA16F format");
    }
    return bValid;
}

void FDeferredFramePlan::Reset()
{
    *this = FDeferredFramePlan{};
}

void FDeferredFramePlan::AddPass(EDeferredPassStage Stage, Stoner::Core::FString Name,
    Stoner::Core::uint32 DrawCount, Stoner::Core::uint32 LightCount,
    Stoner::Core::TArray<Stoner::Core::FString> Reads,
    Stoner::Core::TArray<Stoner::Core::FString> Writes, bool bCullEligible)
{
    Passes.push_back({Stage, static_cast<Stoner::Core::uint32>(Passes.size() + 1),
        std::move(Name), std::move(Reads), std::move(Writes), DrawCount, LightCount, bCullEligible});
}

const FDeferredPassRecord* FDeferredFramePlan::FindPass(EDeferredPassStage Stage) const noexcept
{
    for (const FDeferredPassRecord& Pass : Passes)
    {
        if (Pass.Stage == Stage)
        {
            return &Pass;
        }
    }
    return nullptr;
}

bool ValidateDeferredDrawCandidate(const FDeferredDrawCandidate& Candidate,
    FDeferredDrawRecord& OutRecord, FDeferredDiagnosticLog* Diagnostics)
{
    OutRecord = {};
    OutRecord.Candidate = Candidate;
    if (!Candidate.Identity.IsValid() || Candidate.MeshId == 0 || Candidate.MaterialId == 0 ||
        Candidate.Name.IsEmpty())
    {
        OutRecord.Reason = "invalid-identity";
        AddError(Diagnostics, EDeferredResult::InvalidDraw, "DEF-DRAW-ID", Candidate.Name,
            "draw requires stable object mesh and material identity");
        return false;
    }
    if (Candidate.Domain != EMaterialDomain::Surface ||
        (Candidate.BlendMode != EMaterialBlendMode::Opaque &&
            Candidate.BlendMode != EMaterialBlendMode::Masked) ||
        !Candidate.bHasShaderBinding || !Candidate.bHasRequiredSemantics)
    {
        OutRecord.Reason = "incompatible-material";
        AddError(Diagnostics, EDeferredResult::InvalidMaterial, "DEF-DRAW-MATERIAL", Candidate.Name,
            "surface stage requires opaque or masked material shader with all PBR semantics");
        return false;
    }
    if (!TryBuildWorldNormalFromModel(Candidate.Model, OutRecord.WorldNormalFromModel))
    {
        OutRecord.Reason = "invalid-model-transform";
        AddError(Diagnostics, EDeferredResult::InvalidDraw, "DEF-DRAW-NORMAL-MATRIX", Candidate.Name,
            "model transform must be finite affine and have an invertible upper-left mat3");
        return false;
    }
    const FDeferredMaterialSurface& Surface = Candidate.Surface;
    const bool bSurfaceValid = IsFinite(Surface.BaseColor) &&
        Surface.BaseColor.R >= 0.0f && Surface.BaseColor.R <= 1.0f &&
        Surface.BaseColor.G >= 0.0f && Surface.BaseColor.G <= 1.0f &&
        Surface.BaseColor.B >= 0.0f && Surface.BaseColor.B <= 1.0f &&
        IsFinite(Surface.Normal) &&
        Surface.Normal.LengthSquared() > Stoner::Core::FMath::DefaultTolerance &&
        InUnitRange(Surface.Metallic) && InUnitRange(Surface.Roughness) &&
        InUnitRange(Surface.AmbientOcclusion) && InUnitRange(Surface.Alpha) &&
        InUnitRange(Surface.AlphaCutoff) && IsFinite(Surface.Emissive) &&
        Surface.Emissive.R >= 0.0f && Surface.Emissive.G >= 0.0f && Surface.Emissive.B >= 0.0f;
    if (!bSurfaceValid)
    {
        OutRecord.Reason = "invalid-surface-values";
        AddError(Diagnostics, EDeferredResult::InvalidMaterial, "DEF-DRAW-SURFACE", Candidate.Name,
            "surface values must be finite and inside their declared semantic ranges");
        return false;
    }
    OutRecord.bAccepted = true;
    OutRecord.Reason = "accepted";
    return true;
}

void SortDeferredDrawRecords(Stoner::Core::TArray<FDeferredDrawRecord>& Draws)
{
    std::sort(Draws.begin(), Draws.end(), [](const FDeferredDrawRecord& Left,
        const FDeferredDrawRecord& Right) {
        if (Left.Candidate.MaterialId != Right.Candidate.MaterialId)
        {
            return Left.Candidate.MaterialId < Right.Candidate.MaterialId;
        }
        if (Left.Candidate.MeshId != Right.Candidate.MeshId)
        {
            return Left.Candidate.MeshId < Right.Candidate.MeshId;
        }
        return Left.Candidate.Identity < Right.Candidate.Identity;
    });
}

Stoner::Core::FString BuildDeferredInputFingerprint(const FDeferredFramePlan& Plan)
{
    std::ostringstream Stream;
    Stream << "frame=" << Plan.FrameId.CStr() << ";layout=" << Plan.SurfaceLayout.LayoutId.CStr()
        << ";draws=" << Plan.AcceptedDraws.size() << ";lights=" << Plan.Lights.Accepted.size()
        << ";transparent=" << Plan.TransparentHandoff.size() << ';';
    for (const FDeferredDrawRecord& Draw : Plan.AcceptedDraws)
    {
        Stream << "d:" << Draw.Candidate.Identity.Slot << ':' << Draw.Candidate.Identity.Generation
            << ':' << Draw.Candidate.MeshId << ':' << Draw.Candidate.MaterialId << ';';
    }
    for (const FDeferredLightRecord& Light : Plan.Lights.Accepted)
    {
        Stream << "l:" << ToString(Light.Type) << ':' << Light.Identity.Slot << ':'
            << Light.Identity.Generation << ';';
    }
    return Stoner::Core::FString(Stream.str());
}

Stoner::Core::FString BuildDeferredFrameDebugDump(const FDeferredFramePlan& Plan)
{
    std::ostringstream Stream;
    Stream << "DeferredFrame id=" << Plan.FrameId.CStr() << " valid=" << (Plan.bValid ? 1 : 0)
        << " layout=" << Plan.SurfaceLayout.LayoutId.CStr() << '\n';
    Stream << "SceneColorHandoff producer="
        << ToString(Plan.SceneColorHandoff.GetProducer())
        << " state=" << ToString(Plan.SceneColorHandoff.GetState())
        << " sceneColorId=" << Plan.SceneColorHandoff.GetSceneColorId()
        << " viewId=" << Plan.SceneColorHandoff.GetViewId()
        << " frameToken=" << Plan.SceneColorHandoff.GetFrameToken() << '\n';
    Stream << "Surface extent=" << Plan.SurfaceLayout.Extent.Width << 'x'
        << Plan.SurfaceLayout.Extent.Height << " samples="
        << static_cast<int>(Plan.SurfaceLayout.SampleCount) << " depth="
        << ToString(Plan.SurfaceLayout.DepthPolicy.Convention) << " clear="
        << Plan.SurfaceLayout.DepthPolicy.FarClearValue << '\n';
    for (const FDeferredSurfaceAttachment& Attachment : Plan.SurfaceLayout.Attachments)
    {
        Stream << "Attachment name=" << Attachment.Name.CStr()
            << " format=" << static_cast<int>(Attachment.Format);
        for (EDeferredSurfaceSemantic Semantic : Attachment.Semantics)
        {
            Stream << " semantic=" << ToString(Semantic);
        }
        Stream << '\n';
    }
    Stream << "Draws accepted=" << Plan.AcceptedDraws.size() << " rejected="
        << Plan.RejectedDraws.size() << " transparent=" << Plan.TransparentHandoff.size() << '\n';
    for (const FDeferredDrawRecord& Draw : Plan.AcceptedDraws)
    {
        Stream << "Draw accepted slot=" << Draw.Candidate.Identity.Slot << " mesh="
            << Draw.Candidate.MeshId << " material=" << Draw.Candidate.MaterialId << '\n';
    }
    for (const FDeferredDrawRecord& Draw : Plan.RejectedDraws)
    {
        Stream << "Draw rejected slot=" << Draw.Candidate.Identity.Slot << " reason="
            << Draw.Reason.CStr() << '\n';
    }
    Stream << "Lights accepted=" << Plan.Lights.Accepted.size() << " culled="
        << Plan.Lights.Culled.size() << " rejected=" << Plan.Lights.Rejected.size() << '\n';
    for (const FDeferredLightRecord& Light : Plan.Lights.Accepted)
    {
        Stream << "Light accepted type=" << ToString(Light.Type) << " slot="
            << Light.Identity.Slot << " volume=" << ToString(Light.Acceptance) << '\n';
    }
    for (const FDeferredLightRecord& Light : Plan.Lights.Culled)
    {
        Stream << "Light culled type=" << ToString(Light.Type) << " slot="
            << Light.Identity.Slot << " reason=" << Light.Reason.CStr() << '\n';
    }
    for (const FDeferredPassRecord& Pass : Plan.Passes)
    {
        Stream << "Pass " << Pass.PassId << ' ' << ToString(Pass.Stage) << " draws="
            << Pass.DrawCount << " lights=" << Pass.LightCount << '\n';
        for (const Stoner::Core::FString& Read : Pass.Reads)
        {
            Stream << "  Read " << Read.CStr() << '\n';
        }
        for (const Stoner::Core::FString& Write : Pass.Writes)
        {
            Stream << "  Write " << Write.CStr() << '\n';
        }
    }
    Stream << "Composition output=" << Plan.Output.Name.CStr() << '\n'
        << "TransparentHandoff count=" << Plan.TransparentHandoff.size() << '\n';
    Stream << "Fingerprint " << Plan.InputFingerprint.CStr() << '\n';
    Stream << Plan.Diagnostics.Dump().CStr();
    return Stoner::Core::FString(Stream.str());
}

} // namespace Stoner::Renderer
