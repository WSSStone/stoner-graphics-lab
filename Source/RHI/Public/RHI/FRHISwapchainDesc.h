#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIFormat.h"

namespace Stoner::RHI
{

struct FRHISwapchainDesc
{
    Stoner::Core::uint32 Width = 0;
    Stoner::Core::uint32 Height = 0;
    Stoner::Core::uint32 FramesInFlight = 2;
    ERHIFormat PreferredFormat = ERHIFormat::B8G8R8A8_UNorm;
    bool bVSync = true;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return Width > 0 && Height > 0 && FramesInFlight > 0;
    }
};

} // namespace Stoner::RHI
