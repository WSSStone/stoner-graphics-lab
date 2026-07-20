#include "Application/FWindow.h"

#include "FWindowDriver.h"

namespace Stoner::Application
{

FWindow::FWindow() = default;
FWindow::~FWindow() { if (Driver) Driver->Destroy(); }

namespace
{

Stoner::Core::uint32 NextStableWindowId()
{
    static Stoner::Core::uint32 NextId = 1;
    return NextId++;
}

} // namespace

bool FWindowDesc::IsValid(FApplicationDiagnosticLog* Diagnostics) const
{
    bool bValid = true;
    if (Title.IsEmpty())
    {
        bValid = false;
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EApplicationDiagnosticSeverity::Error, EApplicationDiagnosticCategory::Validation,
                EApplicationResult::ValidationFailed, "APP-WINDOW-TITLE", "Title", "Window title must not be empty");
        }
    }
    if (ClientWidth == 0 || ClientHeight == 0)
    {
        bValid = false;
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EApplicationDiagnosticSeverity::Error, EApplicationDiagnosticCategory::Validation,
                EApplicationResult::ValidationFailed, "APP-WINDOW-SIZE-ZERO", "ClientSize", "Window dimensions must be positive");
        }
    }
    if (ClientWidth > MaxClientWidth || ClientHeight > MaxClientHeight)
    {
        bValid = false;
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EApplicationDiagnosticSeverity::Error, EApplicationDiagnosticCategory::Validation,
                EApplicationResult::ValidationFailed, "APP-WINDOW-SIZE-LIMIT", "ClientSize", "Window dimensions exceed v1 safe maximum");
        }
    }
    if (Diagnostics != nullptr)
    {
        Diagnostics->SortStable();
    }
    return bValid;
}

EApplicationResult FWindow::Create(const FWindowDesc& InDesc, EWindowRuntimeAvailability RuntimeAvailability)
{
    Diagnostics.Clear();
    PendingEvents.clear();
    if (RuntimeAvailability != EWindowRuntimeAvailability::Available)
    {
        LifecycleState = EWindowLifecycleState::Uncreated;
        WindowId = 0;
        Diagnostics.Add(EApplicationDiagnosticSeverity::Error, EApplicationDiagnosticCategory::RuntimeAvailability,
            EApplicationResult::RuntimeUnavailable, "APP-WINDOW-RUNTIME", "WindowDriver", ToString(RuntimeAvailability));
        Diagnostics.SortStable();
        return EApplicationResult::RuntimeUnavailable;
    }
    if (!InDesc.IsValid(&Diagnostics))
    {
        LifecycleState = EWindowLifecycleState::Uncreated;
        WindowId = 0;
        return EApplicationResult::ValidationFailed;
    }

    Desc = InDesc;
    WindowId = NextStableWindowId();
    LifecycleState = EWindowLifecycleState::Active;
    DisplayMode = Desc.DisplayMode;
    ClientWidth = Desc.ClientWidth;
    ClientHeight = Desc.ClientHeight;
    bVisible = Desc.bVisible;
    bFocused = true;
    bMinimized = false;
    UpdateDrawableState();
    QueueEvent(FWindowEvent::Created(WindowId, ClientWidth, ClientHeight, NextSequence++));
    Diagnostics.Add(EApplicationDiagnosticSeverity::Info, EApplicationDiagnosticCategory::Window,
        EApplicationResult::Success, "APP-WINDOW-CREATED", Desc.DebugName, "Primary window created");
    Diagnostics.SortStable();
    return EApplicationResult::Success;
}

EApplicationResult FWindow::CreateRealWindow(const FWindowDesc& InDesc, EWindowRuntimeAvailability RuntimeAvailability)
{
    if (RuntimeAvailability != EWindowRuntimeAvailability::Available)
        return Create(InDesc, RuntimeAvailability);
    Driver = CreateGlfwWindowDriver();
    if (!Driver || Driver->GetRuntimeAvailability() != EWindowRuntimeAvailability::Available)
    {
        Driver.reset();
        return Create(InDesc, EWindowRuntimeAvailability::DependencyUnavailable);
    }
    if (!InDesc.IsValid(&Diagnostics)) return EApplicationResult::ValidationFailed;
    const Stoner::Core::uint32 StableId = NextStableWindowId();
    const EApplicationResult DriverResult = Driver->Create(InDesc, StableId);
    if (DriverResult != EApplicationResult::Success)
    {
        Driver.reset();
        return DriverResult;
    }
    Desc = InDesc;
    WindowId = StableId;
    LifecycleState = EWindowLifecycleState::Active;
    DisplayMode = InDesc.DisplayMode;
    ClientWidth = InDesc.ClientWidth;
    ClientHeight = InDesc.ClientHeight;
    DrawableWidth = Driver->GetDrawableWidth();
    DrawableHeight = Driver->GetDrawableHeight();
    PlatformWindow = Driver->GetPlatformWindow();
    bVisible = InDesc.bVisible;
    bFocused = true;
    bMinimized = DrawableWidth == 0 || DrawableHeight == 0;
    UpdateDrawableState();
    QueueEvent(FWindowEvent::Created(WindowId, ClientWidth, ClientHeight, NextSequence++));
    return EApplicationResult::Success;
}

