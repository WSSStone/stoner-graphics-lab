#pragma once

#include "Asset/AssetMinimal.h"
#include "FProductionContentComposition.h"
#include "Renderer/FDeferredRenderGraphDeclaration.h"

namespace Stoner::Demo
{

struct FProductionContentDeferredExecutionResources
{
    Renderer::FDeferredFramePlan Plan;
    Renderer::FDeferredRenderGraphDeclaration Graph;
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

    [[nodiscard]] bool IsValid() const noexcept;
    void Release() noexcept;
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
        FProductionContentDeferredExecutionResources& OutResources,
        Core::FString* OutReason = nullptr);
};

} // namespace Stoner::Demo
