#include "ProductionContentDemoTests.h"

#include "FDemoConfiguration.h"
#include "FDemoValidationMonitor.h"
#include "FProductionContentComposition.h"
#include "FProductionContentDeferredExecution.h"
#include "FProductionSubmissionHarness.h"
#include "FLabProductionFrameContext.h"
#include "FLabProductionPreviewExecutor.h"
#include "FOutputTransformValidationCommand.h"
#include "FProductionAuthorityWindowExtent.h"
#include "FProductionPresentationPixels.h"
#include "RendererStaticModelRealizationTestSupport.h"
#include "Renderer/FDeferredFrameUniformResources.h"
#include "Renderer/FUIRenderSession.h"

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
        "production-content-lantern-v2", "--render-path", "deferred-full",
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
        const char* Directory, const char* Leaf,
        Core::TArray<Asset::FShaderInterfaceBinding> Bindings) -> bool
    {
        const std::string LogicalPath =
            std::string("Engine/Shaders/") + Directory + "/" + Leaf;
        Asset::FShaderAssetDesc Desc;
        Desc.Id = Id("ShaderProgram", LogicalPath.c_str());
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
                "ShaderPayload", LogicalPath.c_str(),
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
                LogicalPath.c_str(),
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
    return Append("Deferred", "DirectionalLight", GBuffer(false)) &&
        Append("Deferred", "PointLight", GBuffer(true)) &&
        Append("Deferred", "SpotLight", GBuffer(true)) &&
        Append("Deferred", "Composition", {
            Binding(2, 0,
                Asset::EShaderResourceKind::CombinedTextureSampler,
                Fragment),
            Binding(2, 2,
                Asset::EShaderResourceKind::CombinedTextureSampler,
                Fragment),
            Binding(2, 4,
                Asset::EShaderResourceKind::CombinedTextureSampler,
                Fragment)}) &&
        Append("PostProcess", "OutputTransform", {
            Binding(0, 0,
                Asset::EShaderResourceKind::CombinedTextureSampler,
                Fragment),
            Binding(0, 1,
                Asset::EShaderResourceKind::UniformBuffer,
                Fragment)});
}

class FPreviewSubmissionFence final : public RHI::IRHIFence
{
public:
    RHI::ERHIResult WaitResult = RHI::ERHIResult::NotReady;
    RHI::ERHIResult ResetResult = RHI::ERHIResult::Success;
    int WaitCalls = 0;
    int ResetCalls = 0;
    bool bOnlyZeroWaits = true;
    RHI::ERHIFenceState GetState() const noexcept override
    { return IsSignaled() ? RHI::ERHIFenceState::Signaled : RHI::ERHIFenceState::Unsignaled; }
    bool IsSignaled() const noexcept override
    { return WaitResult == RHI::ERHIResult::Success; }
    RHI::ERHIResult Wait(Core::uint64 Timeout = 0) override
    { ++WaitCalls; bOnlyZeroWaits = bOnlyZeroWaits && Timeout == 0; return WaitResult; }
    RHI::ERHIResult Reset() override
    {
        ++ResetCalls;
        if (ResetResult == RHI::ERHIResult::Success) WaitResult = RHI::ERHIResult::NotReady;
        return ResetResult;
    }
    RHI::ERHIResult Signal() override
    { WaitResult = RHI::ERHIResult::Success; return RHI::ERHIResult::Success; }
};

class FPreviewSubmissionQueue final : public RHI::IRHICommandQueue
{
public:
    int OrdinarySubmits = 0;
    int DeferredSubmits = 0;
    int IdleCalls = 0;
    RHI::ERHIResult DeferredResult = RHI::ERHIResult::Success;
    Core::TArray<Core::TSharedPtr<RHI::IRHIFence>> SubmittedFences;
    RHI::ERHIQueueType GetQueueType() const noexcept override
    { return RHI::ERHIQueueType::Graphics; }
    Core::uint32 GetSubmittedCommandBufferCount() const noexcept override
    { return static_cast<Core::uint32>(DeferredSubmits); }
    RHI::ERHIResult Submit(const Core::TSharedPtr<RHI::IRHICommandBuffer>&,
        const Core::TArray<Core::TSharedPtr<RHI::IRHISemaphore>>&,
        const Core::TArray<Core::TSharedPtr<RHI::IRHISemaphore>>&,
        const Core::TSharedPtr<RHI::IRHIFence>&) override
    { ++OrdinarySubmits; return RHI::ERHIResult::Failed; }
    RHI::ERHIResult SubmitDeferred(const Core::TSharedPtr<RHI::IRHICommandBuffer>&,
        const Core::TArray<Core::TSharedPtr<RHI::IRHISemaphore>>&,
        const Core::TArray<Core::TSharedPtr<RHI::IRHISemaphore>>&,
        const Core::TSharedPtr<RHI::IRHIFence>& Fence) override
    {
        ++DeferredSubmits;
        if (DeferredResult == RHI::ERHIResult::Success) SubmittedFences.push_back(Fence);
        return DeferredResult;
    }
    RHI::ERHIResult WaitIdle() override
    { ++IdleCalls; return RHI::ERHIResult::Failed; }
};

class FPreviewSubmissionDevice final : public Stoner::Tests::StaticModelRealization::FDevice
{
public:
    Core::TSharedPtr<FPreviewSubmissionQueue> TestQueue =
        Core::MakeShared<FPreviewSubmissionQueue>();
    RHI::TRHIObjectResult<RHI::IRHICommandQueue> CreateCommandQueue(RHI::ERHIQueueType) override
    { return {RHI::ERHIResult::Success, TestQueue}; }
    RHI::TRHIObjectResult<RHI::IRHIFence> CreateFence(bool) override
    { return {RHI::ERHIResult::Success, Core::MakeShared<FPreviewSubmissionFence>()}; }
};

void TestPreviewSubmissionHarness(FProductionContentDemoTestResult& Result)
{
    using namespace Stoner::Tests::StaticModelRealization;
    const auto Device = Core::MakeShared<FPreviewSubmissionDevice>();
    FProductionSubmissionHarness Harness;
    const auto Initialized = Harness.Initialize(Device);
    auto Commands = Core::MakeShared<FTrackedCommandBuffer>();
    (void)Commands->Begin();
    (void)Commands->End();
    const auto Fence = Core::MakeShared<FPreviewSubmissionFence>();
    const auto Submitted = Harness.SubmitDeferred(Commands, {}, {}, Fence);
    const auto Duplicate = Harness.SubmitDeferred(Commands, {}, {},
        Core::MakeShared<FPreviewSubmissionFence>());
    const auto Pending = Harness.PollDeferred(Fence);
    const auto EarlyRetire = Harness.RetireDeferred(Fence);
    const auto EarlyRelease = Harness.Release();
    const auto EarlyReinitialize = Harness.Initialize(Device);
    Record(Result, Initialized == RHI::ERHIResult::Success &&
            Submitted.bSubmissionAccepted && !Submitted.bCompletionObserved &&
            !Duplicate.bSubmissionAccepted && Device->TestQueue->DeferredSubmits == 1 &&
            !Pending.bCompletionObserved && !EarlyRetire.bRetired &&
            EarlyRelease == RHI::ERHIResult::NotReady &&
            EarlyReinitialize == RHI::ERHIResult::NotReady && Fence->ResetCalls == 0 &&
            Device->TestQueue->OrdinarySubmits == 0 && Device->TestQueue->IdleCalls == 0,
        "Production deferred harness retains pending ownership through poll, retire, release and reinitialization without synchronous fallback");
    std::weak_ptr<FTrackedCommandBuffer> WeakCommands = Commands;
    Commands.reset();
    const bool bRetainedBeforeCompletion = !WeakCommands.expired();
    Fence->WaitResult = RHI::ERHIResult::Failed;
    const auto FailedPoll = Harness.PollDeferred(Fence);
    const auto FailedRetire = Harness.RetireDeferred(Fence);
    Fence->WaitResult = RHI::ERHIResult::Success;
    const auto Completed = Harness.PollDeferred(Fence);
    const auto Retired = Harness.RetireDeferred(Fence);
    Record(Result, bRetainedBeforeCompletion && !FailedPoll.bCompletionObserved &&
            !FailedRetire.bRetired && Completed.bCompletionObserved &&
            Completed.Result == RHI::ERHIResult::Failed && Retired.bRetired &&
            Retired.Result == RHI::ERHIResult::Failed && WeakCommands.expired() &&
            Fence->ResetCalls == 1 && Fence->bOnlyZeroWaits &&
            Harness.Release() == RHI::ERHIResult::Success,
        "Deferred poll failure cannot prove completion; later completion releases owners once and preserves first failure");

    (void)Harness.Initialize(Device);
    Core::TArray<Core::TSharedPtr<FPreviewSubmissionFence>> Fences;
    Core::TArray<Core::TSharedPtr<FTrackedCommandBuffer>> CommandBuffers;
    for (int Index = 0; Index < 3; ++Index)
    {
        auto Command = Core::MakeShared<FTrackedCommandBuffer>();
        (void)Command->Begin();
        (void)Command->End();
        CommandBuffers.push_back(Command);
        Fences.push_back(Core::MakeShared<FPreviewSubmissionFence>());
    }
    const auto First = Harness.SubmitDeferred(CommandBuffers[0], {}, {}, Fences[0]);
    const auto Second = Harness.SubmitDeferred(CommandBuffers[1], {}, {}, Fences[1]);
    const auto Third = Harness.SubmitDeferred(CommandBuffers[2], {}, {}, Fences[2]);
    Record(Result, First.bSubmissionAccepted && Second.bSubmissionAccepted &&
            !Third.bSubmissionAccepted && Third.Result == RHI::ERHIResult::NotReady &&
            Device->TestQueue->DeferredSubmits == 3,
        "Production deferred harness bounds accepted work to two frame slots");
    bool bRetryRetainedCompletion = true;
    for (int Index = 0; Index < 2; ++Index)
    {
        Fences[Index]->WaitResult = RHI::ERHIResult::Success;
        (void)Harness.PollDeferred(Fences[Index]);
        Fences[Index]->ResetResult = RHI::ERHIResult::NotReady;
        const auto ResetPending = Harness.RetireDeferred(Fences[Index]);
        Fences[Index]->ResetResult = RHI::ERHIResult::Success;
        const auto ResetComplete = Harness.RetireDeferred(Fences[Index]);
        bRetryRetainedCompletion = bRetryRetainedCompletion &&
            ResetPending.bCompletionObserved && !ResetPending.bRetired &&
            ResetPending.Result == RHI::ERHIResult::NotReady &&
            ResetComplete.bRetired && ResetComplete.Result == RHI::ERHIResult::Success;
    }
    Record(Result, bRetryRetainedCompletion,
        "Deferred fence reset NotReady retains completion proof and permits successful retirement retry");
    Device->TestQueue->DeferredResult = RHI::ERHIResult::Unsupported;
    const auto Unsupported = Harness.SubmitDeferred(CommandBuffers[2], {}, {}, Fences[2]);
    Record(Result, Unsupported.Result == RHI::ERHIResult::Unsupported &&
            !Unsupported.bSubmissionAccepted && Harness.Release() == RHI::ERHIResult::Success &&
            Device->TestQueue->OrdinarySubmits == 0 && Device->TestQueue->IdleCalls == 0,
        "Unsupported deferred submission releases unaccepted owners without ordinary Submit fallback");
}

