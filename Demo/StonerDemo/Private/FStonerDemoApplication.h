#pragma once

#include "Core/CoreMinimal.h"
#include "FDemoBackendFactory.h"
#include "FDemoConfiguration.h"
#include "FDemoDiagnostics.h"
#include "FDemoValidationMonitor.h"
#include "RHI/FRHIShaderModuleDesc.h"

namespace Stoner::Demo
{

class FProductionSubmissionHarness;
class FProductionContentSession;

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

struct FDemoProductionReadbackEvidence
{
    Stoner::Core::uint64 FrameToken = 0;
    Stoner::Core::FString Name;
    Stoner::Core::FString Digest;
    Stoner::Core::uint64 ByteCount = 0;
    Stoner::Core::uint32 Width = 0;
    Stoner::Core::uint32 Height = 0;
    Stoner::Core::uint32 RowPitchBytes = 0;
    Stoner::RHI::ERHIFormat Format = Stoner::RHI::ERHIFormat::Unknown;
    bool bNonBlank = false;
    Stoner::Core::TArray<Stoner::Core::uint8> Bytes;
};

struct FDemoProductionCapture
{
    Stoner::Core::uint64 FrameToken = 0;
    Stoner::Core::uint64 ExpectedFrameToken = 0;
    Stoner::Core::uint32 Cycle = 0;
    Stoner::Core::FString Name;
    Stoner::Core::FString Digest;
    Stoner::Core::uint32 Width = 0;
    Stoner::Core::uint32 Height = 0;
    Stoner::Core::uint32 RowPitchBytes = 0;
    Stoner::Core::uint64 CaptureStartedNs = 0;
    Stoner::RHI::ERHIFormat Format = Stoner::RHI::ERHIFormat::Unknown;
    bool bPresented = false;
    bool bWindowOnlyCapture = false;
    Stoner::Core::TArray<Stoner::Core::uint8> Bytes;
};

struct FDemoProductionExecutionInspection
{
    EDemoGraphicsBackend RequestedBackend = EDemoGraphicsBackend::Vulkan;
    EDemoGraphicsBackend ExecutedBackend = EDemoGraphicsBackend::Vulkan;
    EDemoRenderPath RenderPath = EDemoRenderPath::DeferredFull;
    Stoner::RHI::FRHIRuntimeSnapshot Runtime;
    Stoner::RHI::FRHIResolvedPresentationState ResolvedPresentationState;
    Stoner::Core::TArray<FDemoProductionReadbackEvidence> Readbacks;
    Stoner::Core::TArray<FDemoProductionCapture> Captures;
    Stoner::Core::TArray<FDemoProductionLifecycleSample> LifecycleSamples;
    FDemoProductionCapture AuthoritativeCapture;
    FDemoProductionCapture LastLifecyclePresentedCapture;
    Stoner::Core::uint64 AuthoritativeFrameToken = 0;
    Stoner::Core::uint64 LastLifecyclePresentedFrameToken = 0;
    Stoner::Core::uint64 FirstPresentedFrameToken = 0;
    Stoner::Core::uint64 LastPresentedFrameToken = 0;
    Stoner::Core::uint64 SettledPresentedFrameToken = 0;
    Stoner::Core::FString PresentationCapabilityDigest;
    Stoner::Core::FString FormalOutputReadbackDigest;
    Stoner::Core::FString SnapshotFingerprint;
    Stoner::Core::FString UniformFingerprint;
    Stoner::Core::FString ShaderFingerprint;
    Stoner::Core::FString PipelineFingerprint;
    Stoner::Core::FString DescriptorFingerprint;
    Stoner::Core::FString DeviceFingerprint;
    Stoner::Core::uint32 CaptureCount = 0;
    Stoner::Core::uint32 FinalOutputCaptureCount = 0;
    Stoner::Core::uint32 ForwardColorCaptureCount = 0;
    Stoner::Core::uint32 PresentedFinalOutputCaptureCount = 0;
    Stoner::Core::uint32 CompletedCycles = 0;
    Stoner::Core::uint32 RssComparisonBackendRecycleCount = 0;
    bool bCookedEnvelopeAuthenticationReused = false;
    bool bCookedGenerationValidationReused = false;
    bool bSubmissionCompleted = false;
    bool bSynchronizationCompleted = false;
    bool bLifecyclePassed = false;

    [[nodiscard]] bool ProvesNativeExecution() const noexcept
    {
        return RequestedBackend == ExecutedBackend &&
            Runtime.ProvesNativeExecution() && bSubmissionCompleted &&
            bSynchronizationCompleted && !Readbacks.empty() &&
            CompletedCycles > 0 && bLifecyclePassed;
    }
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
    [[nodiscard]] const FDemoProductionExecutionInspection&
        GetProductionExecutionInspection() const noexcept
    {
        return ProductionExecutionInspection;
    }
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
    [[nodiscard]] EDemoExitCode InitializeProductionContent();
    [[nodiscard]] FDemoProductionLifecycleCounters
        ReleaseProductionContentCycle();
    [[nodiscard]] bool ShouldRecycleProductionBackendForRssComparison()
        const noexcept;
    [[nodiscard]] bool ShouldPrimeProductionBackendForRssComparison()
        const noexcept;
    [[nodiscard]] EDemoExitCode SuspendProductionBackendForRssComparison();
    [[nodiscard]] EDemoExitCode ResumeProductionBackendAfterRssComparison();
    [[nodiscard]] EDemoExitCode RunProductionContent();
    [[nodiscard]] EDemoExitCode RunProductionCameraPreview();
    [[nodiscard]] bool WriteConfiguredOutputTransformNativeProbe();
    void RecordProductionCapture(FDemoProductionCapture Capture);
    [[nodiscard]] bool PresentProductionCaptureWithRecovery(
        FDemoProductionCapture& Capture,
        const Core::TSharedPtr<RHI::IRHITexture>& FormalOutput,
        Core::uint64 FrameToken,
        FDemoProductionPresentationResult& PresentationScratch);
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
    Renderer::FOutputTransformSettings ProductionOutputSettings;
    Renderer::FResolvedOutputTransformSettings
        ProductionResolvedOutputSettings;
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
    Stoner::Core::TUniquePtr<FProductionSubmissionHarness>
        ProductionSubmissionHarness;
    class FWindowHolder;
    std::unique_ptr<FWindowHolder> Window;
    class FProductionContentRuntime;
    std::unique_ptr<FProductionContentRuntime> ProductionRuntime;
    std::unique_ptr<FProductionContentSession> ProductionContentSession;
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
    FDemoProductionExecutionInspection ProductionExecutionInspection;
    Stoner::RHI::FRHIRuntimeSnapshot ProductionRuntimeBaseline;
};

} // namespace Stoner::Demo
