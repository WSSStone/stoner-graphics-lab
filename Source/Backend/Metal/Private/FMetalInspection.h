#pragma once

#include "FMetalDeviceOwnerState.h"

namespace Stoner::Backend::Metal::Private
{

[[nodiscard]] FMetalBackendInspection CaptureMetalInspection(
    const Core::TSharedPtr<FMetalDeviceOwnerState>& Owner) noexcept;

[[nodiscard]] bool HasZeroMetalOwnership(
    const FMetalBackendInspection& Inspection,
    bool bIncludeDevice = true) noexcept;

} // namespace Stoner::Backend::Metal::Private
