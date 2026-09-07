#pragma once

#include "FDemoBackendFactory.h"

#include <functional>

namespace Stoner::Application { class FWindow; }

namespace Stoner::Demo
{
struct FInteractiveLabRunResult
{
    EDemoExitCode ExitCode = EDemoExitCode::InitializationFailed;
    Core::uint32 SubmittedFrames = 0;
    Core::uint32 RenderCompletedFrames = 0;
    Core::uint32 PresentedFrames = 0;
    RHI::ERHIShutdownAssurance ShutdownAssurance = RHI::ERHIShutdownAssurance::Unknown;
    Core::FString FirstFailure;
};

// Optional typed window actions for in-process lifecycle fixtures and the
// bounded validation driver. Invoked on the event thread before input service;
// never invoked after terminal ownership handoff.
using FInteractiveLabWindowService = std::function<void(Application::FWindow&, Core::uint32)>;

[[nodiscard]] FInteractiveLabRunResult RunInteractiveLab(
    const FDemoConfiguration& Configuration, const IDemoBackendFactory& Factory,
    FInteractiveLabWindowService WindowService = {});
} // namespace Stoner::Demo
