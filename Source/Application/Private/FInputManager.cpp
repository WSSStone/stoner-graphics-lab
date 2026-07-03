#include "Application/FInputManager.h"

#include <algorithm>

namespace Stoner::Application
{

void FInputManager::QueueEvent(const FInputEvent& Event)
{
    PendingEvents.push_back(Event);
}

void FInputManager::QueueEvents(const Stoner::Core::TArray<FInputEvent>& Events)
{
    PendingEvents.insert(PendingEvents.end(), Events.begin(), Events.end());
}

void FInputManager::BeginFrame()
{
    CurrentState.BeginFrame();
}

void FInputManager::PollFrame(EWindowLifecycleState WindowState, bool bWindowFocused)
{
    BeginFrame();
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

    if (!bWindowFocused)
    {
        CurrentState.ApplyEvent(FInputEvent::FocusLost(), &Diagnostics);
    }

    SortInputEventsStable(PendingEvents);
    for (const FInputEvent& Event : PendingEvents)
    {
        CurrentState.ApplyEvent(Event, &Diagnostics);
    }
    PendingEvents.clear();
    Diagnostics.SortStable();
}

void FInputManager::Clear()
{
    PendingEvents.clear();
    CurrentState.ClearAll();
    Diagnostics.Clear();
}

} // namespace Stoner::Application
