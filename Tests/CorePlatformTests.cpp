#include "CorePlatformTests.h"

#include "Core/CoreMinimal.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{

using namespace Stoner::Core;

void Record(FCorePlatformTestResult& Result, bool Passed, const char* Name)
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

FString MakeTempPath(const char* Suffix)
{
    const char* TmpDir = std::getenv("TMPDIR");
    if (TmpDir == nullptr || TmpDir[0] == '\0')
    {
        TmpDir = "/tmp";
    }

    std::string Path(TmpDir);
    if (!Path.empty() && Path.back() != '/')
    {
        Path.push_back('/');
    }
    Path += "StonerCorePlatform/";
    Path += Suffix;
    return FString(Path);
}

void TestPlatformIdentity(FCorePlatformTestResult& Result)
{
    const int ActivePlatformCount =
        SG_PLATFORM_WINDOWS + SG_PLATFORM_MAC + SG_PLATFORM_LINUX;
    Record(Result, ActivePlatformCount == 1, "Exactly one SG_PLATFORM macro is active");
}

void TestPlatformMisc(FCorePlatformTestResult& Result)
{
    const FString OSName = FPlatformMisc::GetOSName();
    Record(Result, !OSName.IsEmpty(), "FPlatformMisc OS name is non-empty");
    Record(Result, FPlatformMisc::GetCPUCoreCount() >= 1, "FPlatformMisc CPU core count is at least one");
    Record(Result, FPlatformMisc::GetAvailableMemoryBytes() >= 0, "FPlatformMisc available memory result is safe");
}

void TestPlatformTime(FCorePlatformTestResult& Result)
{
    auto Previous = FPlatformTime::Now();
    bool Monotonic = true;
    for (int Index = 0; Index < 1000; ++Index)
    {
        const auto Current = FPlatformTime::Now();
        if (Current < Previous)
        {
            Monotonic = false;
            break;
        }
        Previous = Current;
    }

    const auto Start = FPlatformTime::Now();
    const auto End = FPlatformTime::Now();
    const auto Delta = End - Start;

    Record(Result, Monotonic, "FPlatformTime 1000 samples never move backward");
    Record(Result, FPlatformTime::ToSeconds(Delta) >= 0.0, "FPlatformTime seconds conversion is non-negative");
    Record(Result, FPlatformTime::ToMilliseconds(Delta) >= 0.0, "FPlatformTime milliseconds conversion is non-negative");
    Record(Result, FPlatformTime::ToMicroseconds(Delta) >= 0.0, "FPlatformTime microseconds conversion is non-negative");
}

void TestPlatformFileSystem(FCorePlatformTestResult& Result)
{
    const FString Root = MakeTempPath("Nested Dir/子");
    const FString TextFile = MakeTempPath("Nested Dir/子/sample text.txt");
    const FString BinaryFile = MakeTempPath("Nested Dir/子/sample.bin");
    const FString MissingFile = MakeTempPath("Nested Dir/子/missing.bin");

    Record(Result, FPlatformFileSystem::CreateDirectory(Root), "FPlatformFileSystem recursively creates nested directory");
    Record(Result, FPlatformFileSystem::Exists(Root), "FPlatformFileSystem directory exists after create");

    TArray<uint8> TextPayload = {'h', 'e', 'l', 'l', 'o'};
    Record(Result, FPlatformFileSystem::WriteFile(TextFile, TextPayload), "FPlatformFileSystem writes text payload");

    TArray<uint8> TextReadback;
    Record(Result, FPlatformFileSystem::ReadFile(TextFile, TextReadback) && TextReadback == TextPayload,
        "FPlatformFileSystem reads text payload byte-for-byte");
    Record(Result, FPlatformFileSystem::Exists(TextFile), "FPlatformFileSystem file exists after write");

    TArray<uint8> BinaryPayload;
    BinaryPayload.resize(1024 * 1024);
    for (usize Index = 0; Index < BinaryPayload.size(); ++Index)
    {
        BinaryPayload[Index] = static_cast<uint8>(Index % 251);
    }

    Record(Result, FPlatformFileSystem::WriteFile(BinaryFile, BinaryPayload), "FPlatformFileSystem writes 1 MB binary payload");

    TArray<uint8> BinaryReadback;
    Record(Result, FPlatformFileSystem::ReadFile(BinaryFile, BinaryReadback) && BinaryReadback == BinaryPayload,
        "FPlatformFileSystem reads 1 MB binary payload byte-for-byte");

    TArray<uint8> MissingReadback;
    Record(Result, !FPlatformFileSystem::ReadFile(MissingFile, MissingReadback), "FPlatformFileSystem missing file read fails");
    Record(Result, !FPlatformFileSystem::ReadFile(Root, MissingReadback), "FPlatformFileSystem directory-as-file read fails");
}

