#include "AssetCookerPublicationConcurrencyTests.h"

#include "AssetCookerPublicationTestSupport.h"
#include "Core/FPlatformFileLease.h"

#include <chrono>
#include <filesystem>
#include <future>
#include <iostream>
#include <string>
#include <thread>

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace
{

int RunProbe(
    const std::filesystem::path& Executable,
    const char* Mode,
    const std::filesystem::path& Image,
    const std::filesystem::path& Output,
    int Timeout,
    int Hold)
{
    const std::string TimeoutText = std::to_string(Timeout);
    const std::string HoldText = std::to_string(Hold);
#if defined(_WIN32)
    std::wstring Command = L"\"" + Executable.wstring() + L"\" " +
        std::filesystem::path(Mode).wstring() + L" \"" + Image.wstring() +
        L"\" \"" + Output.wstring() + L"\" " +
        std::filesystem::path(TimeoutText).wstring() + L" " +
        std::filesystem::path(HoldText).wstring();
    STARTUPINFOW Startup{};
    Startup.cb = sizeof(Startup);
    PROCESS_INFORMATION Process{};
    if (!::CreateProcessW(nullptr, Command.data(), nullptr, nullptr, FALSE, 0,
            nullptr, nullptr, &Startup, &Process)) return -1;
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
        ::execl(Executable.c_str(), Executable.c_str(), Mode, Image.c_str(),
            Output.c_str(), TimeoutText.c_str(), HoldText.c_str(),
            static_cast<char*>(nullptr));
        ::_exit(127);
    }
    if (Child < 0) return -1;
    int Status = 0;
    if (::waitpid(Child, &Status, 0) != Child || !WIFEXITED(Status)) return -1;
    return WEXITSTATUS(Status);
#endif
}

} // namespace

FAssetCookerPublicationConcurrencyTestResult
RunAssetCookerPublicationConcurrencyTests(const char* ProbeExecutable)
{
    using namespace Stoner;
    using namespace Stoner::Tests::AssetCookerPublication;
    namespace Private = AssetCooker::Private;
    FAssetCookerPublicationConcurrencyTestResult Result;
    const auto Root = std::filesystem::temp_directory_path() /
        "stoner-cooker-publication-concurrency";
    std::filesystem::remove_all(Root);
    std::filesystem::path Content;
    const FRun SeedRun = Seed(Root, Content);
    const auto Output = Root / "Published";

    Core::TArray<std::future<Private::FCookedGenerationPublicationResult>> Writers;
    for (int Index = 0; Index < 8; ++Index)
        Writers.push_back(std::async(std::launch::async, [&SeedRun, &Output]
        {
            return Private::FCookedGenerationPublisher::Publish(
                Request(SeedRun, Output));
        }));
    bool Converged = true;
    for (auto& Writer : Writers) Converged = Converged && Writer.get().Succeeded();
    Converged = Converged &&
        ValidateCurrent(Output).Result == Asset::EAssetResult::Success;
    Record(Result.Passed, Result.Failed, Converged,
        "overlapping equivalent writers converge to one complete generation");

    const std::filesystem::path Probe = ProbeExecutable
        ? std::filesystem::path(ProbeExecutable) : std::filesystem::path();
    std::filesystem::create_directories(Output);
    Core::FPlatformFileLease Owner;
    const auto Held = Core::FPlatformFileLease::Acquire(
        Core::FString((Output / ".publish.lock").generic_string()), 1000,
        Core::FString("owner=test"), Owner);
    Record(Result.Passed, Result.Failed,
        Held.IsSuccess() && RunProbe(Probe, "publish",
            SeedRun.Result.GenerationImageRoot.ToStdString(), Output, 30, 0) == 9,
        "a competing writer process reports the bounded lease timeout");
    Owner.Release();

    auto Holder = std::async(std::launch::async, [&]
    {
        return RunProbe(Probe, "hold",
            SeedRun.Result.GenerationImageRoot.ToStdString(), Output, 1000, 100);
    });
    for (int Attempt = 0;
         Attempt < 100 && !std::filesystem::exists(Output / ".publish.lock");
         ++Attempt)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const int WaitedWriter = RunProbe(Probe, "publish",
        SeedRun.Result.GenerationImageRoot.ToStdString(), Output, 1000, 0);
    Record(Result.Passed, Result.Failed,
        Holder.get() == 0 && WaitedWriter == 0 &&
            ValidateCurrent(Output).Result == Asset::EAssetResult::Success,
        "a competing writer process waits and succeeds after lease release");

    Record(Result.Passed, Result.Failed,
        RunProbe(Probe, "crash", SeedRun.Result.GenerationImageRoot.ToStdString(),
            Output, 1000, 0) == 0 &&
        RunProbe(Probe, "publish", SeedRun.Result.GenerationImageRoot.ToStdString(),
            Output, 1000, 0) == 0 &&
        ValidateCurrent(Output).Result == Asset::EAssetResult::Success,
        "process exit releases publication ownership for the next writer");

    bool ReaderSawOnlyComplete = true;
    auto Writer = std::async(std::launch::async, [&SeedRun, &Output]
    {
        return Private::FCookedGenerationPublisher::Publish(
            Request(SeedRun, Output));
    });
    while (Writer.wait_for(std::chrono::milliseconds(0)) !=
        std::future_status::ready)
    {
        ReaderSawOnlyComplete = ReaderSawOnlyComplete &&
            ValidateCurrent(Output).Result == Asset::EAssetResult::Success;
        std::this_thread::yield();
    }
    ReaderSawOnlyComplete = ReaderSawOnlyComplete && Writer.get().Succeeded() &&
        ValidateCurrent(Output).Result == Asset::EAssetResult::Success;
    Record(Result.Passed, Result.Failed, ReaderSawOnlyComplete,
        "readers observe only a complete old or complete new current pointer");

    std::filesystem::remove_all(Root);
    return Result;
}