EApplicationResult FWindow::RequestClose()
{
    if (LifecycleState == EWindowLifecycleState::Uncreated || LifecycleState == EWindowLifecycleState::Destroyed)
    {
        Diagnostics.Add(EApplicationDiagnosticSeverity::Warning, EApplicationDiagnosticCategory::Window,
            EApplicationResult::InvalidLifecycle, "APP-WINDOW-CLOSE-LIFECYCLE", "Window", "Close requested outside active lifecycle");
        Diagnostics.SortStable();
        return EApplicationResult::InvalidLifecycle;
    }
    LifecycleState = EWindowLifecycleState::CloseRequested;
    if (Driver) Driver->RequestClose();
    QueueEvent(FWindowEvent::CloseRequested(NextSequence++));
    return EApplicationResult::Success;
}

EApplicationResult FWindow::Destroy()
{
    if (LifecycleState == EWindowLifecycleState::Destroyed)
    {
        Diagnostics.Add(EApplicationDiagnosticSeverity::Info, EApplicationDiagnosticCategory::Window,
            EApplicationResult::Success, "APP-WINDOW-DESTROY-IDEMPOTENT", "Window", "Repeated destroy ignored safely");
        Diagnostics.SortStable();
        return EApplicationResult::Success;
    }
    if (Driver) Driver->Destroy();
    Driver.reset();
    PlatformWindow.Clear();
    LifecycleState = EWindowLifecycleState::Destroyed;
    bVisible = false;
    bFocused = false;
    bMinimized = false;
    UpdateDrawableState();
    QueueEvent(FWindowEvent::Destroyed(NextSequence++));
    Diagnostics.Add(EApplicationDiagnosticSeverity::Info, EApplicationDiagnosticCategory::Window,
        EApplicationResult::Success, "APP-WINDOW-DESTROYED", "Window", "Primary window destroyed");
    Diagnostics.SortStable();
    return EApplicationResult::Success;
}

EApplicationResult FWindow::SetDisplayMode(EWindowDisplayMode NewMode, bool bRuntimeAllowsMode)
{
    if (!IsActive())
    {
        Diagnostics.Add(EApplicationDiagnosticSeverity::Warning, EApplicationDiagnosticCategory::Window,
            EApplicationResult::InvalidLifecycle, "APP-WINDOW-MODE-LIFECYCLE", "DisplayMode", "Display mode change outside active lifecycle");
        Diagnostics.SortStable();
        return EApplicationResult::InvalidLifecycle;
    }
    if (!bRuntimeAllowsMode)
    {
        Diagnostics.Add(EApplicationDiagnosticSeverity::Warning, EApplicationDiagnosticCategory::Window,
            EApplicationResult::UnsupportedMode, "APP-WINDOW-MODE-UNSUPPORTED", "DisplayMode", "Display mode change rejected and previous state preserved");
        Diagnostics.SortStable();
        return EApplicationResult::UnsupportedMode;
    }
    DisplayMode = NewMode;
    Diagnostics.Add(EApplicationDiagnosticSeverity::Info, EApplicationDiagnosticCategory::Window,
        EApplicationResult::Success, "APP-WINDOW-MODE-CHANGED", "DisplayMode", ToString(NewMode));
    Diagnostics.SortStable();
    return EApplicationResult::Success;
}

void FWindow::QueueEvent(const FWindowEvent& Event)
{
    FWindowEvent Copy = Event;
    if (Copy.Sequence == 0)
    {
        Copy.Sequence = NextSequence++;
    }
    PendingEvents.push_back(Copy);
}