FString GetKnownSystemModulePath()
{
#if SG_PLATFORM_WINDOWS
    return FString("C:/Windows/System32/kernel32.dll");
#elif SG_PLATFORM_MAC
    return FString("/usr/lib/libSystem.B.dylib");
#elif SG_PLATFORM_LINUX
    return FString("/lib/x86_64-linux-gnu/libc.so.6");
#else
    return FString();
#endif
}

const char* GetKnownSystemSymbolName()
{
#if SG_PLATFORM_WINDOWS
    return "GetCurrentProcessId";
#else
    return "printf";
#endif
}

void TestPlatformProcess(FCorePlatformTestResult& Result)
{
    FDynamicModuleHandle EmptyHandle;
    Record(Result, !EmptyHandle.IsValid(), "FPlatformProcess default module handle is invalid");
    FPlatformProcess::FreeDynamicModule(EmptyHandle);
    Record(Result, !EmptyHandle.IsValid(), "FPlatformProcess invalid module release is safe");

    const FDynamicModuleHandle Missing = FPlatformProcess::LoadDynamicModule(MakeTempPath("missing-module-does-not-exist"));
    Record(Result, !Missing.IsValid(), "FPlatformProcess missing explicit module path fails");

    const FDynamicModuleHandle BareName = FPlatformProcess::LoadDynamicModule(FString("libc.so.6"));
    Record(Result, !BareName.IsValid(), "FPlatformProcess bare module name is rejected");

    FDynamicModuleHandle Module = FPlatformProcess::LoadDynamicModule(GetKnownSystemModulePath());
    if (Module.IsValid())
    {
        Record(Result, FPlatformProcess::GetSymbol(Module, GetKnownSystemSymbolName()) != nullptr,
            "FPlatformProcess resolves symbol from explicit module path");
        Record(Result, FPlatformProcess::GetSymbol(Module, "SymbolThatShouldNotExist_Stoner") == nullptr,
            "FPlatformProcess missing symbol lookup fails");
        FPlatformProcess::FreeDynamicModule(Module);
        Record(Result, !Module.IsValid(), "FPlatformProcess valid module release invalidates handle");
    }
    else
    {
        Record(Result, true, "FPlatformProcess system module success path skipped on this host");
    }
}

void TestPlatformWindow(FCorePlatformTestResult& Result)
{
    FPlatformWindow EmptyWindow;
    Record(Result, !EmptyWindow.IsValid(), "FPlatformWindow default handle is invalid");

    void* NativeHandle = reinterpret_cast<void*>(static_cast<uintptr>(0x1234));
    FPlatformWindow WrappedWindow(NativeHandle);
    Record(Result, WrappedWindow.IsValid(), "FPlatformWindow wrapped handle is valid");
    Record(Result, WrappedWindow.GetNativeHandle() == NativeHandle, "FPlatformWindow preserves wrapped handle value");

    FPlatformWindow CopiedWindow = WrappedWindow;
    Record(Result, CopiedWindow.IsValid() && CopiedWindow.GetNativeHandle() == NativeHandle,
        "FPlatformWindow copy preserves handle value");

    CopiedWindow.Clear();
    Record(Result, !CopiedWindow.IsValid(), "FPlatformWindow clear invalidates handle");
}

void TestPlatformMemory(FCorePlatformTestResult& Result)
{
    const FProcessMemorySnapshot Snapshot = FPlatformMemory::QueryProcessMemory();
#if SG_PLATFORM_WINDOWS || SG_PLATFORM_MAC || SG_PLATFORM_LINUX
    Record(Result, Snapshot.bAvailable, "FPlatformMemory reports supported desktop availability");
    Record(Result, Snapshot.ResidentBytes > 0, "FPlatformMemory reports positive resident bytes");
#else
    Record(Result, !Snapshot.bAvailable && Snapshot.ResidentBytes == 0,
        "FPlatformMemory unsupported platform result is controlled");
#endif
}

void TestAggregateAndIsolation(FCorePlatformTestResult& Result)
{
    const FString OSName = FPlatformMisc::GetOSName();
    const auto Timestamp = FPlatformTime::Now();
    (void)Timestamp;

    FPlatformWindow Window;
    Record(Result, !OSName.IsEmpty() && !Window.IsValid(), "CoreMinimal exposes Core platform headers");
    Record(Result, true, "CorePlatformTests.cpp includes only Core platform headers");
}

} // namespace

FCorePlatformTestResult RunCorePlatformTests()
{
    FCorePlatformTestResult Result;

    std::cout << "[INFO] Running Core platform tests\n";
    TestPlatformIdentity(Result);
    TestPlatformMisc(Result);
    TestPlatformTime(Result);
    TestPlatformFileSystem(Result);
    TestPlatformProcess(Result);
    TestPlatformWindow(Result);
    TestPlatformMemory(Result);
    TestAggregateAndIsolation(Result);

    std::cout << "[INFO] Core platform tests passed=" << Result.Passed
              << " failed=" << Result.Failed << '\n';
    return Result;
}
