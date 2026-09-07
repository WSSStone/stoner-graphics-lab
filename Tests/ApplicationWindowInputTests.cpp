#include "ApplicationWindowInputTests.h"

#include "Application/ApplicationMinimal.h"
#include "FWindowDriver.h"

#include <algorithm>
#include <iostream>
#include <string>

namespace
{

using namespace Stoner::Application;

class FScriptedWindowDriver final : public IWindowDriver
{
public:
    const char* GetDriverName() const noexcept override { return "ScriptedNative"; }
    EWindowRuntimeAvailability GetRuntimeAvailability() const noexcept override { return Availability; }
    EApplicationResult Create(
        const FWindowDesc& Desc, Stoner::Core::uint32) override
    {
        bHighDensityFramebuffer = Desc.bHighDensityFramebuffer;
        bCreated = Availability == EWindowRuntimeAvailability::Available;
        return bCreated ? EApplicationResult::Success : EApplicationResult::RuntimeUnavailable;
    }
    void Destroy() override { bCreated = false; }
    void Poll() override
    {
        if (!bQueueFocusLossOnNextPoll) return;
        WindowEvents.push_back(FWindowEvent::FocusLost(NextSequence++));
        InputEvents.push_back(FInputEvent::FocusLost(NextSequence++));
        InputEvents.push_back(FInputEvent::KeyDown(EKey::W, NextSequence++));
        bQueueFocusLossOnNextPoll = false;
    }
    void RequestClose() override { WindowEvents.push_back(FWindowEvent::CloseRequested(NextSequence++)); }
    EApplicationResult SetClientSize(
        Stoner::Core::uint32 Width, Stoner::Core::uint32 Height) override
    {
        DrawableWidth = Width;
        DrawableHeight = Height;
        WindowEvents.push_back(FWindowEvent::Resized(Width, Height, NextSequence++));
        WindowEvents.push_back(FWindowEvent::DrawableResized(
            Width, Height, NextSequence++));
        return EApplicationResult::Success;
    }
    EApplicationResult Minimize() override
    {
        DrawableWidth = 0;
        DrawableHeight = 0;
        WindowEvents.push_back(FWindowEvent::Minimized(NextSequence++));
        return EApplicationResult::Success;
    }
    EApplicationResult Restore() override
    {
        DrawableWidth = 1280;
        DrawableHeight = 720;
        WindowEvents.push_back(FWindowEvent::Restored(
            DrawableWidth, DrawableHeight, NextSequence++));
        return EApplicationResult::Success;
    }
    Stoner::Core::FPlatformWindow GetPlatformWindow() const noexcept override
    { return bCreated ? Stoner::Core::FPlatformWindow(const_cast<int*>(&NativeToken)) : Stoner::Core::FPlatformWindow{}; }
    Stoner::Core::uint32 GetDrawableWidth() const noexcept override { return DrawableWidth; }
    Stoner::Core::uint32 GetDrawableHeight() const noexcept override { return DrawableHeight; }
    Stoner::Core::TArray<FWindowEvent> ConsumeWindowEvents() override
    { auto Result = WindowEvents; WindowEvents.clear(); return Result; }
    Stoner::Core::TArray<FInputEvent> ConsumeInputEvents() override
    { auto Result = InputEvents; InputEvents.clear(); return Result; }

    void QueueWindow(FWindowEvent Event) { WindowEvents.push_back(std::move(Event)); }
    void QueueInput(FInputEvent Event) { InputEvents.push_back(std::move(Event)); }
    void SetDrawable(Stoner::Core::uint32 Width, Stoner::Core::uint32 Height)
    { DrawableWidth = Width; DrawableHeight = Height; }

    EWindowRuntimeAvailability Availability = EWindowRuntimeAvailability::Available;
    bool bCreated = false;
    bool bHighDensityFramebuffer = true;
    bool bQueueFocusLossOnNextPoll = false;
    int NativeToken = 7;
    Stoner::Core::uint32 DrawableWidth = 2560;
    Stoner::Core::uint32 DrawableHeight = 1440;
    Stoner::Core::uint64 NextSequence = 100;
    Stoner::Core::TArray<FWindowEvent> WindowEvents;
    Stoner::Core::TArray<FInputEvent> InputEvents;
};

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

