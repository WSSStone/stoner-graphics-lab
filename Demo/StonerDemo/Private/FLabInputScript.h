#pragma once
#include "Core/CoreMinimal.h"
#include "Application/FLabSettingsSnapshot.h"
#include "RHI/FRHIPresentationCapabilities.h"
#include <functional>
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
struct FLabProfileObservation
{
    RHI::FRHIPresentationCapabilities Capabilities;
    Core::uint64 FrameToken=0, SettingsRevision=0, OutputGeneration=0, NativeOutputGeneration=0;
    Core::FString ProfileId;
};
struct FLabProfileResult
{
    Core::uint32 StepIndex=0;
    Core::FString RequestedProfile, Outcome, Reason, BeforeState, AfterState;
    Application::FLabSettingsSnapshot Before, After;
    FLabProfileObservation Start, Finish;
    bool bSupported=false, bComplete=false;
};
class FLabInputScript
{
public:
    std::function<bool(FLabProfileObservation&)> ObserveProfile;
    std::function<std::chrono::steady_clock::time_point()> ProfileClock;
    const Core::TArray<FLabProfileResult>& GetProfileResults() const { return ProfileResults; }
    bool Load(const Core::FString& Path,Core::FString& Reason);
    bool Decode(const Core::TArray<Core::uint8>& Bytes,Core::FString& Reason);
    bool Service(Application::FWindow&,Application::FInteractiveLabSession&,Core::uint32 Presented,Core::FString& Reason);
    bool IsComplete() const noexcept { return Cursor==Steps.size() && !Expected && !PendingProfile && LastPresented>=ResumeAfterPresented; }
    Core::uint32 GetCompletedSteps() const noexcept { return static_cast<Core::uint32>(Cursor)-((Expected || PendingProfile) ? 1u : 0u); }
    Core::uint32 GetStepCount() const noexcept { return static_cast<Core::uint32>(Steps.size()); }
    const Core::FString& GetDigest() const noexcept { return Digest; }
private:
    bool ServiceProfile(Application::FInteractiveLabSession&, Core::FString&);
    bool StartProfile(Application::FInteractiveLabSession&, const FLabScriptStep&, Core::FString&);
    Core::TArray<FLabProfileResult> ProfileResults;
    std::optional<Core::usize> PendingProfile;
    std::chrono::steady_clock::time_point ProfileStarted{};
    Core::TArray<FLabScriptStep> Steps;
    Core::usize Cursor=0;
    std::optional<Application::FLabSettingsSnapshot> Expected;
    Core::FString Digest;
    std::chrono::steady_clock::time_point ProgressTime{};
    Core::uint32 LastPresented=0, ResumeAfterPresented=0;
    Core::usize LastCursor=0;

};
}