void TestSlotUniformIsolation(FProductionContentDemoTestResult& Result,
    const Core::TSharedPtr<RHI::IRHIDevice>& Device,
    const Core::TSharedPtr<const Renderer::FStaticModelRenderSnapshot>& Snapshot,
    const Renderer::FDeferredFramePlan& Plan)
{
    using namespace Stoner::Tests::StaticModelRealization;
    const auto TestDevice = std::dynamic_pointer_cast<FDevice>(Device);
    const auto Ledger = TestDevice->Ledger();
    const auto FirstOwnedRecord = Ledger->Created.size();
    const auto ReadBuffers = [](const auto& Buffers) {
        Core::TArray<Core::TArray<Core::uint8>> Bytes;
        for (const auto& Buffer : Buffers)
        {
            const auto Tracked = std::dynamic_pointer_cast<FTrackedBuffer>(Buffer);
            Bytes.push_back(Tracked ? Tracked->GetData() : Core::TArray<Core::uint8>{});
        }
        return Bytes;
    };
    Core::TArray<Core::TSharedPtr<RHI::IRHIBuffer>> SnapshotBuffers;
    for (const auto& Draw : Snapshot->GetDrawResources())
        for (const auto& Binding : Draw.BufferBindings)
            SnapshotBuffers.push_back(Binding.Buffer);
    const auto OriginalBytes = ReadBuffers(SnapshotBuffers);
    Renderer::FDeferredFrameUniformResources Slot0;
    Renderer::FDeferredFrameUniformResources Slot1;
    const auto Init0 = Slot0.Initialize(Device, Snapshot, Plan);
    const auto Init1 = Slot1.Initialize(Device, Snapshot, Plan);
    bool bPrivateBindings = Init0 == RHI::ERHIResult::Success &&
        Init1 == RHI::ERHIResult::Success && Slot0.IsValid() && Slot1.IsValid() &&
        Slot0.GetSurfaceDraws().size() == Plan.AcceptedDraws.size() &&
        Slot1.GetSurfaceDraws().size() == Plan.AcceptedDraws.size();
    bool bImmutableImagesPreserved = bPrivateBindings;
    Core::uint32 ComparedImages = 0;
    if (bPrivateBindings)
    {
        for (Core::usize Index = 0; Index < Slot0.GetSurfaceDraws().size(); ++Index)
        {
            const auto& Draw0 = Slot0.GetSurfaceDraws()[Index];
            const auto& Draw1 = Slot1.GetSurfaceDraws()[Index];
            bPrivateBindings = bPrivateBindings && Draw0.VertexBuffer == Draw1.VertexBuffer &&
                Draw0.IndexBuffer == Draw1.IndexBuffer && Draw0.Pipeline == Draw1.Pipeline;
            for (const auto& BaseSet : Draw0.DescriptorSets)
            {
                if (BaseSet->GetSetIndex() > 1) continue;
                const auto Set0 = std::dynamic_pointer_cast<FTrackedDescriptorSet>(BaseSet);
                const auto Found1 = std::find_if(Draw1.DescriptorSets.begin(), Draw1.DescriptorSets.end(),
                    [&BaseSet](const auto& Set) { return Set->GetSetIndex() == BaseSet->GetSetIndex(); });
                const auto Set1 = Found1 == Draw1.DescriptorSets.end() ? nullptr :
                    std::dynamic_pointer_cast<FTrackedDescriptorSet>(*Found1);
                const auto Buffer0 = Set0 ? Set0->GetBoundBuffer(0) : nullptr;
                const auto Buffer1 = Set1 ? Set1->GetBoundBuffer(0) : nullptr;
                bPrivateBindings = bPrivateBindings && Set0 && Set1 && Set0 != Set1 &&
                    Buffer0 && Buffer1 && Buffer0 != Buffer1 &&
                    std::find(Slot0.GetOwnedBuffers().begin(), Slot0.GetOwnedBuffers().end(), Buffer0) != Slot0.GetOwnedBuffers().end() &&
                    std::find(Slot1.GetOwnedBuffers().begin(), Slot1.GetOwnedBuffers().end(), Buffer1) != Slot1.GetOwnedBuffers().end() &&
                    std::find(SnapshotBuffers.begin(), SnapshotBuffers.end(), Buffer0) == SnapshotBuffers.end();
                const auto& OriginalSets = Snapshot->GetDrawResources()[
                    Plan.AcceptedDraws[Index].Candidate.Identity.Slot - 1].DescriptorSets;
                const auto Original = std::find_if(OriginalSets.begin(), OriginalSets.end(),
                    [&BaseSet](const auto& Set) { return Set->GetSetIndex() == BaseSet->GetSetIndex(); });
                const auto SourceSet = Original == OriginalSets.end() ? nullptr :
                    std::dynamic_pointer_cast<FTrackedDescriptorSet>(*Original);
                if (!SourceSet || !Set0 || !Set1)
                {
                    bImmutableImagesPreserved = false;
                    continue;
                }
                for (const auto& Binding : SourceSet->GetPipelineLayout()->GetDesc().Bindings)
                {
                    if (Binding.SetIndex != SourceSet->GetSetIndex()) continue;
                    for (Core::uint32 Element = 0; Element < Binding.ArrayCount; ++Element)
                    {
                        const auto Image = SourceSet->GetBoundTexture(Binding.BindingSlot, Element);
                        const auto Sampler = SourceSet->GetBoundSampler(Binding.BindingSlot, Element);
                        if (Image || Sampler)
                        {
                            ++ComparedImages;
                            bImmutableImagesPreserved = bImmutableImagesPreserved &&
                                Set0->GetBoundTexture(Binding.BindingSlot, Element) == Image &&
                                Set1->GetBoundTexture(Binding.BindingSlot, Element) == Image &&
                                Set0->GetBoundSampler(Binding.BindingSlot, Element) == Sampler &&
                                Set1->GetBoundSampler(Binding.BindingSlot, Element) == Sampler;
                        }
                    }
                }
            }
        }
    }
    Record(Result, bPrivateBindings && ReadBuffers(SnapshotBuffers) == OriginalBytes,
        "Renderer slot uniforms and descriptors are private while immutable scene geometry and pipelines remain shared");
    Record(Result, bImmutableImagesPreserved && ComparedImages > 0,
        "Cloned mutable descriptor sets preserve the exact immutable material texture and sampler bindings");
    const auto Slot0Before = ReadBuffers(Slot0.GetOwnedBuffers());
    const auto Slot1Before = ReadBuffers(Slot1.GetOwnedBuffers());
    const auto Slot1BuffersBefore = Slot1.GetOwnedBuffers();
    const auto Slot1DrawsBefore = Slot1.GetSurfaceDraws();
    const auto Slot1FrameBefore = Slot1.GetFrameUniformBuffer();
    const auto CreatesBeforeUpdate = Ledger->Created.size();
    auto MovedPlan = Plan;
    MovedPlan.View.CameraPosition.X += 1.0f;
    const auto Updated = Slot1.Update(Device, MovedPlan);
    bool bDescriptorsStable = Slot1DrawsBefore.size() == Slot1.GetSurfaceDraws().size();
    if (bDescriptorsStable)
        for (Core::usize Index = 0; Index < Slot1DrawsBefore.size(); ++Index)
            bDescriptorsStable = bDescriptorsStable && Slot1DrawsBefore[Index].DescriptorSets ==
                Slot1.GetSurfaceDraws()[Index].DescriptorSets;
    Record(Result, Updated == RHI::ERHIResult::Success &&
            ReadBuffers(Slot1.GetOwnedBuffers()) != Slot1Before &&
            ReadBuffers(Slot0.GetOwnedBuffers()) == Slot0Before &&
            ReadBuffers(SnapshotBuffers) == OriginalBytes,
        "Updating one slot changes its camera bytes without modifying another slot or the immutable snapshot");
    Record(Result, Updated == RHI::ERHIResult::Success && bDescriptorsStable &&
            Slot1.GetOwnedBuffers() == Slot1BuffersBefore &&
            Slot1.GetFrameUniformBuffer() == Slot1FrameBefore &&
            Ledger->Created.size() == CreatesBeforeUpdate,
        "Slot update preserves buffer and descriptor identities and creates no new GPU resources");
    auto InvalidPlan = Plan;
    if (!InvalidPlan.AcceptedDraws.empty()) InvalidPlan.AcceptedDraws.front().Candidate.Identity.Slot = 0;
    const auto BeforeInvalid = ReadBuffers(Slot1.GetOwnedBuffers());
    const auto Rejected = Slot1.Update(Device, InvalidPlan);
    Record(Result, Rejected != RHI::ERHIResult::Success &&
            ReadBuffers(Slot1.GetOwnedBuffers()) == BeforeInvalid &&
            ReadBuffers(Slot0.GetOwnedBuffers()) == Slot0Before,
        "Invalid slot draw identity is rejected before any uniform upload");
    Slot0.Release();
    Slot1.Release();
    bool bOwnedReleasedOnce = true;
    for (Core::usize Index = FirstOwnedRecord; Index < Ledger->Created.size(); ++Index)
        bOwnedReleasedOnce = bOwnedReleasedOnce &&
            Ledger->ReleaseCounts[Ledger->Created[Index].ToStdString()] == 1;
    Record(Result, !Slot0.IsValid() && !Slot1.IsValid() &&
            bOwnedReleasedOnce &&
            std::all_of(SnapshotBuffers.begin(), SnapshotBuffers.end(), [](const auto& Buffer) {
                return Buffer && Buffer->GetLifecycleState() == RHI::ERHIResourceLifecycleState::Valid;
            }) && Snapshot->GetMeshes().front().VertexBuffer->GetLifecycleState() ==
                RHI::ERHIResourceLifecycleState::Valid,
        "Slot uniform cleanup leaves shared scene buffers and geometry valid");

    const auto BeforeFailedBuild = Ledger->Created.size();
    Ledger->Failure = EFailurePoint::BufferUpload;
    Ledger->FailureOccurrence = Ledger->Calls[EFailurePoint::BufferUpload] + 1;
    Renderer::FDeferredFrameUniformResources FailedSlot;
    const auto FailedBuild = FailedSlot.Initialize(Device, Snapshot, Plan);
    FailedSlot.Release();
    Ledger->Failure = EFailurePoint::None;
    bool bFailedBuildReleased = Ledger->Created.size() > BeforeFailedBuild;
    for (Core::usize Index = BeforeFailedBuild; Index < Ledger->Created.size(); ++Index)
        bFailedBuildReleased = bFailedBuildReleased &&
            Ledger->ReleaseCounts[Ledger->Created[Index].ToStdString()] == 1;
    Record(Result, FailedBuild != RHI::ERHIResult::Success && !FailedSlot.IsValid() &&
            bFailedBuildReleased && ReadBuffers(SnapshotBuffers) == OriginalBytes,
        "Slot initialization upload failure releases every partial private allocation exactly once");
}

