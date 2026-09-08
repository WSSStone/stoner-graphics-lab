#include "Application/FInputManager.h"

#include <algorithm>

namespace Stoner::Application
{

void FInputManager::QueueEvent(const FInputEvent& Event)
{
    if (Event.EventType == EInputEventType::Overflow)
    { PendingEvents.clear(); bPendingOverflow = true; }
    if (bPendingOverflow) return;
    if (PendingEvents.size() == MaximumEventsPerInterval)
    {
        PendingEvents.clear();
        bPendingOverflow = true;
        return;
    }
    PendingEvents.push_back(Event);
}

void FInputManager::QueueEvents(const Stoner::Core::TArray<FInputEvent>& Events)
{
    for (const auto& Event : Events) QueueEvent(Event);
}

void FInputManager::BeginFrame()
{
    CurrentState.BeginFrame();
}

void FInputManager::PollFrame(EWindowLifecycleState WindowState, bool bWindowFocused)
{
    BeginFrame();
    FrameEvents.clear();
    bFrameOverflow = bPendingOverflow;
    bPendingOverflow = false;
    if (bFrameOverflow)
    {
        PendingEvents.clear();
        CurrentState.ClearAll();
        FrameEvents.push_back(FInputEvent::Overflow());
        Diagnostics.Add(EApplicationDiagnosticSeverity::Warning,
            EApplicationDiagnosticCategory::Input, EApplicationResult::InvalidInput,
            "APP-INPUT-OVERFLOW", "Input", "Input interval canceled after event budget overflow");
        auto& Records = Diagnostics.GetMutableRecords();
        if (Records.size() > 256) Records.erase(Records.begin());
        return;
    }
    if (WindowState == EWindowLifecycleState::Uncreated || WindowState == EWindowLifecycleState::Destroyed)
    {
        CurrentState.ClearAll();
        Diagnostics.Add(EApplicationDiagnosticSeverity::Warning, EApplicationDiagnosticCategory::Input,
            EApplicationResult::InvalidLifecycle, "APP-INPUT-LIFECYCLE", "InputManager",
            "Polling input outside active window lifecycle returned empty state");
        PendingEvents.clear();
        Diagnostics.SortStable();
        return;
    }

    CurrentState.SetFocused(bWindowFocused);
    if (!bWindowFocused)
    {
        CurrentState.ApplyEvent(FInputEvent::FocusLost(), &Diagnostics);
    }

    std::size_t TextBytes = 0;
    std::erase_if(PendingEvents, [&](const FInputEvent& Event) {
        if (Event.EventType != EInputEventType::Text) return false;
        const auto Scalar = Event.UnicodeScalar;
        if (Scalar == 0 || Scalar > 0x10FFFF || (Scalar >= 0xD800 && Scalar <= 0xDFFF))
            return true;
        TextBytes += Scalar <= 0x7F ? 1 : Scalar <= 0x7FF ? 2 : Scalar <= 0xFFFF ? 3 : 4;
        return false;
    });
    if (TextBytes > 4096)
    {
        std::erase_if(PendingEvents, [](const auto& Event) { return Event.EventType == EInputEventType::Text; });
        Diagnostics.Add(EApplicationDiagnosticSeverity::Warning,
            EApplicationDiagnosticCategory::Input, EApplicationResult::InvalidInput,
            "APP-INPUT-TEXT-BUDGET", "Text", "Committed text interval exceeds 4096 UTF-8 bytes");
    }
    SortInputEventsStable(PendingEvents);
    for (const FInputEvent& Event : PendingEvents)
    {
        CurrentState.ApplyEvent(Event, &Diagnostics);
    }
    FrameEvents = std::move(PendingEvents);
    PendingEvents.clear();
    auto& Records = Diagnostics.GetMutableRecords();
    if (Records.size() > 256) Records.erase(Records.begin(), Records.end() - 256);
    Diagnostics.SortStable();
}

void FInputManager::Clear()
{
    PendingEvents.clear();
    FrameEvents.clear();
    bPendingOverflow = bFrameOverflow = false;
    CurrentState.ClearAll();
    Diagnostics.Clear();
}

} // namespace Stoner::Application
