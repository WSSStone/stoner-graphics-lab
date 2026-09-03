#pragma once

#include "Core/CoreMinimal.h"

namespace Stoner::RHI
{

struct FRHIPresentationSurfaceDesc
{
    Stoner::Core::uint64 SurfaceId = 0;
    Stoner::Core::FPlatformWindow Window;
    Stoner::Core::FString DebugName = "PrimarySurface";

    [[nodiscard]] bool IsValid() const noexcept { return Window.IsValid(); }
};

} // namespace Stoner::RHI
