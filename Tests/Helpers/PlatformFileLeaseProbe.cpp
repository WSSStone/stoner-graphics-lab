#include "Core/FPlatformFileLease.h"

#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#else
#include <unistd.h>
#endif

namespace
{

int ParseNonNegative(const char* Text)
{
    if (Text == nullptr)
    {
        return -1;
    }
    char* End = nullptr;
    const long Value = std::strtol(Text, &End, 10);
    if (End == Text || *End != '\0' || Value < 0 || Value > 600000)
    {
        return -1;
    }
    return static_cast<int>(Value);
}

void SpawnSleepingChild(const char* Self, int Milliseconds)
{
#if defined(_WIN32)
    (void)Self;
    std::wstring Command = L"cmd.exe /C ping 127.0.0.1 -n 2 >NUL";
    STARTUPINFOW Startup{};
    Startup.cb = sizeof(Startup);
    PROCESS_INFORMATION Process{};
    if (::CreateProcessW(
            nullptr, Command.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW, nullptr, nullptr, &Startup, &Process))
    {
        ::CloseHandle(Process.hThread);
        ::CloseHandle(Process.hProcess);
    }
    (void)Milliseconds;
#else
    if (::fork() == 0)
    {
        const std::string Delay = std::to_string(Milliseconds);
        ::execl(
            Self,
            Self,
            "sleep-only",
            Delay.c_str(),
            static_cast<char*>(nullptr));
        ::_exit(0);
    }
#endif
}

} // namespace

int main(int ArgCount, char* Arguments[])
{
    if (ArgCount == 3 && std::string(Arguments[1]) == "sleep-only")
    {
        const int Delay = ParseNonNegative(Arguments[2]);
        if (Delay < 0)
        {
            return 2;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(Delay));
        return 0;
    }
    if (ArgCount != 5)
    {
        return 2;
    }
    const int TimeoutMilliseconds = ParseNonNegative(Arguments[3]);
    const int HoldMilliseconds = ParseNonNegative(Arguments[4]);
    if (TimeoutMilliseconds < 0 || HoldMilliseconds < 0)
    {
        return 2;
    }

    Stoner::Core::FPlatformFileLease Lease;
    const auto Status = Stoner::Core::FPlatformFileLease::Acquire(
        Stoner::Core::FString(Arguments[2]),
        static_cast<Stoner::Core::uint64>(TimeoutMilliseconds),
        Stoner::Core::FString("owner=probe\n"),
        Lease);
    if (Status.Result == Stoner::Core::EPlatformFileResult::TimedOut)
    {
        return 9;
    }
    if (!Status.IsSuccess())
    {
        return 10;
    }

    const std::string Mode(Arguments[1]);
    if (Mode == "crash")
    {
        std::_Exit(0);
    }
    if (Mode == "spawn-child-and-exit")
    {
        SpawnSleepingChild(Arguments[0], HoldMilliseconds);
        std::_Exit(0);
    }
    if (Mode != "acquire")
    {
        return 2;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(HoldMilliseconds));
    return 0;
}
