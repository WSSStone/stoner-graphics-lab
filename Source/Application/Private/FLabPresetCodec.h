#pragma once
#include "Application/FFreeCameraState.h"
#include "Application/FLabSettingsSnapshot.h"
#include <span>

namespace Stoner::Application
{
struct FLabPresetWorkload
{
    Stoner::Core::FString Revision, ProductionRoot, SourceIdentityDigest;
    bool operator==(const FLabPresetWorkload&) const = default;
};
struct FLabPresetSourceContext
{
    FWindowExtent DrawableExtent;
    Stoner::Core::FString Backend, CookedGeneration, SoftwareRevision;
};
// Only the persistent scalar camera/settings fields participate in the wire
// record. Derived matrices, process revisions and UI visibility never do.
struct FLabPreset
{
    FLabPresetWorkload Workload;
    FLabPresetSourceContext SourceContext;
    FFreeCameraState Camera;
    FLabSettingsSnapshot Output;
};
class FLabPresetCodec
{
public:
    static constexpr Stoner::Core::usize MaximumBytes=65536;
    [[nodiscard]] static bool Decode(std::span<const Stoner::Core::uint8> Bytes,
        const FLabPresetWorkload& ExpectedWorkload, std::span<const FLabDebugStage> Stages,
        FLabPreset& OutPreset, Stoner::Core::FString& OutReason);
    [[nodiscard]] static bool Encode(const FLabPreset& Preset,
        std::span<const FLabDebugStage> Stages,
        Stoner::Core::TArray<Stoner::Core::uint8>& OutBytes,
        Stoner::Core::FString& OutReason);
};
}
