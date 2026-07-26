#include "CorePlatformOwnershipTests.h"

#include "Core/FPlatformMisc.h"
#include "Core/SGPlatform.h"

#include <iostream>

#if SG_PLATFORM_MAC
#include <mach/mach.h>

namespace
{
void Record(FCorePlatformOwnershipTestResult& Result, bool Passed, const char* Name)
{
    if (Passed)
    {
        ++Result.Passed;
        std::cout << "[PASS] " << Name << '\n';
    }
    else
    {
        ++Result.Failed;
        std::cout << "[FAIL] " << Name << '\n';
    }
}

} // namespace
#endif

FCorePlatformOwnershipTestResult RunCorePlatformOwnershipTests()
{
    FCorePlatformOwnershipTestResult Result;
    std::cout << "[INFO] Running Core platform ownership tests\n";

#if SG_PLATFORM_MAC
    const mach_port_t Host = mach_host_self();
    mach_port_urefs_t ReferencesBefore = 0;
    const kern_return_t BeforeResult = Host == MACH_PORT_NULL
        ? KERN_INVALID_NAME
        : mach_port_get_refs(
            mach_task_self(), Host, MACH_PORT_RIGHT_SEND, &ReferencesBefore);

    for (int Index = 0; Index < 1024; ++Index)
    {
        (void)Stoner::Core::FPlatformMisc::GetAvailableMemoryBytes();
    }

    mach_port_urefs_t ReferencesAfter = 0;
    const kern_return_t AfterResult = Host == MACH_PORT_NULL
        ? KERN_INVALID_NAME
        : mach_port_get_refs(
            mach_task_self(), Host, MACH_PORT_RIGHT_SEND, &ReferencesAfter);
    const kern_return_t ReleaseResult = Host == MACH_PORT_NULL
        ? KERN_INVALID_NAME
        : mach_port_deallocate(mach_task_self(), Host);

    Record(Result, BeforeResult == KERN_SUCCESS && AfterResult == KERN_SUCCESS,
        "FPlatformMisc ownership probe reads Mach host send-right references");
    Record(Result,
        BeforeResult == KERN_SUCCESS && AfterResult == KERN_SUCCESS &&
            ReferencesAfter == ReferencesBefore,
        "FPlatformMisc repeated available-memory queries preserve Mach host send-right references");
    Record(Result, ReleaseResult == KERN_SUCCESS,
        "FPlatformMisc ownership probe releases its Mach host send right");
#else
    std::cout << "[INFO] Core platform ownership tests are not applicable on this host\n";
#endif

    std::cout << "[INFO] Core platform ownership tests passed=" << Result.Passed
              << " failed=" << Result.Failed << '\n';
    return Result;
}
