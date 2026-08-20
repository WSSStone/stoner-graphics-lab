#include "Core/FPlatformProcess.h"

#include "Core/SGPlatform.h"
#include "FPlatformProcessInternal.h"

#include <algorithm>
#include <chrono>
#include <cerrno>
#include <filesystem>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if SG_PLATFORM_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace Stoner::Core
{

namespace
{

std::filesystem::path ToNativePath(const FString& Path)
{
    const std::string Utf8Path = Path.ToStdString();
    std::u8string NativeUtf8Path;
    NativeUtf8Path.reserve(Utf8Path.size());
    for (const unsigned char Byte : Utf8Path)
    {
        NativeUtf8Path.push_back(static_cast<char8_t>(Byte));
    }
    return std::filesystem::path(NativeUtf8Path);
}

void AppendBounded(
    std::string& Output,
    const char* Data,
    std::size_t Size,
    usize Limit,
    bool& Truncated)
{
    const std::size_t Remaining =
        Output.size() < Limit ? Limit - Output.size() : 0;
    const std::size_t Accepted = std::min(Size, Remaining);
    Output.append(Data, Accepted);
    Truncated = Truncated || Accepted != Size;
}

#if SG_PLATFORM_WINDOWS

bool Utf8ToWide(const FString& Input, std::wstring& Out)
{
    const std::string Text = Input.ToStdString();
    if (Text.empty())
    {
        Out.clear();
        return true;
    }
    const int Required = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, Text.data(),
        static_cast<int>(Text.size()), nullptr, 0);
    if (Required <= 0)
    {
        return false;
    }
    Out.resize(static_cast<std::size_t>(Required));
    return MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, Text.data(),
        static_cast<int>(Text.size()), Out.data(), Required) == Required;
}

std::wstring QuoteWindowsArgument(const std::wstring& Argument)
{
    if (!Argument.empty() &&
        Argument.find_first_of(L" \t\n\v\"") == std::wstring::npos)
    {
        return Argument;
    }
    std::wstring Quoted(1, L'"');
    std::size_t Backslashes = 0;
    for (const wchar_t Character : Argument)
    {
        if (Character == L'\\')
        {
            ++Backslashes;
            continue;
        }
        if (Character == L'"')
        {
            Quoted.append(Backslashes * 2 + 1, L'\\');
            Quoted.push_back(L'"');
            Backslashes = 0;
            continue;
        }
        Quoted.append(Backslashes, L'\\');
        Backslashes = 0;
        Quoted.push_back(Character);
    }
    Quoted.append(Backslashes * 2, L'\\');
    Quoted.push_back(L'"');
    return Quoted;
}

void CloseHandleIfValid(HANDLE& Handle)
{
    if (Handle != nullptr && Handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(Handle);
        Handle = nullptr;
    }
}

void DrainWindowsPipe(
    HANDLE& Pipe,
    std::string& Output,
    usize Limit,
    bool& Truncated)
{
    if (Pipe == nullptr)
    {
        return;
    }
    for (;;)
    {
        DWORD Available = 0;
        if (!PeekNamedPipe(Pipe, nullptr, 0, nullptr, &Available, nullptr))
        {
            CloseHandleIfValid(Pipe);
            return;
        }
        if (Available == 0)
        {
            return;
        }
        char Buffer[4096];
        DWORD Read = 0;
        const DWORD Requested = std::min<DWORD>(Available, sizeof(Buffer));
        if (!ReadFile(Pipe, Buffer, Requested, &Read, nullptr) || Read == 0)
        {
            CloseHandleIfValid(Pipe);
            return;
        }
        AppendBounded(Output, Buffer, Read, Limit, Truncated);
    }
}

