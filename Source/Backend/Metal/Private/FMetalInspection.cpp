#include "FMetalInspection.h"

namespace Stoner::Backend::Metal::Private
{

FMetalBackendInspection CaptureMetalInspection(
    const Core::TSharedPtr<FMetalDeviceOwnerState>& Owner) noexcept
{
    return Owner ? Owner->Inspect() : FMetalBackendInspection{};
}

bool HasZeroMetalOwnership(
    const FMetalBackendInspection& Inspection,
    bool bIncludeDevice) noexcept
{
    return (!bIncludeDevice || Inspection.DeviceOwnershipCount == 0) &&
        Inspection.LiveObjectCount == 0 &&
        Inspection.ResourceOwnershipCount == 0 &&
        Inspection.PipelineOwnershipCount == 0 &&
        Inspection.CommandOwnershipCount == 0 &&
        Inspection.SynchronizationOwnershipCount == 0 &&
        Inspection.SubmissionOwnershipCount == 0 &&
        Inspection.PresentationOwnershipCount == 0 &&
        Inspection.InFlightSubmissionCount == 0;
}

} // namespace Stoner::Backend::Metal::Private
