#pragma once

#include "Core/FPlatformTypes.h"

namespace Stoner::Core
{

struct FProcessMemorySnapshot
{
    uint64 ResidentBytes = 0;
    bool bAvailable = false;
};

class FPlatformMemory
{
public:
    static void ReleaseUnusedHeapPages() noexcept;
    [[nodiscard]] static FProcessMemorySnapshot QueryProcessMemory() noexcept;
};

} // namespace Stoner::Core
