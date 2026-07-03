#include "Application/FInputEvent.h"
#include "Application/FWindowEvent.h"
#include "FWindowDriver.h"

namespace Stoner::Application
{

class FHeadlessWindowDriver final : public IWindowDriver
{
public:
    [[nodiscard]] const char* GetDriverName() const noexcept override { return "Headless"; }
    [[nodiscard]] EWindowRuntimeAvailability GetRuntimeAvailability() const noexcept override { return EWindowRuntimeAvailability::Available; }

    void QueueWindowEvent(const FWindowEvent& Event) { WindowEvents.push_back(Event); }
    void QueueInputEvent(const FInputEvent& Event) { InputEvents.push_back(Event); }

    [[nodiscard]] Stoner::Core::TArray<FWindowEvent> ConsumeWindowEvents() override
    {
        SortWindowEventsStable(WindowEvents);
        Stoner::Core::TArray<FWindowEvent> Result = WindowEvents;
        WindowEvents.clear();
        return Result;
    }

    [[nodiscard]] Stoner::Core::TArray<FInputEvent> ConsumeInputEvents() override
    {
        SortInputEventsStable(InputEvents);
        Stoner::Core::TArray<FInputEvent> Result = InputEvents;
        InputEvents.clear();
        return Result;
    }

private:
    Stoner::Core::TArray<FWindowEvent> WindowEvents;
    Stoner::Core::TArray<FInputEvent> InputEvents;
};

} // namespace Stoner::Application
