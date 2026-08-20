#include "DeferredRenderingTests.h"
#include "ShaderTestFixtures.h"

#include "Renderer/FDeferredFrameExecutor.h"
#include "Renderer/FDeferredLightVolume.h"
#include "Renderer/FDeferredRenderer.h"
#include "VulkanRHI/FVulkanCommandBuffer.h"
#include "VulkanRHI/VulkanDevice.h"

#include <algorithm>
#include <cstddef>
#include <iostream>

namespace
{

using namespace Stoner::Backend::Vulkan;
using namespace Stoner::Core;
using namespace Stoner::Renderer;
using namespace Stoner::RHI;

void Record(FDeferredRenderingTestResult& Result, bool bPassed, const char* Name)
{
    if (bPassed)
    {
        ++Result.Passed;
        std::cout << "[PASS] " << Name << '\n';
    }
    else
    {
        ++Result.Failed;
        std::cout << "[FAIL] " << Name << '\n';
    }
}

bool ContainsResourceName(const TArray<FString>& Values, const char* Name)
{
    return std::find(Values.begin(), Values.end(), FString(Name)) != Values.end();
}

FDeferredViewData MakeView(EDeferredDepthConvention Convention = EDeferredDepthConvention::StandardZ)
{
    FDeferredViewData View;
    View.Name = "DeferredTestView";
    View.Extent = {32, 24};
    View.DepthPolicy = MakeDeferredDepthPolicy(Convention, 0.1f, 100.0f);
    return View;
}

FDeferredDrawCandidate MakeDraw(uint32 Slot = 1, EMaterialBlendMode Blend = EMaterialBlendMode::Opaque)
{
    FDeferredDrawCandidate Draw;
    Draw.Identity = {Slot, 1};
    Draw.MeshId = Slot;
    Draw.MaterialId = Slot;
    Draw.Name = "Draw";
    Draw.BlendMode = Blend;
    return Draw;
}

FDeferredFrameInputs MakeInputs(EDeferredDepthConvention Convention = EDeferredDepthConvention::StandardZ)
{
    FDeferredFrameInputs Inputs;
    Inputs.FrameId = "DeferredTestFrame";
    Inputs.View = MakeView(Convention);
    Inputs.Output = {"DeferredOutput", ERHIFormat::R8G8B8A8_UNorm, Inputs.View.Extent};
    Inputs.DrawCandidates = {MakeDraw()};
    Inputs.DirectionalLights = {{{1, 1}, "Sun", FVector3(0.0f, -1.0f, 0.0f),
        FColor::OpaqueWhite(), 1.0f}};
    Inputs.PointLights = {{{2, 1}, "Point", FVector3(0.0f, 0.0f, 1.0f),
        FColor::OpaqueWhite(), 2.0f, 5.0f}};
    FDeferredSpotLight Spot;
    Spot.Identity = {3, 1};
    Spot.Name = "Spot";
    Spot.Position = FVector3(0.0f, 1.0f, 1.0f);
    Spot.Direction = FVector3(0.0f, -1.0f, 0.0f);
    Spot.Range = 6.0f;
    Inputs.SpotLights = {Spot};
    return Inputs;
}

FRHIShaderModuleDesc MakeShader(ERHIShaderStage Stage)
{
    FRHIShaderModuleDesc Desc;
    Desc.Stage = Stage;
    Desc.EntryPoint = "main";
    const char* Identity = Stage == ERHIShaderStage::Vertex
        ? "DeferredTestVS"
        : "DeferredTestFS";
    Desc.Payload = Stoner::Tests::MakeMinimalShaderPayload(
        Stage, Desc.EntryPoint.View(), Identity);
    return Desc;
}

TRHIObjectResult<IRHIGraphicsPipeline> MakePipeline(FVulkanDevice& Device,
    const TSharedPtr<IRHIPipelineLayout>& Layout, TArray<ERHIFormat> Colors,
    ERHIFormat Depth = ERHIFormat::Unknown, bool bSurface = false)
{
    const auto Vertex = Device.CreateShaderModule(MakeShader(ERHIShaderStage::Vertex));
    const auto Fragment = Device.CreateShaderModule(MakeShader(ERHIShaderStage::Fragment));
    FRHIGraphicsPipelineDesc Desc;
    Desc.ShaderModules = {Vertex.Object, Fragment.Object};
    Desc.PipelineLayout = Layout;
    const FDeferredVertexLayoutContract VertexLayout = bSurface
        ? GetDeferredSurfaceVertexLayout() : GetDeferredFullscreenVertexLayout();
    Desc.VertexInput.Stride = VertexLayout.Stride;
    Desc.VertexInput.Attributes = VertexLayout.Attributes;
    Desc.RenderTargets.ColorFormats = std::move(Colors);
    Desc.RenderTargets.DepthStencilFormat = Depth;
    Desc.DepthStencil.bDepthTestEnabled = Depth != ERHIFormat::Unknown;
    Desc.DepthStencil.bDepthWriteEnabled = Depth != ERHIFormat::Unknown;
    return Device.CreateGraphicsPipeline(Desc);
}

struct FExecutionFixture
{
    FVulkanDevice Device;
    FDeferredFrameExecutionBindings Bindings;

