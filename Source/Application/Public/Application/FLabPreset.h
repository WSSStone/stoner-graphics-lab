#pragma once
#include "Application/FFreeCameraState.h"
#include "Application/FLabSettingsSnapshot.h"

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
}
