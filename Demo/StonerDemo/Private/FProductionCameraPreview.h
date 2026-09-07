#pragma once

#include "Application/FFreeCameraController.h"
#include "Application/FInputEvent.h"
#include "FProductionCameraPreset.h"

#include <set>

namespace Stoner::Demo
{

struct FProductionCameraPreviewUpdate
{
    bool bCameraChanged = false;
    bool bSnapshotRequested = false;
    bool bExitRequested = false;
};

struct FProductionCameraCandidate
{
    Core::FString WorkloadRevision;
    Core::FString Backend;
    Core::uint32 Width = 0;
    Core::uint32 Height = 0;
    FProductionCameraPreset Camera;
    Core::FString MatrixSha256;
    Core::FString CanonicalJson;

    [[nodiscard]] bool IsValid() const noexcept;
};

class FProductionCameraPreviewController
{
public:
    [[nodiscard]] bool Initialize(
        const FProductionCameraPreset& Preset,
        Core::uint32 Width,
        Core::uint32 Height,
        Core::FString* OutReason = nullptr);
    [[nodiscard]] FProductionCameraPreviewUpdate Update(
        const Core::TArray<Application::FInputEvent>& Events,
        double DeltaSeconds);
    [[nodiscard]] FProductionCameraPreviewUpdate Update(
        const Core::TArray<Application::FInputEvent>& Events,
        double DeltaSeconds,
        const Application::FWindowDisplayState& CurrentDisplay);
    [[nodiscard]] const FProductionCameraPreset& GetCamera() const noexcept
    {
        return Camera;
    }
    [[nodiscard]] bool IsLookCaptured() const noexcept
    {
        return bRightMouseHeld;
    }
    [[nodiscard]] FProductionCameraCandidate BuildCandidate(
        const char* Backend,
        const Core::FString& WorkloadRevision) const;
    void Reset() noexcept;

private:
    [[nodiscard]] bool BuildInitialCameraState(
        const FProductionCameraPreset& Preset,
        Core::uint32 InWidth,
        Core::uint32 InHeight,
        Application::FFreeCameraState& OutState,
        Core::FString* OutReason) const;
    [[nodiscard]] bool SyncCameraFromController() noexcept;
    void ClearInputState() noexcept;
    [[nodiscard]] bool IsHeld(Application::EKey Key) const;
    [[nodiscard]] static bool IsNavigationKey(Application::EKey Key) noexcept;

    FProductionCameraPreset InitialCamera;
    FProductionCameraPreset Camera;
    Application::FFreeCameraState InitialCameraState;
    Application::FFreeCameraController CameraController;
    Application::FWindowDisplayState DisplayState;
    Core::uint32 Width = 0;
    Core::uint32 Height = 0;
    bool bRightMouseHeld = false;
    bool bHasPointer = false;
    bool bInputQuarantined = false;
    bool bNeedsZeroInterval = false;
    float PointerX = 0.0f;
    float PointerY = 0.0f;
    std::set<Application::EKey> HeldKeys;
};

[[nodiscard]] bool WriteProductionCameraCandidate(
    const Core::FString& Path,
    const FProductionCameraCandidate& Candidate,
    Core::FString* OutReason = nullptr);

} // namespace Stoner::Demo
