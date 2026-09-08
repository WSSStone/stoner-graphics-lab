#pragma once

#include "FDemoBackendFactory.h"
#include "FLabProductionFrameContext.h"

#include <functional>

namespace Stoner::Application { class FWindow; class FInteractiveLabSession; }

namespace Stoner::Demo
{
struct FInteractiveLabRunResult
{
    EDemoExitCode ExitCode = EDemoExitCode::InitializationFailed;
    Core::uint32 SubmittedFrames = 0;
    Core::uint32 RenderCompletedFrames = 0;
    Core::uint32 PresentedFrames = 0;
    Core::uint32 UIFramesSubmitted = 0;
    Core::uint32 DiagnosticFramesSubmitted = 0;
    Core::uint32 UISceneFallbackFrames = 0;
    Core::uint64 LastRecordedSettingsRevision = 0;
    float LastRecordedExposureStops = 0;
    float LastRecordedUIWhiteMultiplier = 0;
    Core::uint64 LastRecordedUIFrameToken = 0;
    Core::FString LastRecordedTransformVersion;
    FLabProductionFrameContextSnapshot FinalFrameState;
    FDemoLabPresentationStatus BeforeNativeShutdown;
    FDemoLabPresentationStatus AfterNativeShutdown;
    RHI::ERHIShutdownAssurance ShutdownAssurance = RHI::ERHIShutdownAssurance::Unknown;
    Core::FString FirstFailure;
};

// Optional typed window actions for in-process lifecycle fixtures and the
// bounded validation driver. Invoked on the event thread before input service;
// never invoked after terminal ownership handoff. SessionService runs after
// input service and before frame admission, for typed settings/lifecycle tests.
using FInteractiveLabSessionService = std::function<void(Application::FInteractiveLabSession&, Core::uint32)>;
using FInteractiveLabWindowService = std::function<void(Application::FWindow&, Core::uint32)>;

[[nodiscard]] FInteractiveLabRunResult RunInteractiveLab(
    const FDemoConfiguration& Configuration, const IDemoBackendFactory& Factory,
    FInteractiveLabWindowService WindowService = {}, FInteractiveLabSessionService SessionService = {});
} // namespace Stoner::Demo