    bool Initialize(const FDeferredFramePlan& Plan)
    {
        FVulkanInstanceDesc InstanceDesc;
        InstanceDesc.RuntimeMode = EVulkanInstanceRuntimeMode::DeterministicFallback;
        if (Device.Initialize(InstanceDesc) != ERHIResult::Success)
        {
            return false;
        }
        const auto MakeTexture = [this, &Plan](ERHIFormat Format, ERHITextureUsage Usage) {
            FRHITextureDesc Desc;
            Desc.Width = Plan.SurfaceLayout.Extent.Width;
            Desc.Height = Plan.SurfaceLayout.Extent.Height;
            Desc.Format = Format;
            Desc.Usage = Usage;
            return Device.CreateTexture(Desc).Object;
        };
        const ERHITextureUsage GBufferUsage = ERHITextureUsage::ColorAttachment |
            ERHITextureUsage::Sampled | ERHITextureUsage::CopySource;
        Bindings.BaseColorAO = MakeTexture(ERHIFormat::R8G8B8A8_UNorm, GBufferUsage);
        Bindings.NormalRoughness = MakeTexture(ERHIFormat::R16G16B16A16_Float, GBufferUsage);
        Bindings.EmissiveMetallic = MakeTexture(ERHIFormat::R16G16B16A16_Float, GBufferUsage);
        Bindings.Depth = MakeTexture(ERHIFormat::D32_Float,
            ERHITextureUsage::DepthStencilAttachment | ERHITextureUsage::Sampled |
            ERHITextureUsage::CopySource);
        Bindings.LightingAccumulation = MakeTexture(ERHIFormat::R16G16B16A16_Float, GBufferUsage);
        Bindings.FinalOutput = MakeTexture(Plan.Output.Format,
            ERHITextureUsage::ColorAttachment | ERHITextureUsage::CopySource);
        Bindings.SurfaceVertexBuffer = Device.CreateBuffer({128, ERHIBufferUsage::Vertex}).Object;
        Bindings.SurfaceIndexBuffer = Device.CreateBuffer({64, ERHIBufferUsage::Index}).Object;
        Bindings.SurfaceIndexCount = 3;
        Bindings.FullscreenVertexBuffer = Device.CreateBuffer({64, ERHIBufferUsage::Vertex}).Object;
        Bindings.SphereVertexBuffer = Device.CreateBuffer({256, ERHIBufferUsage::Vertex}).Object;
        Bindings.SphereIndexBuffer = Device.CreateBuffer({256, ERHIBufferUsage::Index}).Object;
        Bindings.SphereIndexCount = 36;
        Bindings.ConeVertexBuffer = Device.CreateBuffer({256, ERHIBufferUsage::Vertex}).Object;
        Bindings.ConeIndexBuffer = Device.CreateBuffer({256, ERHIBufferUsage::Index}).Object;
        Bindings.ConeIndexCount = 24;
        Bindings.CommandBuffer = Device.CreateCommandBuffer(ERHIQueueType::Graphics).Object;

        FRHIPipelineLayoutDesc LayoutDesc;
        for (const FDeferredShaderBindingContract& Binding : GetCanonicalDeferredShaderBindings())
        {
            LayoutDesc.Bindings.push_back({Binding.Set, Binding.Binding, Binding.Type, 1,
                Binding.Visibility});
        }
        const auto Layout = Device.CreatePipelineLayout(LayoutDesc).Object;
        const auto SurfacePass = Device.CreateRenderPass({{
            {ERHIAttachmentRole::Color, ERHIFormat::R8G8B8A8_UNorm, ERHISampleCount::One,
                ERHIAttachmentLoadOp::Clear, ERHIAttachmentStoreOp::Store},
            {ERHIAttachmentRole::Color, ERHIFormat::R16G16B16A16_Float, ERHISampleCount::One,
                ERHIAttachmentLoadOp::Clear, ERHIAttachmentStoreOp::Store},
            {ERHIAttachmentRole::Color, ERHIFormat::R16G16B16A16_Float, ERHISampleCount::One,
                ERHIAttachmentLoadOp::Clear, ERHIAttachmentStoreOp::Store},
            {ERHIAttachmentRole::DepthStencil, ERHIFormat::D32_Float, ERHISampleCount::One,
                ERHIAttachmentLoadOp::Clear, ERHIAttachmentStoreOp::Store}}}).Object;
        FRHIFramebufferDesc SurfaceFramebuffer;
        SurfaceFramebuffer.RenderPass = SurfacePass;
        SurfaceFramebuffer.Attachments = {{Bindings.BaseColorAO, 0, 0}, {Bindings.NormalRoughness, 0, 0},
            {Bindings.EmissiveMetallic, 0, 0}, {Bindings.Depth, 0, 0}};
        SurfaceFramebuffer.Width = Plan.SurfaceLayout.Extent.Width;
        SurfaceFramebuffer.Height = Plan.SurfaceLayout.Extent.Height;
        Bindings.Surface.RenderPass = SurfacePass;
        Bindings.Surface.Framebuffer = Device.CreateFramebuffer(SurfaceFramebuffer).Object;
        Bindings.Surface.Pipeline = MakePipeline(Device, Layout,
            {ERHIFormat::R8G8B8A8_UNorm, ERHIFormat::R16G16B16A16_Float,
                ERHIFormat::R16G16B16A16_Float}, ERHIFormat::D32_Float, true).Object;

        const auto MakeStage = [this, &Plan, &Layout](const TSharedPtr<IRHITexture>& Target,
            ERHIFormat Format) {
            FDeferredStageBindings Stage;
            Stage.RenderPass = Device.CreateRenderPass({{{ERHIAttachmentRole::Color, Format,
                ERHISampleCount::One, ERHIAttachmentLoadOp::Clear,
                ERHIAttachmentStoreOp::Store}}}).Object;
            FRHIFramebufferDesc Desc;
            Desc.RenderPass = Stage.RenderPass;
            Desc.Attachments = {{Target, 0, 0}};
            Desc.Width = Plan.SurfaceLayout.Extent.Width;
            Desc.Height = Plan.SurfaceLayout.Extent.Height;
            Stage.Framebuffer = Device.CreateFramebuffer(Desc).Object;
            Stage.Pipeline = MakePipeline(Device, Layout, {Format}).Object;
            return Stage;
        };
        Bindings.Directional = MakeStage(Bindings.LightingAccumulation, ERHIFormat::R16G16B16A16_Float);
        Bindings.PointOutside = MakeStage(Bindings.LightingAccumulation, ERHIFormat::R16G16B16A16_Float);
        Bindings.PointInside = Bindings.PointOutside;
        Bindings.SpotOutside = MakeStage(Bindings.LightingAccumulation, ERHIFormat::R16G16B16A16_Float);
        Bindings.SpotInside = Bindings.SpotOutside;
        Bindings.Composition = MakeStage(Bindings.FinalOutput, Plan.Output.Format);
        Bindings.Transparency = Bindings.Composition;
        return Bindings.Surface.Pipeline && Bindings.Composition.Pipeline;
    }

