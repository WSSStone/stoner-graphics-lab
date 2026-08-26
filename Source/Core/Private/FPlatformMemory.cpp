#include "Core/FPlatformMemory.h"

#include "Core/SGPlatform.h"

#include <chrono>
#include <thread>

#if SG_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#elif SG_PLATFORM_MAC
#include <mach/mach.h>
#include <malloc/malloc.h>
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
#if SG_PLATFORM_MAC
    // A native graphics lifecycle can leave freed allocations cached across
    // several malloc zones. Converge the allocator at the authoritative RSS
    // comparison points without changing the sampled cycles or threshold.
    const auto Release = []
    {
        constexpr int MaxReliefPasses = 8;
        for (int Pass = 0; Pass < MaxReliefPasses; ++Pass)
            if (malloc_zone_pressure_relief(nullptr, 0) == 0) break;
    };
    Release();
    // Completed Metal handlers can publish zero ownership immediately before
    // the native command buffer and driver allocations leave their callback.
    // Keep the authoritative sample fixed, but give that release a bounded,
    // deterministic quiescence window and converge the zones once more.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Release();
#elif SG_PLATFORM_LINUX && defined(__GLIBC__)
    // malloc_trim() reports whether it released pages. A single pass can
    // leave another arena immediately trimmable after the first arena scan,
    // which makes an exact lifecycle checkpoint depend on allocator timing.
    // Bound the convergence loop so the comparison point measures the fully
    // released glibc heap without turning a driver/allocator defect into an
    // unbounded validation stall.
    const auto Release = []
    {
        constexpr int MaxTrimPasses = 8;
        for (int Pass = 0; Pass < MaxTrimPasses; ++Pass)
            if (malloc_trim(0) == 0) break;
    };
    Release();
    // glibc arena release and kernel RSS accounting can settle just after the
    // first trim. Use the same fixed comparison-point quiescence as Metal,
    // then retain exactly one post-quiescence authoritative RSS sample.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Release();
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
