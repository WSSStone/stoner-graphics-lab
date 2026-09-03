#include "Renderer/FOutputTransformExecutor.h"

#include <algorithm>

namespace Stoner::Renderer
{

namespace
{

void Fail(FOutputTransformExecutionResult& Out,
    EOutputTransformResult Result,
    const char* Code,
    const char* Stage,
    const char* Message)
{
    Out.Result = Result;
    Out.FinalState = EOutputTransformPlanState::Failed;
    Out.bFormalOutputPublished = false;
    Out.PublishedFormalOutputId = 0;
    Out.Diagnostics.Add(EOutputTransformDiagnosticSeverity::Error, Result,
        Code, Stage, "FormalOutput", Message);
}

bool ContainsPass(const FOutputTransformGraphDeclaration& Declaration,
    Stoner::Core::uint32 PassIndex) noexcept
{
    return std::any_of(Declaration.OrderedPasses.begin(),
        Declaration.OrderedPasses.end(),
        [PassIndex](FRenderGraphPassHandle Handle) {
            return Handle.Index == PassIndex;
        });
}

[[nodiscard]] bool IsExactNativeBinding(
    const FOutputTransformPlan& Plan,
    const FOutputTransformNativeFrameBinding& Frame) noexcept
{
    const auto& Ticket = Frame.PresentationFrame;
    const auto& State = Frame.ResolvedState;
    const auto& Settings = Plan.ResolvedSettings;
    if (!Frame.FormalOutput || !Ticket.IsValid() || !State.IsValid() ||
        !Ticket.Matches(State) || Ticket.FrameToken != Plan.FrameToken ||
        Ticket.Width != Plan.OutputDesc.Width ||
        Ticket.Height != Plan.OutputDesc.Height ||
        Ticket.Format != Plan.OutputDesc.Format ||
        Ticket.ColorSpace != Settings.ColorSpace ||
        State.NativeEncoding != Settings.NativeEncoding ||
        Frame.FormalOutput->GetLifecycleState() !=
            Stoner::RHI::ERHIResourceLifecycleState::Valid)
    {
        return false;
    }
    const auto& Desc = Frame.FormalOutput->GetDesc();
    if (Desc.Width != Ticket.Width || Desc.Height != Ticket.Height ||
        Desc.Format != Ticket.Format ||
        !Stoner::RHI::HasRHIFlag(
            Desc.Usage, Stoner::RHI::ERHITextureUsage::ColorAttachment) ||
        (Settings.bRequirePresentation && !Stoner::RHI::HasRHIFlag(
            Desc.Usage, Stoner::RHI::ERHITextureUsage::Present)) ||
        ((Settings.bRequireReadback ||
             Plan.DiagnosticBypass.Mode !=
                 EOutputTransformDebugBypassMode::Disabled) &&
            !Stoner::RHI::HasRHIFlag(
                Desc.Usage, Stoner::RHI::ERHITextureUsage::CopySource)))
    {
        return false;
    }
    return true;
}

void ReleaseNativeFrameAfterFailure(
    FOutputTransformExecutionResult& Out,
    IOutputTransformNativeFrameExecutor* Native,
    const FOutputTransformNativeFrameBinding& Frame) noexcept
{
    if (!Native || !Out.bNativeFrameAcquired || Out.bNativePresented)
    {
        return;
    }
    const auto Release = Native->ReleaseAfterFailure(Frame);
    Out.bNativeReleasedAfterFailure =
        Release == Stoner::RHI::ERHIResult::Success;
    Out.OutstandingTerminalOwnerCount =
        Native->GetOutstandingTerminalOwnerCount();
}

[[nodiscard]] bool NativeStepSucceeded(
    FOutputTransformExecutionResult& Out,
    IOutputTransformNativeFrameExecutor* Native,
    const FOutputTransformNativeFrameBinding& Frame,
    Stoner::RHI::ERHIResult Result,
    const char* Code,
    const char* Stage,
    const char* Message)
{
    Out.NativeResult = Result;
    if (Result == Stoner::RHI::ERHIResult::Success)
    {
        return true;
    }
    const EOutputTransformResult Mapped =
        Result == Stoner::RHI::ERHIResult::Unsupported ||
            Result == Stoner::RHI::ERHIResult::Unavailable ||
            Result == Stoner::RHI::ERHIResult::NotReady ||
            Result == Stoner::RHI::ERHIResult::ResizeRequired
        ? EOutputTransformResult::Unsupported
        : EOutputTransformResult::TerminalFailed;
    Fail(Out, Mapped, Code, Stage, Message);
    ReleaseNativeFrameAfterFailure(Out, Native, Frame);
    return false;
}

} // namespace

FOutputTransformExecutionResult FOutputTransformExecutor::Execute(
    const FOutputTransformPlan& Plan,
    FRenderGraph& Graph,
    const FOutputTransformGraphDeclaration& Declaration,
    const FOutputTransformExecutionBindings& Bindings) const
{
    FOutputTransformExecutionResult Out;
    Out.FrameToken = Plan.FrameToken;
    Out.PlanFingerprint = Plan.PlanFingerprint;
    if (!Plan.IsValid() || !Declaration.IsValid() ||
        Graph.GetState() != ERenderGraphState::Compiled ||
        !FHDRPostProcessPipeline().ValidateOutputGraph(
            Graph, Plan, Declaration, &Out.Diagnostics))
    {
        Fail(Out, EOutputTransformResult::InvalidGraph, "OT-EXEC-GRAPH",
            "Binding", "execution requires the exact validated compiled output graph");
        return Out;
    }
    const FRenderGraphResourceRecord* SceneColor =
        Graph.FindResource(Declaration.SceneColor);
    if (!SceneColor ||
        (SceneColor->Desc.Ownership == ERenderGraphResourceOwnership::Imported &&
            Bindings.SceneColorExternalToken == 0))
    {
        Fail(Out, EOutputTransformResult::InvalidBinding,
            "OT-EXEC-SCENE-COLOR", "Binding",
            "the current produced SceneColor requires one nonzero external token");
        return Out;
    }

    IOutputTransformNativeFrameExecutor* Native =
        Bindings.NativeFrameExecutor;
    if (Bindings.bRequireNativeExecution && !Native)
    {
        Fail(Out, EOutputTransformResult::InvalidBinding,
            "OT-EXEC-NATIVE-REQUIRED", "Binding",
            "native execution was required but no backend-neutral frame executor was bound");
        return Out;
    }
    FOutputTransformNativeFrameBinding NativeFrame;
    if (Native)
    {
        const auto Acquire = Native->Acquire(Plan, NativeFrame);
        Out.NativeResult = Acquire;
        if (Acquire != Stoner::RHI::ERHIResult::Success)
        {
            const EOutputTransformResult Mapped =
                Acquire == Stoner::RHI::ERHIResult::Unsupported ||
                    Acquire == Stoner::RHI::ERHIResult::Unavailable ||
                    Acquire == Stoner::RHI::ERHIResult::NotReady ||
                    Acquire == Stoner::RHI::ERHIResult::ResizeRequired
                ? EOutputTransformResult::Unsupported
                : EOutputTransformResult::TerminalFailed;
            Fail(Out, Mapped, "OT-EXEC-ACQUIRE", "Acquire",
                "formal native image acquisition did not complete for the requested mode generation");
            Out.OutstandingTerminalOwnerCount =
                Native->GetOutstandingTerminalOwnerCount();
            return Out;
        }
        Out.bNativeFrameAcquired = true;
        Out.PresentationFrame = NativeFrame.PresentationFrame;
        Out.ResolvedPresentationState = NativeFrame.ResolvedState;
        if (!IsExactNativeBinding(Plan, NativeFrame))
        {
            Fail(Out, EOutputTransformResult::InvalidBinding,
                "OT-EXEC-NATIVE-BINDING", "Binding",
                "acquired native image does not exactly match the frame token extent format colorspace encoding and usage contract");
            ReleaseNativeFrameAfterFailure(Out, Native, NativeFrame);
            return Out;
        }
    }

    FRenderGraphExecutionDesc ExecuteDesc;
    if (SceneColor->Desc.Ownership == ERenderGraphResourceOwnership::Imported)
    {
        ExecuteDesc.ImportedResources.push_back(
            {Declaration.SceneColor, Bindings.SceneColorExternalToken});
    }
    ExecuteDesc.bFailTransientResolution = Bindings.bFailTransientResolution;
    ExecuteDesc.ScheduleVisitor =
        [&Out, &Graph, &Declaration, &Bindings, Native, &Plan, &NativeFrame](
            const FRenderGraphScheduleEvent& Event) {
            if (Event.Kind != ERenderGraphScheduleEventKind::Pass)
            {
                return ERenderGraphResult::Success;
            }
            if (Bindings.bFailSchedule)
            {
                return ERenderGraphResult::ExecutionFailed;
            }
            if (Native && Native->RecordScheduleEvent(
                    Plan, Declaration, Event, NativeFrame) !=
                    Stoner::RHI::ERHIResult::Success)
            {
                Out.NativeResult = Stoner::RHI::ERHIResult::Failed;
                return ERenderGraphResult::ExecutionFailed;
            }
            ++Out.ExecutedPassCount;
            if (ContainsPass(Declaration, Event.PassIndex) &&
                Graph.GetPasses()[Event.PassIndex].Desc.Type ==
                    ERenderGraphPassType::Graphics)
            {
                ++Out.ExecutedFullscreenPassCount;
                ++Out.ExecutedFullImageVisitCount;
            }
            return ERenderGraphResult::Success;
        };
    const ERenderGraphResult GraphResult = Graph.Execute(ExecuteDesc);
    if (GraphResult != ERenderGraphResult::Success)
    {
        Fail(Out, EOutputTransformResult::ExecutionFailed,
            "OT-EXEC-SCHEDULE", "Execution",
            "compiled schedule failed before terminal completion");
        ReleaseNativeFrameAfterFailure(Out, Native, NativeFrame);
        return Out;
    }
    Out.FinalState = EOutputTransformPlanState::Completed;

    if (Native)
    {
        if (!NativeStepSucceeded(Out, Native, NativeFrame,
                Native->Submit(Plan, NativeFrame), "OT-EXEC-SUBMIT",
                "Submit", "the ordered native command sequence did not submit"))
            return Out;
        Out.bNativeSubmitted = true;
        if (!NativeStepSucceeded(Out, Native, NativeFrame,
                Native->WaitForCompletion(Plan, NativeFrame),
                "OT-EXEC-COMPLETION", "Completion",
                "the submitted native frame did not complete"))
            return Out;
        Out.bNativeCompletionObserved = true;
    }
    Out.DiagnosticGpuReadbackCopyCount =
        Declaration.DiagnosticReadbackCopyCount;
    if (Plan.DiagnosticBypass.Mode !=
        EOutputTransformDebugBypassMode::Disabled)
    {
        if (Bindings.bFailDiagnosticReadback)
        {
            Fail(Out, EOutputTransformResult::TerminalFailed,
                "OT-EXEC-DIAGNOSTIC-READBACK", "DiagnosticBypass",
                "requested non-authoritative diagnostic readback did not complete");
            ReleaseNativeFrameAfterFailure(Out, Native, NativeFrame);
            return Out;
        }
        Out.DiagnosticBypass = Plan.DiagnosticBypassRecord;
        Out.bDiagnosticBypassProduced = Out.DiagnosticBypass.IsValid();
        Out.Diagnostics.Add(EOutputTransformDiagnosticSeverity::Info,
            EOutputTransformResult::Success, "OT-EXEC-DIAGNOSTIC-PUBLISHED",
            "DiagnosticBypass", Plan.DiagnosticBypass.SourceStageName,
            "diagnostic output completed as explicitly non-authoritative evidence");
    }
    Out.GpuReadbackCopyCount = Declaration.GpuReadbackCopyCount;
    if (Plan.ResolvedSettings.bRequireReadback)
    {
        if (Bindings.bFailReadback)
        {
            Fail(Out, EOutputTransformResult::TerminalFailed,
                "OT-EXEC-READBACK", "FormalReadback",
                "requested readback did not complete for the current frame");
            ReleaseNativeFrameAfterFailure(Out, Native, NativeFrame);
            return Out;
        }
    }
    const bool bAnyReadback = Plan.ResolvedSettings.bRequireReadback ||
        Plan.DiagnosticBypass.Mode !=
            EOutputTransformDebugBypassMode::Disabled;
    if (bAnyReadback && Native &&
        !NativeStepSucceeded(Out, Native, NativeFrame,
            Native->CompleteReadback(Plan, NativeFrame),
            "OT-EXEC-READBACK", "Readback",
            "the exact same-frame native readback did not complete"))
        return Out;
    if (bAnyReadback)
    {
        Out.bNativeReadbackCompleted = Native != nullptr;
        Out.CpuReadbackInitiationCount =
            Plan.ResolvedSettings.bRequireReadback ? 1u : 0u;
        Out.DiagnosticCpuReadbackInitiationCount =
            Plan.DiagnosticBypass.Mode !=
                EOutputTransformDebugBypassMode::Disabled ? 1u : 0u;
    }
    if (Plan.ResolvedSettings.bRequirePresentation &&
        Bindings.bFailPresentation)
    {
        Fail(Out, EOutputTransformResult::TerminalFailed,
            "OT-EXEC-PRESENT", "Presentation",
            "requested presentation did not complete for the current frame");
        ReleaseNativeFrameAfterFailure(Out, Native, NativeFrame);
        return Out;
    }
    if (Plan.ResolvedSettings.bRequirePresentation && Native)
    {
        if (!NativeStepSucceeded(Out, Native, NativeFrame,
                Native->Present(Plan, NativeFrame), "OT-EXEC-PRESENT",
                "Presentation", "the completed native frame did not present"))
            return Out;
        Out.bNativePresented = true;
        Out.OutstandingTerminalOwnerCount =
            Native->GetOutstandingTerminalOwnerCount();
        if (Out.OutstandingTerminalOwnerCount != 0)
        {
            Fail(Out, EOutputTransformResult::TerminalFailed,
                "OT-EXEC-OWNER-LEAK", "Release",
                "terminal operation succeeded but retained a frame owner");
            return Out;
        }
    }

    Out.Result = EOutputTransformResult::Success;
    Out.FinalState = EOutputTransformPlanState::Published;
    Out.bFormalOutputPublished = true;
    Out.PublishedFormalOutputId = Plan.FormalOutputId;
    Out.Diagnostics.Add(EOutputTransformDiagnosticSeverity::Info,
        EOutputTransformResult::Success, "OT-EXEC-PUBLISHED", "Publish",
        "FormalOutput", "all requested terminal operations completed before publication");
    return Out;
}

} // namespace Stoner::Renderer
