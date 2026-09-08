#pragma once

#include "Application/FInputEvent.h"
#include "Application/FWindowEvent.h"
#include "Application/FWindowDisplayState.h"
#include "Core/FPlatformWindow.h"

#include <memory>

namespace Stoner::Application
{

class FWindow;

class IWindowDriver
{
public:
    virtual ~IWindowDriver() = default;

    [[nodiscard]] virtual const char* GetDriverName() const noexcept = 0;
    [[nodiscard]] virtual EWindowRuntimeAvailability GetRuntimeAvailability() const noexcept = 0;
    virtual EApplicationResult Create(const FWindowDesc&, Stoner::Core::uint32) { return EApplicationResult::RuntimeUnavailable; }
    virtual void Destroy() {}
    virtual void Poll() {}
    virtual void RequestClose() {}
    virtual EApplicationResult SetClientSize(
        Stoner::Core::uint32, Stoner::Core::uint32)
    { return EApplicationResult::UnsupportedMode; }
    virtual EApplicationResult SetCursorMode(ECursorMode)
    { return EApplicationResult::UnsupportedMode; }
    [[nodiscard]] virtual bool EmitsLifecycleInputResets() const noexcept
    { return false; }
    virtual EApplicationResult ReadClipboardUtf8(Stoner::Core::FString&)
    { return EApplicationResult::UnsupportedMode; }
    virtual EApplicationResult WriteClipboardUtf8(const Stoner::Core::FString&)
    { return EApplicationResult::UnsupportedMode; }
    virtual EApplicationResult Minimize()
    { return EApplicationResult::UnsupportedMode; }
    virtual EApplicationResult Restore()
    { return EApplicationResult::UnsupportedMode; }
    [[nodiscard]] virtual Stoner::Core::FPlatformWindow GetPlatformWindow() const noexcept { return {}; }
    [[nodiscard]] virtual Stoner::Core::uint32 GetDrawableWidth() const noexcept { return 0; }
    [[nodiscard]] virtual Stoner::Core::uint32 GetDrawableHeight() const noexcept { return 0; }
    [[nodiscard]] virtual float GetContentScaleX() const noexcept { return 1.0f; }
    [[nodiscard]] virtual float GetContentScaleY() const noexcept { return 1.0f; }
    [[nodiscard]] virtual Stoner::Core::TArray<FWindowEvent> ConsumeWindowEvents() = 0;
    [[nodiscard]] virtual Stoner::Core::TArray<FInputEvent> ConsumeInputEvents() = 0;
};

class FWindowTestAccess
{
public:
    static void InstallDriver(FWindow& Window, std::unique_ptr<IWindowDriver> Driver);
};

[[nodiscard]] std::unique_ptr<IWindowDriver> CreateGlfwWindowDriver();
[[nodiscard]] std::unique_ptr<IWindowDriver> CreateHeadlessWindowDriver();
[[nodiscard]] bool IsGlfwInputMappingAvailable() noexcept;
[[nodiscard]] EKey TranslateGlfwKeyCode(int Key) noexcept;
[[nodiscard]] EMouseButton TranslateGlfwMouseButtonCode(int Button) noexcept;

} // namespace Stoner::Application