FProcessExecutionResult ExecuteWindows(const FProcessExecutionRequest& Request)
{
    FProcessExecutionResult Result;
    std::wstring Executable;
    if (!Utf8ToWide(Request.ExecutablePath, Executable))
    {
        return Result;
    }
    std::wstring CommandLine = QuoteWindowsArgument(Executable);
    for (const FString& Argument : Request.Arguments)
    {
        std::wstring WideArgument;
        if (!Utf8ToWide(Argument, WideArgument))
        {
            return Result;
        }
        CommandLine.push_back(L' ');
        CommandLine += QuoteWindowsArgument(WideArgument);
    }
    CommandLine.push_back(L'\0');

    SECURITY_ATTRIBUTES Security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE StdoutRead = nullptr;
    HANDLE StdoutWrite = nullptr;
    HANDLE StderrRead = nullptr;
    HANDLE StderrWrite = nullptr;
    if (!CreatePipe(&StdoutRead, &StdoutWrite, &Security, 0) ||
        !CreatePipe(&StderrRead, &StderrWrite, &Security, 0) ||
        !SetHandleInformation(StdoutRead, HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(StderrRead, HANDLE_FLAG_INHERIT, 0))
    {
        CloseHandleIfValid(StdoutRead);
        CloseHandleIfValid(StdoutWrite);
        CloseHandleIfValid(StderrRead);
        CloseHandleIfValid(StderrWrite);
        Result.Status = EProcessExecutionStatus::LaunchFailed;
        return Result;
    }

    STARTUPINFOW Startup{};
    Startup.cb = sizeof(Startup);
    Startup.dwFlags = STARTF_USESTDHANDLES;
    Startup.hStdOutput = StdoutWrite;
    Startup.hStdError = StderrWrite;
    Startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION Process{};
    const BOOL Created = CreateProcessW(
        Executable.c_str(), CommandLine.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr, &Startup, &Process);
    CloseHandleIfValid(StdoutWrite);
    CloseHandleIfValid(StderrWrite);
    if (!Created)
    {
        CloseHandleIfValid(StdoutRead);
        CloseHandleIfValid(StderrRead);
        Result.Status = EProcessExecutionStatus::LaunchFailed;
        return Result;
    }
    CloseHandleIfValid(Process.hThread);

    std::string Stdout;
    std::string Stderr;
    const auto Deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(Request.Limits.TimeoutMilliseconds);
    bool TimedOut = false;
    for (;;)
    {
        DrainWindowsPipe(
            StdoutRead, Stdout, Request.Limits.MaxStdoutBytes,
            Result.bStdoutTruncated);
        DrainWindowsPipe(
            StderrRead, Stderr, Request.Limits.MaxStderrBytes,
            Result.bStderrTruncated);
        const DWORD Wait = WaitForSingleObject(Process.hProcess, 5);
        if (Wait == WAIT_OBJECT_0)
        {
            break;
        }
        if (std::chrono::steady_clock::now() >= Deadline)
        {
            TimedOut = true;
            TerminateProcess(Process.hProcess, 1);
            WaitForSingleObject(Process.hProcess, INFINITE);
            break;
        }
    }
    for (int Attempt = 0; Attempt < 100 &&
         (StdoutRead != nullptr || StderrRead != nullptr); ++Attempt)
    {
        DrainWindowsPipe(
            StdoutRead, Stdout, Request.Limits.MaxStdoutBytes,
            Result.bStdoutTruncated);
        DrainWindowsPipe(
            StderrRead, Stderr, Request.Limits.MaxStderrBytes,
            Result.bStderrTruncated);
        if (StdoutRead != nullptr || StderrRead != nullptr)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    CloseHandleIfValid(StdoutRead);
    CloseHandleIfValid(StderrRead);
    DWORD ExitCode = 0;
    GetExitCodeProcess(Process.hProcess, &ExitCode);
    CloseHandleIfValid(Process.hProcess);
    Result.Status = TimedOut
        ? EProcessExecutionStatus::TimedOut
        : EProcessExecutionStatus::Completed;
    Result.ExitCode = TimedOut ? -1 : static_cast<int32>(ExitCode);
    Result.StandardOutput = FString(Stdout);
    Result.StandardError = FString(Stderr);
    return Result;
}

#else

void CloseDescriptor(int& Descriptor)
{
    if (Descriptor >= 0)
    {
        close(Descriptor);
        Descriptor = -1;
    }
}

void DrainPosixPipe(
    int& Descriptor,
    std::string& Output,
    usize Limit,
    bool& Truncated)
{
    if (Descriptor < 0)
    {
        return;
    }
    char Buffer[4096];
    for (;;)
    {
        const ssize_t Read = read(Descriptor, Buffer, sizeof(Buffer));
        if (Read > 0)
        {
            AppendBounded(
                Output, Buffer, static_cast<std::size_t>(Read), Limit,
                Truncated);
            continue;
        }
        if (Read == 0)
        {
            CloseDescriptor(Descriptor);
        }
        else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
        {
            CloseDescriptor(Descriptor);
        }
        return;
    }
}

FProcessExecutionResult ExecutePosix(const FProcessExecutionRequest& Request)
{
    FProcessExecutionResult Result;
    const std::filesystem::path Executable = ToNativePath(Request.ExecutablePath);
    if (access(Executable.c_str(), X_OK) != 0)
    {
        Result.Status = EProcessExecutionStatus::LaunchFailed;
        return Result;
    }

    std::vector<std::string> ArgumentStorage;
    ArgumentStorage.reserve(Request.Arguments.size() + 1);
    ArgumentStorage.push_back(Request.ExecutablePath.ToStdString());
    for (const FString& Argument : Request.Arguments)
    {
        ArgumentStorage.push_back(Argument.ToStdString());
    }
    std::vector<char*> Arguments;
    Arguments.reserve(ArgumentStorage.size() + 1);
    for (std::string& Argument : ArgumentStorage)
    {
        Arguments.push_back(Argument.data());
    }
    Arguments.push_back(nullptr);

    int StdoutPipe[2]{-1, -1};
    int StderrPipe[2]{-1, -1};
    if (pipe(StdoutPipe) != 0 || pipe(StderrPipe) != 0)
    {
        CloseDescriptor(StdoutPipe[0]);
        CloseDescriptor(StdoutPipe[1]);
        CloseDescriptor(StderrPipe[0]);
        CloseDescriptor(StderrPipe[1]);
        Result.Status = EProcessExecutionStatus::LaunchFailed;
        return Result;
    }

    const pid_t Child = fork();
    if (Child < 0)
    {
        CloseDescriptor(StdoutPipe[0]);
        CloseDescriptor(StdoutPipe[1]);
        CloseDescriptor(StderrPipe[0]);
        CloseDescriptor(StderrPipe[1]);
        Result.Status = EProcessExecutionStatus::LaunchFailed;
        return Result;
    }
    if (Child == 0)
    {
        close(StdoutPipe[0]);
        close(StderrPipe[0]);
        if (dup2(StdoutPipe[1], STDOUT_FILENO) < 0 ||
            dup2(StderrPipe[1], STDERR_FILENO) < 0)
        {
            _exit(126);
        }
        close(StdoutPipe[1]);
        close(StderrPipe[1]);
        execv(Executable.c_str(), Arguments.data());
        _exit(127);
    }

    CloseDescriptor(StdoutPipe[1]);
    CloseDescriptor(StderrPipe[1]);
    fcntl(StdoutPipe[0], F_SETFL, fcntl(StdoutPipe[0], F_GETFL) | O_NONBLOCK);
    fcntl(StderrPipe[0], F_SETFL, fcntl(StderrPipe[0], F_GETFL) | O_NONBLOCK);

    std::string Stdout;
    std::string Stderr;
    int WaitStatus = 0;
    bool Exited = false;
    bool TimedOut = false;
    const auto Deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(Request.Limits.TimeoutMilliseconds);
    while (!Exited || StdoutPipe[0] >= 0 || StderrPipe[0] >= 0)
    {
        DrainPosixPipe(
            StdoutPipe[0], Stdout, Request.Limits.MaxStdoutBytes,
            Result.bStdoutTruncated);
        DrainPosixPipe(
            StderrPipe[0], Stderr, Request.Limits.MaxStderrBytes,
            Result.bStderrTruncated);
        if (!Exited)
        {
            const pid_t Waited = waitpid(Child, &WaitStatus, WNOHANG);
            Exited = Waited == Child;
            if (!Exited && std::chrono::steady_clock::now() >= Deadline)
            {
                TimedOut = true;
                kill(Child, SIGKILL);
                while (waitpid(Child, &WaitStatus, 0) < 0 && errno == EINTR)
                {
                }
                Exited = true;
            }
        }
        if (!Exited || StdoutPipe[0] >= 0 || StderrPipe[0] >= 0)
        {
            poll(nullptr, 0, 1);
        }
    }
    CloseDescriptor(StdoutPipe[0]);
    CloseDescriptor(StderrPipe[0]);
    Result.StandardOutput = FString(Stdout);
    Result.StandardError = FString(Stderr);
    if (TimedOut)
    {
        Result.Status = EProcessExecutionStatus::TimedOut;
    }
    else if (WIFEXITED(WaitStatus))
    {
        Result.Status = EProcessExecutionStatus::Completed;
        Result.ExitCode = static_cast<int32>(WEXITSTATUS(WaitStatus));
    }
    else
    {
        Result.Status = EProcessExecutionStatus::Terminated;
    }
    return Result;
}

#endif

} // namespace

bool Detail::IsExplicitDynamicModulePath(const FString& Path)
{
    if (Path.IsEmpty())
    {
        return false;
    }

    const std::filesystem::path NativePath = ToNativePath(Path);
#if SG_PLATFORM_WINDOWS
    return NativePath.has_parent_path() || NativePath.has_root_name();
#else
    return NativePath.has_parent_path();
#endif
}

bool Detail::IsValidProcessRequest(const FProcessExecutionRequest& Request)
{
    constexpr usize MaxArgumentCount = 256;
    constexpr usize MaxArgumentBytes = 1024U * 1024U;
    constexpr usize MaxCaptureBytes = 16U * 1024U * 1024U;
    constexpr uint64 MaxTimeoutMilliseconds = 60U * 60U * 1000U;
    if (!IsExplicitDynamicModulePath(Request.ExecutablePath) ||
        Request.Arguments.size() > MaxArgumentCount ||
        Request.Limits.TimeoutMilliseconds == 0 ||
        Request.Limits.TimeoutMilliseconds > MaxTimeoutMilliseconds ||
        Request.Limits.MaxStdoutBytes > MaxCaptureBytes ||
        Request.Limits.MaxStderrBytes > MaxCaptureBytes)
    {
        return false;
    }
    usize TotalBytes = Request.ExecutablePath.View().size();
    if (TotalBytes > MaxArgumentBytes)
    {
        return false;
    }
    for (const FString& Argument : Request.Arguments)
    {
        if (Argument.View().size() > MaxArgumentBytes - TotalBytes)
        {
            return false;
        }
        TotalBytes += Argument.View().size();
    }
    return true;
}

FProcessExecutionResult FPlatformProcess::Execute(
    const FProcessExecutionRequest& Request)
{
    if (!Detail::IsValidProcessRequest(Request))
    {
        return {};
    }
#if SG_PLATFORM_WINDOWS
    return ExecuteWindows(Request);
#else
    return ExecutePosix(Request);
#endif
}

FDynamicModuleHandle::~FDynamicModuleHandle() noexcept
{
    Reset();
}

FDynamicModuleHandle::FDynamicModuleHandle(FDynamicModuleHandle&& Other) noexcept
    : Handle_(std::exchange(Other.Handle_, nullptr))
{
}

FDynamicModuleHandle& FDynamicModuleHandle::operator=(FDynamicModuleHandle&& Other) noexcept
{
    if (this != &Other)
    {
        Reset();
        Handle_ = std::exchange(Other.Handle_, nullptr);
    }
    return *this;
}

void FDynamicModuleHandle::Reset() noexcept
{
    if (!IsValid())
    {
        return;
    }

#if SG_PLATFORM_WINDOWS
    FreeLibrary(reinterpret_cast<HMODULE>(Handle_));
#else
    dlclose(Handle_);
#endif
    Handle_ = nullptr;
}

FDynamicModuleHandle FPlatformProcess::LoadDynamicModule(const FString& ExplicitPath)
{
    if (!Detail::IsExplicitDynamicModulePath(ExplicitPath))
    {
        return {};
    }

    const std::filesystem::path NativePath = ToNativePath(ExplicitPath);
#if SG_PLATFORM_WINDOWS
    HMODULE Module = LoadLibraryW(NativePath.c_str());
    return FDynamicModuleHandle(reinterpret_cast<void*>(Module));
#else
    void* Module = dlopen(NativePath.c_str(), RTLD_NOW | RTLD_LOCAL);
    return FDynamicModuleHandle(Module);
#endif
}

void* FPlatformProcess::GetSymbol(
    const FDynamicModuleHandle& Module,
    const char* SymbolName) noexcept
{
    if (!Module.IsValid() || SymbolName == nullptr || SymbolName[0] == '\0')
    {
        return nullptr;
    }

#if SG_PLATFORM_WINDOWS
    FARPROC Symbol = GetProcAddress(reinterpret_cast<HMODULE>(Module.Handle_), SymbolName);
    return reinterpret_cast<void*>(Symbol);
#else
    return dlsym(Module.Handle_, SymbolName);
#endif
}

void FPlatformProcess::FreeDynamicModule(FDynamicModuleHandle& Module) noexcept
{
    Module.Reset();
}

} // namespace Stoner::Core