void TestLabFrameContext(FProductionContentDemoTestResult& Result,
    const FLabProductionFrameContextConfig& Config, RHI::ERHIFormat OutputFormat)
{
    const auto Device = std::dynamic_pointer_cast<FPreviewSubmissionDevice>(Config.Device);
    const auto Width = Config.Composition.DeferredInputs.View.Extent.Width;
    const auto Height = Config.Composition.DeferredInputs.View.Extent.Height;
    const auto MakeTarget = [&](Core::uint64 Token, Core::uint32 Slot, Core::uint32 Image) {
        RHI::FRHITextureDesc Desc;
        Desc.Width = Width;
        Desc.Height = Height;
        Desc.Format = OutputFormat;
        Desc.Usage = RHI::ERHITextureUsage::ColorAttachment | RHI::ERHITextureUsage::Present;
        RHI::FRHIBorrowedAcquiredTarget Target;
        Target.Texture = Device->CreateTexture(Desc).Object;
        Target.FrameSlotIndex = Slot;
        Target.Frame.FrameToken = Token;
        Target.Frame.ModeGeneration = 7;
        Target.Frame.SwapchainImageGeneration = 9;
        Target.Frame.ImageIndex = Image;
        Target.Frame.Width = Width;
        Target.Frame.Height = Height;
        Target.Frame.Format = OutputFormat;
        Target.Frame.ColorSpace = RHI::ERHIPresentationColorSpace::SrgbNonlinear;
        return Target;
    };
    {
        using namespace Renderer;
        using namespace RHI;
        FUIRenderSession UI(Device, 73);
        UI.BeginEligibleFrame(1, true);
        FUITextureRequest TextureRequest;
        TextureRequest.RequestId = 1; TextureRequest.LogicalSlot = 1;
        TextureRequest.Width = TextureRequest.Height = 1;
        TextureRequest.Format = ERHIFormat::R8G8B8A8_UNorm;
        TextureRequest.ColorDomain = EUITextureColorDomain::AlphaCoverage;
        TextureRequest.PixelBytes = {255,255,255,255};
        const auto Texture = UI.PrepareTexture(TextureRequest);
        const auto Lease = UI.AcquireTexture(Texture.TextureId);
        FUICompositionSettings UISettings;
        UISettings.OutputProfileId = "Sdr.sRGB.v1";
        UISettings.BlendDomain = ERenderGraphColorDomain::DisplayLinearRec709D65;
        UISettings.UIReferenceWhiteNits = UISettings.NativePackingWhiteNits = 100;
        UISettings.DisplayGeneration = 1;
        FRHIShaderModuleDesc Vertex, Fragment;
        Vertex.Stage = ERHIShaderStage::Vertex; Fragment.Stage = ERHIShaderStage::Fragment;
        const FRHIShaderModuleDesc Modules[] = {Vertex,Fragment}; // tracked RHI; no native shader claim
        Core::uint64 PacketId = 1;
        Core::TSharedPtr<FProductionContentPreviewGraph> LastPreparedGraph;
        const FLabProductionFrameContext::FPrepareUI PrepareUI = [&](const auto& FrameResources,
            Core::uint64 Budget, Core::TSharedPtr<FUIRenderFrame>& Out,
            Core::TSharedPtr<FProductionContentPreviewGraph>& OutGraph) {
            const auto& Scene = FrameResources.Bindings.OutputTransformStages.back().Input;
            if (FrameResources.OutputTransformPlan.FrameToken == 0 ||
                FrameResources.Bindings.FinalOutput == nullptr ||
                FrameResources.Bindings.OutputTransformStages.size() != 3) return ERHIResult::InvalidState;
            if (Budget < static_cast<Core::uint64>(Width) * Height * 8) return ERHIResult::Unavailable;
            FUIDrawSnapshot Draw(73,PacketId++,1,1);
            const FUIVertex Vertices[] = {{{0,0},{0,0},0xffffffff},{{16,0},{1,0},0xffffffff},{{0,16},{0,1},0xffffffff}};
            const Core::uint32 Indices[] = {0,1,2};
            FUIDrawCommand Command; Command.IndexCount = 3; Command.TextureId = Texture.TextureId;
            Command.ClipRect = {0,0,16,16};
            if (!Draw.SetDisplay({0,0},{static_cast<float>(Width),static_cast<float>(Height)},{1,1}) ||
                !Draw.SetVertices(Vertices) || !Draw.SetIndices(Indices) || !Draw.SetCommands({&Command,1}) ||
                !Draw.SetTextureLeases({&Lease,1}) || !Draw.Publish()) return ERHIResult::InvalidState;
            auto GraphComposition=Config.Composition;
            GraphComposition.FrameToken=FrameResources.OutputTransformPlan.FrameToken;
            Core::TSharedPtr<FProductionContentPreviewGraph> Graph;
            if (!FProductionContentDeferredExecutionBuilder::BuildPreviewGraph(GraphComposition,
                FrameResources.OutputSettings,&UISettings,FrameResources.Bindings.FormalOutput->GetFormat(),Graph))
                return ERHIResult::InvalidState;
            const auto Prepared=UI.PrepareFrame(Draw,UISettings,1,0,Scene,Modules,Modules,Out);
            if (Prepared==ERHIResult::Success) { OutGraph=Graph; LastPreparedGraph=Graph; }
            return Prepared;
        };
        auto UIContextOwner = Core::MakeShared<FLabProductionFrameContext>();
        auto& UIContext = *UIContextOwner;
        auto Composition = Config.Composition; Composition.FrameToken = 81;
        const auto Target = MakeTarget(81,0,0);
        (void)UIContext.Initialize(Config); (void)UIContext.ReserveFrame(81,0);
        (void)UIContext.BeginFrame(81,0,Target);
        const auto BaseBytes = UIContext.Snapshot().ActiveAttachmentBytes;
        const auto Output = FOutputTransformSettingsValidator().Validate(Config.OutputSettings).Settings;
        FRHIResolvedPresentationState Resolved;
        Resolved.ModeGeneration = Target.Frame.ModeGeneration;
        Resolved.SwapchainImageGeneration = Target.Frame.SwapchainImageGeneration;
        Resolved.Width = Width; Resolved.Height = Height; Resolved.Format = OutputFormat;
        Resolved.ColorSpace = Target.Frame.ColorSpace; Resolved.NativeEncoding = Output.NativeEncoding;
        Resolved.ReferenceWhiteNits = Output.ReferenceWhiteNits; Resolved.TargetPeakNits = Output.TargetPeakNits;
        FOutputTransformPreviewTicket UITicket;
        FOutputTransformExecutor UIExecutor;
        const auto TicketRecorded = RecordLabProductionPreview(UIContextOwner,Composition,0,Resolved,
            [](Core::uint64,Core::uint32,const Core::TSharedPtr<IRHIFence>&,bool& Ack) {
                Ack = true; return ERHIResult::Success;
            },UITicket,PrepareUI);
        const auto Recorded = TicketRecorded.NativeResult;
        Record(Result, TicketRecorded.Result == EOutputTransformResult::Success && UITicket.IsValid(),
            "Lab preview ticket validates the terminal UI graph against its already-recorded deferred command");
        const auto* Resources = UIContext.GetResources(81,0);
        Record(Result,Resources && Resources->PreviewOutputGraph==LastPreparedGraph && LastPreparedGraph &&
            LastPreparedGraph->Plan.PlanFingerprint==Resources->OutputTransformPlan.PlanFingerprint &&
            LastPreparedGraph->Graph.GetState()==ERenderGraphState::Executed,
            "preview submission retains the exact output graph built during UI preparation");
        auto InvalidGraphSettings=Config.OutputSettings;
        InvalidGraphSettings.DiagnosticBypass.StageName="missing-stage";
        InvalidGraphSettings.DiagnosticBypass.Mode=EOutputTransformDebugBypassMode::BoundedVisualization;
        auto PreservedGraph=LastPreparedGraph;
        Record(Result,!FProductionContentDeferredExecutionBuilder::BuildPreviewGraph(Composition,
            InvalidGraphSettings,&UISettings,OutputFormat,PreservedGraph) && PreservedGraph==LastPreparedGraph,
            "failed output graph preparation preserves the prior compiled graph owner");
        const auto Frame = Resources && Resources->Bindings.OutputTransformStages.size() == 4
            ? Resources->Bindings.OutputTransformStages[2].UIFrame : nullptr;
        const auto FenceIndex = Device->TestQueue->SubmittedFences.size();
        const auto Submitted = UIExecutor.SubmitPreview(UITicket).NativeResult;
        Record(Result, Recorded == ERHIResult::Success && Submitted == ERHIResult::Success &&
            Frame && Resources->IsValid() && Resources->OutputTransformPlan.TerminalUI &&
            Resources->Bindings.OutputTransformStages.back().Input == Frame->GetOutput() &&
            UIContext.Snapshot().ActiveAttachmentBytes == BaseBytes + static_cast<Core::uint64>(Width)*Height*8,
            "Lab slot records leased terminal UI before the sole output transfer and counts its target budget");
        // The same atlas cannot be sampled by a second frame until its upload completes.
        auto BusyComposition = Composition; BusyComposition.FrameToken = 82;
        (void)UIContext.ReserveFrame(82,1); (void)UIContext.BeginFrame(82,1,MakeTarget(82,1,1));
        const auto BusyRecorded = UIContext.RecordFrame(82,1,BusyComposition,nullptr,PrepareUI);
        const auto* BusyResources = UIContext.GetResources(82,1);
        Record(Result, BusyRecorded == ERHIResult::Success &&
            UIContext.Snapshot().LastUIPreparationResult == ERHIResult::NotReady &&
            BusyResources && BusyResources->Bindings.OutputTransformStages.size() == 3,
            "Pending UI upload falls back before recording and preserves a valid scene-only frame");
        (void)UIContext.CancelFrame(82,1); (void)UIContext.RetireCancelled(82,1);
        if (Frame && Device->TestQueue->SubmittedFences.size() > FenceIndex)
        {
            Record(Result, Frame->ReleaseCompleted() == ERHIResult::NotReady,
                "Lab UI owner cannot release resources before its actual render fence");
            const auto Fence = std::dynamic_pointer_cast<FPreviewSubmissionFence>(
                Device->TestQueue->SubmittedFences[FenceIndex]);
            Fence->WaitResult = ERHIResult::Success;
            bool Complete = false;
            const auto TicketPoll = UIExecutor.PollPreview(UITicket);
            Complete = TicketPoll.bRenderCompleted;
            const auto Polled = TicketPoll.NativeResult;
            FRHIPresentationLease Presentation; Presentation.Frame = Target.Frame;
            auto PresentFence = Core::MakeShared<FPreviewSubmissionFence>();
            Presentation.PresentationCompletionFence = PresentFence;
            const auto Queued = UIContext.QueuePresentation(81,0,Presentation);
            const auto Retired = UIExecutor.RetirePreview(UITicket).NativeResult;
            Record(Result, Polled == ERHIResult::Success && Complete && !Frame->GetOutput() &&
                Queued == ERHIResult::Success && Retired == ERHIResult::Success &&
                UIContext.Snapshot().RetainedPresentationCount == 1 && !PresentFence->IsSignaled(),
                "UI render owners retire while the independent native presentation lease remains pending");
            PresentFence->WaitResult = ERHIResult::Success;
            bool Released = false; (void)UIContext.PollPresentation(81,0,Released);
            Composition.FrameToken = 83;
            (void)UIContext.ReserveFrame(83,0); (void)UIContext.BeginFrame(83,0,MakeTarget(83,0,2));
            const auto CancelRecorded = UIContext.RecordFrame(83,0,Composition,nullptr,PrepareUI);
            Resources = UIContext.GetResources(83,0);
            const auto CancelFrame = Resources && Resources->Bindings.OutputTransformStages.size() == 4
                ? Resources->Bindings.OutputTransformStages[2].UIFrame : nullptr;
            (void)UIContext.CancelFrame(83,0);
            const auto CancelRetired = UIContext.RetireCancelled(83,0);
            Record(Result, CancelRecorded == ERHIResult::Success && CancelFrame &&
                CancelRetired == ERHIResult::Success && !CancelFrame->GetOutput(),
                "Unsubmitted UI recording is reset before cancellation releases its texture reservations");
            FUIRenderSession FailingUI(Device,73);
            FailingUI.BeginEligibleFrame(1,true);
            const auto NewTexture = FailingUI.PrepareTexture(TextureRequest);
            const auto NewLease = FailingUI.AcquireTexture(NewTexture.TextureId);
            Composition.FrameToken = 84;
            (void)UIContext.ReserveFrame(84,0); (void)UIContext.BeginFrame(84,0,MakeTarget(84,0,0));
            Resources = UIContext.GetResources(84,0);
            const auto FailureCommand = Resources ? std::dynamic_pointer_cast<
                Stoner::Tests::StaticModelRealization::FTrackedCommandBuffer>(Resources->Bindings.CommandBuffer) : nullptr;
            if (FailureCommand) FailureCommand->BufferTextureCopyResult = ERHIResult::Timeout;
            Core::uint64 FailurePacketId = 1;
            const FLabProductionFrameContext::FPrepareUI PrepareFailure =
                [&](const auto& FrameResources,Core::uint64,Core::TSharedPtr<FUIRenderFrame>& Out,
                    Core::TSharedPtr<FProductionContentPreviewGraph>&) {
                    const auto& Stages = FrameResources.Bindings.OutputTransformStages;
                    const auto& Scene = Stages.size() == 4 ? Stages[2].Input : Stages.back().Input;
                    FUIDrawSnapshot Draw(73,FailurePacketId++,1,1);
                    const FUIVertex V[] = {{{0,0},{0,0},0xffffffff},{{16,0},{1,0},0xffffffff},{{0,16},{0,1},0xffffffff}};
                    const Core::uint32 I[] = {0,1,2};
                    FUIDrawCommand C; C.IndexCount = 3; C.TextureId = NewTexture.TextureId; C.ClipRect = {0,0,16,16};
                    (void)Draw.SetDisplay({0,0},{static_cast<float>(Width),static_cast<float>(Height)},{1,1});
                    (void)Draw.SetVertices(V); (void)Draw.SetIndices(I); (void)Draw.SetCommands({&C,1});
                    (void)Draw.SetTextureLeases({&NewLease,1}); (void)Draw.Publish();
                    return FailingUI.PrepareFrame(Draw,UISettings,1,0,Scene,Modules,Modules,Out);
                };
            const auto FailedRecord = UIContext.RecordFrame(84,0,Composition,nullptr,PrepareFailure);
            Core::TSharedPtr<FUIRenderFrame> Retry;
            Core::TSharedPtr<FProductionContentPreviewGraph> RetryGraph;
            const auto BeforeDiscard = PrepareFailure(*Resources,0,Retry,RetryGraph);
            (void)UIContext.CancelFrame(84,0);
            const auto FailedRetire = UIContext.RetireCancelled(84,0);
            const auto AfterDiscard = PrepareFailure(*Resources,0,Retry,RetryGraph);
            Record(Result, BeforeDiscard == ERHIResult::NotReady && AfterDiscard == ERHIResult::Success,
                "Partial upload failure retains reservations until the command is discarded");
            if (Retry) (void)Retry->CancelAfterCommandDiscard();
            Record(Result, FailedRecord == ERHIResult::Timeout && UIContext.Snapshot().bFailed &&
                FailedRetire == ERHIResult::Success && Device->TestQueue->SubmittedFences.size() == FenceIndex + 1,
                "Partial UI recording preserves Timeout and discards the command without scene fallback or submission");
        }
        (void)UIContext.Shutdown();
        Device->TestQueue->SubmittedFences.clear();
    }
    // Exercise the upper Renderer preview ticket with real Demo frame
    // resources and independently controlled render/presentation fences.
    {
        auto TicketContext = Core::MakeShared<FLabProductionFrameContext>();
        (void)TicketContext->Initialize(Config);
        const auto Target = MakeTarget(99, 0, 0);
        (void)TicketContext->ReserveFrame(99, 0);
        (void)TicketContext->BeginFrame(99, 0, Target);
        auto Frame = Config.Composition;
        Frame.FrameToken = 99;
        Renderer::FResolvedOutputTransformSettings Output;
        Core::FString Reason;
        Output = Renderer::FOutputTransformSettingsValidator().Validate(Config.OutputSettings).Settings;
        RHI::FRHIResolvedPresentationState Resolved;
        Resolved.ModeGeneration = Target.Frame.ModeGeneration;
        Resolved.SwapchainImageGeneration = Target.Frame.SwapchainImageGeneration;
        Resolved.Width = Width; Resolved.Height = Height;
        Resolved.Format = OutputFormat;
        Resolved.ColorSpace = Target.Frame.ColorSpace;
        Resolved.NativeEncoding = Output.NativeEncoding;
        Resolved.ReferenceWhiteNits = Output.ReferenceWhiteNits;
        Resolved.TargetPeakNits = Output.TargetPeakNits;
        Renderer::FOutputTransformPreviewTicket Ticket;
        Core::uint32 Cancels = 0;
        FLabPreviewCancelCallback Cancel = [&Cancels](Core::uint64, Core::uint32,
            const Core::TSharedPtr<RHI::IRHIFence>&, bool& Ack) {
            ++Cancels; Ack = true; return RHI::ERHIResult::Success;
        };
        const auto FenceIndex = Device->TestQueue->SubmittedFences.size();
        const auto Recorded = RecordLabProductionPreview(TicketContext, Frame, 0, Resolved, Cancel, Ticket);
        Renderer::FOutputTransformExecutor Executor;
        if (Recorded.Result != Renderer::EOutputTransformResult::Success)
            std::cerr << "Preview record failed: result=" << static_cast<int>(Recorded.Result)
                << " native=" << static_cast<int>(Recorded.NativeResult)
                << " state=" << static_cast<int>(TicketContext->GetFrameState(99, 0))
                << " reason=" << TicketContext->Snapshot().FailureReason.CStr()
                << " diagnostics=" << Recorded.Diagnostics.Dump().CStr() << std::endl;
        const auto Submitted = Executor.SubmitPreview(Ticket);
        const auto Pending = Executor.PollPreview(Ticket);
        const auto EarlyRetire = Executor.RetirePreview(Ticket);
        Record(Result, Recorded.Result == Renderer::EOutputTransformResult::Success &&
            Ticket.IsValid() && Submitted.bQueued && !Submitted.bRenderCompleted &&
            !Submitted.Execution.Succeeded() && !Pending.bRenderCompleted && !EarlyRetire.bRetired &&
            Cancels == 0 && Device->TestQueue->OrdinarySubmits == 0 && Device->TestQueue->IdleCalls == 0,
            "Demo preview ticket queues the recorded scene without readback, synchronous wait or early retirement");
        if (Device->TestQueue->SubmittedFences.size() > FenceIndex)
        {
            const auto Fence = std::dynamic_pointer_cast<FPreviewSubmissionFence>(
                Device->TestQueue->SubmittedFences[FenceIndex]);
            Fence->WaitResult = RHI::ERHIResult::Success;
            const auto Complete = Executor.PollPreview(Ticket);
            RHI::FRHIPresentationLease PresentLease;
            PresentLease.Frame = Target.Frame;
            auto PresentFence = Core::MakeShared<FPreviewSubmissionFence>();
            PresentLease.PresentationCompletionFence = PresentFence;
            (void)TicketContext->QueuePresentation(99, 0, PresentLease);
            const auto Retired = Executor.RetirePreview(Ticket);
            Record(Result, Complete.bRenderCompleted && Retired.bRetired && Cancels == 0 &&
                TicketContext->Snapshot().BusySlotCount == 0 &&
                TicketContext->Snapshot().RetainedPresentationCount == 1 && !PresentFence->IsSignaled(),
                "Renderer ticket retires the render slot while the independent presentation lease remains pending");
            PresentFence->WaitResult = RHI::ERHIResult::Success;
            bool Released = false;
            (void)TicketContext->PollPresentation(99, 0, Released);
            const auto CancelTarget = MakeTarget(100, 0, 1);
            (void)TicketContext->ReserveFrame(100, 0);
            (void)TicketContext->BeginFrame(100, 0, CancelTarget);
            Frame.FrameToken = 100;
            Renderer::FOutputTransformPreviewTicket CancelTicket;
            const auto CancelRecorded = RecordLabProductionPreview(
                TicketContext, Frame, 0, Resolved, Cancel, CancelTicket);
            const auto* CancelResources = TicketContext->GetResources(100, 0);
            const auto Commands = CancelResources ? std::dynamic_pointer_cast<
                Stoner::Tests::StaticModelRealization::FTrackedCommandBuffer>(
                    CancelResources->Bindings.CommandBuffer) : nullptr;
            if (Commands)
            {
                Commands->ResetResult = RHI::ERHIResult::NotReady;
                const auto CancelPending = Executor.RetirePreview(CancelTicket);
                Commands->ResetResult = RHI::ERHIResult::Success;
                const auto CancelRetired = Executor.RetirePreview(CancelTicket);
                Record(Result, CancelRecorded.Result == Renderer::EOutputTransformResult::Success &&
                    !CancelPending.bRetired && CancelRetired.bRetired && Cancels == 1 &&
                    TicketContext->Snapshot().BusySlotCount == 0 &&
                    Device->TestQueue->SubmittedFences.size() == FenceIndex + 1,
                    "Unsubmitted preview cancellation retains acknowledgement across a delayed command reset without reacquiring or resubmitting");
            }
            (void)TicketContext->Shutdown();
        }
        Device->TestQueue->SubmittedFences.clear();
    }
    {
        FLabProductionFrameContext ModeContext;
        auto ModeSettings = Config.OutputSettings;
        ModeSettings.DynamicRange = Renderer::EOutputDynamicRange::HDR;
        ModeSettings.OutputDeviceProfileId = "Hdr.Linear.1000.v1";
        ModeSettings.SDRToneMapVersion.Clear();
        ModeSettings.PreferredNativeEncoding = RHI::ERHIPresentationNativeEncoding::ScRgb80;
        ModeSettings.NativeReferenceWhiteNits = 0;
        ModeSettings.HDRViewingVersion = Renderer::GInitialHDRViewingVersion;
        const auto OldTarget = MakeTarget(90,0,0);
        auto OldFrame = Config.Composition; OldFrame.FrameToken = 90;
        bool Ready = ModeContext.Initialize(Config) == RHI::ERHIResult::Success &&
            ModeContext.ReserveFrame(90,0) == RHI::ERHIResult::Success &&
            ModeContext.BeginFrame(90,0,OldTarget) == RHI::ERHIResult::Success &&
            ModeContext.RecordFrame(90,0,OldFrame) == RHI::ERHIResult::Success &&
            ModeContext.SubmitFrame(90,0) == RHI::ERHIResult::Success;
        const auto BeforeMode = ModeContext.Snapshot();
        Record(Result,Ready && ModeContext.ReconfigureOutputSettings(ModeSettings) == RHI::ERHIResult::NotReady &&
            ModeContext.Snapshot().ActiveAttachmentBytes == BeforeMode.ActiveAttachmentBytes,
            "output format recreation retains old attachments while their render fence is pending");
        if (Ready && !Device->TestQueue->SubmittedFences.empty())
        {
            const auto Render = std::dynamic_pointer_cast<FPreviewSubmissionFence>(Device->TestQueue->SubmittedFences.back());
            Render->WaitResult = RHI::ERHIResult::Success;
            bool Completed = false;
            Ready = ModeContext.PollRender(90,0,Completed) == RHI::ERHIResult::Success && Completed;
            const auto PresentationFence = Core::MakeShared<FPreviewSubmissionFence>();
            RHI::FRHIPresentationLease Presentation; Presentation.Frame = OldTarget.Frame;
            Presentation.PresentationCompletionFence = PresentationFence;
            Ready = Ready && ModeContext.QueuePresentation(90,0,Presentation) == RHI::ERHIResult::Success &&
                ModeContext.RetireRenderResources(90,0) == RHI::ERHIResult::Success;
            const auto Bytes = ModeContext.Snapshot().ActiveAttachmentBytes;
            auto InvalidMode = ModeSettings; InvalidMode.ManualExposureStops = 17;
            Record(Result,Ready && ModeContext.ReconfigureOutputSettings(InvalidMode) != RHI::ERHIResult::Success &&
                ModeContext.Snapshot().ActiveAttachmentBytes == Bytes,
                "invalid output recreation preserves the drained reusable render bundles");
            const auto Reconfigured = ModeContext.ReconfigureOutputSettings(ModeSettings);
            Record(Result,Ready && Reconfigured == RHI::ERHIResult::Success &&
                ModeContext.Snapshot().ActiveAttachmentBytes == 0 && ModeContext.Snapshot().RetainedPresentationCount == 1 &&
                !PresentationFence->IsSignaled() && OldTarget.Texture->GetLifecycleState() == RHI::ERHIResourceLifecycleState::Valid,
                "same-extent format change releases render-only bundles without retiring old presentation ownership");
            auto NewTarget = MakeTarget(91,0,1);
            const auto Resolved = Renderer::FOutputTransformSettingsValidator().Validate(ModeSettings).Settings;
            auto Desc = NewTarget.Texture->GetDesc(); Desc.Format = Resolved.OutputFormat;
            NewTarget.Texture = Device->CreateTexture(Desc).Object;
            NewTarget.Frame.Format = Resolved.OutputFormat; NewTarget.Frame.ColorSpace = Resolved.ColorSpace;
            ++NewTarget.Frame.ModeGeneration; ++NewTarget.Frame.SwapchainImageGeneration;
            auto NewFrame = Config.Composition; NewFrame.FrameToken = 91;
            const bool NewRecorded = ModeContext.ReserveFrame(91,0) == RHI::ERHIResult::Success &&
                ModeContext.BeginFrame(91,0,NewTarget) == RHI::ERHIResult::Success &&
                ModeContext.RecordFrame(91,0,NewFrame) == RHI::ERHIResult::Success;
            const auto* Resources = ModeContext.GetResources(91,0);
            Record(Result,NewRecorded && Resources && Resources->OutputTransformPlan.OutputDesc.Format == Resolved.OutputFormat &&
                Resources->OutputTransformPlan.ResolvedSettings.DynamicRange == Renderer::EOutputDynamicRange::HDR &&
                ModeContext.Snapshot().ActiveAttachmentBytes <= FLabProductionFrameLimits::MaxAttachmentBytes,
                "new borrowed HDR format rebuilds a bounded slot with the new output policy");
            (void)ModeContext.CancelFrame(91,0); (void)ModeContext.RetireCancelled(91,0);
            PresentationFence->WaitResult = RHI::ERHIResult::Success;
            bool Retired = false; (void)ModeContext.PollPresentation(90,0,Retired);
            Record(Result,Retired && ModeContext.Shutdown() == RHI::ERHIResult::Success,
                "output recreation fixture retires presentation only after its separate completion");
        }
        Device->TestQueue->SubmittedFences.clear();
    }
    FLabProductionFrameContext Context;
    const auto SceneOwnersBefore = Config.SceneLease.use_count();
    const auto DeviceOwnersBefore = Config.Device.use_count();
    const auto Initialized = Context.Initialize(Config);
    Record(Result, Context.ReleaseAfterDeviceShutdown(RHI::ERHIShutdownAssurance::IdleAssumed) ==
        RHI::ERHIResult::InvalidState && Context.Snapshot().bInitialized,
        "IdleAssumed cannot authorize host resource release while the native device is active");
    const auto CreatesBeforeInvalid = Device->Ledger()->Created.size();
    const auto TooWide = Context.Reconfigure(4097, 1);
    const auto TooManyPixels = Context.Reconfigure(4096, 1921);
    Record(Result, Initialized == RHI::ERHIResult::Success &&
            TooWide != RHI::ERHIResult::Success && TooManyPixels != RHI::ERHIResult::Success &&
            Device->Ledger()->Created.size() == CreatesBeforeInvalid,
        "Lab drawable axis and pixel limits reject requests before attachment allocation");
    (void)Context.Reconfigure(0, Height);
    const bool bPaused = Context.Snapshot().bPausedZeroExtent;
    const auto PausedReserve = Context.ReserveFrame(100, 0);
    const auto Resumed = Context.Reconfigure(Width, Height);
    Record(Result, bPaused && PausedReserve == RHI::ERHIResult::NotReady &&
            Resumed == RHI::ERHIResult::Success,
        "A zero drawable axis pauses admission and a valid later extent resumes");

    const auto Target0 = MakeTarget(101, 0, 0);
    const auto Target1 = MakeTarget(102, 1, 1);
    auto Frame0 = Config.Composition;
    auto Frame1 = Config.Composition;
    Frame0.FrameToken = 101;
    Frame1.FrameToken = 102;
    Frame1.DeferredInputs.View.CameraPosition.X += 1.0f;
    Frame1.CameraPosition = Frame1.DeferredInputs.View.CameraPosition;
    const auto Reserved0 = Context.ReserveFrame(101, 0);
    const auto Begun0 = Context.BeginFrame(101, 0, Target0);
    const auto Recorded0 = Context.RecordFrame(101, 0, Frame0);
    const auto* Resources0 = Context.GetResources(101, 0);
    const auto Attachments0 = Resources0 ? Resources0->OwnedTextures :
        Core::TArray<Core::TSharedPtr<RHI::IRHITexture>>{};
    const auto Submitted0 = Context.SubmitFrame(101, 0);
    auto LiveSettings = Config.OutputSettings; LiveSettings.ManualExposureStops = 2;
    LiveSettings.DiagnosticBypass.StageName="ManualExposure";
    LiveSettings.DiagnosticBypass.Mode=Renderer::EOutputTransformDebugBypassMode::HDRPreservingReadback;
    LiveSettings.DiagnosticBypass.VisualizationMinimum=-2;
    LiveSettings.DiagnosticBypass.VisualizationMaximum=6;
    auto InvalidDebug=LiveSettings; InvalidDebug.DiagnosticBypass.StageName="missing-stage";
    Record(Result,Context.UpdateOutputSettings(InvalidDebug)==RHI::ERHIResult::InvalidState,
        "unknown diagnostic stage rejects before a live settings transaction is accepted");
    const auto StagedSettings = Context.UpdateOutputSettings(LiveSettings);
    Record(Result,StagedSettings == RHI::ERHIResult::Success && Resources0 &&
        Resources0->OutputSettings.ManualExposureStops == Config.OutputSettings.ManualExposureStops,
        "staging live output settings cannot mutate an already submitted slot");
    const auto Reserved1 = Context.ReserveFrame(102, 1);
    const auto Begun1 = Context.BeginFrame(102, 1, Target1);
    const auto Recorded1 = Context.RecordFrame(102, 1, Frame1);
    const auto* SettingsResources1 = Context.GetResources(102,1);
    Record(Result,SettingsResources1 && SettingsResources1->OutputSettings.ManualExposureStops == 2 &&
        Resources0->OutputSettings.ManualExposureStops == Config.OutputSettings.ManualExposureStops,
        "next acquired slot consumes live settings while old slot retains its recorded state");
    Record(Result,SettingsResources1 &&
        SettingsResources1->OutputTransformPlan.DiagnosticBypass.SourceStageName==Core::FString("ManualExposure") &&
        SettingsResources1->OutputTransformPlan.DiagnosticBypass.SourceDomain==Renderer::ERenderGraphColorDomain::SceneLinearRec709D65 &&
        SettingsResources1->OutputTransformPlan.DiagnosticBypass.VisualizationMinimum==-2 &&
        SettingsResources1->OutputTransformPlan.DiagnosticBypass.VisualizationMaximum==6 &&
        !SettingsResources1->OutputTransformPlan.RequiresDiagnosticReadback() &&
        SettingsResources1->Bindings.Readbacks.empty() &&
        Resources0->OutputSettings.DiagnosticBypass.Mode==Renderer::EOutputTransformDebugBypassMode::Disabled,
        "numeric selection retains stage/domain/range without readback or mutation of the submitted slot");
    const auto Submitted1 = Context.SubmitFrame(102, 1);
    const auto Third = Context.ReserveFrame(103, 0);
    bool bCompleted = true;
    const auto Pending = Context.PollRender(101, 0, bCompleted);
    RHI::FRHIRenderLease EarlyLease;
    Record(Result, Reserved0 == RHI::ERHIResult::Success && Begun0 == RHI::ERHIResult::Success &&
            Recorded0 == RHI::ERHIResult::Success && Submitted0 == RHI::ERHIResult::Success &&
            Reserved1 == RHI::ERHIResult::Success && Begun1 == RHI::ERHIResult::Success &&
            Recorded1 == RHI::ERHIResult::Success && Submitted1 == RHI::ERHIResult::Success &&
            Third == RHI::ERHIResult::NotReady && Pending == RHI::ERHIResult::NotReady &&
            !bCompleted && !Context.GetRenderLease(101, 0, EarlyLease) &&
            Context.Snapshot().BusySlotCount == 2 &&
            Device->TestQueue->OrdinarySubmits == 0 && Device->TestQueue->IdleCalls == 0,
        "Lab context submits exactly two deferred frames and exposes no completion proof while they are pending");
    if (Device->TestQueue->SubmittedFences.size() != 2)
        return;
    const auto Render0 = std::dynamic_pointer_cast<FPreviewSubmissionFence>(Device->TestQueue->SubmittedFences[0]);
    const auto Render1 = std::dynamic_pointer_cast<FPreviewSubmissionFence>(Device->TestQueue->SubmittedFences[1]);
    Render0->WaitResult = RHI::ERHIResult::Success;
    const auto Completed0 = Context.PollRender(101, 0, bCompleted);
    bool bCompletedAgain = false;
    const auto CompletedAgain = Context.PollRender(101, 0, bCompletedAgain);
    RHI::FRHIRenderLease Lease0;
    const bool bTypedProof = Context.GetRenderLease(101, 0, Lease0);
    const auto EarlyRetire = Context.RetireRenderResources(101, 0);
    RHI::FRHIPresentationLease Presentation0;
    Presentation0.Frame = Target0.Frame;
    const auto PresentFence0 = Core::MakeShared<FPreviewSubmissionFence>();
    Presentation0.PresentationCompletionFence = PresentFence0;
    const auto Queued0 = Context.QueuePresentation(101, 0, Presentation0);
    const auto Command0 = std::dynamic_pointer_cast<
        Stoner::Tests::StaticModelRealization::FTrackedCommandBuffer>(
            Resources0->Bindings.CommandBuffer);
    Command0->ResetResult = RHI::ERHIResult::NotReady;
    const auto PendingReset0 = Context.RetireRenderResources(101, 0);
    Command0->ResetResult = RHI::ERHIResult::Success;
    const auto Retired0 = Context.RetireRenderResources(101, 0);
    const auto ReuseReserve = Context.ReserveFrame(103, 0);
    const auto Target2 = MakeTarget(103, 0, 2);
    const auto ReuseBegin = Context.BeginFrame(103, 0, Target2);
    const auto* Reused = Context.GetResources(103, 0);
    Record(Result, Completed0 == RHI::ERHIResult::Success &&
            CompletedAgain == RHI::ERHIResult::Success && bCompletedAgain &&
            PendingReset0 == RHI::ERHIResult::NotReady &&
            bTypedProof && Lease0.Matches(Target0) &&
            EarlyRetire == RHI::ERHIResult::NotReady && Queued0 == RHI::ERHIResult::Success &&
            Retired0 == RHI::ERHIResult::Success && ReuseReserve == RHI::ERHIResult::Success &&
            ReuseBegin == RHI::ERHIResult::Success && Reused && Reused->OwnedTextures == Attachments0 &&
            Context.Snapshot().RetainedPresentationCount == 1 && !PresentFence0->IsSignaled(),
        "Render-complete slot reuse preserves scene attachments while its independent presentation lease remains pending");
    auto Frame2 = Config.Composition;
    Frame2.FrameToken = 103;
    const auto Recorded2 = Context.RecordFrame(103, 0, Frame2);
    const auto Cancelled2 = Context.CancelFrame(103, 0);
    Command0->ResetResult = RHI::ERHIResult::Unavailable;
    const auto CancelResetFailure = Context.RetireCancelled(103, 0);
    Command0->ResetResult = RHI::ERHIResult::Success;
    const auto CancelRetired2 = Context.RetireCancelled(103, 0);
    Record(Result, Recorded2 == RHI::ERHIResult::Success &&
            Cancelled2 == RHI::ERHIResult::Success &&
            CancelResetFailure == RHI::ERHIResult::Unavailable &&
            CancelRetired2 == RHI::ERHIResult::Unavailable &&
            Command0->GetState() == RHI::ERHICommandBufferState::Idle,
        "Cancellation resets recorded commands and retains the first reset failure through successful retry");
    Render1->WaitResult = RHI::ERHIResult::Failed;
    const auto FailedPoll = Context.PollRender(102, 1, bCompleted);
    const bool bFailureDidNotComplete = !bCompleted;
    Render1->WaitResult = RHI::ERHIResult::Success;
    const auto CompletedFailure = Context.PollRender(102, 1, bCompleted);
    const auto Cancelled1 = Context.CancelFrame(102, 1);
    const auto Retired1 = Context.RetireCancelled(102, 1);
    Record(Result, FailedPoll == RHI::ERHIResult::Failed && bFailureDidNotComplete &&
            CompletedFailure == RHI::ERHIResult::Failed && bCompleted &&
            Cancelled1 == RHI::ERHIResult::Success &&
            Retired1 == RHI::ERHIResult::Failed && Context.Snapshot().BusySlotCount == 0,
        "Lab context can drain a failed frame after later render completion without hiding its first failure");
    PresentFence0->WaitResult = RHI::ERHIResult::Success;
    bool bPresentRetired = false;
    (void)Context.PollPresentation(101, 0, bPresentRetired);
    const auto Shutdown = Context.Shutdown();
    Record(Result, bPresentRetired && Shutdown == RHI::ERHIResult::Success &&
            Context.Snapshot().ActiveAttachmentBytes == 0 &&
            Config.SceneLease.use_count() == SceneOwnersBefore &&
            Config.Device.use_count() == DeviceOwnersBefore &&
            Target0.Texture->GetLifecycleState() == RHI::ERHIResourceLifecycleState::Valid &&
            Target1.Texture->GetLifecycleState() == RHI::ERHIResourceLifecycleState::Valid &&
            Target2.Texture->GetLifecycleState() == RHI::ERHIResourceLifecycleState::Valid,
        "Lab shutdown drains render owners and leaves all borrowed native targets uninvalidated");
}

} // namespace

