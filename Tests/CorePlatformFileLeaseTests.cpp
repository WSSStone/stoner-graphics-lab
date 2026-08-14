#include "CorePlatformFileLeaseTests.h"

#include "Core/FPlatformFileLease.h"
#include "Core/FPlatformFileSystem.h"
#include "Core/SGPlatform.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <type_traits>

#if SG_PLATFORM_WINDOWS
#define NOMINMAX
#include <Windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace
{

using namespace Stoner::Core;

static_assert(!std::is_copy_constructible_v<FPlatformFileLease>);
static_assert(!std::is_copy_assignable_v<FPlatformFileLease>);
static_assert(std::is_nothrow_move_constructible_v<FPlatformFileLease>);
static_assert(std::is_nothrow_move_assignable_v<FPlatformFileLease>);

void Record(
    FCorePlatformFileLeaseTestResult& Result,
    bool Passed,
    const char* Name)
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

int RunProbe(
    const std::filesystem::path& Executable,
    const char* Mode,
    const std::filesystem::path& LeasePath,
    int TimeoutMilliseconds,
    int HoldMilliseconds)
{
    const std::string Timeout = std::to_string(TimeoutMilliseconds);
    const std::string Hold = std::to_string(HoldMilliseconds);
#if SG_PLATFORM_WINDOWS
    std::wstring Command = L"\"" + Executable.wstring() + L"\" " +
        std::filesystem::path(Mode).wstring() + L" \"" +
        LeasePath.wstring() + L"\" " +
        std::filesystem::path(Timeout).wstring() + L" " +
        std::filesystem::path(Hold).wstring();
    STARTUPINFOW Startup{};
    Startup.cb = sizeof(Startup);
    PROCESS_INFORMATION Process{};
    if (!::CreateProcessW(
            nullptr, Command.data(), nullptr, nullptr, TRUE, 0, nullptr,
            nullptr, &Startup, &Process))
    {
        return -1;
    }
    ::WaitForSingleObject(Process.hProcess, INFINITE);
    DWORD ExitCode = 0;
    ::GetExitCodeProcess(Process.hProcess, &ExitCode);
    ::CloseHandle(Process.hThread);
    ::CloseHandle(Process.hProcess);
    return static_cast<int>(ExitCode);
#else
    const pid_t Child = ::fork();
    if (Child == 0)
    {
        ::execl(
            Executable.c_str(),
            Executable.c_str(),
            Mode,
            LeasePath.c_str(),
            Timeout.c_str(),
            Hold.c_str(),
            static_cast<char*>(nullptr));
        ::_exit(127);
    }
    if (Child < 0)
    {
        return -1;
    }
    int Status = 0;
    if (::waitpid(Child, &Status, 0) != Child || !WIFEXITED(Status))
    {
        return -1;
    }
    return WEXITSTATUS(Status);
#endif
}

} // namespace

FCorePlatformFileLeaseTestResult RunCorePlatformFileLeaseTests(
    const char* ProbeExecutable)
{
    FCorePlatformFileLeaseTestResult Result;
    const auto Token = std::chrono::steady_clock::now()
        .time_since_epoch().count();
    const std::filesystem::path Root = std::filesystem::temp_directory_path() /
        ("StonerFileLease-" + std::to_string(Token));
    const std::filesystem::path LeasePath = Root / "lease.lock";
    std::error_code Error;
    std::filesystem::create_directories(Root, Error);

    FPlatformFileLease Owner;
    Record(Result,
        FPlatformFileLease::Acquire(
            FString(LeasePath.generic_string()), 1000,
            FString("owner=primary\n"), Owner).IsSuccess() && Owner.IsHeld(),
        "File lease acquires native ownership");

    TArray<uint8> Metadata;
    Record(Result,
        FPlatformFileSystem::ReadFile(
            FString(LeasePath.generic_string()), Metadata) && !Metadata.empty(),
        "File lease writes readable owner metadata");

    EPlatformFileResult ContendedResult = EPlatformFileResult::Success;
    std::thread Contender([&]
    {
        FPlatformFileLease Other;
        ContendedResult = FPlatformFileLease::Acquire(
            FString(LeasePath.generic_string()), 30,
            FString("owner=contender\n"), Other).Result;
    });
    Contender.join();
    Record(Result,
        ContendedResult == EPlatformFileResult::TimedOut,
        "Same-process contender observes bounded timeout");

    const std::filesystem::path Probe = ProbeExecutable != nullptr
        ? std::filesystem::path(ProbeExecutable)
        : std::filesystem::path();
    Record(Result,
        !Probe.empty() && RunProbe(Probe, "acquire", LeasePath, 30, 0) == 9,
        "Cross-process contender observes bounded timeout");

    FPlatformFileLease Moved(std::move(Owner));
    Record(Result,
        Moved.IsHeld() && !Owner.IsHeld(),
        "File lease move transfers ownership exactly once");
    Moved.Release();

    Record(Result,
        RunProbe(Probe, "crash", LeasePath, 1000, 0) == 0,
        "Lease probe exits abruptly after acquisition");
    FPlatformFileLease Recovered;
    Record(Result,
        FPlatformFileLease::Acquire(
            FString(LeasePath.generic_string()), 1000,
            FString("owner=recovered\n"), Recovered).IsSuccess(),
        "Native lease is released by process exit");
    Recovered.Release();

    Record(Result,
        RunProbe(Probe, "spawn-child-and-exit", LeasePath, 1000, 500) == 0,
        "Lease probe starts inherited-handle sentinel child");
    FPlatformFileLease NonInherited;
    Record(Result,
        FPlatformFileLease::Acquire(
            FString(LeasePath.generic_string()), 100,
            FString("owner=non-inherited\n"), NonInherited).IsSuccess(),
        "Native lease ownership is not inherited by child process");
    NonInherited.Release();

    std::filesystem::remove_all(Root, Error);
    return Result;
}
