#include "Application/FInputEvent.h"
#include "Application/FWindowEvent.h"
#include "FWindowDriver.h"
#include "FWindowEventBuffer.h"

namespace Stoner::Application
{

class FHeadlessWindowDriver final : public IWindowDriver
{
public:
    [[nodiscard]] const char* GetDriverName() const noexcept override { return "Headless"; }
    [[nodiscard]] EWindowRuntimeAvailability GetRuntimeAvailability() const noexcept override { return EWindowRuntimeAvailability::Available; }
    EApplicationResult SetCursorMode(ECursorMode NewMode) override
    {
        if (NewMode != ECursorMode::Normal && NewMode != ECursorMode::Disabled)
            return EApplicationResult::InvalidInput;
        CursorMode = NewMode;
        return EApplicationResult::Success;
    }

    EApplicationResult ReadClipboardUtf8(Stoner::Core::FString& Out) override
    { Out = Clipboard; return EApplicationResult::Success; }
    EApplicationResult WriteClipboardUtf8(const Stoner::Core::FString& Text) override
    { Clipboard = Text; return EApplicationResult::Success; }

    void QueueWindowEvent(const FWindowEvent& Event) { Events.Push(Event); }
    void QueueInputEvent(const FInputEvent& Event) { Events.Push(Event); }

    [[nodiscard]] Stoner::Core::TArray<FWindowEvent> ConsumeWindowEvents() override
    { return Events.TakeWindow(); }
    [[nodiscard]] Stoner::Core::TArray<FInputEvent> ConsumeInputEvents() override
    { return Events.TakeInput(); }

private:
    FWindowEventBuffer Events;
    Stoner::Core::FString Clipboard;
    ECursorMode CursorMode = ECursorMode::Normal;
};

std::unique_ptr<IWindowDriver> CreateHeadlessWindowDriver()
{
    return std::make_unique<FHeadlessWindowDriver>();
}

} // namespace Stoner::Application
