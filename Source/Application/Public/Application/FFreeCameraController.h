#pragma once

#include "Application/FFreeCameraState.h"
#include "Core/CoreMinimal.h"

namespace Stoner::Application
{

// Actions have already passed through the Application input ownership
// decision. The controller therefore owns only camera integration state and
// does not duplicate the press ledger or UI quarantine router.
struct FFreeCameraActions
{
    float ForwardAxis = 0.0f;
    float RightAxis = 0.0f;
    float UpAxis = 0.0f;
    float LookDeltaX = 0.0f;
    float LookDeltaY = 0.0f;
    float ScrollDeltaY = 0.0f;
    bool bFast = false;
    bool bLookCaptured = false;
    bool bReset = false;

    [[nodiscard]] bool IsValid() const noexcept;
};

struct FFreeCameraUpdateResult
{
    bool bCameraChanged = false;
    FCameraChangeSet ChangeSet;
};

class FFreeCameraController
{
public:
    [[nodiscard]] bool Initialize(
        const FFreeCameraState& InitialState,
        const FWindowDisplayState& CurrentDisplay,
        Stoner::Core::FString* OutReason = nullptr);

    [[nodiscard]] FFreeCameraUpdateResult Update(
        const FFreeCameraActions& Actions,
        const FWindowDisplayState& CurrentDisplay,
        double DeltaSeconds) noexcept;

    [[nodiscard]] bool Reset(
        const FWindowDisplayState& CurrentDisplay,
        FCameraChangeSet* OutChangeSet = nullptr) noexcept;

    // Explicit commands are independent of keyboard focus. Update still owns
    // the focus-gated movement/look path; both require a valid drawable.
    [[nodiscard]] bool SetNavigationParameters(float MovementSpeed, float VerticalFovRadians,
        const FWindowDisplayState& CurrentDisplay, FCameraChangeSet* OutChangeSet = nullptr) noexcept;

    // Restore persistent scalar fields only. The caller may prepare this on a
    // controller copy and publish it together with a settings transaction.
    [[nodiscard]] bool RestorePreset(const FFreeCameraState& Preset,
        const FWindowDisplayState& CurrentDisplay, FCameraChangeSet* OutChangeSet = nullptr) noexcept;

    [[nodiscard]] const FFreeCameraState& GetState() const noexcept
    {
        return State;
    }

    [[nodiscard]] bool IsInitialized() const noexcept
    {
        return bInitialized;
    }

private:
    [[nodiscard]] bool Commit(
        FFreeCameraState Candidate,
        ECameraChangeFlags Flags,
        FWindowExtent PreviousExtent,
        FFreeCameraUpdateResult& OutResult) noexcept;

    FFreeCameraState InitialState;
    FFreeCameraState State;
    FWindowExtent CurrentDrawableExtent;
    bool bInitialized = false;
    bool bNeedsZeroInterval = true;
};

} // namespace Stoner::Application
