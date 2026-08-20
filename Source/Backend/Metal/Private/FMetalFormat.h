#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIFormat.h"

namespace Stoner::Backend::Metal::Private
{

[[nodiscard]] Core::uint64 ToMetalPixelFormat(
    RHI::ERHIFormat Format) noexcept;
[[nodiscard]] bool IsMetalFormatSupported(
    void* NativeDevice,
    RHI::ERHIFormat Format) noexcept;

} // namespace Stoner::Backend::Metal::Private
