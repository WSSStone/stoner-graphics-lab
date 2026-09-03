#include "RendererRenderGraphTests.h"

#include "Renderer/RendererMinimal.h"

#include <chrono>
#include <iostream>
#include <string>

namespace
{

using namespace Stoner::Renderer;
using Stoner::Core::uint32;

void Record(FRendererRenderGraphTestResult& Result, bool bPassed, const char* Name)
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

struct FRepresentativeGraph
{
    FRenderGraph Graph{"Representative"};
    FRenderGraphResourceHandle Imported;
    FRenderGraphResourceHandle FinalOutput;
};

FRenderGraphPassDesc Pass(std::string Name, ERenderGraphPassType Type)
{
    FRenderGraphPassDesc Desc = FRenderGraphPassDesc::Make(std::move(Name), Type);
    Desc.Callback = [](FRenderGraphExecutionContext&) {
        return ERenderGraphResult::Success;
    };
    return Desc;
}

FRepresentativeGraph BuildRepresentativeGraph()
{
    FRepresentativeGraph Fixture;
    FRenderGraphBuilder Builder = Fixture.Graph.CreateBuilder();

    Fixture.Imported = Builder.ImportResource(FRenderGraphResourceDesc::ImportedBuffer("ImportedInput", 256, false));
    const FRenderGraphResourceHandle A = Builder.CreateResource(FRenderGraphResourceDesc::Texture2D("GBufferA", 32, 32));
    const FRenderGraphResourceHandle B = Builder.CreateResource(FRenderGraphResourceDesc::Texture2D("Lighting", 32, 32));
    const FRenderGraphResourceHandle LateScratch = Builder.CreateResource(FRenderGraphResourceDesc::Texture2D("LateScratch", 32, 32));
    Fixture.FinalOutput = Builder.CreateResource(FRenderGraphResourceDesc::Texture2D("FinalOutput", 32, 32));
    const FRenderGraphResourceHandle Unused = Builder.CreateResource(FRenderGraphResourceDesc::Texture2D("UnusedBranch", 32, 32));

    FRenderGraphPassDesc Producer = Pass("Producer", ERenderGraphPassType::Graphics);
    Producer.Accesses.push_back({A, ERenderGraphAccessType::Write, ERenderGraphResourceState::Write});
    (void)Builder.AddPass(Producer);

    FRenderGraphPassDesc Compute = Pass("Compute", ERenderGraphPassType::Compute);
    Compute.Accesses.push_back({A, ERenderGraphAccessType::ReadWrite, ERenderGraphResourceState::ReadWrite});
    Compute.Accesses.push_back({B, ERenderGraphAccessType::Write, ERenderGraphResourceState::Write});
    (void)Builder.AddPass(Compute);

    FRenderGraphPassDesc Copy = Pass("CopyUtility", ERenderGraphPassType::Copy);
    Copy.Accesses.push_back({Fixture.Imported, ERenderGraphAccessType::Read, ERenderGraphResourceState::Read});
    Copy.Accesses.push_back({Fixture.FinalOutput, ERenderGraphAccessType::Write, ERenderGraphResourceState::Write});
    (void)Builder.AddPass(Copy);

    FRenderGraphPassDesc Output = Pass("Output", ERenderGraphPassType::Graphics);
    Output.Accesses.push_back({B, ERenderGraphAccessType::Read, ERenderGraphResourceState::Read});
    Output.Accesses.push_back({Fixture.FinalOutput, ERenderGraphAccessType::Read, ERenderGraphResourceState::Read});
    (void)Builder.AddPass(Output);

    FRenderGraphPassDesc Resolve = Pass("ResolveFinal", ERenderGraphPassType::Copy);
    Resolve.Accesses.push_back({Fixture.FinalOutput, ERenderGraphAccessType::Write, ERenderGraphResourceState::Write});
    (void)Builder.AddPass(Resolve);

    FRenderGraphPassDesc ResolveAgain = Pass("ResolveFinalAgain", ERenderGraphPassType::Copy);
    ResolveAgain.Accesses.push_back({Fixture.FinalOutput, ERenderGraphAccessType::Write, ERenderGraphResourceState::Write});
    (void)Builder.AddPass(ResolveAgain);

    FRenderGraphPassDesc SideEffect = Pass("SideEffectStats", ERenderGraphPassType::SideEffect);
    SideEffect.bPreserveForSideEffects = true;
    SideEffect.Accesses.push_back({LateScratch, ERenderGraphAccessType::Write, ERenderGraphResourceState::Write});
    (void)Builder.AddPass(SideEffect);

    FRenderGraphPassDesc UnusedPass = Pass("Unused", ERenderGraphPassType::Graphics);
    UnusedPass.Accesses.push_back({Unused, ERenderGraphAccessType::Write, ERenderGraphResourceState::Write});
    (void)Builder.AddPass(UnusedPass);

    (void)Builder.MarkOutput(Fixture.FinalOutput);
    return Fixture;
}

void TestDeclarationSchedulingAndNegativeValidation(FRendererRenderGraphTestResult& Result)
{
    FRepresentativeGraph Fixture = BuildRepresentativeGraph();
    Record(Result, Fixture.Graph.Compile() == ERenderGraphResult::Success, "Render graph declaration compile succeeds");
    const FCompiledRenderGraph& Compiled = Fixture.Graph.GetCompiledGraph();
    Record(Result, Compiled.ScheduledPasses.size() >= 7 && Compiled.ScheduledPasses[0] == 0 && Compiled.ScheduledPasses[1] == 1, "Render graph schedule is deterministic");
    Record(Result, !Compiled.DependencyEdges.empty(), "Render graph dependency edges are recorded");

    Stoner::Core::FString FirstDump = Fixture.Graph.Dump();
    for (int Index = 0; Index < 20; ++Index)
    {
        FRepresentativeGraph Repeated = BuildRepresentativeGraph();
        Record(Result, Repeated.Graph.Compile() == ERenderGraphResult::Success && Repeated.Graph.Dump() == FirstDump, "Render graph repeated compilation is stable");
    }

    FRenderGraph ReadBeforeWrite{"ReadBeforeWrite"};
    FRenderGraphBuilder RBW = ReadBeforeWrite.CreateBuilder();
    const FRenderGraphResourceHandle Transient = RBW.CreateResource(FRenderGraphResourceDesc::Buffer("Transient", 128));
    FRenderGraphPassDesc Read = Pass("Read", ERenderGraphPassType::Graphics);
    Read.Accesses.push_back({Transient, ERenderGraphAccessType::Read, ERenderGraphResourceState::Read});
    (void)RBW.AddPass(Read);
    (void)RBW.MarkOutput(Transient);
    Record(Result, ReadBeforeWrite.Compile() == ERenderGraphResult::CompileFailed, "Render graph rejects transient read before write");

    FRenderGraph CrossGraph{"CrossGraph"};
    FRenderGraphBuilder CrossBuilder = CrossGraph.CreateBuilder();
    FRenderGraphPassDesc CrossPass = Pass("Cross", ERenderGraphPassType::Graphics);
    CrossPass.Accesses.push_back({Fixture.FinalOutput, ERenderGraphAccessType::Read, ERenderGraphResourceState::Read});
    Record(Result, !CrossBuilder.AddPass(CrossPass).IsValid(), "Render graph rejects cross-graph resource handle");

    FRenderGraph Cycle{"Cycle"};
    FRenderGraphBuilder CycleBuilder = Cycle.CreateBuilder();
    const FRenderGraphResourceHandle CycleOutput = CycleBuilder.CreateResource(FRenderGraphResourceDesc::Buffer("CycleOutput", 64));
    FRenderGraphPassDesc CycleA = Pass("CycleA", ERenderGraphPassType::Graphics);
    CycleA.Accesses.push_back({CycleOutput, ERenderGraphAccessType::Write, ERenderGraphResourceState::Write});
    FRenderGraphPassDesc CycleB = Pass("CycleB", ERenderGraphPassType::Graphics);
    CycleB.Accesses.push_back({CycleOutput, ERenderGraphAccessType::Read, ERenderGraphResourceState::Read});
    const FRenderGraphPassHandle CycleAHandle = CycleBuilder.AddPass(CycleA);
    const FRenderGraphPassHandle CycleBHandle = CycleBuilder.AddPass(CycleB);
    (void)CycleBuilder.AddDependency(CycleAHandle, CycleBHandle);
    (void)CycleBuilder.AddDependency(CycleBHandle, CycleAHandle);
    (void)CycleBuilder.MarkOutput(CycleOutput);
    Record(Result, Cycle.Compile() == ERenderGraphResult::CompileFailed, "Render graph rejects explicit dependency cycle");

    FRenderGraph ReadOnlyImport{"ReadOnlyImport"};
    FRenderGraphBuilder ImportBuilder = ReadOnlyImport.CreateBuilder();
    const FRenderGraphResourceHandle Imported = ImportBuilder.ImportResource(FRenderGraphResourceDesc::ImportedBuffer("ReadOnly", 64, true));
    FRenderGraphPassDesc ImportWriter = Pass("ImportWriter", ERenderGraphPassType::Graphics);
    ImportWriter.Accesses.push_back({Imported, ERenderGraphAccessType::Write, ERenderGraphResourceState::Write});
    (void)ImportBuilder.AddPass(ImportWriter);
    (void)ImportBuilder.MarkOutput(Imported);
    Record(Result, ReadOnlyImport.Compile() == ERenderGraphResult::CompileFailed, "Render graph rejects writes to read-only imported resources");

    FRenderGraph ZeroOutput{"ZeroOutput"};
    FRenderGraphBuilder ZeroBuilder = ZeroOutput.CreateBuilder();
    FRenderGraphPassDesc NoOutputPass = Pass("NoOutput", ERenderGraphPassType::Graphics);
    (void)ZeroBuilder.AddPass(NoOutputPass);
    Record(Result, ZeroOutput.Compile() == ERenderGraphResult::CompileFailed, "Render graph rejects zero-output graph without side effects");

    FRenderGraph SideEffectOnly{"SideEffectOnly"};
    FRenderGraphBuilder SideBuilder = SideEffectOnly.CreateBuilder();
    FRenderGraphPassDesc Preserved = Pass("Preserved", ERenderGraphPassType::SideEffect);
    Preserved.bPreserveForSideEffects = true;
    (void)SideBuilder.AddPass(Preserved);
    Record(Result, SideEffectOnly.Compile() == ERenderGraphResult::Success, "Render graph accepts zero-output side-effect graph");
}

void TestLifetimesAliasingAndTransitions(FRendererRenderGraphTestResult& Result)
{
    FRepresentativeGraph Fixture = BuildRepresentativeGraph();
    Record(Result, Fixture.Graph.Compile() == ERenderGraphResult::Success, "Render graph lifetime fixture compiles");
    const FCompiledRenderGraph& Compiled = Fixture.Graph.GetCompiledGraph();

    const bool bHasImportedLifetime = std::any_of(Compiled.ResourceLifetimes.begin(), Compiled.ResourceLifetimes.end(), [](const FRenderGraphResourceLifetime& Lifetime) {
        return Lifetime.bImported;
    });
    const bool bHasExportedLifetime = std::any_of(Compiled.ResourceLifetimes.begin(), Compiled.ResourceLifetimes.end(), [](const FRenderGraphResourceLifetime& Lifetime) {
        return Lifetime.bExported;
    });
    Record(Result, bHasImportedLifetime && bHasExportedLifetime, "Render graph tracks imported and exported lifetimes");

    const bool bHasEligibleAlias = std::any_of(Compiled.AliasingDecisions.begin(), Compiled.AliasingDecisions.end(), [](const FRenderGraphAliasingDecision& Decision) {
        return Decision.State == ERenderGraphAliasDecisionState::Eligible;
    });
    const bool bHasRejectedAlias = std::any_of(Compiled.AliasingDecisions.begin(), Compiled.AliasingDecisions.end(), [](const FRenderGraphAliasingDecision& Decision) {
        return Decision.State == ERenderGraphAliasDecisionState::Rejected;
    });
    Record(Result, bHasEligibleAlias && bHasRejectedAlias, "Render graph reports aliasing eligibility and rejection");

    bool Reasons[5] = {};
    for (const FRenderGraphTransitionRecord& Transition : Compiled.TransitionPlan)
    {
        switch (Transition.Reason)
        {
        case ERenderGraphTransitionReason::ReadAfterWrite: Reasons[0] = true; break;
        case ERenderGraphTransitionReason::WriteAfterRead: Reasons[1] = true; break;
        case ERenderGraphTransitionReason::WriteAfterWrite: Reasons[2] = true; break;
        case ERenderGraphTransitionReason::GraphicsToCompute: Reasons[3] = true; break;
        case ERenderGraphTransitionReason::ComputeToGraphics: Reasons[4] = true; break;
        }
    }
    Record(Result, Reasons[0] && Reasons[1] && Reasons[2] && Reasons[3] && Reasons[4], "Render graph produces required transition reasons");

    FRenderGraphCommandContext Commands;
    const ERenderGraphResult ExecuteResult = Fixture.Graph.Execute({{{Fixture.Imported, 99}}, &Commands, false, {}});
    Record(Result, ExecuteResult == ERenderGraphResult::Success, "Render graph executes representative graph");
    Record(Result, Commands.EmittedTransitions.size() == Compiled.TransitionPlan.size(), "Render graph emits compiled transition plan");
    Record(Result, Fixture.Graph.GetResources()[1].BackingAllocationId != Fixture.Graph.GetResources()[2].BackingAllocationId, "Render graph keeps alias-eligible resources on separate backing storage");
}

void TestRedundantTransitionElision(FRendererRenderGraphTestResult& Result)
{
    // Producer writes R; two consecutive graphics passes then read R in the same Read
    // state. The first read needs one ReadAfterWrite transition; the second read is
    // redundant (same state, same queue kind) and must be elided (FR-010 / SC-006).
    FRenderGraph Graph{"RedundantElision"};
    FRenderGraphBuilder Builder = Graph.CreateBuilder();
    const FRenderGraphResourceHandle R = Builder.CreateResource(FRenderGraphResourceDesc::Texture2D("Shared", 16, 16));

    FRenderGraphPassDesc Producer = Pass("Producer", ERenderGraphPassType::Graphics);
    Producer.Accesses.push_back({R, ERenderGraphAccessType::Write, ERenderGraphResourceState::Write});
    (void)Builder.AddPass(Producer);

    FRenderGraphPassDesc ReaderOne = Pass("ReaderOne", ERenderGraphPassType::Graphics);
    ReaderOne.Accesses.push_back({R, ERenderGraphAccessType::Read, ERenderGraphResourceState::Read});
    (void)Builder.AddPass(ReaderOne);

    FRenderGraphPassDesc ReaderTwo = Pass("ReaderTwo", ERenderGraphPassType::Graphics);
    ReaderTwo.Accesses.push_back({R, ERenderGraphAccessType::Read, ERenderGraphResourceState::Read});
    (void)Builder.AddPass(ReaderTwo);

    (void)Builder.MarkOutput(R);
    Record(Result, Graph.Compile() == ERenderGraphResult::Success, "Render graph redundant-transition fixture compiles");

    const FCompiledRenderGraph& Compiled = Graph.GetCompiledGraph();
    uint32 TransitionsForR = 0;
    bool bSecondReaderTransition = false;
    for (const FRenderGraphTransitionRecord& Transition : Compiled.TransitionPlan)
    {
        if (Transition.Resource == R)
        {
            ++TransitionsForR;
        }
        if (Transition.AfterPassIndex == 2) // ReaderTwo declared third
        {
            bSecondReaderTransition = true;
        }
    }
    Record(Result, TransitionsForR == 1 && !bSecondReaderTransition,
        "Render graph elides redundant transition for repeated same-state read");
}

void TestExecutionFailuresAndDebugDump(FRendererRenderGraphTestResult& Result)
{
    FRepresentativeGraph MissingImport = BuildRepresentativeGraph();
    Record(Result, MissingImport.Graph.Compile() == ERenderGraphResult::Success, "Render graph missing import fixture compiles");
    Record(Result, MissingImport.Graph.Execute({}) == ERenderGraphResult::ResourceUnavailable, "Render graph rejects missing imported resources");

    FRenderGraph CulledImport{"CulledImport"};
    FRenderGraphBuilder CulledImportBuilder = CulledImport.CreateBuilder();
    const FRenderGraphResourceHandle CulledImported = CulledImportBuilder.ImportResource(FRenderGraphResourceDesc::ImportedBuffer("DebugImport", 64, true));
    const FRenderGraphResourceHandle CulledImportOutput = CulledImportBuilder.CreateResource(FRenderGraphResourceDesc::Buffer("Output", 64));
    int CulledImportInvoked = 0;
    FRenderGraphPassDesc CulledImportProducer = Pass("Producer", ERenderGraphPassType::Graphics);
    CulledImportProducer.Accesses.push_back({CulledImportOutput, ERenderGraphAccessType::Write, ERenderGraphResourceState::Write});
    CulledImportProducer.Callback = [&CulledImportInvoked](FRenderGraphExecutionContext&) {
        ++CulledImportInvoked;
        return ERenderGraphResult::Success;
    };
    (void)CulledImportBuilder.AddPass(CulledImportProducer);
    FRenderGraphPassDesc CulledImportDebug = Pass("UnusedDebugImport", ERenderGraphPassType::Graphics);
    CulledImportDebug.Accesses.push_back({CulledImported, ERenderGraphAccessType::Read, ERenderGraphResourceState::Read});
    (void)CulledImportBuilder.AddPass(CulledImportDebug);
    (void)CulledImportBuilder.MarkOutput(CulledImportOutput);
    Record(Result, CulledImport.Compile() == ERenderGraphResult::Success, "Render graph culled-import fixture compiles");
    Record(Result,
        CulledImport.GetCompiledGraph().CulledPasses.size() == 1 &&
        CulledImport.Execute({}) == ERenderGraphResult::Success &&
        CulledImportInvoked == 1 &&
        !CulledImport.GetResources()[CulledImported.Index].bResolvedDuringExecution,
        "Render graph ignores missing imports used only by culled passes");

    FRenderGraph CulledTransient{"CulledTransient"};
    FRenderGraphBuilder CulledTransientBuilder = CulledTransient.CreateBuilder();
    const FRenderGraphResourceHandle CulledScratch = CulledTransientBuilder.CreateResource(FRenderGraphResourceDesc::Buffer("CulledScratch", 64));
    int SideEffectInvoked = 0;
    FRenderGraphPassDesc RequiredSideEffect = Pass("RequiredSideEffect", ERenderGraphPassType::SideEffect);
    RequiredSideEffect.bPreserveForSideEffects = true;
    RequiredSideEffect.Callback = [&SideEffectInvoked](FRenderGraphExecutionContext&) {
        ++SideEffectInvoked;
        return ERenderGraphResult::Success;
    };
    (void)CulledTransientBuilder.AddPass(RequiredSideEffect);
    FRenderGraphPassDesc CulledScratchWriter = Pass("UnusedScratchWriter", ERenderGraphPassType::Graphics);
    CulledScratchWriter.Accesses.push_back({CulledScratch, ERenderGraphAccessType::Write, ERenderGraphResourceState::Write});
    (void)CulledTransientBuilder.AddPass(CulledScratchWriter);
    Record(Result, CulledTransient.Compile() == ERenderGraphResult::Success, "Render graph culled-transient fixture compiles");
    Record(Result,
        CulledTransient.GetCompiledGraph().CulledPasses.size() == 1 &&
        CulledTransient.Execute({{}, nullptr, true, {}}) == ERenderGraphResult::Success &&
        SideEffectInvoked == 1 &&
        !CulledTransient.GetResources()[CulledScratch.Index].bResolvedDuringExecution,
        "Render graph ignores transient resolution failures used only by culled passes");

    FRepresentativeGraph TransientFailure = BuildRepresentativeGraph();
    Record(Result, TransientFailure.Graph.Compile() == ERenderGraphResult::Success, "Render graph transient failure fixture compiles");
    Record(Result, TransientFailure.Graph.Execute({{{TransientFailure.Imported, 10}}, nullptr, true, {}}) == ERenderGraphResult::ResourceUnavailable, "Render graph reports transient resolution failure");

    FRenderGraph FailFast{"FailFast"};
    FRenderGraphBuilder Builder = FailFast.CreateBuilder();
    const FRenderGraphResourceHandle Output = Builder.CreateResource(FRenderGraphResourceDesc::Buffer("Output", 64));
    int Invoked = 0;
    FRenderGraphPassDesc First = Pass("First", ERenderGraphPassType::Graphics);
    First.Accesses.push_back({Output, ERenderGraphAccessType::Write, ERenderGraphResourceState::Write});
    First.Callback = [&Invoked](FRenderGraphExecutionContext&) {
        ++Invoked;
        return ERenderGraphResult::ExecutionFailed;
    };
    (void)Builder.AddPass(First);
    FRenderGraphPassDesc Second = Pass("Second", ERenderGraphPassType::Graphics);
    Second.Accesses.push_back({Output, ERenderGraphAccessType::Read, ERenderGraphResourceState::Read});
    Second.Callback = [&Invoked](FRenderGraphExecutionContext&) {
        ++Invoked;
        return ERenderGraphResult::Success;
    };
    (void)Builder.AddPass(Second);
    (void)Builder.MarkOutput(Output);
    Record(Result, FailFast.Compile() == ERenderGraphResult::Success, "Render graph fail-fast fixture compiles");
    Record(Result, FailFast.Execute({}) == ERenderGraphResult::ExecutionFailed && Invoked == 1, "Render graph stops on first pass failure");

    FRenderGraph Invalidated{"Invalidated"};
    FRenderGraphBuilder InvalidBuilder = Invalidated.CreateBuilder();
    const FRenderGraphResourceHandle InvalidOutput = InvalidBuilder.CreateResource(FRenderGraphResourceDesc::Buffer("Output", 64));
    FRenderGraphPassDesc InvalidPass = Pass("Write", ERenderGraphPassType::Graphics);
    InvalidPass.Accesses.push_back({InvalidOutput, ERenderGraphAccessType::Write, ERenderGraphResourceState::Write});
    (void)InvalidBuilder.AddPass(InvalidPass);
    (void)InvalidBuilder.MarkOutput(InvalidOutput);
    Record(Result, Invalidated.Compile() == ERenderGraphResult::Success, "Render graph invalidation fixture compiles");
    Invalidated.Invalidate();
    Record(Result, Invalidated.Execute({}) == ERenderGraphResult::InvalidState, "Render graph rejects execution after invalidation");
    Invalidated.Reset();
    Record(Result, Invalidated.GetState() == ERenderGraphState::Draft && Invalidated.GetPasses().empty(), "Render graph reset clears declarations and schedules");

    FRepresentativeGraph DumpFixture = BuildRepresentativeGraph();
    Record(Result, DumpFixture.Graph.Compile() == ERenderGraphResult::Success, "Render graph dump fixture compiles");
    const Stoner::Core::FString DumpA = DumpFixture.Graph.Dump();
    const Stoner::Core::FString DumpB = DumpFixture.Graph.Dump();
    Record(Result, DumpA == DumpB && DumpA.View().find("Transitions") != std::string_view::npos && DumpA.View().find("Aliasing") != std::string_view::npos, "Render graph debug dump is stable and complete");
}

void TestElapsedTimeScenario(FRendererRenderGraphTestResult& Result)
{
    const auto Start = std::chrono::steady_clock::now();
    FRepresentativeGraph Fixture = BuildRepresentativeGraph();
    FRenderGraphCommandContext Commands;
    const bool bPassed = Fixture.Graph.Compile() == ERenderGraphResult::Success &&
        !Fixture.Graph.Dump().IsEmpty() &&
        Fixture.Graph.Execute({{{Fixture.Imported, 123}}, &Commands, false, {}}) == ERenderGraphResult::Success;
    const auto End = std::chrono::steady_clock::now();
    const auto Elapsed = std::chrono::duration_cast<std::chrono::seconds>(End - Start).count();
    Record(Result, bPassed && Elapsed < 60, "Render graph representative declare compile inspect execute is under 60 seconds");
}

void TestTypedResourcesExternalEffectsAndScheduleVisitation(FRendererRenderGraphTestResult& Result)
{
    FRenderGraph Graph{"TypedOutputSchedule"};
    FRenderGraphBuilder Builder = Graph.CreateBuilder();
    const FRenderGraphResourceHandle SceneColor = Builder.CreateTexture(
        "SceneColor",
        64,
        64,
        Stoner::RHI::ERHIFormat::R16G16B16A16_Float,
        Stoner::RHI::ERHISampleCount::One,
        Stoner::RHI::ERHITextureUsage::Sampled |
            Stoner::RHI::ERHITextureUsage::ColorAttachment,
        ERenderGraphColorDomain::SceneLinearRec709D65);
    const FRenderGraphResourceHandle EncodedOutput = Builder.CreateTexture(
        "EncodedOutput",
        64,
        64,
        Stoner::RHI::ERHIFormat::R8G8B8A8_UNorm,
        Stoner::RHI::ERHISampleCount::One,
        Stoner::RHI::ERHITextureUsage::ColorAttachment |
            Stoner::RHI::ERHITextureUsage::CopySource,
        ERenderGraphColorDomain::EncodedSrgb);

    Stoner::Core::TArray<Stoner::Core::FString> ObservedExecution;
    FRenderGraphPassDesc Producer = Pass("Producer", ERenderGraphPassType::Graphics);
    Producer.Accesses.push_back({SceneColor, ERenderGraphAccessType::Write,
        ERenderGraphResourceState::Write});
    Producer.Callback = [&ObservedExecution](FRenderGraphExecutionContext&) {
        ObservedExecution.push_back("pass:Producer");
        return ERenderGraphResult::Success;
    };
    (void)Builder.AddPass(Producer);

    FRenderGraphPassDesc Encode = Pass("Encode", ERenderGraphPassType::Graphics);
    Encode.Accesses.push_back({SceneColor, ERenderGraphAccessType::Read,
        ERenderGraphResourceState::Read});
    Encode.Accesses.push_back({EncodedOutput, ERenderGraphAccessType::Write,
        ERenderGraphResourceState::Write});
    Encode.Callback = [&ObservedExecution](FRenderGraphExecutionContext&) {
        ObservedExecution.push_back("pass:Encode");
        return ERenderGraphResult::Success;
    };
    (void)Builder.AddPass(Encode);

    FRenderGraphPassDesc Readback = Pass("Readback", ERenderGraphPassType::Copy);
    Readback.ExternalSideEffect = ERenderGraphExternalSideEffect::Readback;
    Readback.Accesses.push_back({EncodedOutput, ERenderGraphAccessType::Read,
        ERenderGraphResourceState::Read});
    Readback.Callback = [&ObservedExecution](FRenderGraphExecutionContext&) {
        ObservedExecution.push_back("pass:Readback");
        return ERenderGraphResult::Success;
    };
    (void)Builder.AddPass(Readback);

    Record(Result, Graph.Compile() == ERenderGraphResult::Success,
        "Render graph compiles a typed output graph with an external readback effect");
    const FCompiledRenderGraph& Compiled = Graph.GetCompiledGraph();
    Record(Result,
        Compiled.ScheduledPasses.size() == 3 && Compiled.CulledPasses.empty(),
        "Render graph external readback effect protects its dependency chain from culling");

    Stoner::Core::TArray<Stoner::Core::FString> VisitedSchedule;
    const ERenderGraphResult VisitResult = Compiled.VisitSchedule(
        [&VisitedSchedule](const FRenderGraphScheduleEvent& Event) {
            if (Event.Kind == ERenderGraphScheduleEventKind::Transition)
            {
                VisitedSchedule.push_back("transition:" +
                    std::to_string(Event.Transition->AfterPassIndex));
            }
            else
            {
                VisitedSchedule.push_back("pass:" + std::to_string(Event.PassIndex));
            }
            return ERenderGraphResult::Success;
        });
    Record(Result,
        VisitResult == ERenderGraphResult::Success &&
            !VisitedSchedule.empty() && VisitedSchedule.front() == "pass:0",
        "Render graph immutable schedule visitor begins with the first compiled pass");

    bool bTransitionsPrecedeOwningPass = true;
    for (uint32 Index = 0; Index < VisitedSchedule.size(); ++Index)
    {
        if (VisitedSchedule[Index].View().find("transition:") == 0)
        {
            const std::string PassToken = "pass:" +
                VisitedSchedule[Index].ToStdString().substr(11);
            bTransitionsPrecedeOwningPass =
                bTransitionsPrecedeOwningPass && Index + 1 < VisitedSchedule.size() &&
                VisitedSchedule[Index + 1] == PassToken;
        }
    }
    Record(Result, bTransitionsPrecedeOwningPass,
        "Render graph schedule visitor places every compiled transition before its owning pass");

    FRenderGraphCommandContext Commands;
    FRenderGraphExecutionDesc Execution;
    Execution.CommandContext = &Commands;
    Execution.ScheduleVisitor = [&ObservedExecution](const FRenderGraphScheduleEvent& Event) {
        if (Event.Kind == ERenderGraphScheduleEventKind::Transition)
        {
            ObservedExecution.push_back("transition:" +
                std::to_string(Event.Transition->AfterPassIndex));
        }
        return ERenderGraphResult::Success;
    };
    Record(Result, Graph.Execute(Execution) == ERenderGraphResult::Success,
        "Render graph executor consumes the compiled schedule visitor contract");
    Record(Result,
        ObservedExecution.size() == 5 &&
            ObservedExecution[0] == "pass:Producer" &&
            ObservedExecution[1] == "transition:1" &&
            ObservedExecution[2] == "pass:Encode" &&
            ObservedExecution[3] == "transition:2" &&
            ObservedExecution[4] == "pass:Readback",
        "Render graph executor interleaves compiled transitions with owning passes");

    FRenderGraph AliasGraph{"TypedAliasCompatibility"};
    FRenderGraphBuilder AliasBuilder = AliasGraph.CreateBuilder();
    const FRenderGraphResourceHandle Linear = AliasBuilder.CreateTexture(
        "Linear",
        32,
        32,
        Stoner::RHI::ERHIFormat::R16G16B16A16_Float,
        Stoner::RHI::ERHISampleCount::One,
        Stoner::RHI::ERHITextureUsage::ColorAttachment,
        ERenderGraphColorDomain::SceneLinearRec709D65);
    const FRenderGraphResourceHandle Display = AliasBuilder.CreateTexture(
        "Display",
        32,
        32,
        Stoner::RHI::ERHIFormat::R16G16B16A16_Float,
        Stoner::RHI::ERHISampleCount::One,
        Stoner::RHI::ERHITextureUsage::ColorAttachment,
        ERenderGraphColorDomain::DisplayLinearRec709D65);
    FRenderGraphPassDesc LinearWrite = Pass("LinearWrite", ERenderGraphPassType::Graphics);
    LinearWrite.Accesses.push_back({Linear, ERenderGraphAccessType::Write,
        ERenderGraphResourceState::Write});
    (void)AliasBuilder.AddPass(LinearWrite);
    FRenderGraphPassDesc DisplayWrite = Pass("DisplayWrite", ERenderGraphPassType::Graphics);
    DisplayWrite.Accesses.push_back({Display, ERenderGraphAccessType::Write,
        ERenderGraphResourceState::Write});
    DisplayWrite.ExternalSideEffect = ERenderGraphExternalSideEffect::Presentation;
    (void)AliasBuilder.AddPass(DisplayWrite);
    FRenderGraphPassDesc ObserveLinear = Pass("ObserveLinear", ERenderGraphPassType::Copy);
    ObserveLinear.Accesses.push_back({Linear, ERenderGraphAccessType::Read,
        ERenderGraphResourceState::Read});
    ObserveLinear.ExternalSideEffect = ERenderGraphExternalSideEffect::Readback;
    (void)AliasBuilder.AddPass(ObserveLinear);
    Record(Result, AliasGraph.Compile() == ERenderGraphResult::Success,
        "Render graph compiles typed resources with distinct color domains");
    const bool bRejectedDomainAlias = std::any_of(
        AliasGraph.GetCompiledGraph().AliasingDecisions.begin(),
        AliasGraph.GetCompiledGraph().AliasingDecisions.end(),
        [Linear, Display](const FRenderGraphAliasingDecision& Decision) {
            return ((Decision.FirstResource == Linear && Decision.SecondResource == Display) ||
                       (Decision.FirstResource == Display && Decision.SecondResource == Linear)) &&
                Decision.State == ERenderGraphAliasDecisionState::Rejected &&
                Decision.Reason == ERenderGraphAliasReason::IncompatibleDescription;
        });
    Record(Result, bRejectedDomainAlias,
        "Render graph typed compatibility rejects aliasing across color domains");
}

} // namespace

FRendererRenderGraphTestResult RunRendererRenderGraphTests()
{
    FRendererRenderGraphTestResult Result;
    TestDeclarationSchedulingAndNegativeValidation(Result);
    TestLifetimesAliasingAndTransitions(Result);
    TestRedundantTransitionElision(Result);
    TestExecutionFailuresAndDebugDump(Result);
    TestElapsedTimeScenario(Result);
    TestTypedResourcesExternalEffectsAndScheduleVisitation(Result);
    return Result;
}