    auto Driver = std::make_unique<FScriptedWindowDriver>();
    FWindow ReusedRealWindow;
    FWindowTestAccess::InstallDriver(ReusedRealWindow, std::move(Driver));
    Record(Result, ReusedRealWindow.CreateRealWindow(ValidDesc()) == EApplicationResult::Success &&
            ReusedRealWindow.IsActive() &&
            ReusedRealWindow.IsRealWindow(),
        "Application real-window reused fixture starts from an active native state");
    Desc = ValidDesc();
    Desc.ClientWidth = 0;
    Record(Result, ReusedRealWindow.CreateRealWindow(Desc) == EApplicationResult::ValidationFailed &&
            ReusedRealWindow.GetLifecycleState() == EWindowLifecycleState::Uncreated &&
            !ReusedRealWindow.IsRealWindow() &&
            !ReusedRealWindow.HasDrawableArea(),
        "Application real-window validation failure clears stale active runtime state");
}

void TestPrivateDriverAndRealWindowEvents(FApplicationWindowInputTestResult& Result)
{
    FWindow Window;
    auto Driver = std::make_unique<FScriptedWindowDriver>();
    FScriptedWindowDriver* Script = Driver.get();
    FWindowTestAccess::InstallDriver(Window, std::move(Driver));
    FWindowDesc PixelExactDesc = ValidDesc();
    PixelExactDesc.bHighDensityFramebuffer = false;
    Record(Result, Window.CreateRealWindow(PixelExactDesc) == EApplicationResult::Success && Script->bCreated &&
            !Script->bHighDensityFramebuffer &&
            Window.IsRealWindow() && Window.GetPlatformWindow().IsValid() &&
            Window.GetDrawableWidth() == 2560 && Window.GetDrawableHeight() == 1440,
        "Application propagates pixel-density policy and exposes native framebuffer extent");

    Script->SetDrawable(1800, 1400);
    Script->QueueWindow(FWindowEvent::DrawableResized(1800, 1400, 102));
    Script->QueueWindow(FWindowEvent::Resized(900, 700, 101));
    Stoner::Core::TArray<FWindowEvent> Events = Window.PollEvents();
    Record(Result, Events.size() >= 3 && Events[Events.size() - 2].EventType == EWindowEventType::Resized &&
            Events.back().EventType == EWindowEventType::DrawableResized &&
            Window.GetClientWidth() == 900 && Window.GetClientHeight() == 700 &&
            Window.GetDrawableWidth() == 1800 && Window.GetDrawableHeight() == 1400,
        "Application preserves logical and framebuffer sizes and callback sequence independently");

    Script->SetDrawable(0, 0);
    Script->QueueWindow(FWindowEvent::Minimized(103));
    (void)Window.PollEvents();
    Record(Result, Window.IsPresentationPaused() && !Window.HasDrawableArea(),
        "Application real driver pauses presentation at zero framebuffer extent");
    Script->SetDrawable(1800, 1400);
    Script->QueueWindow(FWindowEvent::Restored(1800, 1400, 104));
    (void)Window.PollEvents();
    Record(Result, !Window.IsPresentationPaused() && Window.HasDrawableArea(),
        "Application real driver restores presentation after non-zero framebuffer extent");

    Record(Result,
        Window.SetClientSize(960, 540) == EApplicationResult::Success &&
            Window.Minimize() == EApplicationResult::Success &&
            Window.Restore() == EApplicationResult::Success,
        "Application forwards explicit size minimize and restore commands");
    (void)Window.PollEvents();
    Record(Result,
        Window.GetClientWidth() == 1280 && Window.GetClientHeight() == 720 &&
            !Window.IsMinimized() && Window.HasDrawableArea(),
        "Application applies commanded window lifecycle events deterministically");

    Script->QueueInput(FInputEvent::KeyDown(EKey::Escape, 105));
    Script->QueueWindow(FWindowEvent::CloseRequested(106));
    const auto Input = Window.PollInputEvents();
    Events = Window.PollEvents();
    Record(Result, std::any_of(Input.begin(), Input.end(),
            [](const FInputEvent& Event)
            {
                return Event.EventType == EInputEventType::KeyDown &&
                    Event.Key == EKey::Escape;
            }) && Window.IsCloseRequested() && !Events.empty(),
        "Application translates Escape and close callbacks while the window loop remains pollable");
    (void)Window.Destroy();
}

