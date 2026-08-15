#include "AssetManagerGenerationLeaseProcessTests.h"

#include "Asset/FGenerationReaderLease.h"
#include "Core/SGPlatform.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>

#if SG_PLATFORM_WINDOWS
#define NOMINMAX
#include <Windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace
{
using namespace Stoner;
using namespace Stoner::Asset;

void Record(
    FAssetManagerGenerationLeaseProcessTestResult& Result,
    bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

int RunProbe(
    const std::filesystem::path& Executable,
    const char* Mode,
    const std::filesystem::path& Publication,
    const std::filesystem::path& Coordination,
    const FAssetDigest& Generation,
    int Timeout,
    int Hold)
{
    const std::string TimeoutText = std::to_string(Timeout);
    const std::string HoldText = std::to_string(Hold);
    const std::string GenerationText = Generation.ToLowerHex().ToStdString();
#if SG_PLATFORM_WINDOWS
    std::wstring Command = L"\"" + Executable.wstring() + L"\" " +
        std::filesystem::path(Mode).wstring() + L" \"" +
        Publication.wstring() + L"\" \"" + Coordination.wstring() +
        L"\" " + std::filesystem::path(GenerationText).wstring() + L" " +
        std::filesystem::path(TimeoutText).wstring() + L" " +
        std::filesystem::path(HoldText).wstring();
    STARTUPINFOW Startup{};
    Startup.cb = sizeof(Startup);
    PROCESS_INFORMATION Process{};
    if (!::CreateProcessW(nullptr, Command.data(), nullptr, nullptr, TRUE, 0,
            nullptr, nullptr, &Startup, &Process))
        return -1;
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
        ::execl(Executable.c_str(), Executable.c_str(), Mode,
            Publication.c_str(), Coordination.c_str(), GenerationText.c_str(),
            TimeoutText.c_str(), HoldText.c_str(),
            static_cast<char*>(nullptr));
        ::_exit(127);
    }
    if (Child < 0) return -1;
    int Status = 0;
    return ::waitpid(Child, &Status, 0) == Child && WIFEXITED(Status)
        ? WEXITSTATUS(Status)
        : -1;
#endif
}
} // namespace

FAssetManagerGenerationLeaseProcessTestResult
RunAssetManagerGenerationLeaseProcessTests(const char* ProbeExecutable)
{
    FAssetManagerGenerationLeaseProcessTestResult Result;
    const auto Probe = ProbeExecutable
        ? std::filesystem::path(ProbeExecutable)
        : std::filesystem::path();
    const auto Token = std::chrono::steady_clock::now()
        .time_since_epoch().count();
    const auto Root = std::filesystem::temp_directory_path() /
        ("sg-generation-lease-process-" + std::to_string(Token));
    const auto Publication = Root / "Published";
    const auto Coordination = Root / "Coordination";
    std::error_code Error;
    std::filesystem::create_directories(Publication, Error);
    std::filesystem::create_directories(Coordination, Error);
    const FAssetDigest First = FAssetDigest::FromBytes(
        Core::TArray<Core::uint8>{1});
    const FAssetDigest Second = FAssetDigest::FromBytes(
        Core::TArray<Core::uint8>{2});

    FGenerationReaderLease ParentReader;
    const bool ParentHeld = FGenerationReaderLease::Acquire(
        Core::FString(Publication.generic_string()),
        Core::FString(Coordination.generic_string()), First, 1000,
        ParentReader) == EAssetResult::Success;
    Record(Result,
        ParentHeld && RunProbe(Probe, "reader", Publication, Coordination,
            First, 1000, 0) == 0,
        "same-generation readers coexist across processes");
    Record(Result,
        RunProbe(Probe, "exclusive", Publication, Coordination,
            First, 30, 0) == 9,
        "exclusive maintenance times out while a reader is held");
    Record(Result,
        RunProbe(Probe, "exclusive", Publication, Coordination,
            Second, 1000, 0) == 0,
        "different generations coordinate independently");
    ParentReader.Release();
    Record(Result,
        RunProbe(Probe, "exclusive", Publication, Coordination,
            First, 1000, 0) == 0,
        "exclusive maintenance succeeds after final reader release");
    Record(Result,
        RunProbe(Probe, "reader-crash", Publication, Coordination,
            First, 1000, 0) == 0 &&
        RunProbe(Probe, "exclusive", Publication, Coordination,
            First, 1000, 0) == 0,
        "process exit releases generation reader ownership without PID heuristics");
    std::filesystem::remove_all(Root, Error);
    return Result;
}