    ~FExecutionFixture()
    {
        (void)Device.Shutdown();
    }
};

void TestSurfaceAndNormalContracts(FDeferredRenderingTestResult& Result)
{
    const auto Standard = MakeDefaultDeferredSurfaceLayout({64, 64}, EDeferredDepthConvention::StandardZ);
    const auto Reversed = MakeDefaultDeferredSurfaceLayout({64, 64}, EDeferredDepthConvention::ReversedZ);
    Record(Result, Standard.IsValid() && Standard.DepthPolicy.FarClearValue == 1.0f &&
        Standard.DepthPolicy.CompareOp == ERHICompareOp::LessEqual &&
        Reversed.IsValid() && Reversed.DepthPolicy.FarClearValue == 0.0f &&
        Reversed.DepthPolicy.CompareOp == ERHICompareOp::GreaterEqual,
        "Deferred surface layout derives clear and comparison from depth convention");
    Record(Result, Standard.FindAttachment(EDeferredSurfaceSemantic::WorldNormal)->Format ==
        ERHIFormat::R16G16B16A16_Float &&
        Standard.FindAttachment(EDeferredSurfaceSemantic::AmbientOcclusion)->Format ==
        ERHIFormat::R8G8B8A8_UNorm,
        "Deferred surface layout stores normalized world normal and UNorm8 ambient occlusion");

    FMatrix4x4 NormalMatrix;
    const bool bNormalValid = TryBuildWorldNormalFromModel(
        FMatrix4x4::Scale(FVector3(2.0f, 4.0f, 5.0f)), NormalMatrix);
    Record(Result, bNormalValid &&
        FMath::IsNearlyEqual(NormalMatrix.M[0][0], 0.5f) &&
        FMath::IsNearlyEqual(NormalMatrix.M[1][1], 0.25f) &&
        FMath::IsNearlyEqual(NormalMatrix.M[2][2], 0.2f) &&
        NormalMatrix.M[0][3] == 0.0f && NormalMatrix.M[3][3] == 1.0f,
        "Deferred world normal transform is affine inverse-transpose under non-uniform scale");
    Record(Result, !TryBuildWorldNormalFromModel(
        FMatrix4x4::Scale(FVector3(1.0f, 0.0f, 1.0f)), NormalMatrix),
        "Deferred draw rejects singular model transforms");

    FDeferredViewData ReconstructedView = MakeView();
    ReconstructedView.ViewProjection = FMatrix4x4::Translation(
        FVector3(-3.0f, 2.0f, -1.0f));
    const bool bHasInverse = ReconstructedView.ViewProjection.TryInverse(
        ReconstructedView.InverseViewProjection);
    const FVector3 WorldPoint(8.0f, -4.0f, 0.5f);
    Record(Result, bHasInverse && ReconstructedView.IsValid() &&
        ReconstructedView.InverseViewProjection.TransformPoint(
            ReconstructedView.ViewProjection.TransformPoint(WorldPoint)).NearlyEquals(WorldPoint),
        "Deferred view keeps an explicit inverse view-projection reconstruction contract");
}

void TestPlanningAndGraph(FDeferredRenderingTestResult& Result)
{
    FDeferredRenderer Renderer;
    FDeferredFramePlan Plan;
    const FDeferredFrameInputs Inputs = MakeInputs();
    Record(Result, Renderer.PrepareFrame(Inputs, Plan) == EDeferredResult::Success &&
        Plan.IsValid() && Plan.AcceptedDraws.size() == 1 && Plan.Lights.Accepted.size() == 3,
        "Deferred renderer accepts opaque geometry and all three supported light types");
    Record(Result, Plan.Passes.size() == 5 &&
        Plan.Passes[0].Stage == EDeferredPassStage::SurfaceData &&
        Plan.Passes[1].Stage == EDeferredPassStage::DirectionalLighting &&
        Plan.Passes[2].Stage == EDeferredPassStage::PointLightVolumes &&
        Plan.Passes[3].Stage == EDeferredPassStage::SpotLightVolumes &&
        Plan.Passes[4].Stage == EDeferredPassStage::Composition,
        "Deferred renderer emits canonical surface light and composition order");
    Record(Result, !ContainsResourceName(Plan.Passes[0].Writes, "LightingAccumulation") &&
        ContainsResourceName(Plan.Passes[1].Writes, "LightingAccumulation") &&
        ContainsResourceName(Plan.Passes[4].Reads, "LightingAccumulation"),
        "Deferred graph ownership keeps lighting accumulation out of surface writes");
    FDeferredDiagnosticLog GraphDiagnostics;
    const auto Graph = BuildDeferredRenderGraphDeclaration(Plan, &GraphDiagnostics);
    Record(Result, Graph.bValid && Graph.Resources.size() == 6 &&
        Graph.Passes.size() == Plan.Passes.size() && Graph.FinalOutput == Inputs.Output.Name,
        "Deferred frame plan declares complete render graph resources accesses and output");

    FDeferredFrameInputs Empty = MakeInputs();
    Empty.DrawCandidates.clear();
    Empty.DirectionalLights.clear();
    Empty.PointLights.clear();
    Empty.SpotLights.clear();
    Record(Result, Renderer.PrepareFrame(Empty, Plan) == EDeferredResult::Success &&
        Plan.Passes.size() == 2 && Plan.Passes.back().Stage == EDeferredPassStage::Composition,
        "Deferred renderer accepts empty and ambient-only frame with one composition output");
    Record(Result, !ContainsResourceName(Plan.Passes[0].Writes, "LightingAccumulation") &&
        ContainsResourceName(Plan.Passes.back().Reads, "LightingAccumulation"),
        "Deferred empty frame graph keeps accumulation as composition input only");

    FDeferredFrameInputs Masked = MakeInputs(EDeferredDepthConvention::ReversedZ);
    Masked.DrawCandidates[0].BlendMode = EMaterialBlendMode::Masked;
    FDeferredDrawCandidate Transparent = MakeDraw(9, EMaterialBlendMode::Translucent);
    Masked.DrawCandidates.push_back(Transparent);
    Record(Result, Renderer.PrepareFrame(Masked, Plan) == EDeferredResult::Success &&
        Plan.AcceptedDraws.size() == 1 && Plan.TransparentHandoff.size() == 1 &&
        Plan.FindPass(EDeferredPassStage::ForwardTransparency) != nullptr &&
        Plan.SurfaceLayout.DepthPolicy.Convention == EDeferredDepthConvention::ReversedZ,
        "Deferred renderer accepts masked geometry and hands transparency to forward ordering");

    const FString StableDump = Plan.DebugDump;
    bool bStable = true;
    for (int Run = 0; Run < 20; ++Run)
    {
        FDeferredFramePlan Repeated;
        bStable = bStable && Renderer.PrepareFrame(Masked, Repeated) == EDeferredResult::Success &&
            Repeated.DebugDump == StableDump;
    }
    Record(Result, bStable, "Deferred frame plan and diagnostics are byte-stable across twenty runs");
}

void TestShaderAndExecutionContracts(FDeferredRenderingTestResult& Result)
{
    const auto Bindings = GetCanonicalDeferredShaderBindings();
    Record(Result, Bindings.size() == 8 && Bindings.front().Set == 0 &&
        Bindings.back().Set == 3 && Bindings.back().Binding == 0,
        "Deferred shader contract exposes canonical set zero through three bindings");
    Record(Result, sizeof(FDeferredFrameViewUniform) == 304 &&
        offsetof(FDeferredFrameViewUniform, InverseViewProjection) == 128 &&
        offsetof(FDeferredFrameViewUniform, DepthConvention) == 288 &&
        sizeof(FDeferredDrawMaterialUniform) == 176 &&
        offsetof(FDeferredDrawMaterialUniform, WorldNormalFromModel) == 64 &&
        sizeof(FDeferredLightUniform) == 64,
        "Deferred mirrored records preserve exact sizes and offsets");
    const auto Surface = GetDeferredSurfaceVertexLayout();
    const auto Fullscreen = GetDeferredFullscreenVertexLayout();
    const auto Volume = GetDeferredVolumeVertexLayout();
    Record(Result, Surface.Stride == 24 && Surface.Attributes.size() == 2 &&
        Fullscreen.Stride == 8 && !Fullscreen.bIndexed &&
        Volume.Stride == 12 && Volume.IndexType == ERHIIndexType::UInt16,
        "Deferred surface fullscreen and volume vertex layouts match canonical contract");

    FDeferredRenderer Renderer;
    FDeferredFramePlan Plan;
    const FDeferredFrameInputs Inputs = MakeInputs();
    (void)Renderer.PrepareFrame(Inputs, Plan);
    const auto Graph = BuildDeferredRenderGraphDeclaration(Plan);
    FExecutionFixture Fixture;
    const bool bFixture = Fixture.Initialize(Plan);
    const FDeferredFrameExecutionResult Execution =
        FDeferredFrameExecutor().Execute(Plan, Graph, Fixture.Bindings);
    auto Commands = std::dynamic_pointer_cast<FVulkanCommandBuffer>(Fixture.Bindings.CommandBuffer);
    bool bHasIndexed = false;
    bool bHasDescriptorSafeOrder = false;
    if (Commands)
    {
        for (const FVulkanRecordedCommand& Command : Commands->GetRecordedCommands())
        {
            bHasIndexed = bHasIndexed || Command.Type == ERHISymbolicCommandType::DrawIndexed;
            bHasDescriptorSafeOrder = bHasDescriptorSafeOrder ||
                Command.Type == ERHISymbolicCommandType::BindGraphicsPipeline;
        }
    }
    Record(Result, Fixture.Bindings.Surface.RenderPass && Fixture.Bindings.Surface.Framebuffer,
        "Deferred executor fixture creates surface pass and framebuffer");
    Record(Result, Fixture.Bindings.Surface.Pipeline != nullptr,
        "Deferred executor fixture creates canonical surface pipeline");
    Record(Result, Fixture.Bindings.Composition.Pipeline != nullptr,
        "Deferred executor fixture creates composition pipeline");
    Record(Result, bFixture, "Deferred executor deterministic RHI fixture creates all stage resources");
    Record(Result, Execution.Succeeded() &&
        Execution.RecordedPassCount == Plan.Passes.size() &&
        Execution.RecordedCommandCount > Execution.RecordedDrawCount &&
        Execution.LocalLightBatchCount == 2 &&
        Execution.LocalLightInstanceCount == 2 &&
        Execution.OmittedLocalLightCount == 0,
        "Deferred executor records every canonical graph pass");
    Record(Result, bHasIndexed && bHasDescriptorSafeOrder,
        "Deferred executor command sequence includes pipeline binding and indexed light volumes");

    FDeferredFrameExecutionBindings Invalid = Fixture.Bindings;
    Invalid.Depth.reset();
    const auto InvalidExecution = FDeferredFrameExecutor().Execute(Plan, Graph, Invalid);
    Record(Result, InvalidExecution.Result == EDeferredResult::InvalidBinding &&
        InvalidExecution.FinalState == EDeferredExecutionState::Failed &&
        Fixture.Bindings.CommandBuffer->GetRecordedCommandCount() == Execution.RecordedCommandCount,
        "Deferred executor rejects incomplete bindings before recording any new commands");

    FDeferredFrameInputs BatchedInputs = MakeInputs();
    BatchedInputs.SpotLights.clear();
    BatchedInputs.PointLights = {
        {{10, 1}, "BatchA", FVector3(0.0f, 0.0f, 1.0f), FColor::OpaqueWhite(), 1.0f, 5.0f},
        {{11, 1}, "BatchB", FVector3(0.0f, 0.0f, 1.0f), FColor::OpaqueWhite(), 1.0f, 5.0f},
        {{12, 1}, "BatchC", FVector3(0.0f, 0.0f, 1.0f), FColor::OpaqueWhite(), 1.0f, 5.0f}};
    FDeferredFramePlan BatchedPlan;
    (void)Renderer.PrepareFrame(BatchedInputs, BatchedPlan);
    FExecutionFixture BatchedFixture;
    const auto BatchedGraph = BuildDeferredRenderGraphDeclaration(BatchedPlan);
    const bool bBatchedFixture = BatchedFixture.Initialize(BatchedPlan);
    const auto BatchedExecution =
        FDeferredFrameExecutor().Execute(BatchedPlan, BatchedGraph, BatchedFixture.Bindings);
    auto BatchedCommands =
        std::dynamic_pointer_cast<FVulkanCommandBuffer>(BatchedFixture.Bindings.CommandBuffer);
    bool bBatchUsesAcceptedLightOffset = false;
    if (BatchedCommands)
    {
        for (const FVulkanRecordedCommand& Command : BatchedCommands->GetRecordedCommands())
        {
            bBatchUsesAcceptedLightOffset = bBatchUsesAcceptedLightOffset ||
                (Command.Type == ERHISymbolicCommandType::DrawIndexed &&
                    Command.B == 3 && Command.C == 0 &&
                    Command.D == 0 && Command.E == 1);
        }
    }
    Record(Result, bBatchedFixture && BatchedExecution.Succeeded() &&
        BatchedExecution.LocalLightBatchCount == 1 &&
        BatchedExecution.LocalLightInstanceCount == 3 &&
        bBatchUsesAcceptedLightOffset &&
        BatchedPlan.FindPass(EDeferredPassStage::SurfaceData)->DrawCount == 1,
        "Deferred executor batches equal-volume lights without increasing surface work");

    FDeferredRenderer ReadbackRenderer({true, true, true, true, ERHISampleCount::One});
    FDeferredFramePlan ReadbackPlan;
    (void)ReadbackRenderer.PrepareFrame(Inputs, ReadbackPlan);
    FExecutionFixture ReadbackFixture;
    const auto ReadbackGraph = BuildDeferredRenderGraphDeclaration(ReadbackPlan);
    const bool bReadbackFixture = ReadbackFixture.Initialize(ReadbackPlan);
    ReadbackFixture.Bindings.Readbacks.push_back(
        {"InvalidReadback", ReadbackFixture.Bindings.FinalOutput, nullptr, {}});
    const auto FailedExecution =
        FDeferredFrameExecutor().Execute(ReadbackPlan, ReadbackGraph, ReadbackFixture.Bindings);
    const FDeferredDiagnostic* Failure = FailedExecution.Diagnostics.GetFirstError();
    Record(Result, bReadbackFixture &&
        FailedExecution.Result == EDeferredResult::ReadbackFailed &&
        FailedExecution.FinalState == EDeferredExecutionState::Failed &&
        FailedExecution.LastCompletedStage == EDeferredPassStage::Composition &&
        FailedExecution.RecordedPassCount + 1 == ReadbackPlan.Passes.size() &&
        Failure && Failure->Code == "DEF-EXEC-READBACK" &&
        ReadbackFixture.Bindings.CommandBuffer->GetRecordedCommandCount() == 0,
        "Deferred executor stops before dependent readback success on the first copy failure");
}

void TestInvalidInputs(FDeferredRenderingTestResult& Result)
{
    FDeferredRenderer Renderer;
    FDeferredFramePlan Plan;
    FDeferredFrameInputs Inputs = MakeInputs();
    Inputs.View.InverseViewProjection.M[0][0] = 2.0f;
    Record(Result, Renderer.PrepareFrame(Inputs, Plan) == EDeferredResult::InvalidView &&
        !Plan.IsValid(), "Deferred renderer rejects mismatched inverse view-projection");

    Inputs = MakeInputs();
    Inputs.DrawCandidates[0].Model = FMatrix4x4::Scale(FVector3(1.0f, 0.0f, 1.0f));
    Record(Result, Renderer.PrepareFrame(Inputs, Plan) == EDeferredResult::Success &&
        Plan.AcceptedDraws.empty() && Plan.RejectedDraws.size() == 1,
        "Deferred renderer records singular-transform draw rejection without corrupting frame");

    Inputs = MakeInputs();
    Inputs.SpotLights[0].InnerConeAngleRadians = 0.7f;
    Inputs.SpotLights[0].OuterConeAngleRadians = 0.5f;
    Record(Result, Renderer.PrepareFrame(Inputs, Plan) == EDeferredResult::Success &&
        Plan.Lights.Rejected.size() == 1 && Plan.Lights.Accepted.size() == 2,
        "Deferred renderer rejects invalid spot cone radians while preserving valid lights");

    FDeferredDiagnosticLog Diagnostics;
    Diagnostics.Add(EDeferredDiagnosticSeverity::Error, EDeferredPassStage::SpotLightVolumes,
        EDeferredResult::InvalidLight, "DEF-LATE", "Spot", "later failure");
    Diagnostics.Add(EDeferredDiagnosticSeverity::Error, EDeferredPassStage::SurfaceData,
        EDeferredResult::InvalidDraw, "DEF-EARLY", "Draw", "first actionable failure");
    const FDeferredDiagnostic* FirstBeforeSort = Diagnostics.GetFirstError();
    const bool bFirstOccurrenceBeforeSort = FirstBeforeSort &&
        FirstBeforeSort->Code == FString("DEF-LATE");
    Diagnostics.SortStable();
    const FDeferredDiagnostic* FirstAfterSort = Diagnostics.GetFirstError();
    Record(Result, bFirstOccurrenceBeforeSort && FirstAfterSort &&
        FirstAfterSort->Code == FString("DEF-LATE"),
        "Deferred diagnostics preserve first occurrence ownership after display sorting");

    const FString DiagnosticDump = Diagnostics.Dump();
    Record(Result, DiagnosticDump.View().find("0x") == std::string_view::npos &&
        DiagnosticDump.View().find("subject=") != std::string_view::npos &&
        DiagnosticDump.View().find("reason=") != std::string_view::npos,
        "Deferred diagnostics are normalized and omit native addresses");
}

void TestLocalLightScaling(FDeferredRenderingTestResult& Result)
{
    FDeferredRenderer Renderer;
    FDeferredFrameInputs Inputs = MakeInputs();
    Inputs.DirectionalLights.clear();
    Inputs.SpotLights.clear();
    Inputs.PointLights.clear();

    FDeferredFramePlan EmptyPlan;
    (void)Renderer.PrepareFrame(Inputs, EmptyPlan);
    for (uint32 Index = 0; Index < 256; ++Index)
    {
        FDeferredPointLight Light;
        Light.Identity = {Index + 1, 1};
        Light.Name = "PointTier";
        Light.Position = FVector3(0.0f, 0.0f, 0.5f);
        Light.Range = 0.1f;
        Inputs.PointLights.push_back(Light);
    }
    std::reverse(Inputs.PointLights.begin(), Inputs.PointLights.end());
    FDeferredFramePlan TierPlan;
    (void)Renderer.PrepareFrame(Inputs, TierPlan);
    Inputs.PointLights.clear();
    FDeferredFramePlan ClearedPlan;
    (void)Renderer.PrepareFrame(Inputs, ClearedPlan);
    Record(Result, EmptyPlan.Lights.Accepted.empty() &&
        TierPlan.Lights.GetAcceptedCount(EDeferredLightType::Point) == 256 &&
        TierPlan.Lights.Accepted.front().Identity.Slot == 1 &&
        TierPlan.Lights.Accepted.back().Identity.Slot == 256 &&
        ClearedPlan.Lights.Accepted.empty() && ClearedPlan.Lights.Culled.empty(),
        "Deferred renderer has no local-light cap and clears stale records across 0-256-0 frames");
    Record(Result, EmptyPlan.FindPass(EDeferredPassStage::SurfaceData)->DrawCount ==
        TierPlan.FindPass(EDeferredPassStage::SurfaceData)->DrawCount,
        "Deferred surface geometry work remains constant as local-light count changes");

    const FDeferredViewData View = MakeView();
    FDeferredLightRecord Outside;
    Outside.Type = EDeferredLightType::Point;
    Outside.Position = FVector3(2.0f, 0.0f, 0.5f);
    Outside.Range = 0.1f;
    const auto OutsideClassification = ClassifyDeferredLightVolume(Outside, View);
    Outside.Position = FVector3(1.1f, 0.0f, 0.5f);
    const auto BoundaryClassification = ClassifyDeferredLightVolume(Outside, View);
    Outside.Position = FVector3::Zero();
    Outside.Range = 1.0f;
    const auto InsideClassification = ClassifyDeferredLightVolume(Outside, View);
    Outside.Position = FVector3(0.2f, 0.0f, 0.0f);
    Outside.Range = 0.1f;
    const auto NearClassification = ClassifyDeferredLightVolume(Outside, View);
    Record(Result,
        OutsideClassification.Acceptance == EDeferredLightAcceptance::CulledOutsideView &&
        BoundaryClassification.Acceptance != EDeferredLightAcceptance::CulledOutsideView &&
        InsideClassification.Acceptance == EDeferredLightAcceptance::AcceptedVolumeCameraInside &&
        NearClassification.Acceptance == EDeferredLightAcceptance::AcceptedVolumeNearPlaneIntersection,
        "Deferred light volumes classify outside boundary camera-inside and near-plane cases");

    const FString StableTierDump = TierPlan.DebugDump;
    bool bTierStable = true;
    for (int Run = 0; Run < 20; ++Run)
    {
        Inputs.PointLights.clear();
        for (uint32 Index = 0; Index < 256; ++Index)
        {
            FDeferredPointLight Light;
            Light.Identity = {Index + 1, 1};
            Light.Name = "PointTier";
            Light.Position = FVector3(0.0f, 0.0f, 0.5f);
            Light.Range = 0.1f;
            Inputs.PointLights.push_back(Light);
        }
        FDeferredFramePlan Repeated;
        bTierStable = bTierStable &&
            Renderer.PrepareFrame(Inputs, Repeated) == EDeferredResult::Success &&
            Repeated.DebugDump == StableTierDump;
    }
    Record(Result, bTierStable,
        "Deferred representative light-tier report is byte-stable across twenty runs");

    FDeferredFrameInputs Representative = MakeInputs();
    Representative.DrawCandidates.clear();
    Representative.PointLights.clear();
    Representative.SpotLights.clear();
    for (uint32 Index = 0; Index < 100; ++Index)
    {
        Representative.DrawCandidates.push_back(MakeDraw(Index + 1));
    }
    for (uint32 Index = 0; Index < 64; ++Index)
    {
        FDeferredPointLight Light;
        Light.Identity = {200 + Index, 1};
        Light.Name = "SC001Point";
        Light.Position = FVector3(0.0f, 0.0f, 0.5f);
        Light.Range = 0.1f;
        Representative.PointLights.push_back(Light);
    }
    for (uint32 Index = 0; Index < 16; ++Index)
    {
        FDeferredSpotLight Light;
        Light.Identity = {300 + Index, 1};
        Light.Name = "SC001Spot";
        Light.Position = FVector3(0.0f, 0.0f, 0.5f);
        Light.Direction = FVector3(0.0f, 0.0f, -1.0f);
        Light.Range = 0.1f;
        Representative.SpotLights.push_back(Light);
    }
    FDeferredFramePlan RepresentativePlan;
    bool bRepresentativeStable =
        Renderer.PrepareFrame(Representative, RepresentativePlan) == EDeferredResult::Success;
    const FString RepresentativeDump = RepresentativePlan.DebugDump;
    for (int Run = 1; Run < 20; ++Run)
    {
        FDeferredFramePlan Repeated;
        bRepresentativeStable = bRepresentativeStable &&
            Renderer.PrepareFrame(Representative, Repeated) == EDeferredResult::Success &&
            Repeated.DebugDump == RepresentativeDump;
    }
    Record(Result, bRepresentativeStable &&
        RepresentativePlan.AcceptedDraws.size() == 100 &&
        RepresentativePlan.Lights.GetAcceptedCount(EDeferredLightType::Directional) == 1 &&
        RepresentativePlan.Lights.GetAcceptedCount(EDeferredLightType::Point) == 64 &&
        RepresentativePlan.Lights.GetAcceptedCount(EDeferredLightType::Spot) == 16 &&
        RepresentativePlan.FindPass(EDeferredPassStage::SurfaceData)->DrawCount == 100,
        "Deferred SC-001 workload is complete and deterministic across twenty runs");
}

} // namespace

FDeferredRenderingTestResult RunDeferredRenderingTests()
{
    FDeferredRenderingTestResult Result;
    TestSurfaceAndNormalContracts(Result);
    TestPlanningAndGraph(Result);
    TestShaderAndExecutionContracts(Result);
    TestInvalidInputs(Result);
    TestLocalLightScaling(Result);
    return Result;
}
