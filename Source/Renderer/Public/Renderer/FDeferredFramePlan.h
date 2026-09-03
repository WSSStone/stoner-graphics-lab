#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FDeferredDiagnostics.h"
#include "Renderer/FHDRSceneColorHandoff.h"
#include "Renderer/FDeferredLightData.h"
#include "Renderer/FDeferredSurfaceData.h"
#include "Renderer/FMeshDrawCommand.h"

namespace Stoner::Renderer
{

struct FDeferredRenderGraphDeclaration;

struct FDeferredViewData
{
    Stoner::Core::FString Name;
    Stoner::Core::FMatrix4x4 View = Stoner::Core::FMatrix4x4::Identity();
    Stoner::Core::FMatrix4x4 Projection = Stoner::Core::FMatrix4x4::Identity();
    Stoner::Core::FMatrix4x4 InverseViewProjection = Stoner::Core::FMatrix4x4::Identity();
    Stoner::Core::FMatrix4x4 ViewProjection = Stoner::Core::FMatrix4x4::Identity();
    Stoner::Core::FVector3 CameraPosition = Stoner::Core::FVector3::Zero();
    FDeferredExtent2D Extent;
    FDeferredDepthPolicy DepthPolicy;

    [[nodiscard]] bool IsValid(FDeferredDiagnosticLog* Diagnostics = nullptr) const;
};

struct FDeferredOutputTarget
{
    Stoner::Core::FString Name;
    Stoner::RHI::ERHIFormat Format =
        Stoner::RHI::ERHIFormat::R16G16B16A16_Float;
    FDeferredExtent2D Extent;

    [[nodiscard]] bool IsValid(const FDeferredViewData& View,
        FDeferredDiagnosticLog* Diagnostics = nullptr) const;
};

struct FDeferredMaterialSurface
{
    Stoner::Core::FColor BaseColor = Stoner::Core::FColor::OpaqueWhite();
    float AmbientOcclusion = 1.0f;
    Stoner::Core::FVector3 Normal = Stoner::Core::FVector3(0.0f, 0.0f, 1.0f);
    float Roughness = 1.0f;
    Stoner::Core::FColor Emissive = Stoner::Core::FColor::Transparent();
    float Metallic = 0.0f;
    float Alpha = 1.0f;
    float AlphaCutoff = 0.5f;
};

struct FDeferredDrawCandidate
{
    FDeferredEntityIdentity Identity;
    Stoner::Core::uint32 MeshId = 0;
    Stoner::Core::uint32 MaterialId = 0;
    Stoner::Core::FString Name;
    Stoner::Core::FMatrix4x4 Model = Stoner::Core::FMatrix4x4::Identity();
    EMaterialDomain Domain = EMaterialDomain::Surface;
    EMaterialBlendMode BlendMode = EMaterialBlendMode::Opaque;
    bool bHasShaderBinding = true;
    bool bHasRequiredSemantics = true;
    FDeferredMaterialSurface Surface;
    FMeshDrawCandidate ForwardCandidate;
};

struct FDeferredDrawRecord
{
    FDeferredDrawCandidate Candidate;
    Stoner::Core::FMatrix4x4 WorldNormalFromModel = Stoner::Core::FMatrix4x4::Identity();
    bool bAccepted = false;
    Stoner::Core::FString Reason;
};

struct FDeferredPassRecord
{
    EDeferredPassStage Stage = EDeferredPassStage::SurfaceData;
    Stoner::Core::uint32 PassId = 0;
    Stoner::Core::FString Name;
    Stoner::Core::TArray<Stoner::Core::FString> Reads;
    Stoner::Core::TArray<Stoner::Core::FString> Writes;
    Stoner::Core::uint32 DrawCount = 0;
    Stoner::Core::uint32 LightCount = 0;
    bool bCullEligible = false;
};

struct FDeferredFramePlan
{
    Stoner::Core::FString FrameId;
    FDeferredViewData View;
    FDeferredOutputTarget Output;
    FDeferredSurfaceLayout SurfaceLayout;
    Stoner::Core::FColor AmbientContribution = Stoner::Core::FColor::Transparent();
    Stoner::Core::TArray<FDeferredDrawRecord> AcceptedDraws;
    Stoner::Core::TArray<FDeferredDrawRecord> RejectedDraws;
    Stoner::Core::TArray<FMeshDrawCommand> TransparentHandoff;
    FDeferredLightSet Lights;
    Stoner::Core::TArray<FDeferredPassRecord> Passes;
    FDeferredDiagnosticLog Diagnostics;
    FHDRSceneColorHandoff SceneColorHandoff;
    Stoner::Core::FString InputFingerprint;
    Stoner::Core::FString DebugDump;
    bool bValid = false;

    void Reset();
    void AddPass(EDeferredPassStage Stage, Stoner::Core::FString Name,
        Stoner::Core::uint32 DrawCount, Stoner::Core::uint32 LightCount,
        Stoner::Core::TArray<Stoner::Core::FString> Reads,
        Stoner::Core::TArray<Stoner::Core::FString> Writes,
        bool bCullEligible = false);
    [[nodiscard]] bool IsValid() const noexcept { return bValid; }
    [[nodiscard]] const FDeferredPassRecord* FindPass(EDeferredPassStage Stage) const noexcept;
};

[[nodiscard]] bool ValidateDeferredDrawCandidate(const FDeferredDrawCandidate& Candidate,
    FDeferredDrawRecord& OutRecord, FDeferredDiagnosticLog* Diagnostics = nullptr);
void SortDeferredDrawRecords(Stoner::Core::TArray<FDeferredDrawRecord>& Draws);
[[nodiscard]] Stoner::Core::FString BuildDeferredInputFingerprint(const FDeferredFramePlan& Plan);
[[nodiscard]] Stoner::Core::FString BuildDeferredFrameDebugDump(const FDeferredFramePlan& Plan);

} // namespace Stoner::Renderer
