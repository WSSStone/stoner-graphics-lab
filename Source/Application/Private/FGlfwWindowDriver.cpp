#include "Application/FInputEvent.h"
#include "Application/FWindowEvent.h"
#include "FWindowDriver.h"
#include "FWindowEventBuffer.h"

#if defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif

namespace Stoner::Application
{

#if defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
namespace
{

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
        glfwWindowHint(GLFW_SCALE_FRAMEBUFFER,
            Desc.bHighDensityFramebuffer ? GLFW_TRUE : GLFW_FALSE);
        glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER,
            Desc.bHighDensityFramebuffer ? GLFW_TRUE : GLFW_FALSE);
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
            Events.Push(FWindowEvent::CloseRequested(NextSequence++));
            Events.Push(FInputEvent::FocusLost(NextSequence++));
            bCloseEventQueued = true;
        }
    }

    EApplicationResult ReadClipboardUtf8(Stoner::Core::FString& Out) override
    {
        if (!Window) return EApplicationResult::InvalidLifecycle;
        const char* Text = glfwGetClipboardString(Window);
        if (!Text) return EApplicationResult::UnsupportedMode;
        std::size_t Length = 0;
        while (Length <= 65536 && Text[Length] != '\0') ++Length;
        if (Length > 65536) return EApplicationResult::InvalidInput;
        Out = Stoner::Core::FString(std::string_view(Text, Length));
        return EApplicationResult::Success;
    }
    EApplicationResult WriteClipboardUtf8(const Stoner::Core::FString& Text) override
    {
        if (!Window) return EApplicationResult::InvalidLifecycle;
        (void)glfwGetError(nullptr);
        glfwSetClipboardString(Window, Text.CStr());
        return glfwGetError(nullptr) == GLFW_NO_ERROR ? EApplicationResult::Success : EApplicationResult::UnsupportedMode;
    }

    void RequestClose() override { if (Window) glfwSetWindowShouldClose(Window, GLFW_TRUE); }
    EApplicationResult SetCursorMode(ECursorMode NewMode) override
    {
        if (!Window) return EApplicationResult::InvalidLifecycle;
        switch (NewMode)
        {
        case ECursorMode::Normal:
            glfwSetInputMode(Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            return EApplicationResult::Success;
        case ECursorMode::Disabled:
            glfwSetInputMode(Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            return EApplicationResult::Success;
        }
        return EApplicationResult::InvalidInput;
    }
    [[nodiscard]] bool EmitsLifecycleInputResets() const noexcept override
    {
        return true;
    }
    EApplicationResult SetClientSize(
        Stoner::Core::uint32 Width, Stoner::Core::uint32 Height) override
    {
        if (!Window) return EApplicationResult::InvalidLifecycle;
        glfwSetWindowSize(Window, static_cast<int>(Width), static_cast<int>(Height));
        return EApplicationResult::Success;
    }
    EApplicationResult Minimize() override
    {
        if (!Window) return EApplicationResult::InvalidLifecycle;
        glfwIconifyWindow(Window);
        return EApplicationResult::Success;
    }
    EApplicationResult Restore() override
    {
        if (!Window) return EApplicationResult::InvalidLifecycle;
        glfwRestoreWindow(Window);
        return EApplicationResult::Success;
    }
    [[nodiscard]] Stoner::Core::FPlatformWindow GetPlatformWindow() const noexcept override { return Stoner::Core::FPlatformWindow(Window); }
    [[nodiscard]] Stoner::Core::uint32 GetDrawableWidth() const noexcept override { return DrawableWidth; }
    [[nodiscard]] Stoner::Core::uint32 GetDrawableHeight() const noexcept override { return DrawableHeight; }
    [[nodiscard]] float GetContentScaleX() const noexcept override
    {
        float X = 1.0f;
        float Y = 1.0f;
        if (Window) glfwGetWindowContentScale(Window, &X, &Y);
        return X > 0.0f ? X : 1.0f;
    }
    [[nodiscard]] float GetContentScaleY() const noexcept override
    {
        float X = 1.0f;
        float Y = 1.0f;
        if (Window) glfwGetWindowContentScale(Window, &X, &Y);
        return Y > 0.0f ? Y : 1.0f;
    }

    [[nodiscard]] Stoner::Core::TArray<FWindowEvent> ConsumeWindowEvents() override
    { return Events.TakeWindow(); }
    [[nodiscard]] Stoner::Core::TArray<FInputEvent> ConsumeInputEvents() override
    { return Events.TakeInput(); }

private:
    static FGlfwWindowDriver* Self(GLFWwindow* Window) { return static_cast<FGlfwWindowDriver*>(glfwGetWindowUserPointer(Window)); }

    void InstallCallbacks()
    {
        glfwSetFramebufferSizeCallback(Window, [](GLFWwindow* Native, int Width, int Height)
        {
            auto* Driver = Self(Native);
            Driver->DrawableWidth = Width > 0 ? static_cast<Stoner::Core::uint32>(Width) : 0;
            Driver->DrawableHeight = Height > 0 ? static_cast<Stoner::Core::uint32>(Height) : 0;
            if (Width == 0 || Height == 0)
            {
                (void)Driver->SetCursorMode(ECursorMode::Normal);
                Driver->Events.Push(FInputEvent::FocusLost(Driver->NextSequence++));
                Driver->Events.Push(FWindowEvent::DrawableResized(
                    0, 0, Driver->NextSequence++));
            }
            else
            {
                Driver->Events.Push(FWindowEvent::DrawableResized(
                    Driver->DrawableWidth, Driver->DrawableHeight,
                    Driver->NextSequence++));
            }
        });
        glfwSetWindowSizeCallback(Window, [](GLFWwindow* Native, int Width, int Height)
        {
            auto* Driver = Self(Native);
            if (Width > 0 && Height > 0)
                Driver->Events.Push(FWindowEvent::Resized(
                    static_cast<Stoner::Core::uint32>(Width), static_cast<Stoner::Core::uint32>(Height), Driver->NextSequence++));
        });
        glfwSetWindowIconifyCallback(Window, [](GLFWwindow* Native, int Iconified)
        {
            auto* Driver = Self(Native);
            if (Iconified == GLFW_TRUE)
            {
                (void)Driver->SetCursorMode(ECursorMode::Normal);
                Driver->Events.Push(FInputEvent::FocusLost(Driver->NextSequence++));
                Driver->Events.Push(FWindowEvent::Minimized(Driver->NextSequence++));
            }
            else
            {
                Driver->RefreshFramebufferExtent();
                int LogicalWidth = 0;
                int LogicalHeight = 0;
                glfwGetWindowSize(Native, &LogicalWidth, &LogicalHeight);
                Driver->Events.Push(FWindowEvent::Restored(
                    LogicalWidth > 0 ? static_cast<Stoner::Core::uint32>(LogicalWidth) : 0,
                    LogicalHeight > 0 ? static_cast<Stoner::Core::uint32>(LogicalHeight) : 0,
                    Driver->NextSequence++));
            }
        });
        glfwSetWindowFocusCallback(Window, [](GLFWwindow* Native, int Focused)
        {
            auto* Driver = Self(Native);
            Driver->Events.Push(Focused == GLFW_TRUE ? FWindowEvent::FocusGained(Driver->NextSequence++) : FWindowEvent::FocusLost(Driver->NextSequence++));
            if (Focused == GLFW_TRUE)
                Driver->Events.Push(FInputEvent::FocusGained(Driver->NextSequence++));
            if (Focused != GLFW_TRUE)
            {
                (void)Driver->SetCursorMode(ECursorMode::Normal);
                Driver->Events.Push(FInputEvent::FocusLost(Driver->NextSequence++));
            }
        });
        glfwSetWindowContentScaleCallback(Window, [](GLFWwindow* Native, float X, float Y)
        {
            auto* Driver = Self(Native);
            Driver->Events.Push(FWindowEvent::ContentScaleChanged(X, Y, Driver->NextSequence++));
        });
        glfwSetCharCallback(Window, [](GLFWwindow* Native, unsigned int Scalar)
        {
            auto* Driver = Self(Native);
            Driver->Events.Push(FInputEvent::Text(Scalar, Driver->NextSequence++));
        });
        glfwSetCursorEnterCallback(Window, [](GLFWwindow* Native, int Entered)
        {
            auto* Driver = Self(Native);
            Driver->Events.Push(FInputEvent::CursorEntered(Entered == GLFW_TRUE, Driver->NextSequence++));
        });
        glfwSetKeyCallback(Window, [](GLFWwindow* Native, int Key, int, int Action, int)
        {
            auto* Driver = Self(Native);
            const EKey Translated = TranslateGlfwKeyCode(Key);
            if (Action == GLFW_PRESS || Action == GLFW_REPEAT)
            {
                auto Event = FInputEvent::KeyDown(Translated, Driver->NextSequence++);
                Event.bRepeat = Action == GLFW_REPEAT;
                Driver->Events.Push(Event);
            }
            else if (Action == GLFW_RELEASE) Driver->Events.Push(FInputEvent::KeyUp(Translated, Driver->NextSequence++));
        });
        glfwSetMouseButtonCallback(Window, [](GLFWwindow* Native, int Button, int Action, int)
        {
            auto* Driver = Self(Native);
            const EMouseButton Translated = TranslateGlfwMouseButtonCode(Button);
            Driver->Events.Push(Action == GLFW_PRESS
                ? FInputEvent::MouseDown(Translated, Driver->NextSequence++)
                : FInputEvent::MouseUp(Translated, Driver->NextSequence++));
        });
        glfwSetCursorPosCallback(Window, [](GLFWwindow* Native, double X, double Y)
        {
            auto* Driver = Self(Native);
            Driver->Events.Push(FInputEvent::PointerMove(static_cast<float>(X), static_cast<float>(Y), Driver->NextSequence++));
        });
        glfwSetScrollCallback(Window, [](GLFWwindow* Native, double X, double Y)
        {
            auto* Driver = Self(Native);
            Driver->Events.Push(FInputEvent::Scroll(static_cast<float>(X), static_cast<float>(Y), Driver->NextSequence++));
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
    FWindowEventBuffer Events;
};

} // namespace

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

EKey TranslateGlfwKeyCode(int Key) noexcept
{
#if defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
    if (Key >= GLFW_KEY_A && Key <= GLFW_KEY_Z)
        return static_cast<EKey>(static_cast<int>(EKey::A) + Key - GLFW_KEY_A);
    if (Key >= GLFW_KEY_0 && Key <= GLFW_KEY_9)
        return static_cast<EKey>(static_cast<int>(EKey::Num0) + Key - GLFW_KEY_0);
    if (Key >= GLFW_KEY_F1 && Key <= GLFW_KEY_F12)
        return static_cast<EKey>(static_cast<int>(EKey::F1) + Key - GLFW_KEY_F1);
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
    case GLFW_KEY_HOME: return EKey::Home;
    case GLFW_KEY_END: return EKey::End;
    case GLFW_KEY_PAGE_UP: return EKey::PageUp;
    case GLFW_KEY_PAGE_DOWN: return EKey::PageDown;
    case GLFW_KEY_INSERT: return EKey::Insert;
    case GLFW_KEY_DELETE: return EKey::Delete;
    case GLFW_KEY_LEFT_SHIFT: return EKey::LeftShift;
    case GLFW_KEY_RIGHT_SHIFT: return EKey::RightShift;
    case GLFW_KEY_LEFT_CONTROL: return EKey::LeftControl;
    case GLFW_KEY_RIGHT_CONTROL: return EKey::RightControl;
    case GLFW_KEY_LEFT_SUPER: return EKey::LeftSuper;
    case GLFW_KEY_RIGHT_SUPER: return EKey::RightSuper;
    case GLFW_KEY_LEFT_ALT: return EKey::LeftAlt;
    case GLFW_KEY_RIGHT_ALT: return EKey::RightAlt;
    default: return EKey::Unknown;
    }
#else
    (void)Key;
    return EKey::Unknown;
#endif
}

bool IsGlfwInputMappingAvailable() noexcept
{
#if defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
    return true;
#else
    return false;
#endif
}

EMouseButton TranslateGlfwMouseButtonCode(int Button) noexcept
{
#if defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
    switch (Button)
    {
    case GLFW_MOUSE_BUTTON_LEFT: return EMouseButton::Left;
    case GLFW_MOUSE_BUTTON_RIGHT: return EMouseButton::Right;
    case GLFW_MOUSE_BUTTON_MIDDLE: return EMouseButton::Middle;
    case GLFW_MOUSE_BUTTON_4: return EMouseButton::X1;
    case GLFW_MOUSE_BUTTON_5: return EMouseButton::X2;
    default: return EMouseButton::Unknown;
    }
#else
    (void)Button;
    return EMouseButton::Unknown;
#endif
}

std::unique_ptr<IWindowDriver> CreateGlfwWindowDriver()
{
    return std::make_unique<FGlfwWindowDriver>();
}

} // namespace Stoner::Application
