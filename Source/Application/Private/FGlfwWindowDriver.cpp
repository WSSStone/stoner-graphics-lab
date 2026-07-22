#include "Application/FInputEvent.h"
#include "Application/FWindowEvent.h"
#include "FWindowDriver.h"

#if defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif

namespace Stoner::Application
{

#if defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
namespace
{

EKey TranslateKey(int Key)
{
    if (Key >= GLFW_KEY_A && Key <= GLFW_KEY_Z)
        return static_cast<EKey>(static_cast<int>(EKey::A) + Key - GLFW_KEY_A);
    if (Key >= GLFW_KEY_0 && Key <= GLFW_KEY_9)
        return static_cast<EKey>(static_cast<int>(EKey::Num0) + Key - GLFW_KEY_0);
    switch (Key)
    {
    case GLFW_KEY_ESCAPE: return EKey::Escape;
    case GLFW_KEY_SPACE: return EKey::Space;
    case GLFW_KEY_ENTER: return EKey::Enter;
    case GLFW_KEY_TAB: return EKey::Tab;
    case GLFW_KEY_BACKSPACE: return EKey::Backspace;
    case GLFW_KEY_LEFT: return EKey::Left;
    case GLFW_KEY_RIGHT: return EKey::Right;
    case GLFW_KEY_UP: return EKey::Up;
    case GLFW_KEY_DOWN: return EKey::Down;
    default: return EKey::Unknown;
    }
}

EMouseButton TranslateMouseButton(int Button)
{
    switch (Button)
    {
    case GLFW_MOUSE_BUTTON_LEFT: return EMouseButton::Left;
    case GLFW_MOUSE_BUTTON_RIGHT: return EMouseButton::Right;
    case GLFW_MOUSE_BUTTON_MIDDLE: return EMouseButton::Middle;
    default: return EMouseButton::Unknown;
    }
}

} // namespace

class FGlfwWindowDriver final : public IWindowDriver
{
public:
    ~FGlfwWindowDriver() override { Destroy(); }
    [[nodiscard]] const char* GetDriverName() const noexcept override { return "GLFW"; }
    [[nodiscard]] EWindowRuntimeAvailability GetRuntimeAvailability() const noexcept override
    {
        return EWindowRuntimeAvailability::Available;
    }

    EApplicationResult Create(const FWindowDesc& Desc, Stoner::Core::uint32 InWindowId) override
    {
        if (Window != nullptr) return EApplicationResult::InvalidLifecycle;
        if (glfwInit() != GLFW_TRUE) return EApplicationResult::RuntimeUnavailable;
        bOwnsGlfw = true;
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_VISIBLE, Desc.bVisible ? GLFW_TRUE : GLFW_FALSE);
        Window = glfwCreateWindow(static_cast<int>(Desc.ClientWidth), static_cast<int>(Desc.ClientHeight), Desc.Title.CStr(), nullptr, nullptr);
        if (Window == nullptr)
        {
            Destroy();
            return EApplicationResult::RuntimeUnavailable;
        }
        WindowId = InWindowId;
        glfwSetWindowUserPointer(Window, this);
        InstallCallbacks();
        RefreshFramebufferExtent();
        return EApplicationResult::Success;
    }

    void Destroy() override
    {
        if (Window != nullptr) glfwDestroyWindow(Window);
        Window = nullptr;
        DrawableWidth = 0;
        DrawableHeight = 0;
        if (bOwnsGlfw) glfwTerminate();
        bOwnsGlfw = false;
    }

    void Poll() override
    {
        if (Window == nullptr) return;
        glfwPollEvents();
        RefreshFramebufferExtent();
        if (glfwWindowShouldClose(Window) == GLFW_TRUE && !bCloseEventQueued)
        {
            WindowEvents.push_back(FWindowEvent::CloseRequested(NextSequence++));
            bCloseEventQueued = true;
        }
    }

    void RequestClose() override { if (Window) glfwSetWindowShouldClose(Window, GLFW_TRUE); }
    [[nodiscard]] Stoner::Core::FPlatformWindow GetPlatformWindow() const noexcept override { return Stoner::Core::FPlatformWindow(Window); }
    [[nodiscard]] Stoner::Core::uint32 GetDrawableWidth() const noexcept override { return DrawableWidth; }
    [[nodiscard]] Stoner::Core::uint32 GetDrawableHeight() const noexcept override { return DrawableHeight; }

    [[nodiscard]] Stoner::Core::TArray<FWindowEvent> ConsumeWindowEvents() override
    {
        SortWindowEventsStable(WindowEvents);
        auto Result = WindowEvents;
        WindowEvents.clear();
        return Result;
    }

    [[nodiscard]] Stoner::Core::TArray<FInputEvent> ConsumeInputEvents() override
    {
        SortInputEventsStable(InputEvents);
        auto Result = InputEvents;
        InputEvents.clear();
        return Result;
    }

private:
    static FGlfwWindowDriver* Self(GLFWwindow* Window) { return static_cast<FGlfwWindowDriver*>(glfwGetWindowUserPointer(Window)); }

