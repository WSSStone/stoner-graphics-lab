#pragma once

#include "RHI/FRHIDeviceCapabilities.h"

namespace Stoner::Backend::Metal::Private
{

[[nodiscard]] RHI::FRHIDeviceCapabilities QueryMetalCapabilities(
    void* NativeDevice) noexcept;

} // namespace Stoner::Backend::Metal::Private
