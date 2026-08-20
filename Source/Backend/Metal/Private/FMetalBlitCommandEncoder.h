#pragma once

#include "FMetalCommandBuffer.h"

namespace Stoner::Backend::Metal::Private
{

[[nodiscard]] RHI::ERHIResult EncodeMetalBlitCommand(
    void* NativeCommandBuffer,
    const FMetalCommandRecord& Record) noexcept;

} // namespace Stoner::Backend::Metal::Private
