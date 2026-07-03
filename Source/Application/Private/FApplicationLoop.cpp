#include "Application/FApplicationLoop.h"

namespace Stoner::Application
{

FApplicationLoopState FApplicationLoop::Run(FWindow& Window,
    FInputManager& InputManager,
    const FApplicationLoopConfig& Config,
    FUpdateCallback UpdateCallback)
{
    FApplicationLoopState State;
    const Stoner::Core::uint32 MaxFrames = Config.MaxFrames == 0 ? 1 : Config.MaxFrames;
    for (Stoner::Core::uint32 Frame = 0; Frame < MaxFrames; ++Frame)
    {
        State.FrameIndex = Frame;
        (void)Window.PollEvents();
        InputManager.PollFrame(Window.GetLifecycleState(), Window.IsFocused());
        State.LastWindowState = Window.GetLifecycleState();
        State.LastInputState = InputManager.GetState();
        State.bCloseRequested = Window.IsCloseRequested() || Window.IsDestroyed();
        State.bPresentationPaused = Window.IsPresentationPaused();
        State.bUpdatedThisFrame = true;
        if (UpdateCallback)
        {
            UpdateCallback(State);
        }
        State.bShouldContinue = !State.bCloseRequested && Frame + 1 < MaxFrames;
        if (State.bPresentationPaused)
        {
            State.Diagnostics.Add(EApplicationDiagnosticSeverity::Info, EApplicationDiagnosticCategory::Loop,
                EApplicationResult::Success, "APP-LOOP-PRESENTATION-PAUSED", "Loop", "Presentation paused while update continues");
        }
        if (State.bCloseRequested)
        {
            State.Diagnostics.Add(EApplicationDiagnosticSeverity::Info, EApplicationDiagnosticCategory::Loop,
                EApplicationResult::Success, "APP-LOOP-CLOSE-EXIT", "Loop", "Close request ended loop");
            break;
        }
    }
    State.Diagnostics.SortStable();
    return State;
}

} // namespace Stoner::Application
