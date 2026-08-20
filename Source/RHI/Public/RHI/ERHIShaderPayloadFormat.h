#pragma once

#include "Core/FPlatformTypes.h"

namespace Stoner::RHI
{

enum class ERHIShaderPayloadFormat : Stoner::Core::uint8
{
    Unknown,
    SPIRV,
    MetalLibrary
};

[[nodiscard]] constexpr bool IsValidRHIShaderPayloadFormat(
    ERHIShaderPayloadFormat Format) noexcept
{
    return Format == ERHIShaderPayloadFormat::SPIRV ||
        Format == ERHIShaderPayloadFormat::MetalLibrary;
}

} // namespace Stoner::RHI