void TestCursorAndDisplayServices(FApplicationWindowInputTestResult& Result)
{
    FWindow Window;
    Record(Result, Window.Create(ValidDesc()) == EApplicationResult::Success,
        "headless window starts with a usable display service");

    const FWindowDisplayState InitialDisplay = Window.GetDisplayState();
    Record(Result, InitialDisplay.IsValid() &&
            InitialDisplay.LogicalExtent == FWindowExtent{1280, 720} &&
            InitialDisplay.DrawableExtent == FWindowExtent{1280, 720} &&
            InitialDisplay.DisplayGeneration != 0 &&
            InitialDisplay.bFocused && !InitialDisplay.bMinimized,
        "headless display snapshot reports logical and drawable extents");

    Record(Result, Window.SetCursorMode(ECursorMode::Disabled) ==
            EApplicationResult::Success &&
            Window.GetCursorMode() == ECursorMode::Disabled,
        "headless window accepts engine cursor capture mode");

    Window.QueueEvent(FWindowEvent::FocusLost(20));
    (void)Window.PollEvents();
    const auto FocusLossInput = Window.PollInputEvents();
    const bool bFocusReset = std::any_of(FocusLossInput.begin(),
        FocusLossInput.end(), [](const FInputEvent& Event)
        {
            return Event.EventType == EInputEventType::FocusLost;
        });
    const FWindowDisplayState UnfocusedDisplay = Window.GetDisplayState();
    Record(Result, bFocusReset && Window.GetCursorMode() == ECursorMode::Normal &&
            !Window.IsFocused() && !UnfocusedDisplay.bFocused &&
            UnfocusedDisplay.DisplayGeneration > InitialDisplay.DisplayGeneration,
        "focus loss restores normal cursor and emits input baseline reset");
    Record(Result, Window.SetCursorMode(ECursorMode::Disabled) !=
            EApplicationResult::Success,
        "unfocused window cannot reacquire a disabled cursor");

    Window.QueueEvent(FWindowEvent::FocusGained(21));
    (void)Window.PollEvents();
    Record(Result, Window.SetCursorMode(ECursorMode::Disabled) ==
            EApplicationResult::Success,
        "focused window can request cursor capture again");

    Window.QueueEvent(FWindowEvent::Minimized(22));
    (void)Window.PollEvents();
    const auto MinimizedInput = Window.PollInputEvents();
    const FWindowDisplayState MinimizedDisplay = Window.GetDisplayState();
    Record(Result, std::any_of(MinimizedInput.begin(), MinimizedInput.end(),
            [](const FInputEvent& Event)
            {
                return Event.EventType == EInputEventType::FocusLost;
            }) && Window.GetCursorMode() == ECursorMode::Normal &&
            MinimizedDisplay.IsValid() && MinimizedDisplay.DrawableExtent.IsZero() &&
            MinimizedDisplay.LogicalExtent == FWindowExtent{1280, 720} &&
            MinimizedDisplay.bMinimized,
        "minimize retains logical size while pausing drawable input");
    Record(Result, Window.SetCursorMode(ECursorMode::Disabled) !=
            EApplicationResult::Success,
        "minimized window cannot reacquire a disabled cursor");

    Window.QueueEvent(FWindowEvent::Restored(800, 600, 23));
    (void)Window.PollEvents();
    const FWindowDisplayState RestoredDisplay = Window.GetDisplayState();
    Record(Result, RestoredDisplay.IsValid() &&
            RestoredDisplay.DrawableExtent == FWindowExtent{800, 600} &&
            !RestoredDisplay.bMinimized && RestoredDisplay.bFocused &&
            RestoredDisplay.DisplayGeneration > MinimizedDisplay.DisplayGeneration,
        "restore publishes a new valid drawable generation");

    auto ResizeDriver = std::make_unique<FScriptedWindowDriver>();
    FScriptedWindowDriver* ResizeScript = ResizeDriver.get();
    FWindow ResizeWindow;
    FWindowTestAccess::InstallDriver(ResizeWindow, std::move(ResizeDriver));
    (void)ResizeWindow.CreateRealWindow(ValidDesc());
    ResizeScript->QueueWindow(FWindowEvent::Resized(900, 450, 25));
    (void)ResizeWindow.PollEvents();
    const FWindowDisplayState LogicalResize = ResizeWindow.GetDisplayState();
    Record(Result, LogicalResize.IsValid() &&
            LogicalResize.LogicalExtent == FWindowExtent{900, 450} &&
            LogicalResize.DrawableExtent == FWindowExtent{2560, 1440} &&
            Stoner::Core::FMath::IsNearlyEqual(LogicalResize.FramebufferScale.X,
                2560.0f / 900.0f, 1.0e-4f) &&
            Stoner::Core::FMath::IsNearlyEqual(LogicalResize.FramebufferScale.Y,
                1440.0f / 450.0f, 1.0e-4f),
        "logical resize recomputes framebuffer scale when drawable extent is unchanged");

    ResizeScript->SetDrawable(0, 0);
    ResizeScript->QueueWindow(FWindowEvent::DrawableResized(0, 0, 26));
    (void)ResizeWindow.PollEvents();
    const FWindowDisplayState ZeroDrawable = ResizeWindow.GetDisplayState();
    const auto ZeroDrawableInput = ResizeWindow.PollInputEvents();
    Record(Result, ZeroDrawable.IsValid() &&
            ZeroDrawable.DrawableExtent.IsZero() && !ZeroDrawable.bMinimized &&
            !ResizeWindow.HasDrawableArea() &&
            std::any_of(ZeroDrawableInput.begin(), ZeroDrawableInput.end(),
                [](const FInputEvent& Event)
                {
                    return Event.EventType == EInputEventType::FocusLost;
                }) &&
            ResizeWindow.SetCursorMode(ECursorMode::Disabled) !=
                EApplicationResult::Success,
        "zero framebuffer extent pauses independently of native minimization");

    ResizeScript->SetDrawable(1024, 576);
    ResizeScript->QueueWindow(FWindowEvent::DrawableResized(1024, 576, 27));
    (void)ResizeWindow.PollEvents();
    Record(Result, ResizeWindow.GetDisplayState().IsValid() &&
            ResizeWindow.HasDrawableArea() &&
            !ResizeWindow.IsMinimized(),
        "non-zero framebuffer callback resumes an unminimized zero-drawable window");

    ResizeScript->SetDrawable(2048, 1536);
    ResizeScript->QueueWindow(FWindowEvent::Restored(1024, 768, 28));
    (void)ResizeWindow.PollEvents();
    const FWindowDisplayState HighDpiRestore = ResizeWindow.GetDisplayState();
    Record(Result, HighDpiRestore.IsValid() &&
            HighDpiRestore.LogicalExtent == FWindowExtent{1024, 768} &&
            HighDpiRestore.DrawableExtent == FWindowExtent{2048, 1536} &&
            Stoner::Core::FMath::IsNearlyEqual(HighDpiRestore.FramebufferScale.X,
                2.0f, 1.0e-4f) &&
            Stoner::Core::FMath::IsNearlyEqual(HighDpiRestore.FramebufferScale.Y,
                2.0f, 1.0e-4f),
        "high DPI restore preserves logical size and actual drawable ratio");

    (void)Window.SetCursorMode(ECursorMode::Disabled);
    Record(Result, Window.RequestClose() == EApplicationResult::Success &&
            Window.GetCursorMode() == ECursorMode::Normal &&
            Window.IsCloseRequested(),
        "close request releases engine cursor capture immediately");
    (void)Window.PollEvents();
    const auto CloseInput = Window.PollInputEvents();
    const auto CloseResetCount = std::count_if(CloseInput.begin(), CloseInput.end(),
        [](const FInputEvent& Event)
        {
            return Event.EventType == EInputEventType::FocusLost;
        });
    Record(Result, CloseResetCount == 1,
        "close request and its polled callback emit one input baseline reset");
    Record(Result, std::any_of(CloseInput.begin(), CloseInput.end(),
            [](const FInputEvent& Event)
            {
                return Event.EventType == EInputEventType::FocusLost;
            }),
        "close event carries the input baseline reset");
    Record(Result, Window.SetCursorMode(ECursorMode::Disabled) !=
            EApplicationResult::Success,
        "closing window cannot reacquire a disabled cursor");

    const auto DestroyInput = [&Window]()
    {
        (void)Window.Destroy();
        return Window.PollInputEvents();
    }();
    Record(Result, std::any_of(DestroyInput.begin(), DestroyInput.end(),
            [](const FInputEvent& Event)
            {
                return Event.EventType == EInputEventType::FocusLost;
            }),
        "engine initiated destruction emits a final input baseline reset");

    auto Driver = std::make_unique<FScriptedWindowDriver>();
    FScriptedWindowDriver* Script = Driver.get();
    FWindow EscapeWindow;
    FWindowTestAccess::InstallDriver(EscapeWindow, std::move(Driver));
    (void)EscapeWindow.CreateRealWindow(ValidDesc());
    Script->QueueInput(FInputEvent::KeyDown(EKey::Escape, 24));
    const auto EscapeInput = EscapeWindow.PollInputEvents();
    Record(Result, !EscapeInput.empty() &&
            EscapeInput.front().Key == EKey::Escape &&
            !EscapeWindow.IsCloseRequested(),
        "Escape input remains an application action and does not close a window");

    Script->bQueueFocusLossOnNextPoll = true;
    const auto ImmediateFocusInput = EscapeWindow.PollInputEvents();
    const auto FocusReset = std::find_if(ImmediateFocusInput.begin(),
        ImmediateFocusInput.end(), [](const FInputEvent& Event)
        {
            return Event.EventType == EInputEventType::FocusLost;
        });
    const auto QueuedDown = std::find_if(ImmediateFocusInput.begin(),
        ImmediateFocusInput.end(), [](const FInputEvent& Event)
        {
            return Event.EventType == EInputEventType::KeyDown &&
                Event.Key == EKey::W;
        });
    Record(Result, FocusReset != ImmediateFocusInput.end() &&
            QueuedDown != ImmediateFocusInput.end() &&
            FocusReset->Sequence < QueuedDown->Sequence &&
            EscapeWindow.GetCursorMode() == ECursorMode::Normal,
        "a focus loss discovered by a later input poll delivers reset before queued downs");
}

