#include "ProductionContentDemoTests.h"

#include "FDemoConfiguration.h"
#include "FDemoValidationMonitor.h"
#include "FProductionContentComposition.h"
#include "FProductionContentDeferredExecution.h"
#include "RendererStaticModelRealizationTestSupport.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>

namespace
{

using namespace Stoner;
using namespace Stoner::Demo;

void Record(
    FProductionContentDemoTestResult& Result,
    bool bPassed,
    const char* Name)
{
    (bPassed ? ++Result.Passed : ++Result.Failed);
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

Core::TArray<const char*> RegularArguments(const char* Backend)
{
    return {
        "StonerDemo", "--mode", "headless-vulkan", "--backend", Backend,
        "--workload", "production-content", "--production-root",
        "StaticModel:ProductionAcceptance/Lantern", "--workload-revision",
        "production-content-v1", "--render-path", "deferred-full",
        "--strict-generation", "generation-test", "--cooked-root",
        "Build/Test/Published", "--lease-root", "Build/Test/Lease",
        "--target-profile", "Config/AssetCooker/Profiles/Mac-Vulkan.json",
        "--production-cycles", "20", "--production-warmup-cycles", "2",
        "--baseline-root", "Content/ProductionAcceptance/Baselines",
        "--device-class-registry",
        "Config/Validation/ProductionContent/DeviceClasses.json"};
}

EDemoExitCode ParseArray(
    Core::TArray<const char*> Arguments,
    FDemoConfiguration& Out,
    Core::FString& Reason)
{
    return FDemoConfiguration::Parse(
        static_cast<int>(Arguments.size()), Arguments.data(), Out, Reason);
}

bool BuildDeferredShaderClosure(
    Core::TArray<Core::TSharedPtr<const Asset::FShaderAsset>>& OutShaders,
    Core::TArray<Core::TSharedPtr<const Asset::FShaderPayloadAsset>>& OutPayloads)
{
    using namespace Stoner::Tests::StaticModelRealization;
    const auto Append = [&OutShaders, &OutPayloads](
        const char* Leaf,
        Core::TArray<Asset::FShaderInterfaceBinding> Bindings) -> bool
    {
        Asset::FShaderAssetDesc Desc;
        Desc.Id = Id("ShaderProgram",
            (std::string("Engine/Shaders/Deferred/") + Leaf).c_str());
        Desc.Version = Version(Leaf);
        Desc.InterfaceBindings = std::move(Bindings);
        Asset::FShaderVariantDefinition Variant;
        Variant.VariantName = "default";
        for (const auto Stage : {
             Asset::EShaderStage::Vertex,
             Asset::EShaderStage::Fragment})
        {
            const char* Suffix = Stage == Asset::EShaderStage::Vertex
                ? "vertex" : "fragment";
            const auto Bytes = Spirv(Stage);
            const Asset::FAssetId PayloadId = Id(
                "ShaderPayload",
                (std::string("Engine/Shaders/Deferred/") + Leaf).c_str(),
                (std::string("payload.vulkan.") + Suffix).c_str());
            Asset::FAssetVersion PayloadVersion;
            PayloadVersion.SourceDigest =
                Asset::FAssetDigest::FromBytes(Bytes);
            PayloadVersion.ContentDigest = PayloadVersion.SourceDigest;
            Asset::FShaderPayloadAsset Payload;
            if (Asset::FShaderPayloadAsset::Create(
                    PayloadId, PayloadVersion,
                    Asset::EShaderBackendFamily::Vulkan,
                    "vulkan-1.3", Asset::EShaderPayloadFormat::SPIRV,
                    Stage, "main", {}, Bytes, Payload) !=
                Asset::EAssetResult::Success)
                return false;
            OutPayloads.push_back(
                Core::MakeShared<const Asset::FShaderPayloadAsset>(
                    std::move(Payload)));

            Asset::FShaderSourceReference Source;
            Source.Stage = Stage;
            Source.EntryPoint = "main";
            Source.Locator = Core::FString(
                std::string(Leaf) + "." + Suffix);
            Source.ExpectedDigest = Version(Source.Locator.View()).ContentDigest;
            const auto SourceId = Id("ShaderSource",
                (std::string("Engine/Shaders/Deferred/") + Leaf).c_str(),
                (std::string("source.") + Suffix).c_str());
            (void)Asset::TSoftAssetRef<Asset::FShaderSourceAsset>::Create(
                SourceId, Source.Source);
            Desc.Stages.push_back(std::move(Source));

            Asset::FShaderPayloadReference Reference;
            Reference.Backend = Asset::EShaderBackendFamily::Vulkan;
            Reference.Profile = "vulkan-1.3";
            Reference.Format = Asset::EShaderPayloadFormat::SPIRV;
            Reference.Stage = Stage;
            Reference.EntryPoint = "main";
            (void)Asset::TSoftAssetRef<Asset::FShaderPayloadAsset>::Create(
                PayloadId, Reference.Payload);
            Reference.Locator = Core::FString(
                std::string(Leaf) + "." + Suffix + ".spv");
            Reference.ExpectedDigest = PayloadVersion.ContentDigest;
            Reference.Producer = "Stoner.Tests";
            Reference.ProducerVersion = "028-v1";
            Variant.Payloads.push_back(std::move(Reference));
        }
        Desc.Variants.push_back(std::move(Variant));
        Asset::FShaderAsset Shader;
        if (Asset::FShaderAsset::CreateValidated(
                std::move(Desc), Shader) != Asset::EAssetResult::Success)
            return false;
        OutShaders.push_back(
            Core::MakeShared<const Asset::FShaderAsset>(std::move(Shader)));
        return true;
    };

    const auto Binding = [](Core::uint32 Set, Core::uint32 Slot,
        Asset::EShaderResourceKind Kind,
        Core::TArray<Asset::EShaderStage> Visibility)
    {
        Asset::FShaderInterfaceBinding Result;
        Result.SetIndex = Set;
        Result.BindingIndex = Slot;
        Result.Kind = Kind;
        Result.Visibility = std::move(Visibility);
        Result.Name = Core::FString(
            "Binding" + std::to_string(Set) + "." + std::to_string(Slot));
        return Result;
    };
    const Core::TArray<Asset::EShaderStage> Both = {
        Asset::EShaderStage::Vertex, Asset::EShaderStage::Fragment};
    const Core::TArray<Asset::EShaderStage> Fragment = {
        Asset::EShaderStage::Fragment};
    const auto GBuffer = [&](bool bFrame)
    {
        Core::TArray<Asset::FShaderInterfaceBinding> Result;
        if (bFrame)
            Result.push_back(Binding(0, 0,
                Asset::EShaderResourceKind::UniformBuffer, Both));
        for (Core::uint32 Slot = 0; Slot < 4; ++Slot)
            Result.push_back(Binding(2, Slot,
                Asset::EShaderResourceKind::CombinedTextureSampler,
                Fragment));
        Result.push_back(Binding(3, 0,
            Asset::EShaderResourceKind::StorageBuffer,
            bFrame ? Both : Fragment));
        return Result;
    };
    return Append("DirectionalLight", GBuffer(false)) &&
        Append("PointLight", GBuffer(true)) &&
        Append("SpotLight", GBuffer(true)) &&
        Append("Composition", {
            Binding(2, 0,
                Asset::EShaderResourceKind::CombinedTextureSampler,
                Fragment),
            Binding(2, 2,
                Asset::EShaderResourceKind::CombinedTextureSampler,
                Fragment),
            Binding(2, 4,
                Asset::EShaderResourceKind::CombinedTextureSampler,
                Fragment)});
}

} // namespace

FProductionContentDemoTestResult RunProductionContentDemoTests()
{
    FProductionContentDemoTestResult Result;
    FDemoConfiguration Vulkan;
    FDemoConfiguration Metal;
    Core::FString Reason;
    Record(Result,
        ParseArray(RegularArguments("vulkan"), Vulkan, Reason) ==
                EDemoExitCode::Success &&
            Vulkan.Workload == EDemoWorkload::ProductionContent &&
            Vulkan.ProductionRoot ==
                Core::FString("StaticModel:ProductionAcceptance/Lantern") &&
            Vulkan.StrictGeneration == Core::FString("generation-test") &&
            Vulkan.WorkloadRevision ==
                Core::FString("production-content-v1") &&
            Vulkan.RenderPath == EDemoRenderPath::DeferredFull &&
            Vulkan.ProductionLifecycleCycles == 20 &&
            Vulkan.ProductionWarmupCycles == 2,
        "production configuration preserves strict root revision and fixed lifecycle");

    auto MetalArguments = RegularArguments("metal");
    for (Core::usize Index = 0; Index + 1 < MetalArguments.size(); ++Index)
        if (Core::FString(MetalArguments[Index]) ==
            Core::FString("--target-profile"))
            MetalArguments[Index + 1] =
                "Config/AssetCooker/Profiles/Mac-Metal-Arm64.json";
    Record(Result,
        ParseArray(std::move(MetalArguments), Metal, Reason) ==
                EDemoExitCode::Success &&
            Metal.ProductionRoot == Vulkan.ProductionRoot &&
            Metal.WorkloadRevision == Vulkan.WorkloadRevision &&
            Metal.RenderPath == Vulkan.RenderPath &&
            Metal.GraphicsBackend == EDemoGraphicsBackend::Metal &&
            Vulkan.GraphicsBackend == EDemoGraphicsBackend::Vulkan,
        "Vulkan and Metal consume the same backend-neutral workload identity");

    auto ForwardArguments = RegularArguments("vulkan");
    for (Core::usize Index = 0; Index + 1 < ForwardArguments.size(); ++Index)
        if (Core::FString(ForwardArguments[Index]) ==
            Core::FString("--render-path"))
            ForwardArguments[Index + 1] = "forward-smoke";
    FDemoConfiguration Forward;
    Record(Result,
        ParseArray(std::move(ForwardArguments), Forward, Reason) ==
                EDemoExitCode::Success &&
            Forward.RenderPath == EDemoRenderPath::ForwardSmoke,
        "production configuration distinguishes Deferred full and Forward smoke");

    auto InvalidCycles = RegularArguments("vulkan");
    for (Core::usize Index = 0; Index + 1 < InvalidCycles.size(); ++Index)
        if (Core::FString(InvalidCycles[Index]) ==
            Core::FString("--production-warmup-cycles"))
            InvalidCycles[Index + 1] = "3";
    FDemoConfiguration Invalid;
    Record(Result,
        ParseArray(std::move(InvalidCycles), Invalid, Reason) ==
            EDemoExitCode::InvalidConfiguration,
        "production lifecycle rejects caller-selected warm-up boundaries");

    FDemoValidationMonitor LifecycleMonitor(Vulkan);
    for (Core::uint32 Cycle = 1; Cycle <= 20; ++Cycle)
    {
        const Core::uint64 Rss = 64ULL * 1024ULL * 1024ULL +
            static_cast<Core::uint64>(Cycle) * 1024ULL;
        LifecycleMonitor.AddSyntheticProductionCycle(
            Cycle, Rss, FDemoProductionLifecycleCounters{});
    }
    Record(Result,
        LifecycleMonitor.EvaluateProductionLifecycle() &&
            LifecycleMonitor.GetProductionWarmupBytes() ==
                64ULL * 1024ULL * 1024ULL + 2ULL * 1024ULL &&
            LifecycleMonitor.GetProductionTerminalBytes() ==
                64ULL * 1024ULL * 1024ULL + 20ULL * 1024ULL,
        "production lifecycle fixes RSS origin after included warm-up and terminal cycle");

    FDemoValidationMonitor LeakingMonitor(Vulkan);
    for (Core::uint32 Cycle = 1; Cycle <= 20; ++Cycle)
    {
        FDemoProductionLifecycleCounters Counters;
        if (Cycle == 13) Counters.RendererOwners = 1;
        LeakingMonitor.AddSyntheticProductionCycle(
            Cycle, 64ULL * 1024ULL * 1024ULL, Counters);
    }
    Record(Result, !LeakingMonitor.EvaluateProductionLifecycle(),
        "production lifecycle rejects any non-baseline ownership counter");

    FDemoValidationMonitor StaleMonitor(Vulkan);
    for (Core::uint32 Cycle = 1; Cycle <= 20; ++Cycle)
    {
        FDemoProductionLifecycleCounters Counters;
        if (Cycle == 20) Counters.bStaleHandleRejected = false;
        StaleMonitor.AddSyntheticProductionCycle(
            Cycle, 64ULL * 1024ULL * 1024ULL, Counters);
    }
    Record(Result, !StaleMonitor.EvaluateProductionLifecycle(),
        "production lifecycle rejects stale-handle aliasing");

    auto CallerClass = RegularArguments("vulkan");
    CallerClass.push_back("--device-class");
    CallerClass.push_back("macos.apple8.metal.rgba8");
    Record(Result,
        ParseArray(std::move(CallerClass), Invalid, Reason) ==
                EDemoExitCode::InvalidConfiguration &&
            Reason == Core::FString("device class must be registry-derived"),
        "production configuration rejects caller-supplied device class tokens");

    std::ifstream BuildFile("Demo/StonerDemo/SConscript", std::ios::binary);
    const std::string BuildText{
        std::istreambuf_iterator<char>(BuildFile),
        std::istreambuf_iterator<char>()};
    Record(Result,
        BuildFile.good() || BuildFile.eof(),
        "production demo build contract is readable");
    Record(Result,
        BuildText.find("AssetCooker") == std::string::npos &&
            BuildText.find("Tools/AssetCooker") == std::string::npos,
        "production demo runtime does not link Tools or AssetCooker");

    auto Fixture = Stoner::Tests::StaticModelRealization::MakeFixture();
    Fixture.Request.RenderTargets.ColorFormats = {
        RHI::ERHIFormat::R8G8B8A8_UNorm,
        RHI::ERHIFormat::R16G16B16A16_Float,
        RHI::ERHIFormat::R16G16B16A16_Float};
    Core::TSharedPtr<const Renderer::FStaticModelRenderSnapshot> Snapshot;
    Renderer::FStaticModelRealizationInspection RealizationInspection;
    const auto Realized = Renderer::FStaticModelRealizer::Realize(
        Fixture.Request, Snapshot, RealizationInspection);
    FProductionContentCompositionConfig CompositionConfig;
    CompositionConfig.WorkloadRevision = "production-content-v1";
    CompositionConfig.FrameToken = 17;
    CompositionConfig.Width = 640;
    CompositionConfig.Height = 360;
    FProductionContentComposition Composition;
    Core::FString CompositionReason;
    const bool bComposed =
        Realized == RHI::ERHIResult::Success &&
        FProductionContentCompositionBuilder::Build(
            Snapshot, CompositionConfig, Composition, &CompositionReason);
    Record(Result,
        bComposed && Composition.FrameToken == 17 &&
            Composition.WorkloadRevision ==
                Core::FString("production-content-v1") &&
            Composition.RootAssetId == Snapshot->GetRootAssetId() &&
            Composition.SnapshotGeneration ==
                Snapshot->GetSnapshotGeneration() &&
            Composition.DeferredInputs.DrawCandidates.size() ==
                Snapshot->GetDraws().size() &&
            Composition.ForwardInputs.DrawCandidates.size() ==
                Snapshot->GetDraws().size(),
        "composition preserves root version frame token and every realized draw");

    Renderer::FDeferredRendererConfiguration DeferredConfig;
    DeferredConfig.bEnableValidationReadback = true;
    Renderer::FDeferredFramePlan DeferredPlan;
    Renderer::FForwardFramePlan ForwardPlan;
    const auto DeferredResult = Renderer::FDeferredRenderer(DeferredConfig)
        .PrepareFrame(Composition.DeferredInputs, DeferredPlan);
    const auto ForwardResult = Renderer::FForwardRenderer().PrepareFrame(
        Composition.ForwardInputs, ForwardPlan);
    Record(Result,
        bComposed &&
            DeferredResult == Renderer::EDeferredResult::Success &&
            DeferredPlan.IsValid() &&
            DeferredPlan.AcceptedDraws.size() == Snapshot->GetDraws().size() &&
            DeferredPlan.FindPass(
                Renderer::EDeferredPassStage::ValidationReadback) != nullptr &&
            ForwardResult == Renderer::EForwardResult::Success &&
            ForwardPlan.IsValid() && ForwardPlan.HasRenderableGeometry() &&
            DeferredPlan.View.ViewProjection.NearlyEquals(
                ForwardPlan.ViewData.ViewProjectionMatrix) &&
            DeferredPlan.View.CameraPosition ==
                ForwardPlan.ViewData.CameraPosition,
        "Deferred full and Forward smoke share one backend-neutral composition");

    Renderer::FDeferredFrameExecutionBindings DeferredBindings;
    Renderer::FForwardFrameExecutionBindings ForwardBindings;
    const bool bDeferredBound = bComposed && BindProductionDeferredDraws(
        *Snapshot, DeferredPlan, DeferredBindings, &CompositionReason);
    const bool bUniformsUploaded = bComposed &&
        UploadProductionDeferredUniforms(
            *Fixture.Request.Device, *Snapshot, DeferredPlan,
            &CompositionReason);
    const bool bForwardBound = bComposed && BindProductionForwardDraws(
        *Snapshot, ForwardPlan, ForwardBindings, &CompositionReason);
    Record(Result,
        bDeferredBound && bForwardBound && bUniformsUploaded &&
            DeferredBindings.SurfaceDraws.size() ==
                DeferredPlan.AcceptedDraws.size() &&
            ForwardBindings.Draws.size() ==
                ForwardPlan.AcceptedOpaqueDraws.size() +
                    ForwardPlan.AcceptedTransparentDraws.size() &&
            std::all_of(
                DeferredBindings.SurfaceDraws.begin(),
                DeferredBindings.SurfaceDraws.end(),
                [](const auto& Draw)
                {
                    return Draw.VertexBuffer && Draw.IndexBuffer &&
                        Draw.Pipeline && Draw.Draw.IndexCount == 3;
                }),
        "aggregate snapshot binds exact indexed geometry materials and descriptors into both paths");

    bool bPackedUniformsMatch = bUniformsUploaded;
    std::set<const RHI::IRHIBuffer*> DrawUniformBuffers;
    for (const auto& Draw : DeferredPlan.AcceptedDraws)
    {
        const Core::uint32 ObjectId = Draw.Candidate.Identity.Slot;
        if (ObjectId == 0 || ObjectId > Snapshot->GetDrawResources().size())
        {
            bPackedUniformsMatch = false;
            break;
        }
        const auto& Resources = Snapshot->GetDrawResources()[ObjectId - 1];
        const auto Found = std::find_if(
            Resources.BufferBindings.begin(), Resources.BufferBindings.end(),
            [](const auto& Binding)
            { return Binding.SetIndex == 1 && Binding.BindingSlot == 0; });
        const auto Buffer = Found == Resources.BufferBindings.end()
            ? Core::TSharedPtr<
                Stoner::Tests::StaticModelRealization::FTrackedBuffer>{}
            : std::dynamic_pointer_cast<
                Stoner::Tests::StaticModelRealization::FTrackedBuffer>(
                    Found->Buffer);
        const auto Expected = Renderer::BuildDeferredDrawMaterialUniform(Draw);
        if (!Buffer || Buffer->GetData().size() < sizeof(Expected) ||
            std::memcmp(Buffer->GetData().data(), &Expected,
                sizeof(Expected)) != 0)
        {
            bPackedUniformsMatch = false;
            break;
        }
        DrawUniformBuffers.insert(Buffer.get());
    }
    Record(Result,
        bPackedUniformsMatch && DrawUniformBuffers.size() ==
            DeferredPlan.AcceptedDraws.size(),
        "per-draw packed material uniforms preserve distinct model transforms");

    Renderer::FForwardFrameExecutionBindings NativeForwardBindings;
    const bool bForwardPrepared = bComposed &&
        PrepareProductionForwardSmoke(
            *Fixture.Request.Device, *Snapshot, ForwardPlan, DeferredPlan,
            NativeForwardBindings, &CompositionReason);
    const auto ForwardExecution = bForwardPrepared
        ? Renderer::FForwardFrameExecutor().Execute(
            ForwardPlan, NativeForwardBindings)
        : Renderer::FForwardFrameExecutionResult{};
    Record(Result,
        bForwardPrepared && ForwardExecution.Succeeded() &&
            NativeForwardBindings.OutputTexture &&
            NativeForwardBindings.AuxiliaryColorTextures.size() == 2 &&
            NativeForwardBindings.DepthTexture &&
            NativeForwardBindings.DepthTexture->GetFormat() ==
                RHI::ERHIFormat::D32_Float &&
            RHI::HasRHIFlag(
                NativeForwardBindings.DepthTexture->GetUsage(),
                RHI::ERHITextureUsage::DepthStencilAttachment) &&
            NativeForwardBindings.ReadbackBuffer &&
            NativeForwardBindings.RenderPass &&
            NativeForwardBindings.RenderPass->GetDesc().Attachments.size() == 4 &&
            NativeForwardBindings.RenderPass->GetDesc().Attachments[3].Role ==
                RHI::ERHIAttachmentRole::DepthStencil &&
            NativeForwardBindings.Framebuffer &&
            NativeForwardBindings.CommandBuffer &&
            !NativeForwardBindings.bTransitionToPresent &&
            NativeForwardBindings.ReadbackRegion.Width ==
                ForwardPlan.OutputTarget.Extent.Width &&
            NativeForwardBindings.ReadbackRegion.Height ==
                ForwardPlan.OutputTarget.Extent.Height &&
            ForwardExecution.RecordedDrawCount ==
                NativeForwardBindings.Draws.size(),
        "Forward smoke records the same aggregate draws and native color readback contract");

    FProductionContentDeferredExecutionResources DeferredResources;
    const auto MissingDeferredShaders = bComposed
        ? FProductionContentDeferredExecutionBuilder::Build(
            Fixture.Request.Device, *Snapshot, Composition, {}, {},
            *Fixture.Request.TargetEvidence, DeferredResources,
            &CompositionReason)
        : RHI::ERHIResult::Failed;
    Record(Result,
        MissingDeferredShaders == RHI::ERHIResult::InvalidState &&
            !DeferredResources.IsValid(),
        "Deferred production execution rejects an incomplete strict shader closure");

    Core::TArray<Core::TSharedPtr<const Asset::FShaderAsset>> RenderShaders;
    Core::TArray<Core::TSharedPtr<const Asset::FShaderPayloadAsset>>
        RenderPayloads;
    const bool bShaderClosure = BuildDeferredShaderClosure(
        RenderShaders, RenderPayloads);
    const auto DeferredBuild = bComposed && bShaderClosure
        ? FProductionContentDeferredExecutionBuilder::Build(
            Fixture.Request.Device, *Snapshot, Composition,
            RenderShaders, RenderPayloads,
            *Fixture.Request.TargetEvidence, DeferredResources,
            &CompositionReason)
        : RHI::ERHIResult::Failed;
    const auto DeferredExecution =
        DeferredBuild == RHI::ERHIResult::Success
        ? Renderer::FDeferredFrameExecutor().Execute(
            DeferredResources.Plan, DeferredResources.Graph,
            DeferredResources.Bindings)
        : Renderer::FDeferredFrameExecutionResult{};
    Record(Result,
        DeferredBuild == RHI::ERHIResult::Success &&
            DeferredResources.IsValid() && DeferredExecution.Succeeded() &&
            DeferredExecution.FinalState ==
                Renderer::EDeferredExecutionState::Recorded &&
            DeferredResources.Bindings.Readbacks.size() == 6 &&
            DeferredExecution.RecordedDrawCount >=
                DeferredResources.Plan.AcceptedDraws.size(),
        "strict shader closure records aggregate production Deferred attachments and readbacks");

    FProductionContentComposition InvalidComposition;
    CompositionConfig.FrameToken = 0;
    Record(Result,
        !FProductionContentCompositionBuilder::Build(
            Snapshot, CompositionConfig, InvalidComposition,
            &CompositionReason) &&
            InvalidComposition.FrameToken == 0,
        "composition rejects an unversioned frame without partial publication");
    Snapshot.reset();
    return Result;
}
