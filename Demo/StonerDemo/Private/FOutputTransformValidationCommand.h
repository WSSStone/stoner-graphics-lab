#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FOutputTransformExecutor.h"

namespace Stoner::Demo
{

// Stable, privacy-bounded identity supplied by the validation harness.  The
// command serializes native facts only; it has no field capable of expressing
// an HDR visual decision.
struct FOutputTransformValidationProbeInput
{
    Core::FString HostPlatform;
    Core::FString Backend;
    Core::FString ProfileKind;
    Core::FString WorkloadRevision;
    Core::FString DeviceClass;
    Core::FString CapabilityDigest;
    Core::FString OutputDeviceProfileId;
    Core::FString TransformVersion;
    Core::FString InsertionDigest;
    Core::FString ReadbackDigest;
    Core::FString FirstFailureCode;
    Core::FString FirstFailureStage;
    Core::FString FirstFailureMessage;
    Core::uint32 Width = 0;
    Core::uint32 Height = 0;
    Core::uint64 FirstFrameToken = 0;
    Core::uint64 LastFrameToken = 0;
    Core::uint64 SettledFrameToken = 0;
    float ExposureStops = 0.0f;
    Renderer::FOutputTransformExecutionResult Execution;
};

class FOutputTransformValidationCommand final
{
public:
    [[nodiscard]] static bool SerializeNormalizedNativeProbe(
        const FOutputTransformValidationProbeInput& Input,
        Core::FString& OutJson,
        Core::FString* OutReason = nullptr);
};

} // namespace Stoner::Demo
