#include "Application/FWindow.h"
#include <cmath>

#include "FWindowDriver.h"

#include <algorithm>
#include <limits>

namespace Stoner::Application
{

FWindow::FWindow() = default;
FWindow::~FWindow() { if (Driver) Driver->Destroy(); }

void FWindowTestAccess::InstallDriver(FWindow& Window, std::unique_ptr<IWindowDriver> Driver)
{
    Window.Driver = std::move(Driver);
}

namespace
{

Stoner::Core::uint32 NextStableWindowId()
{
    static Stoner::Core::uint32 NextId = 1;
    return NextId++;
}

[[nodiscard]] bool IsValidCursorMode(ECursorMode Mode) noexcept
{
    return Mode == ECursorMode::Normal || Mode == ECursorMode::Disabled;
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
    PendingInputEvents.clear();
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
    ValidationContentScale=0;
    WindowId = NextStableWindowId();
    LifecycleState = EWindowLifecycleState::Active;
    DisplayMode = Desc.DisplayMode;
    ClientWidth = Desc.ClientWidth;
    ClientHeight = Desc.ClientHeight;
    DrawableWidth = ClientWidth;
    DrawableHeight = ClientHeight;
    bVisible = Desc.bVisible;
    bFocused = true;
    bMinimized = false;
    CursorMode = ECursorMode::Normal;
    ContentScaleX = 1.0f;
    ContentScaleY = 1.0f;
    FramebufferScaleX = 1.0f;
    FramebufferScaleY = 1.0f;
    DisplayGeneration = 1;
    UpdateDrawableState();
    QueueEvent(FWindowEvent::Created(WindowId, ClientWidth, ClientHeight, NextSequence++));
    Diagnostics.Add(EApplicationDiagnosticSeverity::Info, EApplicationDiagnosticCategory::Window,
        EApplicationResult::Success, "APP-WINDOW-CREATED", Desc.DebugName, "Primary window created");
    Diagnostics.SortStable();
    return EApplicationResult::Success;
}

EApplicationResult FWindow::CreateRealWindow(const FWindowDesc& InDesc, EWindowRuntimeAvailability RuntimeAvailability)
{
    Diagnostics.Clear();
    PendingEvents.clear();
    PendingInputEvents.clear();
    if (RuntimeAvailability != EWindowRuntimeAvailability::Available)
        return Create(InDesc, RuntimeAvailability);
    if (!Driver) Driver = CreateGlfwWindowDriver();
    if (!Driver || Driver->GetRuntimeAvailability() != EWindowRuntimeAvailability::Available)
    {
        ResetRuntimeState();
        return Create(InDesc, EWindowRuntimeAvailability::DependencyUnavailable);
    }
    if (!InDesc.IsValid(&Diagnostics))
    {
        ResetRuntimeState();
        return EApplicationResult::ValidationFailed;
    }
    const Stoner::Core::uint32 StableId = NextStableWindowId();
    const EApplicationResult DriverResult = Driver->Create(InDesc, StableId);
    if (DriverResult != EApplicationResult::Success)
    {
        ResetRuntimeState();
        return DriverResult;
    }
    Desc = InDesc;
    ValidationContentScale=0;
    WindowId = StableId;
    LifecycleState = EWindowLifecycleState::Active;
    DisplayMode = InDesc.DisplayMode;
    ClientWidth = InDesc.ClientWidth;
    ClientHeight = InDesc.ClientHeight;
    DrawableWidth = Driver->GetDrawableWidth();
    DrawableHeight = Driver->GetDrawableHeight();
    ContentScaleX = Driver->GetContentScaleX();
    ContentScaleY = Driver->GetContentScaleY();
    PlatformWindow = Driver->GetPlatformWindow();
    bVisible = InDesc.bVisible;
    bFocused = true;
    // A zero native drawable can occur without an iconify/minimize event.
    // Keep the native minimized fact independent from presentation pause.
    bMinimized = false;
    CursorMode = ECursorMode::Normal;
    FramebufferScaleX = 1.0f;
    FramebufferScaleY = 1.0f;
    DisplayGeneration = 1;
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
    ClearPointerCapture();
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
    if (Driver) (void)Driver->SetCursorMode(ECursorMode::Normal);
    CursorMode = ECursorMode::Normal;
    if (Driver) Driver->Destroy();
    Driver.reset();
    PlatformWindow.Clear();
    LifecycleState = EWindowLifecycleState::Destroyed;
    bVisible = false;
    bFocused = false;
    bMinimized = false;
    UpdateDrawableState();
    // The native driver cannot emit its callback reset after it has been
    // destroyed, so make the engine-initiated teardown reset observable too.
    ClearPointerCapture();
    QueueEvent(FWindowEvent::Destroyed(NextSequence++));
    Diagnostics.Add(EApplicationDiagnosticSeverity::Info, EApplicationDiagnosticCategory::Window,
        EApplicationResult::Success, "APP-WINDOW-DESTROYED", "Window", "Primary window destroyed");
    Diagnostics.SortStable();
    return EApplicationResult::Success;
}

EApplicationResult FWindow::SetCursorMode(ECursorMode NewMode)
{
    if (!IsValidCursorMode(NewMode))
        return EApplicationResult::InvalidInput;
    if (LifecycleState == EWindowLifecycleState::Uncreated ||
        LifecycleState == EWindowLifecycleState::Destroyed)
        return EApplicationResult::InvalidLifecycle;
    if (NewMode == ECursorMode::Disabled &&
        (LifecycleState != EWindowLifecycleState::Active || !bFocused ||
            bMinimized || !bDrawable))
        return EApplicationResult::InvalidLifecycle;
    if (Driver)
    {
        const EApplicationResult Result = Driver->SetCursorMode(NewMode);
        if (Result != EApplicationResult::Success) return Result;
    }
    CursorMode = NewMode;
    return EApplicationResult::Success;
}

FWindowDisplayState FWindow::GetDisplayState() const noexcept
{
    FWindowDisplayState State;
    State.LogicalExtent = {ClientWidth, ClientHeight};
    State.DrawableExtent = {DrawableWidth, DrawableHeight};
    State.ContentScale = {ContentScaleX, ContentScaleY};
    State.FramebufferScale = {FramebufferScaleX, FramebufferScaleY};
    State.DisplayGeneration = DisplayGeneration;
    State.bFocused = bFocused;
    State.bMinimized = bMinimized;
    return State;
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

EApplicationResult FWindow::SetClientSize(
    Stoner::Core::uint32 Width, Stoner::Core::uint32 Height)
{
    if (!IsActive() || !Driver) return EApplicationResult::InvalidLifecycle;
    if (Width == 0 || Height == 0 || Width > FWindowDesc::MaxClientWidth ||
        Height > FWindowDesc::MaxClientHeight)
        return EApplicationResult::InvalidInput;
    return Driver->SetClientSize(Width, Height);
}

EApplicationResult FWindow::Minimize()
{
    if (!IsActive() || !Driver) return EApplicationResult::InvalidLifecycle;
    return Driver->Minimize();
}

EApplicationResult FWindow::Restore()
{
    if (!IsActive() || !Driver) return EApplicationResult::InvalidLifecycle;
    return Driver->Restore();
}

EApplicationResult FWindow::SetValidationContentScale(float Scale)
{
    if (!IsActive() || !Desc.bValidationOverrides) return EApplicationResult::InvalidLifecycle;
    if (!std::isfinite(Scale) || Scale<0.5f || Scale>4.0f) return EApplicationResult::InvalidInput;
    ValidationContentScale=Scale;
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
    const Stoner::Core::uint32 PreviousDrawableWidth = DrawableWidth;
    const Stoner::Core::uint32 PreviousDrawableHeight = DrawableHeight;
    const float PreviousContentScaleX = ContentScaleX;
    const float PreviousContentScaleY = ContentScaleY;
    if (Driver)
    {
        Driver->Poll();
        for (const FWindowEvent& Event : Driver->ConsumeWindowEvents()) QueueEvent(Event);
    }
    SortWindowEventsStable(PendingEvents);
    Stoner::Core::TArray<FWindowEvent> Events = PendingEvents;
    PendingEvents.clear();
    for (const FWindowEvent& Event : Events)
    {
        ApplyEvent(Event);
    }
    if (Driver)
    {
        if (!bMinimized)
        {
            DrawableWidth = Driver->GetDrawableWidth();
            DrawableHeight = Driver->GetDrawableHeight();
        }
        ContentScaleX = ValidationContentScale>0 ? ValidationContentScale : Driver->GetContentScaleX();
        ContentScaleY = ValidationContentScale>0 ? ValidationContentScale : Driver->GetContentScaleY();
        if (DrawableWidth != PreviousDrawableWidth ||
            DrawableHeight != PreviousDrawableHeight)
        {
            UpdateDrawableState();
            BumpDisplayGeneration();
        }
        if (ContentScaleX != PreviousContentScaleX ||
            ContentScaleY != PreviousContentScaleY)
            BumpDisplayGeneration();
    }
    return Events;
}

Stoner::Core::TArray<FInputEvent> FWindow::PollInputEvents()
{
    Stoner::Core::TArray<FInputEvent> Events = std::move(PendingInputEvents);
    PendingInputEvents.clear();
    if (Driver)
    {
        Driver->Poll();
        const auto DriverEvents = Driver->ConsumeInputEvents();
        Events.insert(Events.end(), DriverEvents.begin(), DriverEvents.end());
    }
    SortInputEventsStable(Events);
    if (std::any_of(Events.begin(), Events.end(), [](const FInputEvent& Event)
        {
            return Event.EventType == EInputEventType::FocusLost || Event.EventType == EInputEventType::Overflow;
        }))
    {
        CursorMode = ECursorMode::Normal;
        if (Driver) (void)Driver->SetCursorMode(ECursorMode::Normal);
    }
    return Events;
}

void FWindow::ApplyEvent(const FWindowEvent& Event)
{
    switch (Event.EventType)
    {
    case EWindowEventType::Created:
        break;
    case EWindowEventType::DisplayCapabilitiesChanged:
        BumpDisplayGeneration();
        break;
    case EWindowEventType::Resized:
        ClientWidth = Event.ClientWidth;
        ClientHeight = Event.ClientHeight;
        bMinimized = false;
        if (!Driver)
        {
            DrawableWidth = Event.ClientWidth;
            DrawableHeight = Event.ClientHeight;
        }
        UpdateDrawableState();
        BumpDisplayGeneration();
        break;
    case EWindowEventType::DrawableResized:
        DrawableWidth = Event.ClientWidth;
        DrawableHeight = Event.ClientHeight;
        UpdateDrawableState();
        if (DrawableWidth == 0 || DrawableHeight == 0)
            ClearPointerCapture(Event.Sequence);
        BumpDisplayGeneration();
        break;
    case EWindowEventType::ContentScaleChanged:
        if (std::isfinite(Event.ContentScaleX) && std::isfinite(Event.ContentScaleY) &&
            Event.ContentScaleX > 0.0f && Event.ContentScaleY > 0.0f)
        {
            ContentScaleX = Event.ContentScaleX;
            ContentScaleY = Event.ContentScaleY;
            BumpDisplayGeneration();
        }
        break;
    case EWindowEventType::Minimized:
        bMinimized = true;
        DrawableWidth = 0;
        DrawableHeight = 0;
        UpdateDrawableState();
        ClearPointerCapture(Event.Sequence);
        BumpDisplayGeneration();
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
        BumpDisplayGeneration();
        break;
    case EWindowEventType::FocusGained:
        bFocused = true;
        BumpDisplayGeneration();
        break;
    case EWindowEventType::FocusLost:
        bFocused = false;
        ClearPointerCapture(Event.Sequence);
        BumpDisplayGeneration();
        break;
    case EWindowEventType::CloseRequested:
    {
        const bool bAlreadyClosing =
            LifecycleState == EWindowLifecycleState::CloseRequested;
        if (LifecycleState == EWindowLifecycleState::Active)
        {
            LifecycleState = EWindowLifecycleState::CloseRequested;
        }
        if (!bAlreadyClosing) ClearPointerCapture(Event.Sequence);
        BumpDisplayGeneration();
        break;
    }
    case EWindowEventType::Destroyed:
        LifecycleState = EWindowLifecycleState::Destroyed;
        bVisible = false;
        bFocused = false;
        bMinimized = false;
        UpdateDrawableState();
        ClearPointerCapture(Event.Sequence);
        BumpDisplayGeneration();
        break;
    case EWindowEventType::UnavailableRuntime:
        Diagnostics.Add(EApplicationDiagnosticSeverity::Error, EApplicationDiagnosticCategory::RuntimeAvailability,
            EApplicationResult::RuntimeUnavailable, "APP-WINDOW-RUNTIME", "WindowDriver", Event.Message);
        break;
    }
    Diagnostics.SortStable();
}

void FWindow::ResetRuntimeState()
{
    if (Driver)
    {
        Driver->Destroy();
    }
    Driver.reset();
    PlatformWindow.Clear();
    WindowId = 0;
    LifecycleState = EWindowLifecycleState::Uncreated;
    DisplayMode = EWindowDisplayMode::Windowed;
    CursorMode = ECursorMode::Normal;
    ClientWidth = 0;
    ClientHeight = 0;
    DrawableWidth = 0;
    DrawableHeight = 0;
    bVisible = false;
    bFocused = false;
    bMinimized = false;
    bDrawable = false;
    bPresentationPaused = false;
    ContentScaleX = 1.0f;
    ContentScaleY = 1.0f;
    FramebufferScaleX = 1.0f;
    FramebufferScaleY = 1.0f;
    DisplayGeneration = 0;
}

void FWindow::UpdateDrawableState()
{
    if (ClientWidth > 0 && ClientHeight > 0 &&
        DrawableWidth > 0 && DrawableHeight > 0)
    {
        FramebufferScaleX = static_cast<float>(DrawableWidth) /
            static_cast<float>(ClientWidth);
        FramebufferScaleY = static_cast<float>(DrawableHeight) /
            static_cast<float>(ClientHeight);
    }
    bDrawable = LifecycleState != EWindowLifecycleState::Destroyed && !bMinimized && DrawableWidth > 0 && DrawableHeight > 0;
    bPresentationPaused = !bDrawable && LifecycleState != EWindowLifecycleState::Destroyed;
}

void FWindow::ClearPointerCapture(Stoner::Core::uint64 Sequence)
{
    if (Driver) (void)Driver->SetCursorMode(ECursorMode::Normal);
    CursorMode = ECursorMode::Normal;
    if (Driver && Driver->EmitsLifecycleInputResets()) return;
    const Stoner::Core::uint64 ResetSequence =
        Sequence == 0 ? NextSequence++ : Sequence;
    if (std::any_of(PendingInputEvents.begin(), PendingInputEvents.end(),
        [ResetSequence](const FInputEvent& Event)
        {
            return Event.EventType == EInputEventType::FocusLost &&
                Event.Sequence == ResetSequence;
        }))
        return;
    PendingInputEvents.push_back(FInputEvent::FocusLost(ResetSequence));
}

void FWindow::BumpDisplayGeneration() noexcept
{
    if (DisplayGeneration == 0)
    {
        DisplayGeneration = 1;
    }
    else if (DisplayGeneration !=
        std::numeric_limits<Stoner::Core::uint64>::max())
    {
        ++DisplayGeneration;
    }
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
