#pragma once

#include "Core/FPlatformTypes.h"

namespace Stoner::Core
{

struct FProcessMemorySnapshot
{
    uint64 ResidentBytes = 0;
    bool bAvailable = false;
    // Diagnostic-only decomposition. ResidentBytes remains the authoritative
    // cross-platform lifecycle gate metric.
    uint64 PhysicalFootprintBytes = 0;
    uint64 InternalBytes = 0;
    uint64 ExternalBytes = 0;
    uint64 ReusableBytes = 0;
    uint64 CompressedBytes = 0;
    uint64 HeapBytesInUse = 0;
    uint64 HeapBytesAllocated = 0;
    bool bDetailedAvailable = false;
    bool bHeapAvailable = false;
};

class FPlatformMemory
{
public:
    static void ReleaseUnusedHeapPages() noexcept;
    [[nodiscard]] static FProcessMemorySnapshot QueryProcessMemory() noexcept;
};

} // namespace Stoner::Core
