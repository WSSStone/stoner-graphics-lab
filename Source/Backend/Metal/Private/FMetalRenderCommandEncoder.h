#pragma once

#include "FMetalCommandBuffer.h"

#include <span>

namespace Stoner::Backend::Metal::Private
{

[[nodiscard]] RHI::ERHIResult EncodeMetalRenderCommands(
    void* NativeCommandBuffer,
    std::span<const FMetalCommandRecord> Records,
    Core::usize& OutConsumed) noexcept;

} // namespace Stoner::Backend::Metal::Private
