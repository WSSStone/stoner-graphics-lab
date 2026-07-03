#pragma once

#include "Application/FInputManager.h"
#include "Application/FWindow.h"

#include <functional>

namespace Stoner::Application
{

struct FApplicationLoopConfig
{
    Stoner::Core::uint32 MaxFrames = 300;
};

struct FApplicationLoopState
{
    Stoner::Core::uint32 FrameIndex = 0;
    bool bShouldContinue = true;
    bool bCloseRequested = false;
    bool bPresentationPaused = false;
    bool bUpdatedThisFrame = false;
    EWindowLifecycleState LastWindowState = EWindowLifecycleState::Uncreated;
    FInputState LastInputState;
    FApplicationDiagnosticLog Diagnostics;
};

class FApplicationLoop
{
public:
    using FUpdateCallback = std::function<void(FApplicationLoopState&)>;

    [[nodiscard]] FApplicationLoopState Run(FWindow& Window,
        FInputManager& InputManager,
        const FApplicationLoopConfig& Config = {},
        FUpdateCallback UpdateCallback = {});
};

} // namespace Stoner::Application
