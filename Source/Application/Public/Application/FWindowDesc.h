#pragma once

#include "Application/FApplicationDiagnostics.h"

namespace Stoner::Application
{

enum class EWindowLifecycleState
{
    Uncreated,
    Active,
    CloseRequested,
    Destroyed
};

enum class EWindowDisplayMode
{
    Windowed,
    Fullscreen
};

enum class EWindowRuntimeAvailability
{
    Available,
    DisplayUnavailable,
    DependencyUnavailable,
    Failed
};

struct FWindowDesc
{
    static constexpr Stoner::Core::uint32 MaxClientWidth = 16384;
    static constexpr Stoner::Core::uint32 MaxClientHeight = 16384;

    Stoner::Core::FString Title = "Stoner Graphics Lab";
    Stoner::Core::uint32 ClientWidth = 1280;
    Stoner::Core::uint32 ClientHeight = 720;
    EWindowDisplayMode DisplayMode = EWindowDisplayMode::Windowed;
    bool bVisible = true;
    Stoner::Core::FString DebugName = "PrimaryWindow";

    [[nodiscard]] bool IsValid(FApplicationDiagnosticLog* Diagnostics = nullptr) const;
};

[[nodiscard]] const char* ToString(EWindowLifecycleState State) noexcept;
[[nodiscard]] const char* ToString(EWindowDisplayMode Mode) noexcept;
[[nodiscard]] const char* ToString(EWindowRuntimeAvailability Availability) noexcept;

} // namespace Stoner::Application
