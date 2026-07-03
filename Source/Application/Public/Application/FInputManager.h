#pragma once

#include "Application/FInputState.h"
#include "Application/FWindowDesc.h"

namespace Stoner::Application
{

class FInputManager
{
public:
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
    FInputState CurrentState;
    FApplicationDiagnosticLog Diagnostics;
};

} // namespace Stoner::Application
