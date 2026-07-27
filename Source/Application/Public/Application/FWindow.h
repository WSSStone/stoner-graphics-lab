#pragma once

#include "Application/FWindowEvent.h"
#include "Application/FInputEvent.h"
#include "Core/FPlatformWindow.h"

#include <memory>

namespace Stoner::Application
{

class IWindowDriver;
class FWindowTestAccess;

class FWindow
{
public:
    FWindow();
    ~FWindow();
    FWindow(const FWindow&) = delete;
    FWindow& operator=(const FWindow&) = delete;
    [[nodiscard]] EApplicationResult Create(const FWindowDesc& InDesc,
        EWindowRuntimeAvailability RuntimeAvailability = EWindowRuntimeAvailability::Available);
    [[nodiscard]] EApplicationResult CreateRealWindow(const FWindowDesc& InDesc,
        EWindowRuntimeAvailability RuntimeAvailability = EWindowRuntimeAvailability::Available);
    [[nodiscard]] EApplicationResult RequestClose();
    [[nodiscard]] EApplicationResult Destroy();
    [[nodiscard]] EApplicationResult SetDisplayMode(EWindowDisplayMode NewMode, bool bRuntimeAllowsMode = true);

    void QueueEvent(const FWindowEvent& Event);
    Stoner::Core::TArray<FWindowEvent> PollEvents();
    Stoner::Core::TArray<FInputEvent> PollInputEvents();
    void ApplyEvent(const FWindowEvent& Event);

    [[nodiscard]] Stoner::Core::uint32 GetWindowId() const noexcept { return WindowId; }
    [[nodiscard]] const FWindowDesc& GetDescription() const noexcept { return Desc; }
    [[nodiscard]] EWindowLifecycleState GetLifecycleState() const noexcept { return LifecycleState; }
    [[nodiscard]] EWindowDisplayMode GetDisplayMode() const noexcept { return DisplayMode; }
    [[nodiscard]] Stoner::Core::uint32 GetClientWidth() const noexcept { return ClientWidth; }
    [[nodiscard]] Stoner::Core::uint32 GetClientHeight() const noexcept { return ClientHeight; }
    [[nodiscard]] Stoner::Core::uint32 GetDrawableWidth() const noexcept { return DrawableWidth; }
    [[nodiscard]] Stoner::Core::uint32 GetDrawableHeight() const noexcept { return DrawableHeight; }
    [[nodiscard]] const Stoner::Core::FPlatformWindow& GetPlatformWindow() const noexcept { return PlatformWindow; }
    [[nodiscard]] bool IsRealWindow() const noexcept { return PlatformWindow.IsValid(); }
    [[nodiscard]] bool IsActive() const noexcept { return LifecycleState == EWindowLifecycleState::Active || LifecycleState == EWindowLifecycleState::CloseRequested; }
    [[nodiscard]] bool IsCloseRequested() const noexcept { return LifecycleState == EWindowLifecycleState::CloseRequested; }
    [[nodiscard]] bool IsDestroyed() const noexcept { return LifecycleState == EWindowLifecycleState::Destroyed; }
    [[nodiscard]] bool IsVisible() const noexcept { return bVisible; }
    [[nodiscard]] bool IsFocused() const noexcept { return bFocused; }
    [[nodiscard]] bool IsMinimized() const noexcept { return bMinimized; }
    [[nodiscard]] bool HasDrawableArea() const noexcept { return bDrawable; }
    [[nodiscard]] bool IsPresentationPaused() const noexcept { return bPresentationPaused; }
    [[nodiscard]] const FApplicationDiagnosticLog& GetDiagnostics() const noexcept { return Diagnostics; }
    [[nodiscard]] FApplicationDiagnosticLog& GetMutableDiagnostics() noexcept { return Diagnostics; }

private:
    friend class FWindowTestAccess;
    void ResetRuntimeState();
    void UpdateDrawableState();

    Stoner::Core::uint32 WindowId = 0;
    FWindowDesc Desc;
    EWindowLifecycleState LifecycleState = EWindowLifecycleState::Uncreated;
    EWindowDisplayMode DisplayMode = EWindowDisplayMode::Windowed;
    Stoner::Core::uint32 ClientWidth = 0;
    Stoner::Core::uint32 ClientHeight = 0;
    Stoner::Core::uint32 DrawableWidth = 0;
    Stoner::Core::uint32 DrawableHeight = 0;
    bool bVisible = false;
    bool bFocused = false;
    bool bMinimized = false;
    bool bDrawable = false;
    bool bPresentationPaused = false;
    Stoner::Core::uint64 NextSequence = 1;
    Stoner::Core::TArray<FWindowEvent> PendingEvents;
    FApplicationDiagnosticLog Diagnostics;
    Stoner::Core::FPlatformWindow PlatformWindow;
    std::unique_ptr<IWindowDriver> Driver;
};

} // namespace Stoner::Application
