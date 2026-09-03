#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FRenderGraphPass.h"

namespace Stoner::Renderer
{

struct FOutputTransformStageResource
{
    Stoner::Core::uint32 StageId = 0;
    Stoner::Core::FString StageName;
    FRenderGraphResourceHandle Resource;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return StageId != 0 && !StageName.IsEmpty() && Resource.IsValid();
    }
};

struct FOutputTransformGraphDeclaration
{
    Stoner::Core::uint32 GraphId = 0;
    Stoner::Core::uint64 PlanId = 0;
    Stoner::Core::uint64 FormalOutputId = 0;
    Stoner::Core::FString PlanFingerprint;
    FRenderGraphResourceHandle SceneColor;
    FRenderGraphResourceHandle ExposedSceneColor;
    Stoner::Core::TArray<FRenderGraphResourceHandle> PreTonemapOutputs;
    FRenderGraphResourceHandle ToneOrViewedColor;
    Stoner::Core::TArray<FRenderGraphResourceHandle> PostTonemapOutputs;
    Stoner::Core::TArray<FRenderGraphResourceHandle> InsertionResources;
    Stoner::Core::TArray<FRenderGraphPassHandle> InsertionPasses;
    Stoner::Core::TArray<FOutputTransformStageResource> StageResources;
    FRenderGraphResourceHandle FormalOutput;
    FRenderGraphResourceHandle ReadbackBuffer;
    FRenderGraphResourceHandle DiagnosticOutput;
    FRenderGraphResourceHandle DiagnosticReadbackBuffer;
    Stoner::Core::TArray<FRenderGraphPassHandle> OrderedPasses;
    FRenderGraphPassHandle ReadbackPass;
    FRenderGraphPassHandle PresentationPass;
    FRenderGraphPassHandle DiagnosticVisualizationPass;
    FRenderGraphPassHandle DiagnosticReadbackPass;
    Stoner::Core::uint32 FormalWriterCount = 0;
    Stoner::Core::uint32 FullscreenPassCount = 0;
    Stoner::Core::uint32 GpuReadbackCopyCount = 0;
    Stoner::Core::uint32 CpuReadbackInitiationCount = 0;
    Stoner::Core::uint32 FullImageVisitCount = 0;
    Stoner::Core::uint32 DiagnosticFullscreenPassCount = 0;
    Stoner::Core::uint32 DiagnosticReadbackCopyCount = 0;
    bool bDiagnosticOutputNonAuthoritative = false;
    bool bValid = false;

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] FRenderGraphResourceHandle FindStageResource(
        Stoner::Core::uint32 StageId) const noexcept;
};

} // namespace Stoner::Renderer