void TestDistinctFocusLossResets(FApplicationWindowInputTestResult& Result)
{
    auto Driver = std::make_unique<FScriptedWindowDriver>();
    FScriptedWindowDriver* Script = Driver.get();
    FWindow Window;
    FWindowTestAccess::InstallDriver(Window, std::move(Driver));
    (void)Window.CreateRealWindow(ValidDesc());

    Script->QueueWindow(FWindowEvent::FocusLost(30));
    Script->QueueInput(FInputEvent::KeyDown(EKey::W, 31));
    Script->QueueWindow(FWindowEvent::FocusGained(32));
    Script->QueueWindow(FWindowEvent::FocusLost(33));
    (void)Window.PollEvents();
    const auto Input = Window.PollInputEvents();

    Stoner::Core::TArray<Stoner::Core::uint64> ResetSequences;
    for (const FInputEvent& Event : Input)
    {
        if (Event.EventType == EInputEventType::FocusLost)
            ResetSequences.push_back(Event.Sequence);
    }
    const auto KeyIt = std::find_if(Input.begin(), Input.end(),
        [](const FInputEvent& Event)
        {
            return Event.EventType == EInputEventType::KeyDown &&
                Event.Key == EKey::W;
        });
    Record(Result, ResetSequences.size() == 2 &&
            ResetSequences[0] == 30 && ResetSequences[1] == 33 &&
            KeyIt != Input.end() &&
            ResetSequences[0] < KeyIt->Sequence &&
            KeyIt->Sequence < ResetSequences[1],
        "distinct focus loss transitions retain ordered input baseline resets");
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

    Input.PollFrame(EWindowLifecycleState::Active, true);
    Record(Result, Input.GetState().IsFocused(),
        "Application input restores focused snapshot state on focused frame");

    Input.Clear();
    Input.QueueEvent(FInputEvent::PointerMove(32.0f, 48.0f, 1));
    Input.PollFrame(EWindowLifecycleState::Active, true);
    Record(Result, Input.GetState().HasPointerPosition(),
        "Application input records pointer position before reset coverage");
    Input.PollFrame(EWindowLifecycleState::Destroyed, true);
    Record(Result, !Input.GetState().HasPointerPosition() &&
            Input.GetState().GetPointerX() == 0.0f &&
            Input.GetState().GetPointerY() == 0.0f &&
            !Input.GetState().IsFocused(),
        "Application input invalid lifecycle clears stale pointer and focus snapshot state");

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

void TestPlatformInputMapping(FApplicationWindowInputTestResult& Result)
{
    constexpr int GlfwKeyHome = 268;
    constexpr int GlfwKeyPageDown = 267;
    constexpr int GlfwKeyLeftShift = 340;
    constexpr int GlfwKeyRightControl = 345;
    constexpr int GlfwKeyF12 = 301;
    constexpr int GlfwMouseButton4 = 3;
    constexpr int GlfwMouseButton5 = 4;

    const bool bPassed = IsGlfwInputMappingAvailable()
        ? TranslateGlfwKeyCode(GlfwKeyHome) == EKey::Home &&
            TranslateGlfwKeyCode(GlfwKeyPageDown) == EKey::PageDown &&
            TranslateGlfwKeyCode(GlfwKeyLeftShift) == EKey::LeftShift &&
            TranslateGlfwKeyCode(GlfwKeyRightControl) == EKey::RightControl &&
            TranslateGlfwKeyCode(GlfwKeyF12) == EKey::F12 &&
            TranslateGlfwMouseButtonCode(GlfwMouseButton4) == EMouseButton::X1 &&
            TranslateGlfwMouseButtonCode(GlfwMouseButton5) == EMouseButton::X2
        : TranslateGlfwKeyCode(GlfwKeyHome) == EKey::Unknown &&
            TranslateGlfwMouseButtonCode(GlfwMouseButton4) == EMouseButton::Unknown;
    Record(Result, bPassed,
        "Application GLFW input mapping covers declared navigation modifier function and extra mouse vocabulary when available");
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

    FWindow DriverWindow;
    auto Driver = std::make_unique<FScriptedWindowDriver>();
    FScriptedWindowDriver* Script = Driver.get();
    FWindowTestAccess::InstallDriver(DriverWindow, std::move(Driver));
    (void)DriverWindow.CreateRealWindow(ValidDesc());
    Script->QueueInput(FInputEvent::KeyDown(EKey::Space, 201));
    Config.MaxFrames = 1;
    FInputManager DriverInput;
    State = Loop.Run(DriverWindow, DriverInput, Config);
    Record(Result, State.LastInputState.WasKeyPressed(EKey::Space) &&
            State.LastInputState.IsKeyHeld(EKey::Space),
        "Application loop ingests native driver input events before deriving frame state");
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
    TestPrivateDriverAndRealWindowEvents(Result);
    TestCursorAndDisplayServices(Result);
    TestDistinctFocusLossResets(Result);
    TestInputTransitions(Result);
    TestFocusLossAndUnknownInput(Result);
    TestPlatformInputMapping(Result);
    TestWindowEventsAndLoop(Result);
    TestFailureModesAndDebugDump(Result);
    return Result;
}
