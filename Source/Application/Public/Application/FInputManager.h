#pragma once

#include "Application/FInputState.h"
#include "Application/FWindowDesc.h"

namespace Stoner::Application
{

class FInputManager
{
public:
    static constexpr std::size_t MaximumEventsPerInterval = 4096;
    [[nodiscard]] bool DidOverflow() const noexcept { return bFrameOverflow; }
    [[nodiscard]] const Stoner::Core::TArray<FInputEvent>& GetFrameEvents() const noexcept { return FrameEvents; }

    void QueueEvent(const FInputEvent& Event);
    void QueueEvents(const Stoner::Core::TArray<FInputEvent>& Events);
    void BeginFrame();
    void PollFrame(EWindowLifecycleState WindowState, bool bWindowFocused = true);
    void Clear();

    [[nodiscard]] const FInputState& GetState() const noexcept { return CurrentState; }
    [[nodiscard]] const FApplicationDiagnosticLog& GetDiagnostics() const noexcept { return Diagnostics; }
    [[nodiscard]] FApplicationDiagnosticLog& GetMutableDiagnostics() noexcept { return Diagnostics; }
    [[nodiscard]] bool HasPendingEvents() const noexcept { return !PendingEvents.empty(); }

private:
    Stoner::Core::TArray<FInputEvent> PendingEvents;
    Stoner::Core::TArray<FInputEvent> FrameEvents;
    bool bPendingOverflow = false;
    bool bFrameOverflow = false;
    FInputState CurrentState;
    FApplicationDiagnosticLog Diagnostics;
};

} // namespace Stoner::Application
