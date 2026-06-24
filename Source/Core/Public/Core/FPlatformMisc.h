#pragma once

#include "Core/FPlatformTypes.h"
#include "Core/FString.h"

namespace Stoner::Core
{

struct FPlatformMisc
{
    [[nodiscard]] static FString GetOSName();
    [[nodiscard]] static uint32 GetCPUCoreCount() noexcept;
    [[nodiscard]] static uint64 GetAvailableMemoryBytes() noexcept;
};

} // namespace Stoner::Core
