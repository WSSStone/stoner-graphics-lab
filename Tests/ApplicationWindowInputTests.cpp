#include "ApplicationWindowInputTests.h"

#include "Application/ApplicationMinimal.h"

#include <iostream>
#include <string>

namespace
{

using namespace Stoner::Application;

void Record(FApplicationWindowInputTestResult& Result, bool bPassed, const char* Name)
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

FWindowDesc ValidDesc()
{
    FWindowDesc Desc;
    Desc.Title = "Application Test";
    Desc.DebugName = "ApplicationTestWindow";
    Desc.ClientWidth = 1280;
    Desc.ClientHeight = 720;
    Desc.DisplayMode = EWindowDisplayMode::Windowed;
    Desc.bVisible = true;
    return Desc;
}

void TestWindowLifecycle(FApplicationWindowInputTestResult& Result)
{
    FWindow Window;
    Record(Result, Window.Create(ValidDesc()) == EApplicationResult::Success &&
            Window.GetLifecycleState() == EWindowLifecycleState::Active &&
            Window.GetClientWidth() == 1280 &&
            Window.GetClientHeight() == 720 &&
            Window.IsVisible(),
        "Application window creates active primary window from valid desc");
    Record(Result, !Window.PollEvents().empty(),
        "Application window exposes creation event on next poll");
    Record(Result, Window.RequestClose() == EApplicationResult::Success &&
            Window.IsCloseRequested(),
        "Application window supports observable close request");
    Record(Result, Window.Destroy() == EApplicationResult::Success &&
            Window.IsDestroyed() &&
            Window.Destroy() == EApplicationResult::Success &&
            Window.GetDiagnostics().CountByCode("APP-WINDOW-DESTROY-IDEMPOTENT") == 1,
        "Application window destroy is safe and idempotent");
}

void TestWindowValidationAndRuntime(FApplicationWindowInputTestResult& Result)
{
    FWindowDesc Desc = ValidDesc();
    Desc.ClientWidth = 0;
    FWindow Window;
    Record(Result, Window.Create(Desc) == EApplicationResult::ValidationFailed &&
            Window.GetDiagnostics().CountByCode("APP-WINDOW-SIZE-ZERO") == 1 &&
            Window.GetLifecycleState() == EWindowLifecycleState::Uncreated,
        "Application window rejects zero dimensions without partial active state");

    Desc = ValidDesc();
    Desc.ClientWidth = FWindowDesc::MaxClientWidth;
    Desc.ClientHeight = FWindowDesc::MaxClientHeight;
    Record(Result, Window.Create(Desc) == EApplicationResult::Success,
        "Application window accepts v1 maximum safe dimensions");

    Desc = ValidDesc();
    Desc.ClientWidth = FWindowDesc::MaxClientWidth + 1;
    Record(Result, Window.Create(Desc) == EApplicationResult::ValidationFailed &&
            Window.GetDiagnostics().CountByCode("APP-WINDOW-SIZE-LIMIT") == 1,
        "Application window rejects dimensions above v1 safe maximum");

    FWindow RealWindow;
    Record(Result, RealWindow.CreateRealWindow(ValidDesc(), EWindowRuntimeAvailability::DependencyUnavailable) == EApplicationResult::RuntimeUnavailable &&
            RealWindow.GetDiagnostics().CountByCode("APP-WINDOW-RUNTIME") == 1,
        "Application real-window path reports unavailable dependency safely");
}

void TestInputTransitions(FApplicationWindowInputTestResult& Result)
{
    FInputManager Input;
    Input.QueueEvent(FInputEvent::KeyDown(EKey::A, 1));
    Input.PollFrame(EWindowLifecycleState::Active, true);
    Record(Result, Input.GetState().WasKeyPressed(EKey::A) && Input.GetState().IsKeyHeld(EKey::A),
        "Application input reports key pressed and held on key down frame");

    Input.PollFrame(EWindowLifecycleState::Active, true);
    Record(Result, !Input.GetState().WasKeyPressed(EKey::A) && Input.GetState().IsKeyHeld(EKey::A),
        "Application input preserves held key without repeating pressed");

    Input.QueueEvent(FInputEvent::KeyUp(EKey::A, 2));
    Input.PollFrame(EWindowLifecycleState::Active, true);
    Record(Result, Input.GetState().WasKeyReleased(EKey::A) && !Input.GetState().IsKeyHeld(EKey::A),
        "Application input reports key released and clears held state");

    Input.QueueEvent(FInputEvent::MouseDown(EMouseButton::Left, 3));
    Input.QueueEvent(FInputEvent::PointerMove(10.0f, 20.0f, 4));
    Input.QueueEvent(FInputEvent::PointerMove(15.0f, 25.0f, 5));
    Input.PollFrame(EWindowLifecycleState::Active, true);
    Record(Result, Input.GetState().WasMouseButtonPressed(EMouseButton::Left) &&
            Input.GetState().IsMouseButtonHeld(EMouseButton::Left) &&
            Input.GetState().GetPointerX() == 15.0f &&
            Input.GetState().GetPointerY() == 25.0f &&
            Input.GetState().GetPointerDeltaX() == 5.0f &&
            Input.GetState().GetPointerDeltaY() == 5.0f,
        "Application input reports mouse button and deterministic pointer delta");

    Input.QueueEvent(FInputEvent::MouseUp(EMouseButton::Left, 6));
    Input.PollFrame(EWindowLifecycleState::Active, true);
    Record(Result, Input.GetState().WasMouseButtonReleased(EMouseButton::Left) &&
            !Input.GetState().IsMouseButtonHeld(EMouseButton::Left),
        "Application input reports mouse release and clears held button");
}

void TestFocusLossAndUnknownInput(FApplicationWindowInputTestResult& Result)
{
    FInputManager Input;
    Input.QueueEvent(FInputEvent::KeyDown(EKey::D, 1));
    Input.QueueEvent(FInputEvent::MouseDown(EMouseButton::Right, 2));
    Input.PollFrame(EWindowLifecycleState::Active, true);
    Input.QueueEvent(FInputEvent::FocusLost(3));
    Input.PollFrame(EWindowLifecycleState::Active, true);
    Record(Result, !Input.GetState().IsKeyHeld(EKey::D) &&
            !Input.GetState().IsMouseButtonHeld(EMouseButton::Right) &&
            Input.GetDiagnostics().CountByCode("APP-INPUT-FOCUS-CLEAR") == 1,
        "Application input clears held keyboard and mouse state on focus loss");

    Input.Clear();
    Input.QueueEvent(FInputEvent::KeyDown(EKey::Unknown, 1));
    Input.QueueEvent(FInputEvent::MouseDown(EMouseButton::Unknown, 2));
    Input.QueueEvent(FInputEvent::Unknown(3));
    Input.PollFrame(EWindowLifecycleState::Active, true);
    Record(Result, Input.GetState().GetHeldKeys().empty() &&
            Input.GetState().GetHeldMouseButtons().empty() &&
            Input.GetDiagnostics().CountByCode("APP-INPUT-UNKNOWN-KEY") == 1 &&
            Input.GetDiagnostics().CountByCode("APP-INPUT-UNKNOWN-MOUSE") == 1 &&
            Input.GetDiagnostics().CountByCode("APP-INPUT-UNKNOWN-EVENT") == 1,
        "Application input reports unknown identifiers without corrupting known state");
}

void TestWindowEventsAndLoop(FApplicationWindowInputTestResult& Result)
{
    FWindow Window;
    (void)Window.Create(ValidDesc());
    Window.QueueEvent(FWindowEvent::Resized(640, 480, 1));
    Window.QueueEvent(FWindowEvent::Resized(1024, 768, 2));
    Stoner::Core::TArray<FWindowEvent> Events = Window.PollEvents();
    Record(Result, !Events.empty() &&
            Window.GetClientWidth() == 1024 &&
            Window.GetClientHeight() == 768,
        "Application window coalesces final resize state deterministically");

    Window.QueueEvent(FWindowEvent::Minimized(3));
    Events = Window.PollEvents();
    Record(Result, Window.IsPresentationPaused() && !Window.HasDrawableArea(),
        "Application window exposes presentation-paused state for minimized windows");
    Window.QueueEvent(FWindowEvent::Restored(800, 600, 4));
    (void)Window.PollEvents();
    Record(Result, !Window.IsPresentationPaused() &&
            Window.HasDrawableArea() &&
            Window.GetClientWidth() == 800,
        "Application window restores drawable state deterministically");

    FInputManager Input;
    FApplicationLoop Loop;
    FApplicationLoopConfig Config;
    Config.MaxFrames = 300;
    FApplicationLoopState State = Loop.Run(Window, Input, Config);
    Record(Result, State.FrameIndex == 299 &&
            State.bUpdatedThisFrame &&
            !State.bCloseRequested,
        "Application loop runs representative 300-frame headless scenario");

    (void)Window.RequestClose();
    State = Loop.Run(Window, Input, Config);
    Record(Result, State.bCloseRequested &&
            !State.bShouldContinue &&
            State.Diagnostics.CountByCode("APP-LOOP-CLOSE-EXIT") == 1,
        "Application loop exits cleanly at close decision point");
}

void TestFailureModesAndDebugDump(FApplicationWindowInputTestResult& Result)
{
    FInputManager Input;
    Input.PollFrame(EWindowLifecycleState::Uncreated, true);
    Record(Result, Input.GetDiagnostics().CountByCode("APP-INPUT-LIFECYCLE") == 1,
        "Application input returns safe empty state before window creation");

    FWindow Window;
    (void)Window.Create(ValidDesc());
    Record(Result, Window.SetDisplayMode(EWindowDisplayMode::Fullscreen, true) == EApplicationResult::Success &&
            Window.GetDisplayMode() == EWindowDisplayMode::Fullscreen,
        "Application window supports successful display-mode transition");
    Record(Result, Window.SetDisplayMode(EWindowDisplayMode::Windowed, false) == EApplicationResult::UnsupportedMode &&
            Window.GetDisplayMode() == EWindowDisplayMode::Fullscreen &&
            Window.GetDiagnostics().CountByCode("APP-WINDOW-MODE-UNSUPPORTED") == 1,
        "Application window preserves prior display mode when transition is unsupported");

    (void)Window.Destroy();
    Input.Clear();
    Input.PollFrame(Window.GetLifecycleState(), true);
    Record(Result, Input.GetDiagnostics().CountByCode("APP-INPUT-LIFECYCLE") == 1,
        "Application input returns safe empty state after window destruction");

    FWindow DumpWindow;
    FInputManager DumpInput;
    (void)DumpWindow.Create(ValidDesc());
    DumpInput.QueueEvent(FInputEvent::KeyDown(EKey::A, 1));
    DumpInput.PollFrame(DumpWindow.GetLifecycleState(), DumpWindow.IsFocused());
    const std::string FirstDump = BuildApplicationDebugDump(DumpWindow, DumpInput).ToStdString();
    bool bStable = true;
    for (int Index = 0; Index < 20; ++Index)
    {
        FWindow RepeatWindow;
        FInputManager RepeatInput;
        (void)RepeatWindow.Create(ValidDesc());
        RepeatInput.QueueEvent(FInputEvent::KeyDown(EKey::A, 1));
        RepeatInput.PollFrame(RepeatWindow.GetLifecycleState(), RepeatWindow.IsFocused());
        bStable = bStable && FirstDump == BuildApplicationDebugDump(RepeatWindow, RepeatInput).ToStdString();
    }
    Record(Result, bStable &&
            FirstDump.find("0x") == std::string::npos &&
            FirstDump.find("NSWindow") == std::string::npos &&
            FirstDump.find("HWND") == std::string::npos,
        "Application debug dump is deterministic and omits native handle details");
}

} // namespace

FApplicationWindowInputTestResult RunApplicationWindowInputTests()
{
    FApplicationWindowInputTestResult Result;
    TestWindowLifecycle(Result);
    TestWindowValidationAndRuntime(Result);
    TestInputTransitions(Result);
    TestFocusLossAndUnknownInput(Result);
    TestWindowEventsAndLoop(Result);
    TestFailureModesAndDebugDump(Result);
    return Result;
}
