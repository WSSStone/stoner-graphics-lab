#include "AssetCookerCliTests.h"

#include "FAssetCookCli.h"
#include "FAssetCookReportCodec.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#endif

namespace
{

using namespace Stoner;
using namespace Stoner::AssetCooker;
using namespace Stoner::AssetCooker::Private;

void Record(FAssetCookerCliTestResult& Result, bool Passed, const char* Label)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Label << '\n';
}

EAssetCookResultCategory Parse(
    std::initializer_list<const char*> Values,
    FAssetCookCliInvocation* Out = nullptr)
{
    Core::TArray<Core::FString> Arguments;
    for (const char* Value : Values) Arguments.emplace_back(Value);
    FAssetCookCliInvocation Invocation;
    Core::FString Reason;
    const auto Result = FAssetCookCli::Parse(Arguments, Invocation, Reason);
    if (Out) *Out = std::move(Invocation);
    return Result;
}

#if !defined(_WIN32)
std::string Quote(const std::string& Value)
{
    std::string Result = "'";
    for (char Character : Value)
        Result += Character == '\'' ? "'\\''" : std::string(1, Character);
    return Result + "'";
}
#endif

std::string ReadText(const std::filesystem::path& Path)
{
    std::ifstream Input(Path, std::ios::binary);
    return {std::istreambuf_iterator<char>(Input),
        std::istreambuf_iterator<char>()};
}

int RunInvalidCommand(
    const char* Executable,
    const std::filesystem::path& Stdout,
    const std::filesystem::path& Stderr)
{
#if defined(_WIN32)
    SECURITY_ATTRIBUTES Security{};
    Security.nLength = sizeof(Security);
    Security.bInheritHandle = TRUE;
    HANDLE StdoutHandle = ::CreateFileW(
        Stdout.c_str(), GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        &Security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE StderrHandle = ::CreateFileW(
        Stderr.c_str(), GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        &Security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (StdoutHandle == INVALID_HANDLE_VALUE ||
        StderrHandle == INVALID_HANDLE_VALUE)
    {
        if (StdoutHandle != INVALID_HANDLE_VALUE) ::CloseHandle(StdoutHandle);
        if (StderrHandle != INVALID_HANDLE_VALUE) ::CloseHandle(StderrHandle);
        return -1;
    }

    STARTUPINFOW Startup{};
    Startup.cb = sizeof(Startup);
    Startup.dwFlags = STARTF_USESTDHANDLES;
    Startup.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);
    Startup.hStdOutput = StdoutHandle;
    Startup.hStdError = StderrHandle;
    PROCESS_INFORMATION Process{};
    const std::filesystem::path ExecutablePath(Executable);
    std::wstring Command = L"\"" + ExecutablePath.wstring() +
        L"\" not-a-command";
    const BOOL Created = ::CreateProcessW(
        ExecutablePath.c_str(), Command.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr, &Startup, &Process);
    int Status = -1;
    if (Created)
    {
        const DWORD Wait = ::WaitForSingleObject(Process.hProcess, 30000);
        DWORD ExitCode = 0;
        if (Wait == WAIT_OBJECT_0 &&
            ::GetExitCodeProcess(Process.hProcess, &ExitCode))
        {
            Status = static_cast<int>(ExitCode);
        }
        else if (Wait == WAIT_TIMEOUT)
        {
            (void)::TerminateProcess(Process.hProcess, 124);
            (void)::WaitForSingleObject(Process.hProcess, 5000);
        }
        ::CloseHandle(Process.hThread);
        ::CloseHandle(Process.hProcess);
    }
    ::CloseHandle(StdoutHandle);
    ::CloseHandle(StderrHandle);
    return Status;
#else
    const std::string Command = Quote(Executable) +
        " not-a-command > " + Quote(Stdout.string()) + " 2> " +
        Quote(Stderr.string());
    return std::system(Command.c_str());
#endif
}

} // namespace