    void InstallCallbacks()
    {
        glfwSetFramebufferSizeCallback(Window, [](GLFWwindow* Native, int Width, int Height)
        {
            auto* Driver = Self(Native);
            Driver->DrawableWidth = Width > 0 ? static_cast<Stoner::Core::uint32>(Width) : 0;
            Driver->DrawableHeight = Height > 0 ? static_cast<Stoner::Core::uint32>(Height) : 0;
            Driver->WindowEvents.push_back(Width == 0 || Height == 0
                ? FWindowEvent::Minimized(Driver->NextSequence++)
                : FWindowEvent::DrawableResized(Driver->DrawableWidth, Driver->DrawableHeight, Driver->NextSequence++));
        });
        glfwSetWindowSizeCallback(Window, [](GLFWwindow* Native, int Width, int Height)
        {
            auto* Driver = Self(Native);
            if (Width > 0 && Height > 0)
                Driver->WindowEvents.push_back(FWindowEvent::Resized(
                    static_cast<Stoner::Core::uint32>(Width), static_cast<Stoner::Core::uint32>(Height), Driver->NextSequence++));
        });
        glfwSetWindowIconifyCallback(Window, [](GLFWwindow* Native, int Iconified)
        {
            auto* Driver = Self(Native);
            if (Iconified == GLFW_TRUE) Driver->WindowEvents.push_back(FWindowEvent::Minimized(Driver->NextSequence++));
            else
            {
                Driver->RefreshFramebufferExtent();
                Driver->WindowEvents.push_back(FWindowEvent::Restored(Driver->DrawableWidth, Driver->DrawableHeight, Driver->NextSequence++));
            }
        });
        glfwSetWindowFocusCallback(Window, [](GLFWwindow* Native, int Focused)
        {
            auto* Driver = Self(Native);
            Driver->WindowEvents.push_back(Focused == GLFW_TRUE ? FWindowEvent::FocusGained(Driver->NextSequence++) : FWindowEvent::FocusLost(Driver->NextSequence++));
            if (Focused != GLFW_TRUE) Driver->InputEvents.push_back(FInputEvent::FocusLost(Driver->NextSequence++));
        });
        glfwSetKeyCallback(Window, [](GLFWwindow* Native, int Key, int, int Action, int)
        {
            auto* Driver = Self(Native);
            const EKey Translated = TranslateKey(Key);
            if (Action == GLFW_PRESS)
            {
                Driver->InputEvents.push_back(FInputEvent::KeyDown(Translated, Driver->NextSequence++));
                if (Translated == EKey::Escape) glfwSetWindowShouldClose(Native, GLFW_TRUE);
            }
            else if (Action == GLFW_RELEASE) Driver->InputEvents.push_back(FInputEvent::KeyUp(Translated, Driver->NextSequence++));
        });
        glfwSetMouseButtonCallback(Window, [](GLFWwindow* Native, int Button, int Action, int)
        {
            auto* Driver = Self(Native);
            const EMouseButton Translated = TranslateMouseButton(Button);
            Driver->InputEvents.push_back(Action == GLFW_PRESS
                ? FInputEvent::MouseDown(Translated, Driver->NextSequence++)
                : FInputEvent::MouseUp(Translated, Driver->NextSequence++));
        });
        glfwSetCursorPosCallback(Window, [](GLFWwindow* Native, double X, double Y)
        {
            auto* Driver = Self(Native);
            Driver->InputEvents.push_back(FInputEvent::PointerMove(static_cast<float>(X), static_cast<float>(Y), Driver->NextSequence++));
        });
        glfwSetScrollCallback(Window, [](GLFWwindow* Native, double X, double Y)
        {
            auto* Driver = Self(Native);
            Driver->InputEvents.push_back(FInputEvent::Scroll(static_cast<float>(X), static_cast<float>(Y), Driver->NextSequence++));
        });
    }

    void RefreshFramebufferExtent()
    {
        int Width = 0;
        int Height = 0;
        glfwGetFramebufferSize(Window, &Width, &Height);
        DrawableWidth = Width > 0 ? static_cast<Stoner::Core::uint32>(Width) : 0;
        DrawableHeight = Height > 0 ? static_cast<Stoner::Core::uint32>(Height) : 0;
    }

    GLFWwindow* Window = nullptr;
    Stoner::Core::uint32 WindowId = 0;
    Stoner::Core::uint32 DrawableWidth = 0;
    Stoner::Core::uint32 DrawableHeight = 0;
    Stoner::Core::uint64 NextSequence = 1;
    bool bOwnsGlfw = false;
    bool bCloseEventQueued = false;
    Stoner::Core::TArray<FWindowEvent> WindowEvents;
    Stoner::Core::TArray<FInputEvent> InputEvents;
};

#else

class FGlfwWindowDriver final : public IWindowDriver
{
public:
    [[nodiscard]] const char* GetDriverName() const noexcept override { return "GLFW"; }
    [[nodiscard]] EWindowRuntimeAvailability GetRuntimeAvailability() const noexcept override
    {
        return EWindowRuntimeAvailability::DependencyUnavailable;
    }
    [[nodiscard]] Stoner::Core::TArray<FWindowEvent> ConsumeWindowEvents() override { return {}; }
    [[nodiscard]] Stoner::Core::TArray<FInputEvent> ConsumeInputEvents() override { return {}; }
};

#endif

std::unique_ptr<IWindowDriver> CreateGlfwWindowDriver()
{
    return std::make_unique<FGlfwWindowDriver>();
}

} // namespace Stoner::Application
