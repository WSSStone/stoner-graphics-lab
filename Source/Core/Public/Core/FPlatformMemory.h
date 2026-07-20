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
    [[nodiscard]] static FProcessMemorySnapshot QueryProcessMemory() noexcept;
};

} // namespace Stoner::Core
