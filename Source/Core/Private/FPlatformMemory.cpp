#include "Core/FPlatformMemory.h"

#include "Core/SGPlatform.h"

#if SG_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#elif SG_PLATFORM_MAC
#include <mach/mach.h>
#elif SG_PLATFORM_LINUX
#include <cstdio>
#include <unistd.h>
#if defined(__GLIBC__)
#include <malloc.h>
#endif
#endif

namespace Stoner::Core
{

void FPlatformMemory::ReleaseUnusedHeapPages() noexcept
{
#if SG_PLATFORM_LINUX && defined(__GLIBC__)
    // malloc_trim() reports whether it released pages. A single pass can
    // leave another arena immediately trimmable after the first arena scan,
    // which makes an exact lifecycle checkpoint depend on allocator timing.
    // Bound the convergence loop so the comparison point measures the fully
    // released glibc heap without turning a driver/allocator defect into an
    // unbounded validation stall.
    constexpr int MaxTrimPasses = 8;
    for (int Pass = 0; Pass < MaxTrimPasses; ++Pass)
    {
        if (malloc_trim(0) == 0) break;
    }
#endif
}

FProcessMemorySnapshot FPlatformMemory::QueryProcessMemory() noexcept
{
#if SG_PLATFORM_WINDOWS
    PROCESS_MEMORY_COUNTERS_EX Counters{};
    Counters.cb = sizeof(Counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&Counters), sizeof(Counters)))
    {
        return {static_cast<uint64>(Counters.WorkingSetSize), true};
    }
#elif SG_PLATFORM_MAC
    mach_task_basic_info_data_t Info{};
    mach_msg_type_number_t Count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&Info), &Count) == KERN_SUCCESS)
    {
        return {static_cast<uint64>(Info.resident_size), true};
    }
#elif SG_PLATFORM_LINUX
    long ResidentPages = 0;
    FILE* Statm = std::fopen("/proc/self/statm", "r");
    if (Statm != nullptr)
    {
        const int ReadCount = std::fscanf(Statm, "%*s %ld", &ResidentPages);
        std::fclose(Statm);
        const long PageSize = sysconf(_SC_PAGESIZE);
        if (ReadCount == 1 && ResidentPages >= 0 && PageSize > 0)
        {
            return {static_cast<uint64>(ResidentPages) * static_cast<uint64>(PageSize), true};
        }
    }
#endif
    return {};
}

} // namespace Stoner::Core
