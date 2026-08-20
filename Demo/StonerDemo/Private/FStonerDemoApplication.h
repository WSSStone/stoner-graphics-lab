#pragma once

#include "Core/CoreMinimal.h"
#include "FDemoBackendFactory.h"
#include "FDemoConfiguration.h"
#include "FDemoDiagnostics.h"
#include "FDemoValidationMonitor.h"
#include "RHI/FRHIShaderModuleDesc.h"

namespace Stoner::Demo
{

[[nodiscard]] Stoner::Renderer::FForwardFramePlan BuildTriangleFramePlan(
    Stoner::Core::uint32 Width,
    Stoner::Core::uint32 Height);

enum class EDemoLifecycleState
{
    Uninitialized,
    Initializing,
    Ready,
    Running,
    PresentationPaused,
    RecreatingPresentation,
    Stopping,
    Stopped,
    Failed
};

enum class EDemoValidationWindowCycleState
{
    Idle,
    WaitingForResize,
    WaitingForMinimize,
    WaitingForRestore
};

struct FDemoPresentationState
{
    Stoner::Core::uint64 Generation = 0;
    bool bInitialized = false;
    void Reset() noexcept { bInitialized = false; Generation = 0; }
};

struct FDemoTriangleResources
{
    bool bInitialized = false;
    void Reset() noexcept { bInitialized = false; }
};

struct FDemoFrameContext
{
    Stoner::Core::uint32 Slot = 0;
    bool bInFlight = false;
};

class FStonerDemoApplication
{
public:
    explicit FStonerDemoApplication(
        FDemoConfiguration InConfiguration,
        Stoner::Core::TSharedPtr<IDemoBackendFactory> InBackendFactory = nullptr);
    ~FStonerDemoApplication();

    [[nodiscard]] EDemoExitCode Run();
    [[nodiscard]] EDemoExitCode Initialize();
    [[nodiscard]] EDemoExitCode Shutdown();
    [[nodiscard]] EDemoLifecycleState GetLifecycleState() const noexcept { return LifecycleState; }
    [[nodiscard]] Stoner::Core::uint32 GetCompletedFrames() const noexcept { return CompletedFrames; }
    [[nodiscard]] std::size_t GetFrameContextCount() const noexcept { return FrameContexts.size(); }
    [[nodiscard]] const FDemoDiagnostics& GetDiagnostics() const noexcept { return Diagnostics; }
    [[nodiscard]] EDemoGraphicsBackend GetGraphicsBackend() const noexcept
    {
        return Configuration.GraphicsBackend;
    }
    void SetFailureInjection(EDemoStage Stage) noexcept { bHasFailureInjection = true; FailureInjectionStage = Stage; }
    [[nodiscard]] EDemoExitCode NotifyDrawableExtent(Stoner::Core::uint32 Width, Stoner::Core::uint32 Height, double NowMilliseconds);
    [[nodiscard]] EDemoExitCode NotifyPresentSuccess(double NowMilliseconds);
    [[nodiscard]] bool IsPresentationInitialized() const noexcept { return PresentationState.bInitialized; }
    [[nodiscard]] Stoner::Core::uint64 GetPresentationGeneration() const noexcept { return PresentationState.Generation; }
    [[nodiscard]] const Stoner::Core::TArray<double>& GetRecoveryDurationsMilliseconds() const noexcept { return RecoveryDurationsMilliseconds; }

private:
    [[nodiscard]] bool ValidateShaderPayloads();
    [[nodiscard]] bool LoadStrictCookedMetalShaderPayloads();
    [[nodiscard]] EDemoExitCode RunDeterministic();
    [[nodiscard]] EDemoExitCode RunNativeHeadless();
    [[nodiscard]] EDemoExitCode RunVisible();
    [[nodiscard]] EDemoExitCode DriveValidationWindowCycle();
    [[nodiscard]] bool ShouldInject(EDemoStage Stage, EDemoExitCode Code, const char* Subject);
    [[nodiscard]] EDemoExitCode FailInitialize(EDemoStage Stage,
        EDemoExitCode Code,
        const char* Subject,
        const char* Reason);

    FDemoConfiguration Configuration;
    EDemoLifecycleState LifecycleState = EDemoLifecycleState::Uninitialized;
    FDemoDiagnostics Diagnostics;
    FDemoValidationMonitor ValidationMonitor;
    FDemoPresentationState PresentationState;
    FDemoTriangleResources TriangleResources;
    Stoner::Core::TArray<FDemoFrameContext> FrameContexts;
    Stoner::Core::uint32 CompletedFrames = 0;
    bool bShutdownComplete = false;
    Stoner::Core::TSharedPtr<IDemoBackendFactory> BackendFactory;
    Stoner::Core::TUniquePtr<IDemoBackendRuntime> BackendRuntime;
    class FWindowHolder;
    std::unique_ptr<FWindowHolder> Window;
    bool bHasFailureInjection = false;
    EDemoStage FailureInjectionStage = EDemoStage::Configuration;
    double RecoveryStartMilliseconds = 0.0;
    double RunStartMilliseconds = 0.0;
    Stoner::Core::uint32 CurrentDrawableWidth = 0;
    Stoner::Core::uint32 CurrentDrawableHeight = 0;
    bool bFirstPresentRecorded = false;
    EDemoValidationWindowCycleState ValidationWindowCycleState =
        EDemoValidationWindowCycleState::Idle;
    Stoner::Core::uint32 ValidationWindowCyclesStarted = 0;
    Stoner::Core::uint32 ValidationWindowExpectedWidth = 0;
    Stoner::Core::uint32 ValidationWindowExpectedHeight = 0;
    Stoner::Core::TArray<double> RecoveryDurationsMilliseconds;
    Stoner::RHI::FRHIShaderModuleDesc TriangleVertexShader;
    Stoner::RHI::FRHIShaderModuleDesc TriangleFragmentShader;
    bool bTriangleShadersLoaded = false;
};

} // namespace Stoner::Demo
