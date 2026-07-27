#include "Core/FPlatformMisc.h"

#include "Core/SGPlatform.h"

#include <thread>

#if SG_PLATFORM_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif SG_PLATFORM_MAC
#include <cstdint>
#include <mach/mach.h>
#include <sys/sysctl.h>
#elif SG_PLATFORM_LINUX
#include <unistd.h>
#endif

namespace Stoner::Core
{

FString FPlatformMisc::GetOSName()
{
#if SG_PLATFORM_WINDOWS
    return FString("Windows");
#elif SG_PLATFORM_MAC
    return FString("macOS");
#elif SG_PLATFORM_LINUX
    return FString("Linux");
#else
    return FString("Unsupported");
#endif
}

uint32 FPlatformMisc::GetCPUCoreCount() noexcept
{
    const unsigned int Count = std::thread::hardware_concurrency();
    return Count > 0 ? static_cast<uint32>(Count) : 1;
}

uint64 FPlatformMisc::GetAvailableMemoryBytes() noexcept
{
#if SG_PLATFORM_WINDOWS
    MEMORYSTATUSEX Status;
    Status.dwLength = sizeof(Status);
    if (GlobalMemoryStatusEx(&Status) != 0)
    {
        return static_cast<uint64>(Status.ullAvailPhys);
    }
    return 0;
#elif SG_PLATFORM_MAC
    vm_statistics64_data_t Stats;
    mach_msg_type_number_t Count = HOST_VM_INFO64_COUNT;
    const mach_port_t Host = mach_host_self();
    if (Host == MACH_PORT_NULL)
    {
        return 0;
    }
    const kern_return_t StatisticsResult =
        host_statistics64(Host, HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&Stats), &Count);
    const kern_return_t DeallocateResult = mach_port_deallocate(mach_task_self(), Host);
    if (StatisticsResult != KERN_SUCCESS || DeallocateResult != KERN_SUCCESS)
    {
        return 0;
    }

    int64_t PageSize = 0;
    size_t PageSizeSize = sizeof(PageSize);
    if (sysctlbyname("hw.pagesize", &PageSize, &PageSizeSize, nullptr, 0) != 0 || PageSize <= 0)
    {
        return 0;
    }

    const uint64 AvailablePages = static_cast<uint64>(Stats.free_count) + static_cast<uint64>(Stats.inactive_count);
    return AvailablePages * static_cast<uint64>(PageSize);
#elif SG_PLATFORM_LINUX
    const long Pages = sysconf(_SC_AVPHYS_PAGES);
    const long PageSize = sysconf(_SC_PAGESIZE);
    if (Pages > 0 && PageSize > 0)
    {
        return static_cast<uint64>(Pages) * static_cast<uint64>(PageSize);
    }
    return 0;
#else
    return 0;
#endif
}

} // namespace Stoner::Core
