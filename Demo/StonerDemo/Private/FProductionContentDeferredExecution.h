#pragma once

#include "Asset/AssetMinimal.h"
#include "FProductionContentComposition.h"
#include "Renderer/FDeferredRenderGraphDeclaration.h"
#include "Renderer/FHDRPostProcessPipeline.h"

namespace Stoner::Renderer
{
class FDeferredFrameUniformResources;
}

namespace Stoner::Demo
{

struct FProductionContentDeferredExecutionResources
{
    Renderer::FDeferredFramePlan Plan;
    Renderer::FDeferredRenderGraphDeclaration Graph;
    Renderer::FOutputTransformPlan OutputTransformPlan;
    Renderer::FDeferredFrameExecutionBindings Bindings;
    Core::TArray<Core::TSharedPtr<RHI::IRHIBuffer>> OwnedBuffers;
    Core::TArray<Core::TSharedPtr<RHI::IRHITexture>> OwnedTextures;
    Core::TArray<Core::TSharedPtr<RHI::IRHISampler>> OwnedSamplers;
    Core::TArray<Core::TSharedPtr<RHI::IRHIShaderModule>> OwnedShaders;
    Core::TArray<Core::TSharedPtr<RHI::IRHIPipelineLayout>> OwnedLayouts;
    Core::TArray<Core::TSharedPtr<RHI::IRHIGraphicsPipeline>> OwnedPipelines;
    Core::TArray<Core::TSharedPtr<RHI::IRHIRenderPass>> OwnedRenderPasses;
    Core::TArray<Core::TSharedPtr<RHI::IRHIFramebuffer>> OwnedFramebuffers;
    Core::TArray<Core::TSharedPtr<RHI::IRHIDescriptorSet>> OwnedDescriptorSets;
    // Preview frame/draw uniform storage is slot-local.  The immutable scene
    // snapshot and its sampled/material resources remain shared by the frame
    // contexts; this object owns only the mutable bindings for one slot.
    Core::TSharedPtr<Renderer::FDeferredFrameUniformResources>
        PreviewUniformResources;
    Core::TSharedPtr<const Renderer::FStaticModelRenderSnapshot>
        SceneLease;
    Renderer::FOutputTransformSettings OutputSettings;
    Core::uint64 AttachmentBytes = 0;
    Renderer::EFrameExecutionPurpose ExecutionPurpose =
        Renderer::EFrameExecutionPurpose::FormalValidation;
    Renderer::EFrameReadbackSelection ReadbackSelection =
        Renderer::EFrameReadbackSelection::Formal;

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] Renderer::FDeferredFrameExecutionBindings
        BuildCycleBindings(bool bAuthoritativeReadbacks) const;
    void Release() noexcept;
};

struct FProductionContentDeferredExecutionBuildOptions
{
    Renderer::EFrameExecutionPurpose ExecutionPurpose =
        Renderer::EFrameExecutionPurpose::FormalValidation;
    Renderer::EFrameReadbackSelection ReadbackSelection =
        Renderer::EFrameReadbackSelection::Formal;
    // Interactive preview renders directly to this borrowed drawable target.
    // The builder never adds it to OwnedTextures or invalidates it on Release.
    Core::TSharedPtr<RHI::IRHITexture> BorrowedFinalOutput;
    // Required for the preview helper's slot-local uniform realization.  The
    // reference parameter remains for formal callers and compatibility.
    Core::TSharedPtr<const Renderer::FStaticModelRenderSnapshot> SceneLease;
};

class FProductionContentDeferredExecutionBuilder
{
public:
    [[nodiscard]] static RHI::ERHIResult Build(
        const Core::TSharedPtr<RHI::IRHIDevice>& Device,
        const Renderer::FStaticModelRenderSnapshot& Snapshot,
        const FProductionContentComposition& Composition,
        const Core::TArray<Core::TSharedPtr<const Asset::FShaderAsset>>&
            RenderShaders,
        const Core::TArray<Core::TSharedPtr<const Asset::FShaderPayloadAsset>>&
            RenderShaderPayloads,
        const Asset::FAssetTargetProfileEvidence& TargetEvidence,
        const Renderer::FOutputTransformSettings& OutputSettings,
        FProductionContentDeferredExecutionResources& OutResources,
        Core::FString* OutReason = nullptr,
        const FProductionContentDeferredExecutionBuildOptions& Options = {});

    // Reuse a preview resource bundle while changing only frame/draw
    // parameters and the borrowed final-output framebuffer.  Shader,
    // pipeline, intermediate attachment and command-buffer ownership remains
    // stable for the slot.
    [[nodiscard]] static RHI::ERHIResult UpdatePreviewFrame(
        const Core::TSharedPtr<RHI::IRHIDevice>& Device,
        const Renderer::FStaticModelRenderSnapshot& Snapshot,
        const Core::TSharedPtr<const Renderer::FStaticModelRenderSnapshot>&
            SceneLease,
        const FProductionContentComposition& Composition,
        FProductionContentDeferredExecutionResources& InOutResources,
        Core::FString* OutReason = nullptr);

    [[nodiscard]] static RHI::ERHIResult RebindPreviewOutput(
        const Core::TSharedPtr<RHI::IRHIDevice>& Device,
        const Core::TSharedPtr<RHI::IRHITexture>& BorrowedFinalOutput,
        FProductionContentDeferredExecutionResources& InOutResources,
        Core::FString* OutReason = nullptr);
};

} // namespace Stoner::Demo