FAssetCookerCliTestResult RunAssetCookerCliTests(
    const char* AssetCookerExecutable)
{
    FAssetCookerCliTestResult Result;
    FAssetCookCliInvocation Invocation;
    Record(Result, Parse({"cook", "--source-root", "Content", "--cook-all",
        "--target-profile", "Config/AssetCooker/Profiles/Mac-Vulkan.json",
        "--output", "Saved/CliOut", "--ddc", "Saved/CliDdc",
        "--workers", "4", "--lease-timeout-ms", "30000",
        "--normalized-report", "--report", "Saved/CliReport/report.json"},
        &Invocation) == EAssetCookResultCategory::Success &&
        Invocation.Command == EAssetCookReportCommand::Cook,
        "cook grammar accepts one complete strict invocation");
    Record(Result, Parse({"plan", "--source-root", "Content", "--cook-all",
        "--target-profile", "Config/AssetCooker/Profiles/Mac-Vulkan.json",
        "--output", "Saved/PlanOut", "--ddc", "Saved/PlanDdc"}) ==
        EAssetCookResultCategory::Success,
        "plan grammar accepts the mutation-free command surface");
    Record(Result, Parse({"validate", "--output", "Saved/Cooked",
        "--strict-files"}) == EAssetCookResultCategory::Success &&
        Parse({"validate-cache", "--ddc", "Saved/DDC", "--max-errors", "64"}) ==
            EAssetCookResultCategory::Success,
        "published and DDC validation commands have distinct strict grammar");
    Record(Result, Parse({"inspect", "--target-profile",
        "Config/AssetCooker/Profiles/Mac-Vulkan.json"}) ==
        EAssetCookResultCategory::Success,
        "inspect accepts exactly one target subject");

    Record(Result,
        Parse({"cook", "--source-root", "Content", "--cook-all", "--root",
            "Image:Representative.png", "--target-profile",
            "Config/AssetCooker/Profiles/Mac-Vulkan.json", "--output", "O",
            "--ddc", "D"}) == EAssetCookResultCategory::InvalidArguments &&
        Parse({"inspect", "--output", "O", "--ddc", "D", "--key",
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}) ==
            EAssetCookResultCategory::InvalidArguments,
        "mutually exclusive root and inspect subjects are rejected");
    Record(Result,
        Parse({"plan", "--source-root", "Content", "--cook-all",
            "--target-profile", "Config/AssetCooker/Profiles/Mac-Vulkan.json",
            "--output", "O", "--ddc", "D", "--clean"}) ==
            EAssetCookResultCategory::InvalidArguments &&
        Parse({"cook", "--source-root", "Content", "--cook-all",
            "--target-profile", "Config/AssetCooker/Profiles/Mac-Vulkan.json",
            "--output", "O", "--ddc", "D", "--unknown"}) ==
            EAssetCookResultCategory::InvalidArguments,
        "plan mutation flags and unknown options are rejected");
    Record(Result,
        Parse({"cook", "--source-root", "Content", "--cook-all",
            "--target-profile", "Config/AssetCooker/Profiles/Mac-Vulkan.json",
            "--output", "O", "--ddc", "D", "--workers", "0"}) ==
            EAssetCookResultCategory::InvalidArguments &&
        Parse({"cook", "--source-root", "Content", "--cook-all",
            "--target-profile", "Config/AssetCooker/Profiles/Mac-Vulkan.json",
            "--output", "O", "--ddc", "D", "--workers", "33"}) ==
            EAssetCookResultCategory::InvalidArguments &&
        Parse({"cook", "--source-root", "Content", "--cook-all",
            "--target-profile", "Config/AssetCooker/Profiles/Mac-Vulkan.json",
            "--output", "O", "--ddc", "D", "--lease-timeout-ms", "600001"}) ==
            EAssetCookResultCategory::InvalidArguments,
        "worker and lease timeout bounds fail closed");
    Record(Result,
        Parse({"plan", "--source-root", "Content", "--cook-all",
            "--target-profile", "Config/AssetCooker/Profiles/Mac-Vulkan.json",
            "--output", "Saved/Out", "--ddc", "Saved/DDC", "--report",
            "Content/report.json"}) == EAssetCookResultCategory::InvalidArguments,
        "report output may not alias source output or DDC roots");

    const std::vector<EAssetCookResultCategory> Categories{
        EAssetCookResultCategory::Success,
        EAssetCookResultCategory::InvalidArguments,
        EAssetCookResultCategory::InvalidProfile,
        EAssetCookResultCategory::DiscoveryFailure,
        EAssetCookResultCategory::GraphFailure,
        EAssetCookResultCategory::CookFailure,
        EAssetCookResultCategory::CacheFailure,
        EAssetCookResultCategory::SourceChanged,
        EAssetCookResultCategory::LeaseTimeout,
        EAssetCookResultCategory::PublishedValidationFailure,
        EAssetCookResultCategory::PublicationFailure,
        EAssetCookResultCategory::IoFailure,
        EAssetCookResultCategory::InternalFailure};
    bool ExitCodesStable = true;
    for (std::size_t Index = 0; Index < Categories.size(); ++Index)
        ExitCodesStable = ExitCodesStable &&
            FAssetCookReportCodec::ExitCode(Categories[Index]) ==
                (Index == 0 ? 0 : static_cast<int>(Index) + 1);
    Record(Result, ExitCodesStable,
        "result categories map bijectively to stable process exit codes 0 and 2-13");

    const auto Root = std::filesystem::temp_directory_path() /
        "stoner-asset-cooker-cli-subprocess";
    std::filesystem::remove_all(Root);
    std::filesystem::create_directories(Root);
    const auto Stdout = Root / "stdout.txt";
    const auto Stderr = Root / "stderr.txt";
    const int Status = RunInvalidCommand(
        AssetCookerExecutable, Stdout, Stderr);
    Record(Result, Status != 0 && ReadText(Stdout).empty() &&
        ReadText(Stderr).find("invalid-arguments") != std::string::npos,
        "real CLI subprocess sends concise argument failure to stderr");
    std::filesystem::remove_all(Root);
    return Result;
}