namespace
{
void TestInteractiveLabConfiguration(FProductionContentDemoTestResult& Result)
{
    const auto ParseLab = [](std::initializer_list<const char*> Extra, FDemoConfiguration& Config) {
        Core::TArray<const char*> Args = {"StonerDemo", "--interactive-lab", "--mode", "interactive",
            "--backend", "vulkan", "--workload", "production-content", "--render-path", "deferred-full",
            "--production-root", "StaticModel:Lantern.glb#idx.scene.0", "--strict-generation", "generation-test",
            "--workload-revision", "production-content-lantern-v3", "--cooked-root", "Build/Lab/Cooked",
            "--lease-root", "Build/Lab/Lease", "--target-profile", "Config/AssetCooker/Profiles/Mac-Vulkan.json"};
        Args.insert(Args.end(), Extra.begin(), Extra.end());
        Core::FString Reason;
        return FDemoConfiguration::Parse(static_cast<int>(Args.size()), Args.data(), Config, Reason);
    };
    FDemoConfiguration Config;
    Record(Result, ParseLab({}, Config) == EDemoExitCode::Success && Config.bInteractiveLab &&
        Config.bLabUI && !Config.bLabForceAcquireHistory && !Config.bVisibleCapture && Config.BaselineRoot.IsEmpty(),
        "lab defaults enable UI and optional retirement selection without requiring an Accepted registry");
    Record(Result, ParseLab({"--lab-ui", "off"}, Config) == EDemoExitCode::Success && !Config.bLabUI,
        "lab UI-off is an explicit preview configuration");
    Record(Result, ParseLab({"--mode", "validate", "--frames", "120"}, Config) == EDemoExitCode::Success &&
        Config.FrameBudget == 120 && Config.WarmupFrames == 0,
        "bounded lab smoke requires no inherited 1000-frame warmup or RSS sample matrix");
    Record(Result, ParseLab({"--mode", "validate"}, Config) == EDemoExitCode::InvalidConfiguration,
        "bounded lab requires an explicitly supplied positive frame budget");
    Record(Result, ParseLab({"--mode", "validate", "--frames", "120", "--lab-vulkan-retirement", "acquire-history"}, Config) ==
        EDemoExitCode::Success && Config.bLabForceAcquireHistory,
        "forced acquire history is accepted for bounded Vulkan lab validation");
    Record(Result, ParseLab({"--lab-vulkan-retirement", "acquire-history"}, Config) == EDemoExitCode::InvalidConfiguration &&
        ParseLab({"--mode", "validate", "--frames", "120", "--backend", "metal", "--lab-vulkan-retirement", "acquire-history"}, Config) == EDemoExitCode::InvalidConfiguration,
        "forced Vulkan fallback cannot alter ordinary interactive or Metal runs");
    bool ConflictsRejected = true;
    for (const auto& Extra : {std::initializer_list<const char*>{"--visible-capture"},
        {"--production-camera-preview", "--camera-preset-output", "Build/Camera.json"},
        {"--production-capture-root", "Build/Capture"}, {"--output-native-probe", "Build/Probe", "--output-native-profile", "native-sdr"}})
        ConflictsRejected &= ParseLab(Extra, Config) == EDemoExitCode::InvalidConfiguration;
    Record(Result, ConflictsRejected, "lab mode rejects calibration and every formal capture/probe entry");
    bool ModesRejected = true;
    for (const auto& Extra : {std::initializer_list<const char*>{"--mode", "headless"},
        {"--mode", "headless-vulkan"}, {"--render-path", "forward-smoke"}, {"--frames-in-flight", "3"},
        {"--width", "4097"}, {"--width", "4096", "--height", "4096"}})
        ModesRejected &= ParseLab(Extra, Config) == EDemoExitCode::InvalidConfiguration;
    Record(Result, ModesRejected, "lab admission enforces native Deferred mode and frozen frame/extent bounds");
    Record(Result, ParseLab({"--backend", "metal", "--output-device-profile", "Hdr.Linear.2000.v1",
        "--output-transform-version", "Hdr.ACES2.0.0_2025-04-04.Rec2020D65.v1"}, Config) == EDemoExitCode::Success,
        "live HDR configuration remains preview without formal visible capture");
    Core::FString Reason;
    const char* NonLab[] = {"StonerDemo", "--lab-ui", "on"};
    Record(Result, FDemoConfiguration::Parse(3, NonLab, Config, Reason) == EDemoExitCode::InvalidConfiguration &&
        ParseLab({"--lab-ui", "maybe"}, Config) == EDemoExitCode::InvalidConfiguration &&
        ParseLab({"--lab-vulkan-retirement", "force-extension"}, Config) == EDemoExitCode::InvalidConfiguration,
        "lab-only and unknown UI/retirement option values are rejected");
}
} // namespace

