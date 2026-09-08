// Feature 030 T035: event/clipboard failure fixtures precede their services.
// Ownership fixtures exercise the private router with current-frame capture values;
// native visible widgets and full adapter integration remain the US2 checkpoint.
#include "Application/FInputManager.h"
#include "Application/FWindow.h"

#include "FWindowDriver.h"
#include "FLabInputRouter.h"
#include "FWindowEventBuffer.h"
#include "FImGuiLabAdapter.h"
#include <iostream>
#include <string>

namespace
{
using namespace Stoner::Application;
using namespace Stoner::Core;
int Failed = 0;
int Passed = 0;
void Check(bool Value, const char* Name)
{
    (Value ? ++Passed : ++Failed);
    std::cout << (Value ? "[PASS] " : "[FAIL] ") << Name << '\n';
}
}

int RunApplicationUIInputTests()
{
    Failed = Passed = 0;
    FLabInputRouter Router;
    FUILabCapture Capture;
    Capture.bKeyboard = Capture.bPointer = true;
    auto Routed = Router.Resolve({FInputEvent::KeyDown(EKey::W),
        FInputEvent::MouseDown(EMouseButton::Right)}, Capture, true);
    Check(Routed.Actions.ForwardAxis == 0 && !Routed.Actions.bLookCaptured,
        "same-frame first widget activation prevents speculative camera movement");
    Capture = {};
    Routed = Router.Resolve({FInputEvent::PointerMove(500, 500)}, Capture, true);
    Check(Routed.Actions.ForwardAxis == 0 && !Routed.Actions.bLookCaptured &&
        Router.GetOwnership().PointerGestureOwner == EInputOwner::UI,
        "UI-owned drag stays owned outside widget and held keys stay quarantined");
    Routed = Router.Resolve({FInputEvent::KeyUp(EKey::W),
        FInputEvent::KeyDown(EKey::W), FInputEvent::MouseUp(EMouseButton::Right)}, Capture, true);
    Check(Routed.Actions.ForwardAxis == 1, "release then fresh press rearms navigation");
    Capture.bScroll = true;
    Routed = Router.Resolve({FInputEvent::Scroll(0, 2)}, Capture, true);
    Check(Routed.Actions.ScrollDeltaY == 0 && Routed.Actions.ForwardAxis == 1,
        "UI wheel ownership does not steal independent keyboard movement");
    Capture = {};
    Routed = Router.Resolve({FInputEvent::FocusLost(1), FInputEvent::KeyDown(EKey::W, 2)}, Capture, true);
    Check(Routed.Actions.ForwardAxis == 0 && !Router.GetOwnership().bFocused,
        "ordered focus loss wins over later queued downs");
    Routed = Router.Resolve({FInputEvent::FocusGained(3)}, Capture, true);
    Check(Routed.Actions.ForwardAxis == 0, "focus restore alone cannot rearm held navigation");
    Routed = Router.Resolve({FInputEvent::KeyDown(EKey::Escape)}, Capture, true);
    Check(Routed.bCancelInteraction && !Routed.Actions.bLookCaptured,
        "Escape cancels interaction without producing an exit command");
    Capture.bTextEditing = true;
    Routed = Router.Resolve({FInputEvent::KeyDown(EKey::F1)}, Capture, true);
    Check(!Routed.bToggleUI, "text editing retains F1 semantics");
    Capture.bTextEditing = false;
    Routed = Router.Resolve({FInputEvent::KeyUp(EKey::F1), FInputEvent::KeyDown(EKey::F1)}, Capture, true);
    Check(Routed.bToggleUI && Routed.Actions.ForwardAxis == 0,
        "fresh F1 toggles UI and quarantines navigation");
    Routed = Router.Resolve({FInputEvent::KeyUp(EKey::W), FInputEvent::KeyDown(EKey::W)}, Capture, true, true);
    Check(Routed.Actions.ForwardAxis == 0 && !Routed.Actions.bLookCaptured,
        "overflow prevents all camera actions in the interval");

    Check(Router.GetOwnership().IsValid(), "overflow preserves a valid ownership snapshot");

    FWindowEventBuffer Buffer;
    Buffer.Push(FWindowEvent::CloseRequested(1));
    for (uint64 I = 2; I < 5000; ++I) Buffer.Push(FInputEvent::PointerMove(1, 1, I));
    Buffer.Push(FWindowEvent::FocusLost(5000));
    const auto WindowEvents = Buffer.TakeWindow();
    const auto InputEvents = Buffer.TakeInput();
    Check(WindowEvents.size() == 2 && WindowEvents[0].EventType == EWindowEventType::CloseRequested &&
        WindowEvents[1].EventType == EWindowEventType::FocusLost &&
        InputEvents.size() == 1 && InputEvents[0].EventType == EInputEventType::Overflow,
        "shared native-event budget preserves independent close and focus latches");
    Buffer.Push(FInputEvent::Text('x', 5001));
    Check(Buffer.TakeInput().size() == 1, "native event buffer resumes after overflow delivery");

    for (uint64 I = 1; I <= 4097; ++I) Buffer.Push(FInputEvent::Text('a', I));
    Check(Buffer.TakeWindow().empty(), "overflow does not invent a native focus loss");
    (void)Buffer.TakeInput();
    Buffer.Push(FWindowEvent::FocusLost(1));
    Buffer.Push(FWindowEvent::Minimized(2));
    for (uint64 I = 3; I < 5000; ++I) Buffer.Push(FInputEvent::Text('a', I));
    Buffer.Push(FWindowEvent::FocusGained(5001));
    Buffer.Push(FWindowEvent::Restored(800, 600, 5002));
    Buffer.Push(FWindowEvent::ContentScaleChanged(1.5f, 1.5f, 5003));
    const auto RestoredEvents = Buffer.TakeWindow();
    Check(RestoredEvents.size() >= 3 && RestoredEvents.back().EventType == EWindowEventType::ContentScaleChanged &&
        RestoredEvents[RestoredEvents.size()-2].EventType == EWindowEventType::Restored,
        "overflow retains the latest focus restore extent and scale after cancellation");
    (void)Buffer.TakeInput();

    FInputManager Input;
    Input.QueueEvents({FInputEvent::KeyDown(EKey::W, 1),
        FInputEvent::FocusLost(2), FInputEvent::KeyDown(EKey::W, 3),
        FInputEvent::MouseDown(EMouseButton::Right, 4),
        FInputEvent::PointerMove(100, 100, 5)});
    Input.PollFrame(EWindowLifecycleState::Active, true);
    Check(!Input.GetState().IsKeyHeld(EKey::W) &&
        !Input.GetState().IsMouseButtonHeld(EMouseButton::Right) &&
        !Input.GetState().HasPointerPosition(),
        "queued downs and pointer motion after focus loss cannot rearm input");

    Input.Clear();
    Input.QueueEvent(FInputEvent::KeyDown(EKey::W));
    Input.PollFrame(EWindowLifecycleState::Active, false);
    Check(!Input.GetState().IsKeyHeld(EKey::W),
        "unfocused intervals discard navigation downs");

    Input.Clear();
    Input.QueueEvent(FInputEvent::KeyDown(EKey::W));
    Input.PollFrame(EWindowLifecycleState::Active);
    for (uint64 Index = 1; Index <= 4097; ++Index)
        Input.QueueEvent(FInputEvent::PointerMove(1, 1, Index));
    Input.QueueEvent(FInputEvent::KeyUp(EKey::W, 5000));
    Input.PollFrame(EWindowLifecycleState::Active);
    Check(!Input.GetState().IsKeyHeld(EKey::W) &&
        !Input.GetState().HasPointerPosition(),
        "input overflow cancels the interval instead of retaining motion");
    Check(Input.DidOverflow() && Input.GetFrameEvents().size() == 1 &&
        Input.GetFrameEvents()[0].EventType == EInputEventType::Overflow,
        "raw UI stream reports cancellation without inventing focus state");
    Check(Input.GetDiagnostics().CountByCode("APP-INPUT-OVERFLOW") == 1,
        "input overflow records one bounded diagnostic");
    Input.Clear();
    Input.QueueEvents({FInputEvent::Text(0x1F642, 3),
        FInputEvent::KeyDown(EKey::LeftSuper, 1), FInputEvent::Text('A', 2)});
    Input.PollFrame(EWindowLifecycleState::Active);
    Check(Input.GetFrameEvents().size() == 3 &&
        Input.GetFrameEvents()[1].UnicodeScalar == 'A' &&
        Input.GetFrameEvents()[2].UnicodeScalar == 0x1F642 &&
        Input.GetState().IsKeyHeld(EKey::LeftSuper),
        "committed non-BMP text preserves ordering with modifier keys");
    Input.Clear();
    Input.QueueEvents({FInputEvent::Text(0xD800), FInputEvent::Text(0x110000)});
    Input.PollFrame(EWindowLifecycleState::Active);
    Check(Input.GetFrameEvents().empty(), "invalid Unicode scalars are rejected");
    Input.Clear();
    for (int Index = 0; Index < 1025; ++Index)
        Input.QueueEvent(FInputEvent::Text(0x1F642));
    Input.PollFrame(EWindowLifecycleState::Active);
    Check(Input.GetFrameEvents().empty() && !Input.DidOverflow(),
        "over-budget committed text is rejected as a whole");

    Input.Clear();
    auto Repeat = FInputEvent::KeyDown(EKey::W, 3);
    Repeat.bRepeat = true;
    Input.QueueEvents({FInputEvent::FocusLost(1), FInputEvent::FocusGained(2), Repeat});
    Input.PollFrame(EWindowLifecycleState::Active);
    Check(!Input.GetState().IsKeyHeld(EKey::W) && Input.GetFrameEvents().back().bRepeat,
        "repeat remains available to UI without rearming camera physical state");

    FWindow Window;
    Check(Window.Create({}) == EApplicationResult::Success, "create clipboard fixture window");
    FString Output = "unchanged";
    Check(Window.ReadClipboardUtf8(Output) == EApplicationResult::UnsupportedMode &&
        Output == FString("unchanged"), "unavailable clipboard preserves destination");
    const auto OldDisplay = Window.GetDisplayState();
    Window.ApplyEvent(FWindowEvent::ContentScaleChanged(1.5f, 1.5f));
    Check(Window.GetDisplayState().ContentScale.X == 1.5f &&
        Window.GetDisplayState().FramebufferScale.X == 1.0f &&
        Window.GetDisplayState().DisplayGeneration > OldDisplay.DisplayGeneration,
        "content scale changes generation without applying drawable ratio twice");
    Check(IsKnownKey(EKey::LeftSuper) && IsKnownKey(EKey::RightSuper) &&
        !IsKnownKey(static_cast<EKey>(9999)), "modifier and invalid key bounds are explicit");
    FWindowTestAccess::InstallDriver(Window, CreateHeadlessWindowDriver());
    const FString Unicode(std::string("e\xCC\x81 \xF0\x9F\x99\x82"));
    Check(Window.WriteClipboardUtf8(Unicode) == EApplicationResult::Success &&
        Window.ReadClipboardUtf8(Output) == EApplicationResult::Success && Output == Unicode,
        "clipboard preserves non-BMP and decomposed UTF-8 bytes without normalization");
    for (const auto& Invalid : {std::string("\xC0\xAF"), std::string("a\0b", 3),
             std::string(65537, 'x')})
        Check(Window.WriteClipboardUtf8(FString(Invalid)) == EApplicationResult::InvalidInput &&
            Window.ReadClipboardUtf8(Output) == EApplicationResult::Success && Output == Unicode,
            "invalid clipboard write preserves previous contents");
    Check(Window.WriteClipboardUtf8("") == EApplicationResult::Success &&
        Window.ReadClipboardUtf8(Output) == EApplicationResult::Success && Output.IsEmpty(),
        "empty clipboard is valid");
    {
        FImGuiLabAdapter UI;
        Check(UI.Initialize(Window) == EApplicationResult::Success,
            "private UI context initializes with the embedded font");
        auto Display = Window.GetDisplayState();
        Check(UI.Frame({}, Display, 1.0 / 60.0) == EApplicationResult::Success &&
            UI.Frame({}, Display, 1.0 / 60.0) == EApplicationResult::Success && UI.GetVertexCount() > 0,
            "real UI core builds bounded control-shell geometry");
        (void)UI.Frame({FInputEvent::PointerMove(60, 70),
            FInputEvent::MouseDown(EMouseButton::Left)}, Display, 1.0 / 60.0);
        Check(UI.GetCapture().bTextEditing && UI.GetCapture().bKeyboard,
            "actual first input-widget click captures keyboard in the same frame");
        (void)UI.Frame({FInputEvent::MouseUp(EMouseButton::Left)}, Display, 1.0 / 60.0);
        (void)Window.WriteClipboardUtf8(Unicode);
#if defined(__APPLE__)
        const auto Shortcut = EKey::LeftSuper;
#else
        const auto Shortcut = EKey::LeftControl;
#endif
        (void)UI.Frame({FInputEvent::KeyDown(Shortcut), FInputEvent::KeyDown(EKey::V)}, Display, 1.0 / 60.0);
        Check(UI.GetClipboardResult() == EApplicationResult::Success && UI.GetText() == Unicode,
            "actual UI paste uses the Application clipboard callback and preserves non-BMP text");
        (void)UI.Frame({FInputEvent::KeyUp(Shortcut), FInputEvent::KeyUp(EKey::V),
            FInputEvent::Text(0x1F642)}, Display, 1.0 / 60.0);
        Check(UI.GetFallbackScalarCount() > 0 && UI.GetText().Len() > Unicode.Len(),
            "committed non-BMP text stays editable while font fallback is diagnosed");
        for (float Scale : {1.0f, 1.5f, 2.0f})
        {
            Display.DrawableExtent = {static_cast<uint32>(1280 * Scale), static_cast<uint32>(720 * Scale)};
            Display.FramebufferScale = {Scale, Scale};
            Display.ContentScale = {Scale, Scale};
            ++Display.DisplayGeneration;
            Check(UI.Frame({}, Display, 1.0 / 60.0) == EApplicationResult::Success && UI.GetVertexCount() > 0,
                "UI logical coordinates accept distinct content and drawable scales");
        }
        Check(UI.Frame({}, Display, 1.0e-300) == EApplicationResult::Success,
            "positive sub-float UI time remains valid without a zero delta assertion");
        auto StaleDisplay = Display;
        --StaleDisplay.DisplayGeneration;
        Check(UI.Frame({}, StaleDisplay, 0.1) == EApplicationResult::InvalidInput && UI.GetVertexCount() == 0,
            "stale UI display generations reject without exposing previous geometry");
        Check(UI.Frame({}, Display, -1.0) == EApplicationResult::InvalidInput,
            "UI rejects invalid time before starting a frame");
    }
    (void)Window.Destroy();
    std::cout << "Application UI input: " << Passed << " passed, "
              << Failed << " failed\n";
    return Failed;
}
