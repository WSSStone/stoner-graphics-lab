#pragma once
#include "Core/CoreMinimal.h"
#include "Application/FLabSettingsSnapshot.h"
#include <optional>
#include <chrono>
namespace Stoner::Application { class FWindow; class FInteractiveLabSession; }
namespace Stoner::Demo
{
struct FLabScriptStep
{
    Core::uint32 AfterPresented=0;
    Core::FString Action, Text;
    double Value=0, Value2=0;
};
class FLabInputScript
{
public:
    bool Load(const Core::FString& Path,Core::FString& Reason);
    bool Decode(const Core::TArray<Core::uint8>& Bytes,Core::FString& Reason);
    bool Service(Application::FWindow&,Application::FInteractiveLabSession&,Core::uint32 Presented,Core::FString& Reason);
    bool IsComplete() const noexcept { return Cursor==Steps.size() && !Expected; }
    Core::uint32 GetCompletedSteps() const noexcept { return static_cast<Core::uint32>(Cursor)-(Expected ? 1u : 0u); }
    Core::uint32 GetStepCount() const noexcept { return static_cast<Core::uint32>(Steps.size()); }
    const Core::FString& GetDigest() const noexcept { return Digest; }
private:
    Core::TArray<FLabScriptStep> Steps;
    Core::usize Cursor=0;
    std::optional<Application::FLabSettingsSnapshot> Expected;
    Core::FString Digest;
    std::chrono::steady_clock::time_point ProgressTime{};
    Core::uint32 LastPresented=0;
    Core::usize LastCursor=0;

};
}
