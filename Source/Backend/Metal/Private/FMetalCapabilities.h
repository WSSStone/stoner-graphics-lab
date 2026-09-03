#pragma once

#include "Core/FPlatformWindow.h"
#include "RHI/ERHIResult.h"
#include "RHI/FRHIDeviceCapabilities.h"
#include "RHI/FRHIPresentationCapabilities.h"

namespace Stoner::Backend::Metal::Private
{

[[nodiscard]] RHI::FRHIDeviceCapabilities QueryMetalCapabilities(
    void* NativeDevice) noexcept;

[[nodiscard]] RHI::ERHIResult QueryMetalPresentationCapabilities(
    void* NativeDevice,
    const Core::FPlatformWindow& Window,
    Core::uint64 SurfaceId,
    Core::uint64 CapabilityGeneration,
    RHI::FRHIPresentationCapabilities& OutCapabilities) noexcept;

} // namespace Stoner::Backend::Metal::Private
