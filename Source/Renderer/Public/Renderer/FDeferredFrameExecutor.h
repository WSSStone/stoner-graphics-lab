#pragma once

#include "Renderer/FDeferredRenderGraphDeclaration.h"
#include "Renderer/FShaderMatrixPacking.h"
#include "RHI/RHIMinimal.h"

namespace Stoner::Renderer
{

struct alignas(16) FDeferredFrameViewUniform
{
    FShaderMatrix4x4 View;
    FShaderMatrix4x4 Projection;
    FShaderMatrix4x4 InverseViewProjection;
    FShaderMatrix4x4 ViewProjection;
    Stoner::Core::FVector4 CameraPosition;
    Stoner::Core::FVector4 OutputExtent;
    Stoner::Core::FVector4 DepthConvention;
};

struct alignas(16) FDeferredDrawMaterialUniform
{
    FShaderMatrix4x4 Model;
    FShaderMatrix4x4 WorldNormalFromModel;
    Stoner::Core::FVector4 BaseColorAO;
    Stoner::Core::FVector4 EmissiveMetallic;
    Stoner::Core::FVector4 RoughnessAlphaCutoffFlags;
};

struct alignas(16) FDeferredLightUniform
{
    Stoner::Core::FVector4 PositionRange;
    Stoner::Core::FVector4 DirectionOuterCos;
    Stoner::Core::FVector4 ColorIntensity;
    Stoner::Core::FVector4 InnerCosTypeVolumeMode;
};

static_assert(sizeof(FDeferredFrameViewUniform) == 304);
static_assert(sizeof(FDeferredDrawMaterialUniform) == 176);
static_assert(sizeof(FDeferredLightUniform) == 64);

struct FDeferredShaderBindingContract
{
    Stoner::Core::uint32 Set = 0;
    Stoner::Core::uint32 Binding = 0;
    Stoner::RHI::ERHIDescriptorType Type = Stoner::RHI::ERHIDescriptorType::UniformBuffer;
    Stoner::RHI::ERHIShaderStageFlags Visibility = Stoner::RHI::ERHIShaderStageFlags::None;
};

struct FDeferredVertexLayoutContract
{
    Stoner::Core::FString Name;
    Stoner::Core::uint32 Stride = 0;
    Stoner::Core::TArray<Stoner::RHI::FRHIVertexAttributeDesc> Attributes;
    bool bIndexed = false;
    Stoner::RHI::ERHIIndexType IndexType = Stoner::RHI::ERHIIndexType::UInt16;
};

enum class EDeferredExecutionState
{
    Uninitialized,
    BindingsValidated,
    Recording,
    Recorded,
    Submitted,
    Completed,
    ReadbackReady,
    Failed,
    Released
};

struct FDeferredStageBindings
{
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIRenderPass> RenderPass;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIFramebuffer> Framebuffer;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIGraphicsPipeline> Pipeline;
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<Stoner::RHI::IRHIDescriptorSet>> DescriptorSets;
};

struct FDeferredReadbackBinding
{
    Stoner::Core::FString Name;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture> Source;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer> Destination;
    Stoner::RHI::FRHITextureBufferCopyRegion Region;
};

struct FDeferredPostProcessStageBinding
{
    Stoner::Core::FString Name;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture> Input;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture> Output;
    FDeferredStageBindings Stage;
};

struct FDeferredSurfaceDrawBinding
{
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer> VertexBuffer;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer> IndexBuffer;
    Stoner::RHI::ERHIIndexType IndexType =
        Stoner::RHI::ERHIIndexType::UInt16;
    Stoner::RHI::FRHIIndexedDrawArguments Draw;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIGraphicsPipeline> Pipeline;
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<
        Stoner::RHI::IRHIDescriptorSet>> DescriptorSets;
};

struct FDeferredFrameExecutionBindings
{
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHICommandBuffer> CommandBuffer;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture> BaseColorAO;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture> NormalRoughness;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture> EmissiveMetallic;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture> Depth;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture> LightingAccumulation;
    // Deferred's own terminal attachment is canonical linear HDR SceneColor.
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture> FinalOutput;
    // Optional Feature 029 chain. When present, this is the only formal output
    // used by readback and presentation; FinalOutput remains the producer
    // handoff and is never presented directly.
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture> FormalOutput;
    Stoner::Core::TArray<FDeferredPostProcessStageBinding>
        OutputTransformStages;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer> SurfaceVertexBuffer;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer> SurfaceIndexBuffer;
    Stoner::Core::uint32 SurfaceIndexCount = 0;
    Stoner::Core::TArray<FDeferredSurfaceDrawBinding> SurfaceDraws;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer> FullscreenVertexBuffer;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer> SphereVertexBuffer;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer> SphereIndexBuffer;
    Stoner::Core::uint32 SphereIndexCount = 0;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer> ConeVertexBuffer;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer> ConeIndexBuffer;
    Stoner::Core::uint32 ConeIndexCount = 0;
    FDeferredStageBindings Surface;
    FDeferredStageBindings Directional;
    FDeferredStageBindings PointOutside;
    FDeferredStageBindings PointInside;
    FDeferredStageBindings SpotOutside;
    FDeferredStageBindings SpotInside;
    FDeferredStageBindings Composition;
    FDeferredStageBindings Transparency;
    Stoner::Core::TArray<FDeferredReadbackBinding> Readbacks;
    bool bTransitionFinalOutputToPresent = false;
};

struct FDeferredFrameExecutionResult
{
    EDeferredResult Result = EDeferredResult::InvalidBinding;
    EDeferredExecutionState FinalState = EDeferredExecutionState::Uninitialized;
    EDeferredPassStage LastCompletedStage = EDeferredPassStage::SurfaceData;
    Stoner::Core::uint32 RecordedPassCount = 0;
    Stoner::Core::uint32 RecordedDrawCount = 0;
    Stoner::Core::uint32 RecordedCommandCount = 0;
    Stoner::Core::uint32 LocalLightBatchCount = 0;
    Stoner::Core::uint32 LocalLightInstanceCount = 0;
    Stoner::Core::uint32 OmittedLocalLightCount = 0;
    FDeferredDiagnosticLog Diagnostics;

    [[nodiscard]] bool Succeeded() const noexcept { return Result == EDeferredResult::Success; }
};

class FDeferredFrameExecutor
{
public:
    [[nodiscard]] FDeferredFrameExecutionResult Execute(const FDeferredFramePlan& Plan,
        const FDeferredRenderGraphDeclaration& Graph,
        const FDeferredFrameExecutionBindings& Bindings) const;
};

[[nodiscard]] Stoner::Core::TArray<FDeferredShaderBindingContract>
GetCanonicalDeferredShaderBindings();
[[nodiscard]] FDeferredVertexLayoutContract GetDeferredSurfaceVertexLayout();
[[nodiscard]] FDeferredVertexLayoutContract GetDeferredFullscreenVertexLayout();
[[nodiscard]] FDeferredVertexLayoutContract GetDeferredVolumeVertexLayout();
[[nodiscard]] FDeferredFrameViewUniform BuildDeferredFrameViewUniform(const FDeferredViewData& View);
[[nodiscard]] FDeferredDrawMaterialUniform BuildDeferredDrawMaterialUniform(
    const FDeferredDrawRecord& Draw);
[[nodiscard]] FDeferredLightUniform BuildDeferredLightUniform(const FDeferredLightRecord& Light);

} // namespace Stoner::Renderer
