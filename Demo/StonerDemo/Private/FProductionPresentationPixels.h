#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIFormat.h"

#include <span>

namespace Stoner::Demo
{

[[nodiscard]] bool BuildAspectFitPresentationPixels(
    std::span<const Core::uint8> Source,
    Core::uint32 SourceWidth,
    Core::uint32 SourceHeight,
    Core::uint32 SourceRowPitch,
    Core::uint32 TargetWidth,
    Core::uint32 TargetHeight,
    RHI::ERHIFormat TargetFormat,
    Core::TArray<Core::uint8>& OutNative);

} // namespace Stoner::Demo
