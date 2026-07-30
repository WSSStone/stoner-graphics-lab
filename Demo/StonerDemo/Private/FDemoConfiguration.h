#pragma once

#include "Core/CoreMinimal.h"

namespace Stoner::Demo
{

enum class EDemoRunMode
{
    InteractiveNative,
    BoundedNative,
    DeterministicHeadless,
    NativeHeadless
};

enum class EDemoExitCode : int
{
    Success = 0,
    InvalidConfiguration = 2,
    RuntimeUnavailable = 3,
    InitializationFailed = 4,
    FrameFailed = 5,
    ValidationFailed = 6,
    ReportFailed = 7
};

struct FDemoConfiguration
{
    EDemoRunMode RunMode = EDemoRunMode::InteractiveNative;
    Stoner::Core::uint32 ClientWidth = 1280;
    Stoner::Core::uint32 ClientHeight = 720;
    Stoner::Core::uint32 FrameBudget = 0;
    Stoner::Core::uint32 WarmupFrames = 0;
    Stoner::Core::uint32 MemorySampleInterval = 0;
    Stoner::Core::uint64 MaxMemoryGrowthBytes = 0;
    double MaxMemoryGrowthPercent = 0.0;
    Stoner::Core::uint32 MaxFramesInFlight = 2;
    bool bEnableValidationLayers = false;
    Stoner::Core::FString ShaderDirectory = "Content/Shaders/Triangle";
    Stoner::Core::FString ValidationOutputPath = "Build/triangle-demo-validation.txt";
    Stoner::Core::FString EvidenceRunId = "local";

    [[nodiscard]] bool IsBounded() const noexcept;
    [[nodiscard]] bool RequiresNativeRuntime() const noexcept;
    [[nodiscard]] bool RequiresVisibleWindow() const noexcept;
    [[nodiscard]] bool IsValid(Stoner::Core::FString* OutReason = nullptr) const;

    static EDemoExitCode Parse(int ArgCount, const char* const* Arguments,
        FDemoConfiguration& OutConfiguration, Stoner::Core::FString& OutReason);
};

[[nodiscard]] const char* ToString(EDemoRunMode Mode) noexcept;

} // namespace Stoner::Demo