Stoner::Core::TArray<FWindowEvent> FWindow::PollEvents()
{
    if (Driver)
    {
        Driver->Poll();
        for (const FWindowEvent& Event : Driver->ConsumeWindowEvents()) QueueEvent(Event);
        DrawableWidth = Driver->GetDrawableWidth();
        DrawableHeight = Driver->GetDrawableHeight();
    }
    SortWindowEventsStable(PendingEvents);
    Stoner::Core::TArray<FWindowEvent> Events = PendingEvents;
    PendingEvents.clear();
    for (const FWindowEvent& Event : Events)
    {
        ApplyEvent(Event);
    }
    return Events;
}

Stoner::Core::TArray<FInputEvent> FWindow::PollInputEvents()
{
    if (!Driver) return {};
    Driver->Poll();
    return Driver->ConsumeInputEvents();
}

void FWindow::ApplyEvent(const FWindowEvent& Event)
{
    switch (Event.EventType)
    {
    case EWindowEventType::Created:
        break;
    case EWindowEventType::Resized:
        ClientWidth = Event.ClientWidth;
        ClientHeight = Event.ClientHeight;
        bMinimized = false;
        DrawableWidth = Event.ClientWidth;
        DrawableHeight = Event.ClientHeight;
        UpdateDrawableState();
        break;
    case EWindowEventType::Minimized:
        bMinimized = true;
        ClientWidth = 0;
        ClientHeight = 0;
        DrawableWidth = 0;
        DrawableHeight = 0;
        UpdateDrawableState();
        Diagnostics.Add(EApplicationDiagnosticSeverity::Info, EApplicationDiagnosticCategory::Loop,
            EApplicationResult::Success, "APP-WINDOW-PRESENTATION-PAUSED", "Window", "Window has no drawable area");
        break;
    case EWindowEventType::Restored:
        bMinimized = false;
        ClientWidth = Event.ClientWidth;
        ClientHeight = Event.ClientHeight;
        DrawableWidth = Event.ClientWidth;
        DrawableHeight = Event.ClientHeight;
        UpdateDrawableState();
        break;
    case EWindowEventType::FocusGained:
        bFocused = true;
        break;
    case EWindowEventType::FocusLost:
        bFocused = false;
        break;
    case EWindowEventType::CloseRequested:
        if (LifecycleState == EWindowLifecycleState::Active)
        {
            LifecycleState = EWindowLifecycleState::CloseRequested;
        }
        break;
    case EWindowEventType::Destroyed:
        LifecycleState = EWindowLifecycleState::Destroyed;
        bVisible = false;
        bFocused = false;
        bMinimized = false;
        UpdateDrawableState();
        break;
    case EWindowEventType::UnavailableRuntime:
        Diagnostics.Add(EApplicationDiagnosticSeverity::Error, EApplicationDiagnosticCategory::RuntimeAvailability,
            EApplicationResult::RuntimeUnavailable, "APP-WINDOW-RUNTIME", "WindowDriver", Event.Message);
        break;
    }
    Diagnostics.SortStable();
}

void FWindow::UpdateDrawableState()
{
    if (!Driver)
    {
        DrawableWidth = ClientWidth;
        DrawableHeight = ClientHeight;
    }
    bDrawable = LifecycleState != EWindowLifecycleState::Destroyed && !bMinimized && DrawableWidth > 0 && DrawableHeight > 0;
    bPresentationPaused = !bDrawable && LifecycleState != EWindowLifecycleState::Destroyed;
}

const char* ToString(EWindowLifecycleState State) noexcept
{
    switch (State)
    {
    case EWindowLifecycleState::Uncreated: return "Uncreated";
    case EWindowLifecycleState::Active: return "Active";
    case EWindowLifecycleState::CloseRequested: return "CloseRequested";
    case EWindowLifecycleState::Destroyed: return "Destroyed";
    }
    return "Unknown";
}

const char* ToString(EWindowDisplayMode Mode) noexcept
{
    switch (Mode)
    {
    case EWindowDisplayMode::Windowed: return "Windowed";
    case EWindowDisplayMode::Fullscreen: return "Fullscreen";
    }
    return "Unknown";
}

const char* ToString(EWindowRuntimeAvailability Availability) noexcept
{
    switch (Availability)
    {
    case EWindowRuntimeAvailability::Available: return "Available";
    case EWindowRuntimeAvailability::DisplayUnavailable: return "DisplayUnavailable";
    case EWindowRuntimeAvailability::DependencyUnavailable: return "DependencyUnavailable";
    case EWindowRuntimeAvailability::Failed: return "Failed";
    }
    return "Unknown";
}

} // namespace Stoner::Application
