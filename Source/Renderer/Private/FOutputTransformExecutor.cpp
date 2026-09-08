#include "Renderer/FOutputTransformExecutor.h"

#include <algorithm>
#include <atomic>

namespace Stoner::Renderer
{

class FOutputTransformPreviewTicketState
{
public:
    Stoner::Core::uint64 TicketId = 0;
    FOutputTransformPlan Plan;
    FOutputTransformNativeFrameBinding NativeFrame;
    Stoner::Core::TSharedPtr<IOutputTransformNativeFrameExecutor>
        NativeOwner;
    FOutputTransformExecutionResult Execution;
    FOutputTransformDiagnosticLog Diagnostics;
    EOutputTransformPreviewState State =
        EOutputTransformPreviewState::Invalid;
    EOutputTransformResult Result = EOutputTransformResult::InvalidGraph;
    Stoner::RHI::ERHIResult NativeResult =
        Stoner::RHI::ERHIResult::Failed;
    EOutputTransformResult FirstFailureResult =
        EOutputTransformResult::Success;
    Stoner::RHI::ERHIResult FirstFailureNativeResult =
        Stoner::RHI::ERHIResult::Success;
    bool bHasFailure = false;
    bool bSubmissionAccepted = false;
    bool bCompletionObserved = false;
    bool bRetired = false;
    bool bReleaseCompleted = false;
};

bool FOutputTransformPreviewTicket::IsValid() const noexcept
{
    return State && State->TicketId != 0 &&
        State->State != EOutputTransformPreviewState::Invalid &&
        State->State != EOutputTransformPreviewState::Retired;
}

Stoner::Core::uint64 FOutputTransformPreviewTicket::GetTicketId()
    const noexcept
{
    return State ? State->TicketId : 0;
}

Stoner::Core::uint64 FOutputTransformPreviewTicket::GetFrameToken()
    const noexcept
{
    return State ? State->Execution.FrameToken : 0;
}

EOutputTransformPreviewState FOutputTransformPreviewTicket::GetState()
    const noexcept
{
    return State ? State->State : EOutputTransformPreviewState::Invalid;
}

namespace
{

std::atomic<Stoner::Core::uint64> GNextPreviewTicketId{1};

[[nodiscard]] EOutputTransformResult MapNativePreviewResult(
    Stoner::RHI::ERHIResult Result) noexcept
{
    switch (Result)
    {
    case Stoner::RHI::ERHIResult::Unsupported:
    case Stoner::RHI::ERHIResult::Unavailable:
    case Stoner::RHI::ERHIResult::NotReady:
    case Stoner::RHI::ERHIResult::ResizeRequired:
        return EOutputTransformResult::Unsupported;
    case Stoner::RHI::ERHIResult::InvalidState:
        return EOutputTransformResult::InvalidBinding;
    case Stoner::RHI::ERHIResult::Timeout:
    case Stoner::RHI::ERHIResult::Failed:
        return EOutputTransformResult::TerminalFailed;
    case Stoner::RHI::ERHIResult::Success:
        return EOutputTransformResult::Success;
    }
    return EOutputTransformResult::TerminalFailed;
}

void AddPreviewDiagnostic(FOutputTransformPreviewTicketState& State,
    EOutputTransformDiagnosticSeverity Severity,
    EOutputTransformResult Result,
    const char* Code,
    const char* Stage,
    const char* Subject,
    const char* Message)
{
    State.Diagnostics.Add(Severity, Result, Code, Stage, Subject, Message);
    State.Execution.Diagnostics.Add(Severity, Result, Code, Stage, Subject,
        Message);
}

[[nodiscard]] FOutputTransformPreviewResult MakePreviewResult(
    const FOutputTransformPreviewTicketState& State)
{
    FOutputTransformPreviewResult Out;
    Out.Result = State.Result;
    Out.State = State.State;
    Out.NativeResult = State.NativeResult;
    Out.TicketId = State.TicketId;
    Out.FrameToken = State.Execution.FrameToken;
    Out.bQueued = State.bSubmissionAccepted;
    Out.bRenderCompleted = State.bCompletionObserved;
    Out.bRetired = State.bRetired;
    Out.bFormalOutputPublished = State.Execution.bFormalOutputPublished;
    Out.OutstandingOwnerCount = State.Execution.OutstandingTerminalOwnerCount;
    Out.Execution = State.Execution;
    Out.Diagnostics = State.Diagnostics;
    return Out;
}

[[nodiscard]] FOutputTransformPreviewResult MakePreviewFailure(
    EOutputTransformResult Result,
    Stoner::RHI::ERHIResult NativeResult,
    const char* Code,
    const char* Stage,
    const char* Subject,
    const char* Message)
{
    FOutputTransformPreviewResult Out;
    Out.Result = Result;
    Out.NativeResult = NativeResult;
    Out.Execution.Result = Result;
    Out.Execution.FinalState = EOutputTransformPlanState::Failed;
    Out.Execution.NativeResult = NativeResult;
    Out.Diagnostics.Add(EOutputTransformDiagnosticSeverity::Error, Result,
        Code, Stage, Subject, Message);
    Out.Execution.Diagnostics.Merge(Out.Diagnostics);
    return Out;
}

void MarkPreviewFailure(FOutputTransformPreviewTicketState& State,
    EOutputTransformResult Result,
    Stoner::RHI::ERHIResult NativeResult,
    const char* Code,
    const char* Stage,
    const char* Subject,
    const char* Message)
{
    if (!State.bHasFailure)
    {
        State.bHasFailure = true;
        State.FirstFailureResult = Result;
        State.FirstFailureNativeResult = NativeResult;
    }
    State.Result = State.FirstFailureResult;
    State.NativeResult = State.FirstFailureNativeResult;
    State.Execution.Result = State.FirstFailureResult;
    State.Execution.FinalState = EOutputTransformPlanState::Failed;
    State.Execution.bFormalOutputPublished = false;
    State.Execution.PublishedFormalOutputId = 0;
    State.Execution.NativeResult = State.FirstFailureNativeResult;
    State.State = EOutputTransformPreviewState::Failed;
    AddPreviewDiagnostic(State, EOutputTransformDiagnosticSeverity::Error,
        Result, Code, Stage, Subject, Message);
}

void RefreshPreviewOwnerCount(FOutputTransformPreviewTicketState& State)
{
    if (State.NativeOwner)
    {
        State.Execution.OutstandingTerminalOwnerCount =
            State.NativeOwner->GetOutstandingTerminalOwnerCount();
    }
}

[[nodiscard]] bool IsPreviewRecordablePlan(
    const FOutputTransformPlan& Plan,
    const FOutputTransformGraphDeclaration& Declaration,
    FOutputTransformPreviewResult& Out)
{
    if (!Plan.IsValid() || Plan.ExecutionPurpose !=
            EFrameExecutionPurpose::InteractivePreview ||
        Plan.ReadbackSelection != EFrameReadbackSelection::None ||
        Plan.ResolvedSettings.bRequireReadback ||
        Declaration.GpuReadbackCopyCount != 0 ||
        Declaration.DiagnosticReadbackCopyCount != 0)
    {
        Out = MakePreviewFailure(EOutputTransformResult::InvalidBinding,
            Stoner::RHI::ERHIResult::InvalidState, "OT-PREVIEW-POLICY",
            "Record", "Plan",
            "preview requires InteractivePreview, None readback, and a zero-readback graph");
        return false;
    }
    return true;
}

void MarkPreviewRetired(FOutputTransformPreviewTicketState& State);

void ReleaseUnsubmittedPreview(
    FOutputTransformPreviewTicketState& State)
{
    if (!State.NativeOwner || State.bSubmissionAccepted ||
        State.bReleaseCompleted)
    {
        return;
    }
    const Stoner::RHI::ERHIResult Release =
        State.NativeOwner->ReleasePreviewAfterFailure(State.NativeFrame);
    if (!State.bHasFailure)
    {
        State.NativeResult = Release;
        State.Execution.NativeResult = Release;
    }
    if (Release == Stoner::RHI::ERHIResult::Success)
    {
        State.Execution.bNativeReleasedAfterFailure = true;
        State.bReleaseCompleted = true;
        // The dedicated release result covers this recording's owner. The
        // count may also include an independent presentation lease or an
        // adjacent slot, so it is retained as diagnostics and does not gate
        // retiring this ticket's shared references.
        MarkPreviewRetired(State);
        return;
    }
    if (Release != Stoner::RHI::ERHIResult::NotReady)
    {
        MarkPreviewFailure(State, MapNativePreviewResult(Release), Release,
            "OT-PREVIEW-CANCEL", "Retire", "Preview",
            "preview cancellation failed; native ownership remains retained");
    }
    RefreshPreviewOwnerCount(State);
}

void MarkPreviewRetired(FOutputTransformPreviewTicketState& State)
{
    State.bRetired = true;
    State.State = EOutputTransformPreviewState::Retired;
    State.Execution.FinalState = EOutputTransformPlanState::Released;
    State.Execution.bFormalOutputPublished = false;
    State.Execution.PublishedFormalOutputId = 0;
    RefreshPreviewOwnerCount(State);
    // Native preview adapters own any in-flight presentation lease
    // independently. Once this render ticket is retired, the ticket must not
    // retain the executor or borrowed target itself.
    State.NativeFrame.FormalOutput.reset();
    State.NativeOwner.reset();
}

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

[[nodiscard]] bool IsExactPreviewNativeBinding(
    const FOutputTransformPlan& Plan,
    const FOutputTransformNativeFrameBinding& Frame) noexcept
{
    if (!IsExactNativeBinding(Plan, Frame))
    {
        return false;
    }
    const auto& State = Frame.ResolvedState;
    const auto& Settings = Plan.ResolvedSettings;
    return State.ReferenceWhiteNits == Settings.ReferenceWhiteNits &&
        State.TargetPeakNits == Settings.TargetPeakNits;
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

const char* ToString(EOutputTransformPreviewState State) noexcept
{
    switch (State)
    {
    case EOutputTransformPreviewState::Invalid: return "Invalid";
    case EOutputTransformPreviewState::Recorded: return "Recorded";
    case EOutputTransformPreviewState::Queued: return "Queued";
    case EOutputTransformPreviewState::RenderCompleted:
        return "RenderCompleted";
    case EOutputTransformPreviewState::Failed: return "Failed";
    case EOutputTransformPreviewState::Retired: return "Retired";
    }
    return "Unknown";
}

FOutputTransformExecutionResult FOutputTransformExecutor::Execute(
    const FOutputTransformPlan& Plan,
    FRenderGraph& Graph,
    const FOutputTransformGraphDeclaration& Declaration,
    const FOutputTransformExecutionBindings& Bindings) const
{
    FOutputTransformExecutionResult Out;
    Out.FrameToken = Plan.FrameToken;
    Out.PlanFingerprint = Plan.PlanFingerprint;
    if (Plan.ExecutionPurpose != EFrameExecutionPurpose::FormalValidation)
    {
        Fail(Out, EOutputTransformResult::InvalidBinding,
            "OT-EXEC-PURPOSE", "Binding",
            "the synchronous formal executor rejects interactive preview plans");
        return Out;
    }
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
    if (Plan.TerminalUI && Native && !Native->SupportsTerminalUI())
    {
        Out.NativeResult = Stoner::RHI::ERHIResult::Unsupported;
        Fail(Out,EOutputTransformResult::Unsupported,"OT-UI-NATIVE-UNSUPPORTED","Binding",
            "native executor cannot record terminal UI composition");
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

FOutputTransformPreviewResult FOutputTransformExecutor::RecordPreview(
    const FOutputTransformPlan& Plan,
    FRenderGraph& Graph,
    const FOutputTransformGraphDeclaration& Declaration,
    const FOutputTransformExecutionBindings& Bindings,
    FOutputTransformPreviewTicket& OutTicket) const
{
    // A ticket is one-shot. Do not overwrite an existing shared state because
    // another copy may still be retaining a native owner.
    if (OutTicket.State)
    {
        return MakePreviewFailure(EOutputTransformResult::InvalidBinding,
            Stoner::RHI::ERHIResult::InvalidState, "OT-PREVIEW-TICKET",
            "Record", "Ticket",
            "a preview ticket can be recorded only once");
    }

    FOutputTransformPreviewResult ValidationResult;
    if (!IsPreviewRecordablePlan(Plan, Declaration, ValidationResult))
    {
        return ValidationResult;
    }
    if (Graph.GetState() != ERenderGraphState::Compiled ||
        !Declaration.IsValid() ||
        !FHDRPostProcessPipeline().ValidateOutputGraph(
            Graph, Plan, Declaration, &ValidationResult.Diagnostics))
    {
        ValidationResult = MakePreviewFailure(
            EOutputTransformResult::InvalidGraph,
            Stoner::RHI::ERHIResult::InvalidState, "OT-PREVIEW-GRAPH",
            "Record", "Graph",
            "preview requires the exact validated compiled zero-readback graph");
        return ValidationResult;
    }

    const FRenderGraphResourceRecord* SceneColor =
        Graph.FindResource(Declaration.SceneColor);
    if (!SceneColor ||
        (SceneColor->Desc.Ownership == ERenderGraphResourceOwnership::Imported &&
            Bindings.SceneColorExternalToken == 0))
    {
        return MakePreviewFailure(EOutputTransformResult::InvalidBinding,
            Stoner::RHI::ERHIResult::InvalidState,
            "OT-PREVIEW-SCENE-COLOR", "Record", "SceneColor",
            "the preview SceneColor requires one nonzero external token");
    }

    // The shared owner is deliberately required. A raw formal executor can
    // implement a blocking wait or destroy its command resources at return,
    // neither of which is a valid asynchronous preview contract.
    if (!Bindings.PreviewFrameExecutor)
    {
        return MakePreviewFailure(EOutputTransformResult::Unsupported,
            Stoner::RHI::ERHIResult::Unsupported, "OT-PREVIEW-ASYNC",
            "Acquire", "NativeExecutor",
            "preview requires a shared executor with asynchronous admission");
    }

    if (Plan.TerminalUI && !Bindings.PreviewFrameExecutor->SupportsTerminalUI())
        return MakePreviewFailure(EOutputTransformResult::Unsupported,Stoner::RHI::ERHIResult::Unsupported,
            "OT-UI-NATIVE-UNSUPPORTED","Binding","TerminalUI","native executor cannot record terminal UI composition");

    if (Plan.HasDiagnosticWidget() && !Bindings.PreviewFrameExecutor->SupportsDiagnosticWidgets())
        return MakePreviewFailure(EOutputTransformResult::Unsupported,Stoner::RHI::ERHIResult::Unsupported,
            "OT-WIDGET-NATIVE-UNSUPPORTED","Binding","DiagnosticWidget","native executor cannot record GPU diagnostic widgets");

    const Stoner::Core::TSharedPtr<FOutputTransformPreviewTicketState> State =
        Stoner::Core::MakeShared<FOutputTransformPreviewTicketState>();
    State->TicketId = GNextPreviewTicketId.fetch_add(1,
        std::memory_order_relaxed);
    if (State->TicketId == 0)
    {
        return MakePreviewFailure(EOutputTransformResult::TerminalFailed,
            Stoner::RHI::ERHIResult::Failed, "OT-PREVIEW-TICKET-WRAP",
            "Record", "Ticket", "preview ticket identity exhausted");
    }
    State->Plan = Plan;
    State->NativeOwner = Bindings.PreviewFrameExecutor;
    State->Execution.FrameToken = Plan.FrameToken;
    State->Execution.PlanFingerprint = Plan.PlanFingerprint;
    State->Execution.Result = EOutputTransformResult::Success;
    State->Execution.FinalState = EOutputTransformPlanState::Executing;
    State->Execution.bFormalOutputPublished = false;
    State->Execution.PublishedFormalOutputId = 0;
    State->State = EOutputTransformPreviewState::Recorded;
    State->Result = EOutputTransformResult::Success;
    State->NativeResult = Stoner::RHI::ERHIResult::Success;

    const Stoner::RHI::ERHIResult Acquire = State->NativeOwner->AcquirePreview(
        Plan, State->NativeFrame);
    State->NativeResult = Acquire;
    State->Execution.NativeResult = Acquire;
    if (Acquire != Stoner::RHI::ERHIResult::Success)
    {
        // AcquirePreview may report NotReady without producing a frame. It
        // cannot be turned into a ticket because there is no target/token to
        // poll here; the caller retries acquisition at the same frame slot.
        // Preserve any native pending-owner diagnostic even though the
        // ordinary no-target retry does not publish a ticket.
        RefreshPreviewOwnerCount(*State);
        FOutputTransformPreviewResult Out = MakePreviewFailure(
            MapNativePreviewResult(Acquire), Acquire,
            "OT-PREVIEW-ACQUIRE", "Acquire", "NativeExecutor",
            "preview asynchronous frame admission did not complete");
        Out.FrameToken = Plan.FrameToken;
        Out.Execution.FrameToken = Plan.FrameToken;
        Out.OutstandingOwnerCount =
            State->Execution.OutstandingTerminalOwnerCount;
        Out.Execution.OutstandingTerminalOwnerCount =
            State->Execution.OutstandingTerminalOwnerCount;
        return Out;
    }
    // Publish the shared state before any frame metadata copies, graph
    // execution, or diagnostic allocation. If a later operation throws, the
    // caller still owns a ticket that can cancel/retire the acquired frame.
    OutTicket.State = State;
    State->Execution.bNativeFrameAcquired = true;
    State->Execution.PresentationFrame = State->NativeFrame.PresentationFrame;
    State->Execution.ResolvedPresentationState = State->NativeFrame.ResolvedState;
    RefreshPreviewOwnerCount(*State);
    if (!IsExactPreviewNativeBinding(Plan, State->NativeFrame))
    {
        MarkPreviewFailure(*State, EOutputTransformResult::InvalidBinding,
            Stoner::RHI::ERHIResult::InvalidState,
            "OT-PREVIEW-NATIVE-BINDING", "Binding", "NativeExecutor",
            "acquired preview target does not match frame token extent format colorspace encoding or usage");
        ReleaseUnsubmittedPreview(*State);
        return MakePreviewResult(*State);
    }

    FRenderGraphExecutionDesc ExecuteDesc;
    if (SceneColor->Desc.Ownership == ERenderGraphResourceOwnership::Imported)
    {
        ExecuteDesc.ImportedResources.push_back(
            {Declaration.SceneColor, Bindings.SceneColorExternalToken});
    }
    ExecuteDesc.bFailTransientResolution = Bindings.bFailTransientResolution;
    ExecuteDesc.ScheduleVisitor =
        [&State, &Graph, &Declaration, &Bindings, &Plan](
            const FRenderGraphScheduleEvent& Event) {
            if (Event.Kind != ERenderGraphScheduleEventKind::Pass)
            {
                return ERenderGraphResult::Success;
            }
            if (Bindings.bFailSchedule)
            {
                return ERenderGraphResult::ExecutionFailed;
            }
            const Stoner::RHI::ERHIResult Recorded =
                State->NativeOwner->RecordScheduleEvent(
                    Plan, Declaration, Event, State->NativeFrame);
            if (Recorded != Stoner::RHI::ERHIResult::Success)
            {
                State->NativeResult = Recorded;
                State->Execution.NativeResult = State->NativeResult;
                return ERenderGraphResult::ExecutionFailed;
            }
            ++State->Execution.ExecutedPassCount;
            if (ContainsPass(Declaration, Event.PassIndex) &&
                Graph.GetPasses()[Event.PassIndex].Desc.Type ==
                    ERenderGraphPassType::Graphics)
            {
                ++State->Execution.ExecutedFullscreenPassCount;
                ++State->Execution.ExecutedFullImageVisitCount;
            }
            return ERenderGraphResult::Success;
        };

    const ERenderGraphResult GraphResult = Graph.Execute(ExecuteDesc);
    if (GraphResult != ERenderGraphResult::Success)
    {
        MarkPreviewFailure(*State, EOutputTransformResult::ExecutionFailed,
            State->NativeResult == Stoner::RHI::ERHIResult::Success
                ? Stoner::RHI::ERHIResult::Failed : State->NativeResult,
            "OT-PREVIEW-SCHEDULE", "Execution", "Graph",
            "compiled preview schedule failed before submission");
        ReleaseUnsubmittedPreview(*State);
        return MakePreviewResult(*State);
    }
    State->Execution.FinalState = EOutputTransformPlanState::Completed;
    State->Execution.GpuReadbackCopyCount = 0;
    State->Execution.CpuReadbackInitiationCount = 0;
    State->Execution.DiagnosticGpuReadbackCopyCount = 0;
    State->Execution.DiagnosticCpuReadbackInitiationCount = 0;
    AddPreviewDiagnostic(*State, EOutputTransformDiagnosticSeverity::Info,
        EOutputTransformResult::Success, "OT-PREVIEW-RECORDED", "Record",
        "Preview", "zero-readback preview command sequence recorded");
    return MakePreviewResult(*State);
}

FOutputTransformPreviewResult FOutputTransformExecutor::SubmitPreview(
    FOutputTransformPreviewTicket& Ticket) const
{
    if (!Ticket.State || Ticket.State->TicketId == 0)
    {
        return MakePreviewFailure(EOutputTransformResult::InvalidBinding,
            Stoner::RHI::ERHIResult::InvalidState, "OT-PREVIEW-TICKET",
            "Submit", "Ticket", "preview submit requires a valid ticket");
    }
    FOutputTransformPreviewTicketState& State = *Ticket.State;
    if (State.State != EOutputTransformPreviewState::Recorded)
    {
        return MakePreviewFailure(EOutputTransformResult::InvalidBinding,
            Stoner::RHI::ERHIResult::InvalidState, "OT-PREVIEW-STATE",
            "Submit", "Ticket",
            "preview submit accepts one recorded ticket exactly once");
    }
    bool bSubmitted = false;
    const Stoner::RHI::ERHIResult Submit = State.NativeOwner
        ? State.NativeOwner->SubmitPreview(State.Plan, State.NativeFrame,
            bSubmitted)
        : Stoner::RHI::ERHIResult::Unsupported;
    State.bSubmissionAccepted = bSubmitted;
    State.Execution.bNativeSubmitted = bSubmitted;
    if (!State.bHasFailure)
    {
        State.NativeResult = Submit;
        State.Execution.NativeResult = Submit;
    }
    if (Submit == Stoner::RHI::ERHIResult::Success && bSubmitted)
    {
        State.State = EOutputTransformPreviewState::Queued;
        State.Result = EOutputTransformResult::Success;
        AddPreviewDiagnostic(State, EOutputTransformDiagnosticSeverity::Info,
            EOutputTransformResult::Success, "OT-PREVIEW-QUEUED", "Submit",
            "Preview", "preview submission was accepted and remains pending");
        RefreshPreviewOwnerCount(State);
        return MakePreviewResult(State);
    }

    const EOutputTransformResult Mapped = MapNativePreviewResult(Submit);
    if (!bSubmitted &&
        (Submit == Stoner::RHI::ERHIResult::NotReady ||
            Submit == Stoner::RHI::ERHIResult::Unavailable ||
            Submit == Stoner::RHI::ERHIResult::ResizeRequired))
    {
        // A busy slot or transition is a retryable admission result. Keep the
        // recorded command sequence and its acquired owner in place.
        State.Result = Mapped;
        AddPreviewDiagnostic(State, EOutputTransformDiagnosticSeverity::Info,
            Mapped, "OT-PREVIEW-ADMISSION-PENDING", "Submit", "Preview",
            "preview submission was not accepted and may be retried");
        RefreshPreviewOwnerCount(State);
        return MakePreviewResult(State);
    }

    if (Submit == Stoner::RHI::ERHIResult::Success && !bSubmitted)
    {
        MarkPreviewFailure(State, EOutputTransformResult::TerminalFailed,
            Stoner::RHI::ERHIResult::InvalidState,
            "OT-PREVIEW-ADMISSION-CONTRACT", "Submit", "Preview",
            "preview executor returned success without confirming submission");
    }
    else if (bSubmitted)
    {
        // A failed result with bSubmitted=true means native work may still be
        // in flight; retain the owner and require an explicit completion poll.
        MarkPreviewFailure(State, Mapped, Submit, "OT-PREVIEW-SUBMIT",
            "Submit", "Preview",
            "preview submission failed after native acceptance; owner remains retained");
    }
    else
    {
        MarkPreviewFailure(State, Mapped, Submit, "OT-PREVIEW-SUBMIT",
            "Submit", "Preview",
            "preview submission was rejected before native acceptance");
    }
    State.State = EOutputTransformPreviewState::Failed;
    if (!bSubmitted)
    {
        ReleaseUnsubmittedPreview(State);
    }
    else
    {
        RefreshPreviewOwnerCount(State);
    }
    return MakePreviewResult(State);
}

FOutputTransformPreviewResult FOutputTransformExecutor::PollPreview(
    FOutputTransformPreviewTicket& Ticket) const
{
    if (!Ticket.State || Ticket.State->TicketId == 0)
    {
        return MakePreviewFailure(EOutputTransformResult::InvalidBinding,
            Stoner::RHI::ERHIResult::InvalidState, "OT-PREVIEW-TICKET",
            "Poll", "Ticket", "preview poll requires a valid ticket");
    }
    FOutputTransformPreviewTicketState& State = *Ticket.State;
    if (State.State == EOutputTransformPreviewState::Retired ||
        State.State == EOutputTransformPreviewState::Invalid ||
        !State.bSubmissionAccepted)
    {
        return MakePreviewFailure(EOutputTransformResult::InvalidBinding,
            Stoner::RHI::ERHIResult::InvalidState, "OT-PREVIEW-STATE",
            "Poll", "Ticket",
            "preview poll requires an accepted submission with retained ownership");
    }
    if (State.bCompletionObserved)
    {
        return MakePreviewResult(State);
    }
    bool bCompleted = false;
    const Stoner::RHI::ERHIResult Poll = State.NativeOwner
        ? State.NativeOwner->PollPreview(State.Plan, State.NativeFrame,
            bCompleted)
        : Stoner::RHI::ERHIResult::Unsupported;
    if (!State.bHasFailure)
    {
        State.NativeResult = Poll;
        State.Execution.NativeResult = Poll;
    }
    if (Poll == Stoner::RHI::ERHIResult::NotReady)
    {
        AddPreviewDiagnostic(State, EOutputTransformDiagnosticSeverity::Info,
            EOutputTransformResult::Success, "OT-PREVIEW-PENDING", "Poll",
            "Preview", "preview render completion remains pending");
        RefreshPreviewOwnerCount(State);
        return MakePreviewResult(State);
    }

    if (Poll == Stoner::RHI::ERHIResult::Success && bCompleted)
    {
        State.bCompletionObserved = true;
        const bool bHadFailure = State.bHasFailure;
        State.State = bHadFailure
            ? EOutputTransformPreviewState::Failed
            : EOutputTransformPreviewState::RenderCompleted;
        if (!bHadFailure)
        {
            State.Result = EOutputTransformResult::Success;
            State.Execution.Result = EOutputTransformResult::Success;
        }
        State.Execution.FinalState = EOutputTransformPlanState::Completed;
        State.Execution.bNativeCompletionObserved = true;
        State.Execution.bFormalOutputPublished = false;
        State.Execution.PublishedFormalOutputId = 0;
        AddPreviewDiagnostic(State, EOutputTransformDiagnosticSeverity::Info,
            State.Result, "OT-PREVIEW-COMPLETED",
            "Poll", "Preview",
            bHadFailure
                ? "preview completion was observed after an earlier failure; failure identity is retained"
                : "preview render completion was observed; no formal output was published");
        RefreshPreviewOwnerCount(State);
        return MakePreviewResult(State);
    }

    if (Poll == Stoner::RHI::ERHIResult::Success && !bCompleted)
    {
        MarkPreviewFailure(State, EOutputTransformResult::TerminalFailed,
            Stoner::RHI::ERHIResult::InvalidState,
            "OT-PREVIEW-POLL-CONTRACT", "Poll", "Preview",
            "preview executor returned success without independent completion proof");
        RefreshPreviewOwnerCount(State);
        return MakePreviewResult(State);
    }

    // A terminal native failure may still mean the fence has completed. The
    // native seam reports that distinction through bCompleted; retain the
    // frame until the caller explicitly retires it.
    State.bCompletionObserved = bCompleted;
    MarkPreviewFailure(State, MapNativePreviewResult(Poll), Poll,
        "OT-PREVIEW-POLL", "Poll", "Preview",
        bCompleted
            ? "preview completion reported a terminal native failure"
            : "preview polling failed without independent completion proof");
    State.Execution.bNativeCompletionObserved = State.bCompletionObserved;
    RefreshPreviewOwnerCount(State);
    return MakePreviewResult(State);
}

FOutputTransformPreviewResult FOutputTransformExecutor::RetirePreview(
    FOutputTransformPreviewTicket& Ticket) const
{
    if (!Ticket.State || Ticket.State->TicketId == 0)
    {
        return MakePreviewFailure(EOutputTransformResult::InvalidBinding,
            Stoner::RHI::ERHIResult::InvalidState, "OT-PREVIEW-TICKET",
            "Retire", "Ticket", "preview retire requires a valid ticket");
    }
    FOutputTransformPreviewTicketState& State = *Ticket.State;
    if (State.State == EOutputTransformPreviewState::Retired)
    {
        return MakePreviewFailure(EOutputTransformResult::InvalidBinding,
            Stoner::RHI::ERHIResult::InvalidState, "OT-PREVIEW-RETIRED",
            "Retire", "Ticket", "preview ticket was already retired");
    }
    if (!State.bSubmissionAccepted)
    {
        ReleaseUnsubmittedPreview(State);
        if (State.bRetired)
        {
            if (!State.bHasFailure)
            {
                State.Result = EOutputTransformResult::Success;
                State.NativeResult = Stoner::RHI::ERHIResult::Success;
                State.Execution.Result = EOutputTransformResult::Success;
            }
            AddPreviewDiagnostic(State,
                EOutputTransformDiagnosticSeverity::Info,
                State.Result, "OT-PREVIEW-CANCELLED",
                "Retire", "Preview",
                "unsubmitted preview recording was cancelled and retired");
        }
        return MakePreviewResult(State);
    }
    if (!State.bCompletionObserved)
    {
        if (!State.bHasFailure)
        {
            State.NativeResult = Stoner::RHI::ERHIResult::NotReady;
            State.Execution.NativeResult = State.NativeResult;
        }
        AddPreviewDiagnostic(State, EOutputTransformDiagnosticSeverity::Info,
            State.Result, "OT-PREVIEW-RETIRE-PENDING", "Retire", "Preview",
            "preview owner cannot retire before native completion is observed");
        RefreshPreviewOwnerCount(State);
        return MakePreviewResult(State);
    }
    const Stoner::RHI::ERHIResult Retire = State.NativeOwner
        ? State.NativeOwner->RetirePreview(State.Plan, State.NativeFrame)
        : Stoner::RHI::ERHIResult::Unsupported;
    if (Retire == Stoner::RHI::ERHIResult::NotReady)
    {
        if (!State.bHasFailure)
        {
            State.NativeResult = Retire;
            State.Execution.NativeResult = Retire;
        }
        RefreshPreviewOwnerCount(State);
        return MakePreviewResult(State);
    }
    if (Retire != Stoner::RHI::ERHIResult::Success)
    {
        MarkPreviewFailure(State, MapNativePreviewResult(Retire), Retire,
            "OT-PREVIEW-RETIRE", "Retire", "Preview",
            "preview native owner did not retire; it remains retained");
        RefreshPreviewOwnerCount(State);
        return MakePreviewResult(State);
    }
    RefreshPreviewOwnerCount(State);
    MarkPreviewRetired(State);
    if (!State.bHasFailure)
    {
        State.Result = EOutputTransformResult::Success;
        State.NativeResult = Stoner::RHI::ERHIResult::Success;
        State.Execution.NativeResult = State.NativeResult;
    }
    AddPreviewDiagnostic(State, EOutputTransformDiagnosticSeverity::Info,
        State.Result, "OT-PREVIEW-RETIRED", "Retire", "Preview",
        State.bHasFailure
            ? "preview native owner retired after completion; first failure identity is retained"
            : "preview native owner retired after completion");
    return MakePreviewResult(State);
}

} // namespace Stoner::Renderer
