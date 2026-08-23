#pragma once

#include "Renderer/FDeferredRenderer.h"
#include "Renderer/FDeferredFrameExecutor.h"
#include "Renderer/FForwardFrameExecutor.h"
#include "Renderer/FForwardRenderer.h"
#include "Renderer/FStaticModelRealization.h"

namespace Stoner::Demo
{

struct FProductionContentCompositionConfig
{
    Core::FString WorkloadRevision;
    Core::uint64 FrameToken = 1;
    Core::uint32 Width = 1280;
    Core::uint32 Height = 720;
    float ModelHalfExtent = 1.5f;
    float ModelForwardDistance = 4.0f;

    [[nodiscard]] bool IsValid() const noexcept;
};

struct FProductionContentComposition
{
    Core::FString WorkloadRevision;
    Asset::FAssetId RootAssetId;
    Asset::FAssetVersion RootVersion;
    Core::uint64 SnapshotGeneration = 0;
    Core::uint64 FrameToken = 0;
    Core::FMatrix4x4 ModelPlacement = Core::FMatrix4x4::Identity();
    Core::FVector3 CameraPosition = Core::FVector3::Zero();
    Renderer::FDeferredFrameInputs DeferredInputs;
    Renderer::FForwardFrameInputs ForwardInputs;
};

class FProductionContentCompositionBuilder
{
public:
    [[nodiscard]] static bool Build(
        const Core::TSharedPtr<const Renderer::FStaticModelRenderSnapshot>&
            Snapshot,
        const FProductionContentCompositionConfig& Config,
        FProductionContentComposition& OutComposition,
        Core::FString* OutReason = nullptr);
};

[[nodiscard]] bool BindProductionDeferredDraws(
    const Renderer::FStaticModelRenderSnapshot& Snapshot,
    const Renderer::FDeferredFramePlan& Plan,
    Renderer::FDeferredFrameExecutionBindings& InOutBindings,
    Core::FString* OutReason = nullptr);

[[nodiscard]] bool UploadProductionDeferredUniforms(
    RHI::IRHIDevice& Device,
    const Renderer::FStaticModelRenderSnapshot& Snapshot,
    const Renderer::FDeferredFramePlan& Plan,
    Core::FString* OutReason = nullptr);

[[nodiscard]] bool BindProductionForwardDraws(
    const Renderer::FStaticModelRenderSnapshot& Snapshot,
    const Renderer::FForwardFramePlan& Plan,
    Renderer::FForwardFrameExecutionBindings& InOutBindings,
    Core::FString* OutReason = nullptr);

[[nodiscard]] bool PrepareProductionForwardSmoke(
    RHI::IRHIDevice& Device,
    const Renderer::FStaticModelRenderSnapshot& Snapshot,
    const Renderer::FForwardFramePlan& ForwardPlan,
    const Renderer::FDeferredFramePlan& DeferredPlan,
    Renderer::FForwardFrameExecutionBindings& OutBindings,
    Core::FString* OutReason = nullptr);

void ReleaseProductionForwardSmoke(
    Renderer::FForwardFrameExecutionBindings& Bindings) noexcept;

} // namespace Stoner::Demo
