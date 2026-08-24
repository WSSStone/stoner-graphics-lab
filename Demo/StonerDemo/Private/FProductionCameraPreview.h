#pragma once

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
    [[nodiscard]] const FProductionCameraPreset& GetCamera() const noexcept
    {
        return Camera;
    }
    [[nodiscard]] FProductionCameraCandidate BuildCandidate(
        const char* Backend,
        const Core::FString& WorkloadRevision) const;
    void Reset() noexcept;

private:
    void RebuildCamera();
    [[nodiscard]] bool IsHeld(Application::EKey Key) const;

    FProductionCameraPreset InitialCamera;
    FProductionCameraPreset Camera;
    Core::uint32 Width = 0;
    Core::uint32 Height = 0;
    Core::FVector3 Position = Core::FVector3::Zero();
    float YawRadians = 0.0f;
    float PitchRadians = 0.0f;
    float VerticalFovRadians = 1.0471975512f;
    float Aspect = 1.0f;
    bool bRightMouseHeld = false;
    bool bHasPointer = false;
    float PointerX = 0.0f;
    float PointerY = 0.0f;
    std::set<Application::EKey> HeldKeys;
};

[[nodiscard]] bool WriteProductionCameraCandidate(
    const Core::FString& Path,
    const FProductionCameraCandidate& Candidate,
    Core::FString* OutReason = nullptr);

} // namespace Stoner::Demo
