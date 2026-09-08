#pragma once
#include "Application/FInputEvent.h"
#include "Application/FWindowEvent.h"
#include <array>
#include <optional>

namespace Stoner::Application
{
// Shared budget for the sole driver's two typed views of one event interval.
// Close/focus loss have independent latches and survive discarded ordinary data.
class FWindowEventBuffer
{
public:
    static constexpr std::size_t MaximumEvents = 4096;
    void Push(const FInputEvent& Event)
    {
        if (Admit(Event.Sequence)) Input.push_back(Event);
    }
    void Push(const FWindowEvent& Event)
    {
        if (Event.EventType == EWindowEventType::CloseRequested || Event.EventType == EWindowEventType::Destroyed)
        { Close = Event; bClose = true; }
        if (Event.EventType == EWindowEventType::FocusLost || Event.EventType == EWindowEventType::Minimized)
        { Focus = Event; bFocus = true; }
        switch (Event.EventType)
        {
        case EWindowEventType::Resized: Latest[0] = Event; break;
        case EWindowEventType::DrawableResized: Latest[1] = Event; break;
        case EWindowEventType::ContentScaleChanged: Latest[2] = Event; break;
        case EWindowEventType::FocusGained:
        case EWindowEventType::FocusLost: Latest[3] = Event; break;
        case EWindowEventType::Minimized:
        case EWindowEventType::Restored: Latest[4] = Event; break;
        default: break;
        }
        if (Admit(Event.Sequence)) Window.push_back(Event);
    }
    Stoner::Core::TArray<FWindowEvent> TakeWindow()
    {
        auto Result = std::move(Window);
        Window.clear();
        if (bOverflow)
        {
            AppendLatches(Result);
        }
        ClearLatches();
        SortWindowEventsStable(Result);
        return Result;
    }
    Stoner::Core::TArray<FInputEvent> TakeInput()
    {
        auto Result = std::move(Input);
        Input.clear();
        if (bOverflow)
        {
            // Leave any unconsumed critical window latches in their typed view.
            AppendLatches(Window);
            ClearLatches();
            Result.push_back(FInputEvent::Overflow(OverflowSequence));
            bOverflow = false;
        }
        SortInputEventsStable(Result);
        return Result;
    }
private:
    void AppendLatches(Stoner::Core::TArray<FWindowEvent>& Out)
    {
        if (bFocus) Out.push_back(Focus);
        if (bClose) Out.push_back(Close);
        for (const auto& Last : Latest)
            if (Last && !(bFocus && Last->EventType == Focus.EventType && Last->Sequence == Focus.Sequence))
                Out.push_back(*Last);
    }
    void ClearLatches()
    {
        bClose = bFocus = false;
        for (auto& Last : Latest) Last.reset();
    }
    bool Admit(Stoner::Core::uint64 Sequence)
    {
        if (bOverflow) return false;
        if (Window.size() + Input.size() < MaximumEvents) return true;
        OverflowSequence = Sequence;
        Window.clear();
        Input.clear();
        bOverflow = true;
        return false;
    }
    Stoner::Core::TArray<FWindowEvent> Window;
    Stoner::Core::TArray<FInputEvent> Input;
    FWindowEvent Close, Focus;
    std::array<std::optional<FWindowEvent>, 5> Latest;
    Stoner::Core::uint64 OverflowSequence = 0;
    bool bClose = false, bFocus = false, bOverflow = false;
};
}
