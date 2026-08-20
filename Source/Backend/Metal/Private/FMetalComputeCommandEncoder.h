#pragma once

#include "FMetalCommandBuffer.h"
#include "RHI/FRHIDeviceCapabilities.h"

#include <span>

namespace Stoner::Backend::Metal::Private
{

[[nodiscard]] RHI::ERHIResult EncodeMetalComputeCommands(
    void* NativeCommandBuffer,
    std::span<const FMetalCommandRecord> Records,
    const RHI::FRHIDeviceCapabilities& Capabilities,
    Core::usize& OutConsumed) noexcept;

} // namespace Stoner::Backend::Metal::Private