FProductionContentDemoTestResult RunProductionContentDemoTests()
{
    FProductionContentDemoTestResult Result;
    TestInteractiveLabConfiguration(Result);
    TestPreviewSubmissionHarness(Result);
    Core::FString Reason;

    FOutputTransformValidationProbeInput Probe;
    Probe.HostPlatform = "macos";
    Probe.Backend = "metal";
    Probe.ProfileKind = "native-hdr-nonvisual";
    Probe.WorkloadRevision = "production-content-lantern-v3";
    Probe.DeviceClass = "macos.apple8.metal.hdr-v1";
    Probe.CapabilityDigest = Core::FString(std::string(64, '1'));
    Probe.OutputDeviceProfileId = "Hdr.PQ.Rec2020.1000.v1";
    Probe.TransformVersion =
        "Hdr.ACES2.0.0_2025-04-04.Rec2020D65.v1";
    Probe.InsertionDigest = Core::FString(std::string(64, '2'));
    Probe.ReadbackDigest = Core::FString(std::string(64, '3'));
    Probe.Width = 512;
    Probe.Height = 512;
    Probe.FirstFrameToken = 1;
    Probe.LastFrameToken = 17;
    Probe.SettledFrameToken = 17;
    Probe.Execution.Result = Renderer::EOutputTransformResult::Success;
    Probe.Execution.FinalState =
        Renderer::EOutputTransformPlanState::Published;
    Probe.Execution.bFormalOutputPublished = true;
    Probe.Execution.PublishedFormalOutputId = 1;
    Probe.Execution.FrameToken = 17;
    Probe.Execution.bNativeFrameAcquired = true;
    Probe.Execution.bNativeSubmitted = true;
    Probe.Execution.bNativeCompletionObserved = true;
    Probe.Execution.bNativeReadbackCompleted = true;
    Probe.Execution.bNativePresented = true;
    auto& State = Probe.Execution.ResolvedPresentationState;
    State.ModeGeneration = 4;
    State.SwapchainImageGeneration = 4;
    State.Width = 512;
    State.Height = 512;
    State.Format = RHI::ERHIFormat::R10G10B10A2_UNorm;
    State.ColorSpace = RHI::ERHIPresentationColorSpace::Hdr10St2084;
    State.NativeEncoding = RHI::ERHIPresentationNativeEncoding::Pq;
    State.DisplayAdaptation =
        RHI::ERHIPresentationDisplayAdaptation::SystemColorManagement;
    State.ReferenceWhiteNits = 100.0f;
    State.TargetPeakNits = 1000.0f;
    auto& Frame = Probe.Execution.PresentationFrame;
    Frame.FrameToken = 17;
    Frame.ModeGeneration = 4;
    Frame.SwapchainImageGeneration = 4;
    Frame.Width = 512;
    Frame.Height = 512;
    Frame.Format = State.Format;
    Frame.ColorSpace = State.ColorSpace;
    Frame.DisplayAdaptation = State.DisplayAdaptation;
    Core::FString ProbeJson;
    Record(Result,
        FOutputTransformValidationCommand::SerializeNormalizedNativeProbe(
            Probe, ProbeJson, &Reason) &&
            ProbeJson.View().find("\"outstandingTerminalOwnerCount\":0") !=
                std::string_view::npos &&
            ProbeJson.View().find("\"hdrMetadataDigest\":null") !=
                std::string_view::npos &&
            ProbeJson.View().find(
                "\"displayAdaptation\":\"system-color-management\"") !=
                std::string_view::npos &&
            ProbeJson.View().find("visualAuthority") ==
                std::string_view::npos &&
            ProbeJson.View().find("visualDecision") ==
                std::string_view::npos,
        "output transform probe serializes same-frame native facts without HDR visual authority");
    Probe.Execution.OutstandingTerminalOwnerCount = 1;
    Record(Result,
        !FOutputTransformValidationCommand::SerializeNormalizedNativeProbe(
            Probe, ProbeJson, &Reason) &&
            Reason == Core::FString(
                "successful native probe lacks same-frame completion or retained a terminal owner"),
        "output transform probe rejects terminal owner leaks");

    const Core::TArray<Core::uint8> SourcePixels = {
        255, 0, 0, 255, 0, 255, 0, 255,
        0, 0, 255, 255, 255, 255, 255, 255};
    Core::TArray<Core::uint8> FittedPixels;
    const bool bFitted = BuildAspectFitPresentationPixels(
        SourcePixels, 2, 2, 8, 4, 2,
        RHI::ERHIFormat::R8G8B8A8_UNorm, FittedPixels);
    Record(Result,
        bFitted && FittedPixels.size() == 32 &&
            FittedPixels[0] == 0 && FittedPixels[3] == 255 &&
            FittedPixels[4] == 255 && FittedPixels[5] == 0 &&
            FittedPixels[8] == 0 && FittedPixels[9] == 255 &&
            FittedPixels[12] == 0 && FittedPixels[15] == 255 &&
            FittedPixels[20] == 0 && FittedPixels[22] == 255 &&
            FittedPixels[24] == 255 && FittedPixels[25] == 255,
        "production presentation preserves aspect with opaque black side bars");
    Core::TArray<Core::uint8> ExactPixels;
    Record(Result,
        BuildAspectFitPresentationPixels(SourcePixels, 2, 2, 8, 2, 2,
            RHI::ERHIFormat::R8G8B8A8_UNorm, ExactPixels) &&
            ExactPixels == SourcePixels,
        "equal production render and drawable extents preserve exact pixels");
    FProductionAuthorityClientExtent RetinaExtent;
    FProductionAuthorityClientExtent NativeExtent;
    FProductionAuthorityClientExtent InvalidExtent;
    Record(Result,
        CalculateProductionAuthorityClientExtent(
            512, 512, 1024, 1024, 512, 512, RetinaExtent) &&
            RetinaExtent.Width == 256 && RetinaExtent.Height == 256 &&
        CalculateProductionAuthorityClientExtent(
            512, 512, 512, 512, 512, 512, NativeExtent) &&
            NativeExtent.Width == 512 && NativeExtent.Height == 512 &&
        !CalculateProductionAuthorityClientExtent(
            512, 512, 0, 0, 512, 512, InvalidExtent),
        "formal production authority derives an exact drawable client extent without image scaling");
    FDemoConfiguration Vulkan;
    FDemoConfiguration Metal;
    Record(Result,
        ParseArray(RegularArguments("vulkan"), Vulkan, Reason) ==
                EDemoExitCode::Success &&
            Vulkan.Workload == EDemoWorkload::ProductionContent &&
            Vulkan.ProductionRoot ==
                Core::FString("StaticModel:ProductionAcceptance/Lantern") &&
            Vulkan.StrictGeneration == Core::FString("generation-test") &&
            Vulkan.WorkloadRevision ==
                Core::FString("production-content-lantern-v2") &&
            Vulkan.RenderPath == EDemoRenderPath::DeferredFull &&
            Vulkan.ProductionLifecycleCycles == 20 &&
            Vulkan.ProductionWarmupCycles == 2,
        "production configuration preserves strict root revision and fixed lifecycle");

    auto ScaleLifecycleArguments = RegularArguments("metal");
    for (Core::usize Index = 0;
         Index + 1 < ScaleLifecycleArguments.size(); ++Index)
    {
        const Core::FString Option(ScaleLifecycleArguments[Index]);
        if (Option == Core::FString("--production-cycles"))
            ScaleLifecycleArguments[Index + 1] = "100";
        else if (Option == Core::FString("--production-warmup-cycles"))
            ScaleLifecycleArguments[Index + 1] = "10";
        else if (Option == Core::FString("--workload-revision"))
            ScaleLifecycleArguments[Index + 1] =
                "production-content-sponza-v2";
    }
    FDemoConfiguration ScaleLifecycle;
    Record(Result,
        ParseArray(std::move(ScaleLifecycleArguments), ScaleLifecycle, Reason) ==
                EDemoExitCode::Success &&
            ScaleLifecycle.ProductionLifecycleCycles == 100 &&
            ScaleLifecycle.ProductionWarmupCycles == 10,
        "production configuration accepts the fixed 100/10 scale lifecycle");

    auto InvalidLanternScaleArguments = RegularArguments("metal");
    for (Core::usize Index = 0;
         Index + 1 < InvalidLanternScaleArguments.size(); ++Index)
    {
        const Core::FString Option(InvalidLanternScaleArguments[Index]);
        if (Option == Core::FString("--production-cycles"))
            InvalidLanternScaleArguments[Index + 1] = "100";
        else if (Option == Core::FString("--production-warmup-cycles"))
            InvalidLanternScaleArguments[Index + 1] = "10";
    }
    FDemoConfiguration InvalidLanternScale;
    Record(Result,
        ParseArray(std::move(InvalidLanternScaleArguments),
            InvalidLanternScale, Reason) == EDemoExitCode::InvalidConfiguration,
        "production configuration rejects 100/10 outside Sponza v2");

    auto VisibleArguments = RegularArguments("vulkan");
    for (Core::usize Index = 0; Index + 1 < VisibleArguments.size(); ++Index)
        if (Core::FString(VisibleArguments[Index]) == Core::FString("--mode"))
            VisibleArguments[Index + 1] = "validate";
    VisibleArguments.insert(VisibleArguments.end(), {
        "--visible-capture", "--width", "512", "--height", "512"});
    FDemoConfiguration Visible;
    Record(Result,
        ParseArray(VisibleArguments, Visible, Reason) == EDemoExitCode::Success &&
            Visible.GetProductionRenderWidth() ==
                FDemoConfiguration::ProductionImageAcceptanceExtent &&
            Visible.GetProductionRenderHeight() ==
                FDemoConfiguration::ProductionImageAcceptanceExtent,
        "formal image acceptance freezes the render extent at exactly 512x512");

    auto CaptureArguments = VisibleArguments;
    CaptureArguments.insert(CaptureArguments.end(), {
        "--production-capture-root",
        "Build/Validation/029/test-captures"});
    Record(Result,
        ParseArray(std::move(CaptureArguments), Visible, Reason) ==
                EDemoExitCode::Success &&
            Visible.ProductionCaptureRoot == Core::FString(
                "Build/Validation/029/test-captures"),
        "visible SDR authority exposes an exact native capture root");

    for (Core::usize Index = 0; Index + 1 < VisibleArguments.size(); ++Index)
        if (Core::FString(VisibleArguments[Index]) == Core::FString("--width"))
            VisibleArguments[Index + 1] = "256";
    Record(Result,
        ParseArray(std::move(VisibleArguments), Visible, Reason) ==
                EDemoExitCode::InvalidConfiguration &&
            Reason == Core::FString(
                "formal image acceptance requires exactly 512x512"),
        "formal image acceptance rejects caller-selected non-canonical extents");

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

    const auto MakeHdrArguments = [](const char* Profile)
    {
        auto Arguments = RegularArguments("metal");
        for (Core::usize Index = 0; Index + 1 < Arguments.size(); ++Index)
        {
            const Core::FString Option(Arguments[Index]);
            if (Option == Core::FString("--mode"))
                Arguments[Index + 1] = "validate";
            else if (Option == Core::FString("--target-profile"))
                Arguments[Index + 1] =
                    "Config/AssetCooker/Profiles/Mac-Metal-Arm64.json";
            else if (Option == Core::FString("--workload-revision"))
                Arguments[Index + 1] = "production-content-lantern-v3";
        }
        Arguments.insert(Arguments.end(), {
            "--visible-capture", "--width", "512", "--height", "512",
            "--output-device-profile", Profile,
            "--output-transform-version",
            Renderer::GInitialHDRViewingVersion,
            "--output-exposure-stops", "0"});
        return Arguments;
    };
    FDemoConfiguration MetalPq;
    Renderer::FOutputTransformSettings MetalPqSettings;
    Renderer::FResolvedOutputTransformSettings MetalPqResolved;
    Record(Result,
        ParseArray(MakeHdrArguments("Hdr.PQ.Rec2020.1000.v1"),
            MetalPq, Reason) == EDemoExitCode::Success &&
            ResolveDemoOutputTransformSettings(
                MetalPq, 100.0f, MetalPqSettings, &MetalPqResolved,
                &Reason) &&
            MetalPqResolved.OutputFormat ==
                RHI::ERHIFormat::R10G10B10A2_UNorm &&
            MetalPqResolved.ColorSpace ==
                RHI::ERHIPresentationColorSpace::Hdr10St2084 &&
            MetalPqResolved.NativeEncoding ==
                RHI::ERHIPresentationNativeEncoding::Pq,
        "visible Metal PQ profile resolves the Renderer-owned packed10/PQ policy");

    auto PqProbeArguments =
        MakeHdrArguments("Hdr.PQ.Rec2020.1000.v1");
    PqProbeArguments.insert(PqProbeArguments.end(), {
        "--output-native-probe",
        "Build/Validation/029/test-pq-probe.json",
        "--output-native-profile", "native-hdr-nonvisual"});
    Record(Result,
        ParseArray(std::move(PqProbeArguments), MetalPq, Reason) ==
                EDemoExitCode::Success &&
            MetalPq.OutputNativeProbeProfile == Core::FString(
                "native-hdr-nonvisual"),
        "visible Metal HDR v3 accepts a machine-only native probe destination");

    FDemoConfiguration MetalEdr;
    Renderer::FOutputTransformSettings MetalEdrSettings;
    Renderer::FResolvedOutputTransformSettings MetalEdrResolved;
    Record(Result,
        ParseArray(MakeHdrArguments("Hdr.Linear.2000.v1"),
            MetalEdr, Reason) == EDemoExitCode::Success &&
            ResolveDemoOutputTransformSettings(
                MetalEdr, 203.0f, MetalEdrSettings, &MetalEdrResolved,
                &Reason) &&
            MetalEdrResolved.OutputFormat ==
                RHI::ERHIFormat::R16G16B16A16_Float &&
            MetalEdrResolved.ColorSpace ==
                RHI::ERHIPresentationColorSpace::ExtendedSrgbLinear &&
            MetalEdrResolved.NativeEncoding ==
                RHI::ERHIPresentationNativeEncoding::MetalEdr &&
            MetalEdrResolved.ReferenceWhiteNits == 203.0f,
        "visible Metal EDR profile preserves the native-resolved reference white");

    auto InvalidVulkanHdr = MakeHdrArguments("Hdr.PQ.Rec2020.1000.v1");
    for (Core::usize Index = 0;
         Index + 1 < InvalidVulkanHdr.size(); ++Index)
    {
        const Core::FString Option(InvalidVulkanHdr[Index]);
        if (Option == Core::FString("--backend"))
            InvalidVulkanHdr[Index + 1] = "vulkan";
    }
    FDemoConfiguration InvalidVulkanHdrConfig;
    Record(Result,
        ParseArray(std::move(InvalidVulkanHdr), InvalidVulkanHdrConfig,
            Reason) ==
            EDemoExitCode::InvalidConfiguration &&
            Reason == Core::FString(
                "Feature 029 HDR presentation is Metal-only"),
        "Feature 029 rejects Vulkan HDR presentation without an authority claim");

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

    FDemoValidationMonitor HostedObservationMonitor(Vulkan);
    for (Core::uint32 Cycle = 1; Cycle <= 20; ++Cycle)
    {
        const Core::uint64 Rss = Cycle < 20
            ? 64ULL * 1024ULL * 1024ULL
            : 96ULL * 1024ULL * 1024ULL;
        HostedObservationMonitor.AddSyntheticProductionCycle(
            Cycle, Rss, FDemoProductionLifecycleCounters{});
    }
    Record(Result,
        HostedObservationMonitor.EvaluateProductionLifecycle() &&
            !HostedObservationMonitor.IsProductionRssWithinLimit(),
        "production lifecycle keeps hosted RSS as a bounded observation without weakening functional authority");

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

    auto CallerAuthority = RegularArguments("vulkan");
    CallerAuthority.push_back("--execution-class");
    CallerAuthority.push_back("maintainer-local-metal");
    Record(Result,
        ParseArray(std::move(CallerAuthority), Invalid, Reason) ==
                EDemoExitCode::InvalidConfiguration &&
            Reason == Core::FString(
                "execution class must be workflow-derived"),
        "production configuration rejects caller promotion to physical authority");

    auto PreviewArguments = RegularArguments("metal");
    for (Core::usize Index = 0; Index + 1 < PreviewArguments.size(); ++Index)
    {
        if (Core::FString(PreviewArguments[Index]) == Core::FString("--mode"))
            PreviewArguments[Index + 1] = "interactive";
        if (Core::FString(PreviewArguments[Index]) ==
            Core::FString("--target-profile"))
            PreviewArguments[Index + 1] =
                "Config/AssetCooker/Profiles/Mac-Metal-Arm64.json";
    }
    PreviewArguments.push_back("--production-camera-preview");
    PreviewArguments.push_back("--camera-preset-output");
    PreviewArguments.push_back("Build/Validation/028/camera/candidate.json");
    FDemoConfiguration Preview;
    Record(Result,
        ParseArray(std::move(PreviewArguments), Preview, Reason) ==
                EDemoExitCode::Success &&
            Preview.bProductionCameraPreview &&
            Preview.ProductionCameraPresetOutput == Core::FString(
                "Build/Validation/028/camera/candidate.json") &&
            Preview.RunMode == EDemoRunMode::InteractiveNative &&
            Preview.RenderPath == EDemoRenderPath::DeferredFull &&
            Preview.GetProductionRenderWidth() ==
                FDemoConfiguration::ProductionCameraPreviewExtent &&
            Preview.GetProductionRenderHeight() ==
                FDemoConfiguration::ProductionCameraPreviewExtent,
        "camera preview requires explicit interactive strict-cooked output");

    auto HeadlessPreview = RegularArguments("vulkan");
    HeadlessPreview.push_back("--production-camera-preview");
    HeadlessPreview.push_back("--camera-preset-output");
    HeadlessPreview.push_back("Build/Validation/028/camera/invalid.json");
    Record(Result,
        ParseArray(std::move(HeadlessPreview), Invalid, Reason) ==
            EDemoExitCode::InvalidConfiguration,
        "camera preview rejects headless or formal validation mode");

    auto FormalOutput = RegularArguments("vulkan");
    FormalOutput.push_back("--camera-preset-output");
    FormalOutput.push_back("Build/Validation/028/camera/override.json");
    Record(Result,
        ParseArray(std::move(FormalOutput), Invalid, Reason) ==
            EDemoExitCode::InvalidConfiguration,
        "formal production validation rejects preview camera output options");

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
    const auto LabTestDevice = Core::MakeShared<FPreviewSubmissionDevice>();
    Fixture.Request.Device = LabTestDevice;
    Fixture.Request.RenderTargets.ColorFormats = {
        RHI::ERHIFormat::R8G8B8A8_UNorm,
        RHI::ERHIFormat::R16G16B16A16_Float,
        RHI::ERHIFormat::R16G16B16A16_Float};
    Core::TSharedPtr<const Renderer::FStaticModelRenderSnapshot> Snapshot;
    Renderer::FStaticModelRealizationInspection RealizationInspection;
    const auto Realized = Renderer::FStaticModelRealizer::Realize(
        Fixture.Request, Snapshot, RealizationInspection);
    FProductionContentCompositionConfig CompositionConfig;
    CompositionConfig.WorkloadRevision = "production-content-lantern-v2";
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
                Core::FString("production-content-lantern-v2") &&
            Composition.RootAssetId == Snapshot->GetRootAssetId() &&
            Composition.SnapshotGeneration ==
                Snapshot->GetSnapshotGeneration() &&
            Composition.DeferredInputs.DrawCandidates.size() ==
                Snapshot->GetDraws().size() &&
            Composition.ForwardInputs.DrawCandidates.size() ==
                Snapshot->GetDraws().size(),
        "composition preserves root version frame token and every realized draw");

    FProductionContentCompositionConfig SponzaV2Config = CompositionConfig;
    SponzaV2Config.WorkloadRevision = "production-content-sponza-v2";
    FProductionContentComposition SponzaV2Composition;
    FProductionCameraPreset SponzaV2Preset;
    const bool bSponzaV2Composed =
        ResolveProductionCameraPreset(
            SponzaV2Config.WorkloadRevision, SponzaV2Preset,
            &CompositionReason) &&
        FProductionContentCompositionBuilder::Build(
            Snapshot, SponzaV2Config, SponzaV2Composition,
            &CompositionReason);
    Record(Result,
        bSponzaV2Composed && SponzaV2Preset.IsValid() &&
            SponzaV2Composition.DeferredInputs.View.View.NearlyEquals(
                SponzaV2Preset.View) &&
            SponzaV2Composition.DeferredInputs.View.Projection.NearlyEquals(
                SponzaV2Preset.Projection) &&
            SponzaV2Composition.ForwardInputs.View.ViewMatrix.NearlyEquals(
                SponzaV2Preset.View) &&
            SponzaV2Composition.ForwardInputs.View.ViewProjectionMatrix
                .NearlyEquals(SponzaV2Preset.ViewProjection) &&
            SponzaV2Composition.DeferredInputs.View.ViewProjection.NearlyEquals(
                SponzaV2Composition.ForwardInputs.View.ViewProjectionMatrix),
        "Sponza v2 frozen camera is exact and backend-neutral across Deferred and Forward");

    FProductionCameraPreset LanternV2Preset;
    FProductionCameraPreset LanternV3Preset;
    FProductionCameraPreset SponzaV3Preset;
    FProductionContentCompositionConfig LanternV3Config = CompositionConfig;
    LanternV3Config.WorkloadRevision = "production-content-lantern-v3";
    FProductionContentComposition LanternV3Composition;
    FProductionContentCompositionConfig SponzaV3Config = CompositionConfig;
    SponzaV3Config.WorkloadRevision = "production-content-sponza-v3";
    FProductionContentComposition SponzaV3Composition;
    const bool bV3PreservesFrozenScene =
        ResolveProductionCameraPreset(
            CompositionConfig.WorkloadRevision, LanternV2Preset,
            &CompositionReason) &&
        ResolveProductionCameraPreset(
            LanternV3Config.WorkloadRevision, LanternV3Preset,
            &CompositionReason) &&
        ResolveProductionCameraPreset(
            SponzaV3Config.WorkloadRevision, SponzaV3Preset,
            &CompositionReason) &&
        FProductionContentCompositionBuilder::Build(
            Snapshot, LanternV3Config, LanternV3Composition,
            &CompositionReason) &&
        FProductionContentCompositionBuilder::Build(
            Snapshot, SponzaV3Config, SponzaV3Composition,
            &CompositionReason);
    Record(Result,
        bV3PreservesFrozenScene &&
            LanternV3Preset.WorkloadRevision ==
                Core::FString("production-content-lantern-v3") &&
            SponzaV3Preset.WorkloadRevision ==
                Core::FString("production-content-sponza-v3") &&
            LanternV3Preset.View.NearlyEquals(LanternV2Preset.View) &&
            LanternV3Preset.Projection.NearlyEquals(
                LanternV2Preset.Projection) &&
            SponzaV3Preset.View.NearlyEquals(SponzaV2Preset.View) &&
            SponzaV3Preset.Projection.NearlyEquals(
                SponzaV2Preset.Projection) &&
            !Composition.DeferredInputs.DirectionalLights.empty() &&
            !LanternV3Composition.DeferredInputs.DirectionalLights.empty() &&
            !SponzaV2Composition.DeferredInputs.DirectionalLights.empty() &&
            !SponzaV3Composition.DeferredInputs.DirectionalLights.empty() &&
            LanternV3Composition.DeferredInputs.DirectionalLights.front().
                Direction == Composition.DeferredInputs.DirectionalLights.front().
                    Direction &&
            SponzaV3Composition.DeferredInputs.DirectionalLights.front().
                Direction == SponzaV2Composition.DeferredInputs.
                    DirectionalLights.front().Direction,
        "Feature 029 v3 aliases preserve the exact v2 camera and scene lighting");

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
    TestSlotUniformIsolation(Result, Fixture.Request.Device, Snapshot, DeferredPlan);

    FProductionCameraPreset MovedCamera;
    Renderer::FDeferredFramePlan CameraDeferredPlan;
    Renderer::FForwardFramePlan CameraForwardPlan;
    const bool bCameraApplied = BuildProductionCameraPreset(
            Composition.WorkloadRevision,
            MakeProductionCameraView(
                {1.0f, -0.5f, 0.25f}, 0.2f, -0.1f),
            MakeProductionPerspective(0.9f, 16.0f / 9.0f),
            MovedCamera, &CompositionReason) &&
        ApplyProductionCameraPreset(Composition, MovedCamera,
            CameraDeferredPlan, CameraForwardPlan, &CompositionReason);
    Record(Result,
        bCameraApplied &&
            CameraDeferredPlan.View.View.NearlyEquals(MovedCamera.View) &&
            CameraDeferredPlan.View.Projection.NearlyEquals(
                MovedCamera.Projection) &&
            CameraDeferredPlan.View.ViewProjection.NearlyEquals(
                CameraForwardPlan.ViewData.ViewProjectionMatrix) &&
            CameraDeferredPlan.View.View.NearlyEquals(
                CameraForwardPlan.ViewData.ViewMatrix) &&
            CameraDeferredPlan.View.CameraPosition ==
                CameraForwardPlan.ViewData.CameraPosition,
        "camera updates feed exact shared matrices to Deferred and Forward plans");

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
            NativeForwardBindings.OutputTexture->GetFormat() ==
                RHI::ERHIFormat::R16G16B16A16_Float &&
            RHI::HasRHIFlag(
                NativeForwardBindings.OutputTexture->GetUsage(),
                RHI::ERHITextureUsage::Sampled) &&
            RHI::HasRHIFlag(
                NativeForwardBindings.OutputTexture->GetUsage(),
                RHI::ERHITextureUsage::CopySource) &&
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
            ForwardPlan.SceneColorHandoff.GetState() ==
                Renderer::EHDRSceneColorState::Declared &&
            ForwardPlan.SceneColorHandoff.GetFormat() ==
                RHI::ERHIFormat::R16G16B16A16_Float &&
            ForwardExecution.RecordedDrawCount ==
                NativeForwardBindings.Draws.size(),
        "Forward smoke records aggregate draws and hands SceneColor to the shared output pipeline");

    FProductionContentDeferredExecutionResources DeferredResources;
    Renderer::FOutputTransformSettings DefaultOutputSettings;
    DefaultOutputSettings.ManualExposureStops = 0.0f;
    DefaultOutputSettings.DynamicRange = Renderer::EOutputDynamicRange::SDR;
    DefaultOutputSettings.SDRToneMapVersion =
        Renderer::GDefaultSDRToneMapVersion;
    DefaultOutputSettings.OutputDeviceProfileId =
        Renderer::GDefaultSDROutputDeviceProfile;
    DefaultOutputSettings.PreferredNativeEncoding =
        RHI::ERHIPresentationNativeEncoding::SdrExplicit;
    DefaultOutputSettings.bRequirePresentation = true;
    DefaultOutputSettings.bRequireReadback = true;
    const auto MissingDeferredShaders = bComposed
        ? FProductionContentDeferredExecutionBuilder::Build(
            Fixture.Request.Device, *Snapshot, Composition, {}, {},
            *Fixture.Request.TargetEvidence, DefaultOutputSettings,
            DeferredResources,
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
            *Fixture.Request.TargetEvidence, DefaultOutputSettings,
            DeferredResources,
            &CompositionReason)
        : RHI::ERHIResult::Failed;
    const auto DeferredExecution =
        DeferredBuild == RHI::ERHIResult::Success
        ? Renderer::FDeferredFrameExecutor().Execute(
            DeferredResources.Plan, DeferredResources.Graph,
            DeferredResources.Bindings)
        : Renderer::FDeferredFrameExecutionResult{};
    const auto LifecycleBindings =
        DeferredResources.BuildCycleBindings(false);
    const auto AuthoritativeBindings =
        DeferredResources.BuildCycleBindings(true);
    Record(Result,
        DeferredBuild == RHI::ERHIResult::Success &&
            DeferredResources.IsValid() && DeferredExecution.Succeeded() &&
            DeferredResources.ExecutionPurpose ==
                Renderer::EFrameExecutionPurpose::FormalValidation &&
            DeferredResources.ReadbackSelection ==
                Renderer::EFrameReadbackSelection::Formal &&
            DeferredExecution.FinalState ==
                Renderer::EDeferredExecutionState::Recorded &&
            DeferredResources.Bindings.Readbacks.size() == 6 &&
            DeferredExecution.RecordedDrawCount >=
                DeferredResources.Plan.AcceptedDraws.size(),
        "strict shader closure records aggregate production Deferred attachments and readbacks");
    FProductionContentDeferredExecutionResources InvalidFormalSelection =
        DeferredResources;
    InvalidFormalSelection.ReadbackSelection =
        Renderer::EFrameReadbackSelection::None;
    Record(Result, !InvalidFormalSelection.IsValid(),
        "Deferred formal resources reject an explicit no-readback selection");
    Record(Result,
        LifecycleBindings.Readbacks.size() == 1 &&
            LifecycleBindings.Readbacks.front().Name ==
                Core::FString("FinalOutput") &&
            LifecycleBindings.Readbacks.front().Source ==
                DeferredResources.Bindings.FormalOutput &&
            DeferredResources.Bindings.FormalOutput !=
                DeferredResources.Bindings.FinalOutput &&
            DeferredResources.Bindings.OutputTransformStages.size() == 3 &&
            AuthoritativeBindings.Readbacks.size() == 6,
        "lifecycle reads only the three-stage formal output while post-lifecycle extraction retains all authoritative attachments");

    RHI::FRHITextureDesc BorrowedDesc =
        DeferredResources.Bindings.FormalOutput
            ? DeferredResources.Bindings.FormalOutput->GetDesc() : RHI::FRHITextureDesc{};
    BorrowedDesc.Usage = RHI::ERHITextureUsage::ColorAttachment |
        RHI::ERHITextureUsage::Present;
    const auto BorrowedTarget = Fixture.Request.Device->CreateTexture(BorrowedDesc);
    FProductionContentDeferredExecutionBuildOptions PreviewOptions;
    PreviewOptions.ExecutionPurpose = Renderer::EFrameExecutionPurpose::InteractivePreview;
    PreviewOptions.ReadbackSelection = Renderer::EFrameReadbackSelection::None;
    PreviewOptions.BorrowedFinalOutput = BorrowedTarget.Object;
    PreviewOptions.SceneLease = Snapshot;
    auto PreviewSettings = DefaultOutputSettings;
    PreviewSettings.bRequireReadback = false;
    FProductionContentDeferredExecutionResources PreviewResources;
    const auto PreviewBuild = FProductionContentDeferredExecutionBuilder::Build(
        Fixture.Request.Device, *Snapshot, Composition, RenderShaders, RenderPayloads,
        *Fixture.Request.TargetEvidence, PreviewSettings, PreviewResources,
        &CompositionReason, PreviewOptions);
    if (PreviewBuild == RHI::ERHIResult::Success)
    {
        const auto Buffers = PreviewResources.OutputParameterBuffers;
        const auto Textures = PreviewResources.OwnedTextures;
        auto Changed = PreviewSettings;
        Changed.ManualExposureStops = 3; Changed.SDRToneMapVersion = "Sdr.NarkowiczAcesFit.v1";
        const auto Updated = FProductionContentDeferredExecutionBuilder::UpdatePreviewFrame(
            Fixture.Request.Device,*Snapshot,Snapshot,Composition,PreviewResources,&CompositionReason,&Changed);
        bool BytesMatch = Buffers.size() == 3;
        const Renderer::EOutputTransformStageKind Kinds[] = {Renderer::EOutputTransformStageKind::ManualExposure,
            Renderer::EOutputTransformStageKind::SDRToneMap,Renderer::EOutputTransformStageKind::OutputDeviceTransform};
        for (std::size_t I = 0; I < Buffers.size() && I < 3; ++I)
        {
            const auto B = std::dynamic_pointer_cast<Stoner::Tests::StaticModelRealization::FTrackedBuffer>(Buffers[I]);
            const auto Expected = Renderer::FHDRPostProcessPipeline().BuildShaderParameterPayload(
                PreviewResources.OutputTransformPlan.ResolvedSettings,Kinds[I]);
            BytesMatch &= B && B->GetDesc().MemoryAccess == RHI::ERHIMemoryAccess::HostVisible &&
                B->GetData() == Expected.Bytes;
        }
        Record(Result,Updated == RHI::ERHIResult::Success && BytesMatch &&
            PreviewResources.OutputSettings.ManualExposureStops == 3 &&
            PreviewResources.OutputTransformPlan.ResolvedSettings.TransformStrategyVersion == "Sdr.NarkowiczAcesFit.v1" &&
            PreviewResources.OutputParameterBuffers == Buffers &&
            PreviewResources.OwnedTextures == Textures,
            "idle preview slot updates exact exposure/tone shader bytes without replacing buffers or attachments");
        Changed.DiagnosticBypass.StageName="SDRToneMap";
        Changed.DiagnosticBypass.Mode=Renderer::EOutputTransformDebugBypassMode::BoundedVisualization;
        Changed.DiagnosticBypass.VisualizationMinimum=2;
        Changed.DiagnosticBypass.VisualizationMaximum=8;
        const auto DebugUpdated=FProductionContentDeferredExecutionBuilder::UpdatePreviewFrame(
            Fixture.Request.Device,*Snapshot,Snapshot,Composition,PreviewResources,&CompositionReason,&Changed);
        Record(Result,DebugUpdated==RHI::ERHIResult::Success &&
            PreviewResources.OutputTransformPlan.DiagnosticBypass.SourceDomain==Renderer::ERenderGraphColorDomain::DisplayLinearRec709D65 &&
            PreviewResources.OutputTransformPlan.DiagnosticBypass.VisualizationMinimum==2 &&
            PreviewResources.OutputTransformPlan.DiagnosticBypass.VisualizationMaximum==8 &&
            !PreviewResources.OutputTransformPlan.HasDiagnosticWidget() &&
            PreviewResources.Bindings.Readbacks.empty() && PreviewResources.OwnedTextures==Textures,
            "hidden bounded selection preserves resolved display-linear stage/range without allocating widget or readback targets");
        Changed.ManualExposureStops = 17;
        const auto Rejected = FProductionContentDeferredExecutionBuilder::UpdatePreviewFrame(
            Fixture.Request.Device,*Snapshot,Snapshot,Composition,PreviewResources,&CompositionReason,&Changed);
        Record(Result,Rejected != RHI::ERHIResult::Success && PreviewResources.OutputSettings.ManualExposureStops == 3,
            "invalid preview edit rejects before replacing effective slot settings");
    }
    const auto PreviewBindings = PreviewResources.BuildCycleBindings(false);
    const auto PreviewExecution = Renderer::FDeferredFrameExecutor().Execute(
        PreviewResources.Plan, PreviewResources.Graph, PreviewBindings);
    const auto PreviewCommands = std::dynamic_pointer_cast<
        Stoner::Tests::StaticModelRealization::FTrackedCommandBuffer>(
            PreviewBindings.CommandBuffer);
    bool bOutputDrawsOnce = PreviewCommands &&
        PreviewBindings.OutputTransformStages.size() == 3;
    if (PreviewCommands)
    {
        for (const auto& Stage : PreviewBindings.OutputTransformStages)
            bOutputDrawsOnce = bOutputDrawsOnce && std::count(
                PreviewCommands->DrawFramebuffers.begin(),
                PreviewCommands->DrawFramebuffers.end(), Stage.Stage.Framebuffer) == 1;
    }
    const bool bNoReadbackBuffers = std::none_of(
        PreviewResources.OwnedBuffers.begin(), PreviewResources.OwnedBuffers.end(),
        [](const auto& Buffer) {
            return Buffer && Buffer->GetUsage() == RHI::ERHIBufferUsage::CopyDestination;
        });
    Record(Result, PreviewBuild == RHI::ERHIResult::Success && PreviewResources.IsValid() &&
            PreviewResources.Bindings.Readbacks.empty() && PreviewBindings.Readbacks.empty() &&
            bNoReadbackBuffers && PreviewResources.Plan.FindPass(
                Renderer::EDeferredPassStage::ValidationReadback) == nullptr &&
            PreviewResources.Bindings.FormalOutput == BorrowedTarget.Object &&
            std::find(PreviewResources.OwnedTextures.begin(), PreviewResources.OwnedTextures.end(),
                BorrowedTarget.Object) == PreviewResources.OwnedTextures.end(),
        "Preview production bindings allocate no readback and borrow the exact present-only terminal target");
    Record(Result, PreviewExecution.Succeeded() && bOutputDrawsOnce &&
            PreviewCommands && PreviewCommands->ReadbackCopies == 0 &&
            PreviewCommands->PresentTransitions == 1,
        "Preview without a validation pass still records all three output stages once and a single Present transition");

    auto RecordedEdit = PreviewSettings; RecordedEdit.ManualExposureStops = -3;
    Record(Result,FProductionContentDeferredExecutionBuilder::UpdatePreviewFrame(
        Fixture.Request.Device,*Snapshot,Snapshot,Composition,PreviewResources,&CompositionReason,&RecordedEdit) ==
        RHI::ERHIResult::InvalidState && PreviewResources.OutputSettings.ManualExposureStops == 3,
        "recorded preview slot rejects parameter mutation before completion and command reset");
    FProductionContentDeferredExecutionResources RejectedPreview;
    auto InvalidPurpose = PreviewOptions;
    InvalidPurpose.ExecutionPurpose = Renderer::EFrameExecutionPurpose::FormalValidation;
    const auto InvalidPurposeBuild = FProductionContentDeferredExecutionBuilder::Build(
        Fixture.Request.Device, *Snapshot, Composition, RenderShaders, RenderPayloads,
        *Fixture.Request.TargetEvidence, PreviewSettings, RejectedPreview,
        &CompositionReason, InvalidPurpose);
    Record(Result, InvalidPurposeBuild == RHI::ERHIResult::InvalidState &&
            RejectedPreview.OwnedTextures.empty() && RejectedPreview.OwnedBuffers.empty(),
        "Formal purpose with preview readback selection is rejected before resource allocation");
    PreviewResources.Release();
    Record(Result, BorrowedTarget.Object && BorrowedTarget.Object->GetLifecycleState() ==
            RHI::ERHIResourceLifecycleState::Valid,
        "Releasing preview resources leaves the borrowed presentation target valid");
    FLabProductionFrameContextConfig LabConfig;
    LabConfig.Device = LabTestDevice;
    LabConfig.SceneLease = Snapshot;
    LabConfig.Composition = Composition;
    LabConfig.RenderShaders = RenderShaders;
    LabConfig.RenderShaderPayloads = RenderPayloads;
    LabConfig.TargetEvidence = *Fixture.Request.TargetEvidence;
    LabConfig.OutputSettings = PreviewSettings;
    TestLabFrameContext(Result, LabConfig, BorrowedDesc.Format);

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
