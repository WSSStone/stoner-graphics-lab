#pragma once

#include "Renderer/FHDRPostProcessPipeline.h"
#include "RHI/FRHIPresentationFrame.h"
#include "RHI/FRHIResolvedPresentationState.h"
#include "RHI/IRHITexture.h"

namespace Stoner::Renderer
{

struct FOutputTransformNativeFrameBinding
{
    Stoner::RHI::FRHIPresentationFrame PresentationFrame;
    Stoner::RHI::FRHIResolvedPresentationState ResolvedState;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture> FormalOutput;
};

// Backend-neutral execution seam. Implementations own only RHI objects and
// synchronization; Renderer remains the sole owner of stage/color policy.
class IOutputTransformNativeFrameExecutor
{
public:
    virtual ~IOutputTransformNativeFrameExecutor() = default;

    virtual Stoner::RHI::ERHIResult Acquire(
        const FOutputTransformPlan& Plan,
        FOutputTransformNativeFrameBinding& OutFrame) = 0;
    virtual Stoner::RHI::ERHIResult RecordScheduleEvent(
        const FOutputTransformPlan& Plan,
        const FOutputTransformGraphDeclaration& Declaration,
        const FRenderGraphScheduleEvent& Event,
        const FOutputTransformNativeFrameBinding& Frame) = 0;
    virtual Stoner::RHI::ERHIResult Submit(
        const FOutputTransformPlan& Plan,
        const FOutputTransformNativeFrameBinding& Frame) = 0;
    virtual Stoner::RHI::ERHIResult WaitForCompletion(
        const FOutputTransformPlan& Plan,
        const FOutputTransformNativeFrameBinding& Frame) = 0;
    virtual Stoner::RHI::ERHIResult CompleteReadback(
        const FOutputTransformPlan& Plan,
        const FOutputTransformNativeFrameBinding& Frame) = 0;
    virtual Stoner::RHI::ERHIResult Present(
        const FOutputTransformPlan& Plan,
        const FOutputTransformNativeFrameBinding& Frame) = 0;
    virtual Stoner::RHI::ERHIResult ReleaseAfterFailure(
        const FOutputTransformNativeFrameBinding& Frame) noexcept = 0;
    [[nodiscard]] virtual Stoner::Core::uint32
    GetOutstandingTerminalOwnerCount() const noexcept = 0;
};

struct FOutputTransformExecutionBindings
{
    Stoner::Core::uint64 SceneColorExternalToken = 0;
    bool bFailTransientResolution = false;
    bool bFailSchedule = false;
    bool bFailReadback = false;
    bool bFailDiagnosticReadback = false;
    bool bFailPresentation = false;
    bool bRequireNativeExecution = false;
    IOutputTransformNativeFrameExecutor* NativeFrameExecutor = nullptr;
};

struct FOutputTransformExecutionResult
{
    EOutputTransformResult Result = EOutputTransformResult::InvalidGraph;
    EOutputTransformPlanState FinalState = EOutputTransformPlanState::Failed;
    bool bFormalOutputPublished = false;
    Stoner::Core::uint64 PublishedFormalOutputId = 0;
    Stoner::Core::uint64 FrameToken = 0;
    Stoner::Core::FString PlanFingerprint;
    Stoner::Core::uint32 ExecutedPassCount = 0;
    Stoner::Core::uint32 ExecutedFullscreenPassCount = 0;
    Stoner::Core::uint32 ExecutedFullImageVisitCount = 0;
    Stoner::Core::uint32 GpuReadbackCopyCount = 0;
    Stoner::Core::uint32 CpuReadbackInitiationCount = 0;
    bool bDiagnosticBypassProduced = false;
    FOutputTransformDiagnosticBypassRecord DiagnosticBypass;
    Stoner::Core::uint32 DiagnosticGpuReadbackCopyCount = 0;
    Stoner::Core::uint32 DiagnosticCpuReadbackInitiationCount = 0;
    Stoner::RHI::ERHIResult NativeResult =
        Stoner::RHI::ERHIResult::Success;
    Stoner::RHI::FRHIPresentationFrame PresentationFrame;
    Stoner::RHI::FRHIResolvedPresentationState ResolvedPresentationState;
    bool bNativeFrameAcquired = false;
    bool bNativeSubmitted = false;
    bool bNativeCompletionObserved = false;
    bool bNativeReadbackCompleted = false;
    bool bNativePresented = false;
    bool bNativeReleasedAfterFailure = false;
    Stoner::Core::uint32 OutstandingTerminalOwnerCount = 0;
    FOutputTransformDiagnosticLog Diagnostics;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return Result == EOutputTransformResult::Success &&
            bFormalOutputPublished &&
            FinalState == EOutputTransformPlanState::Published;
    }
};

class FOutputTransformExecutor
{
public:
    [[nodiscard]] FOutputTransformExecutionResult Execute(
        const FOutputTransformPlan& Plan,
        FRenderGraph& Graph,
        const FOutputTransformGraphDeclaration& Declaration,
        const FOutputTransformExecutionBindings& Bindings) const;
};

} // namespace Stoner::Renderer
