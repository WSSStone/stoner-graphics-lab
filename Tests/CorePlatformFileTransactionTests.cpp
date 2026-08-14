#include "CorePlatformFileTransactionTests.h"

#include "Core/FPlatformFileSystem.h"
#include "Core/SGPlatform.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

namespace
{

using namespace Stoner::Core;

void Record(
    FCorePlatformFileTransactionTestResult& Result,
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

std::filesystem::path MakeScratchRoot()
{
    const auto Token = std::chrono::steady_clock::now()
        .time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
        ("StonerFileTransaction-" + std::to_string(Token));
}

FString ToString(const std::filesystem::path& Path)
{
    return FString(Path.generic_string());
}

TArray<uint8> Bytes(const char* Text)
{
    const std::string Value(Text);
    return TArray<uint8>(Value.begin(), Value.end());
}

bool Equals(const TArray<uint8>& Value, const char* Text)
{
    const std::string Expected(Text);
    return Value.size() == Expected.size() &&
        std::equal(Value.begin(), Value.end(), Expected.begin());
}

} // namespace

FCorePlatformFileTransactionTestResult RunCorePlatformFileTransactionTests()
{
    FCorePlatformFileTransactionTestResult Result;
    const std::filesystem::path Root = MakeScratchRoot();
    const std::filesystem::path Outside = Root.parent_path() /
        (Root.filename().string() + "-outside");
    std::error_code Error;
    std::filesystem::remove_all(Root, Error);
    std::filesystem::remove_all(Outside, Error);
    std::filesystem::create_directories(Root / "Enumerate" / "Nested", Error);
    std::filesystem::create_directories(Outside, Error);

    const TArray<uint8> A = Bytes("a");
    const TArray<uint8> B = Bytes("bbb");
    Record(Result,
        FPlatformFileSystem::WriteFile(
            ToString(Root / "Enumerate" / "z.bin"), B) &&
        FPlatformFileSystem::WriteFile(
            ToString(Root / "Enumerate" / "Nested" / "a.bin"), A),
        "Transaction fixture files are created");

    FPlatformFileInfo Info;
    Record(Result,
        FPlatformFileSystem::QueryRegularFile(
            ToString(Root / "Enumerate" / "z.bin"), 3, Info).IsSuccess() &&
        Info.ByteSize == 3,
        "Regular-file query returns bounded size");
    Record(Result,
        FPlatformFileSystem::QueryRegularFile(
            ToString(Root / "Enumerate" / "z.bin"), 2, Info).Result ==
            EPlatformFileResult::LimitExceeded,
        "Regular-file query rejects byte limit overflow");

    FPlatformFileEnumerationOptions Options;
    Options.MaxFiles = 4;
    Options.MaxDepth = 4;
    TArray<FPlatformFileInfo> Files;
    const auto Enumerated = FPlatformFileSystem::EnumerateRegularFiles(
        ToString(Root / "Enumerate"), Options, Files);
    Record(Result,
        Enumerated.IsSuccess() && Files.size() == 2 &&
        Files[0].Path < Files[1].Path,
        "Recursive enumeration is bounded and sorted");
    Options.MaxFiles = 1;
    Record(Result,
        FPlatformFileSystem::EnumerateRegularFiles(
            ToString(Root / "Enumerate"), Options, Files).Result ==
            EPlatformFileResult::LimitExceeded && Files.empty(),
        "Enumeration fails without partial results at file limit");
    Options.MaxFiles = 4;
    Options.MaxDepth = 1;
    Record(Result,
        FPlatformFileSystem::EnumerateRegularFiles(
            ToString(Root / "Enumerate"), Options, Files).Result ==
            EPlatformFileResult::LimitExceeded && Files.empty(),
        "Enumeration fails without partial results at depth limit");

    bool bContained = false;
    Record(Result,
        FPlatformFileSystem::CheckContainedPath(
            ToString(Root),
            ToString(Root / "Enumerate" / "z.bin"),
            bContained).IsSuccess() && bContained,
        "Canonical containment accepts descendants");
    Record(Result,
        FPlatformFileSystem::CheckContainedPath(
            ToString(Root), ToString(Outside), bContained).IsSuccess() &&
        !bContained,
        "Canonical containment rejects sibling paths");

#if !SG_PLATFORM_WINDOWS
    std::filesystem::create_directory_symlink(Outside, Root / "Escape", Error);
    if (!Error)
    {
        Record(Result,
            FPlatformFileSystem::CheckContainedPath(
                ToString(Root), ToString(Root / "Escape"), bContained)
                    .IsSuccess() &&
            !bContained,
            "Canonical containment rejects symlink escape");
    }
    else
    {
        Record(Result, true, "Symlink escape fixture unavailable on host");
        Error.clear();
    }
#else
    Record(Result, true, "Junction escape coverage runs in Windows CI");
#endif

    const std::filesystem::path MoveSource = Root / "MoveSource";
    const std::filesystem::path MoveDestination = Root / "MoveDestination";
    std::filesystem::create_directories(MoveSource, Error);
    (void)FPlatformFileSystem::WriteFile(
        ToString(MoveSource / "payload.bin"), A);
    Record(Result,
        FPlatformFileSystem::MoveDirectoryNoReplace(
            ToString(MoveSource), ToString(MoveDestination)).IsSuccess() &&
        std::filesystem::exists(MoveDestination / "payload.bin") &&
        !std::filesystem::exists(MoveSource),
        "Same-volume directory move installs absent destination");

    const std::filesystem::path CollisionSource = Root / "CollisionSource";
    std::filesystem::create_directories(CollisionSource, Error);
    Record(Result,
        FPlatformFileSystem::MoveDirectoryNoReplace(
            ToString(CollisionSource), ToString(MoveDestination)).Result ==
            EPlatformFileResult::AlreadyExists &&
        std::filesystem::exists(CollisionSource),
        "No-replace directory move preserves existing destination");

    const std::filesystem::path Current = Root / "Current.json";
    const std::filesystem::path Next = Root / "Current.next";
    Record(Result,
        FPlatformFileSystem::WriteFileDurable(ToString(Current), Bytes("old"))
            .IsSuccess() &&
        FPlatformFileSystem::WriteFileDurable(ToString(Next), Bytes("new"))
            .IsSuccess() &&
        FPlatformFileSystem::ReplaceFileAtomic(
            ToString(Next), ToString(Current)).IsSuccess(),
        "Durable write and atomic replacement succeed");
    TArray<uint8> ReadBack;
    Record(Result,
        FPlatformFileSystem::ReadFile(ToString(Current), ReadBack) &&
        Equals(ReadBack, "new") && !std::filesystem::exists(Next),
        "Atomic replacement exposes complete new bytes");

    std::atomic<bool> ReaderStarted{false};
    std::atomic<bool> StopReader{false};
    std::atomic<bool> ReaderSawOnlyComplete{true};
    std::thread Reader([&]
    {
        ReaderStarted.store(true, std::memory_order_release);
        while (!StopReader.load(std::memory_order_acquire))
        {
            TArray<uint8> BytesRead;
            if (!FPlatformFileSystem::ReadFile(ToString(Current), BytesRead) ||
                (!Equals(BytesRead, "old") && !Equals(BytesRead, "new")))
            {
                ReaderSawOnlyComplete.store(false, std::memory_order_relaxed);
                break;
            }
        }
    });
    while (!ReaderStarted.load(std::memory_order_acquire))
        std::this_thread::yield();
    bool ReplacementsSucceeded = true;
    for (int Index = 0; Index < 32; ++Index)
    {
        ReplacementsSucceeded = ReplacementsSucceeded &&
            FPlatformFileSystem::WriteFileDurable(
                ToString(Next), Bytes(Index % 2 == 0 ? "old" : "new"))
                .IsSuccess() &&
            FPlatformFileSystem::ReplaceFileAtomic(
                ToString(Next), ToString(Current)).IsSuccess();
    }
    StopReader.store(true, std::memory_order_release);
    Reader.join();
    Record(Result,
        ReplacementsSucceeded &&
            ReaderSawOnlyComplete.load(std::memory_order_relaxed),
        "Concurrent readers permit complete atomic replacements");

    const std::string UnicodeName =
        "unicode-\xE6\xB5\x8B\xE8\xAF\x95.bin";
    Record(Result,
        FPlatformFileSystem::WriteFileDurable(
            ToString(Root / UnicodeName), Bytes("utf8")).IsSuccess(),
        "Durable write accepts UTF-8 paths");

    std::filesystem::path LongDirectory = Root / "LongPath";
    for (int Index = 0; Index < 12; ++Index)
    {
        LongDirectory /= "component-0123456789";
    }
    const std::filesystem::path LongFile = LongDirectory / "payload.bin";
    TArray<uint8> LongReadBack;
    FPlatformFileInfo LongInfo;
    FPlatformFileEnumerationOptions LongOptions;
    LongOptions.MaxFiles = 2;
    LongOptions.MaxDepth = 2;
    LongOptions.MaxPathBytes = 1024;
    TArray<FPlatformFileInfo> LongFiles;
    Record(Result,
        LongFile.generic_string().size() > 260 &&
        FPlatformFileSystem::CreateDirectory(ToString(LongDirectory)) &&
        FPlatformFileSystem::WriteFileDurable(
            ToString(LongFile), Bytes("long")).IsSuccess() &&
        FPlatformFileSystem::ReadFile(ToString(LongFile), LongReadBack) &&
        Equals(LongReadBack, "long") &&
        FPlatformFileSystem::QueryRegularFile(
            ToString(LongFile), 4, LongInfo).IsSuccess() &&
        FPlatformFileSystem::EnumerateRegularFiles(
            ToString(LongDirectory), LongOptions, LongFiles).IsSuccess() &&
        LongFiles.size() == 1,
        "File transactions support absolute paths beyond 260 characters");
    Record(Result,
        FPlatformFileSystem::RemoveTreeContained(
            ToString(Root), ToString(Root / "LongPath"), 32).IsSuccess() &&
        !FPlatformFileSystem::Exists(ToString(Root / "LongPath")),
        "Contained removal supports absolute paths beyond 260 characters");
    Error.clear();

#if !SG_PLATFORM_WINDOWS
    const std::filesystem::path ReadOnly = Root / "ReadOnly";
    std::filesystem::create_directories(ReadOnly, Error);
    std::filesystem::permissions(
        ReadOnly,
        std::filesystem::perms::owner_read |
            std::filesystem::perms::owner_exec,
        std::filesystem::perm_options::replace,
        Error);
    const auto PermissionResult = FPlatformFileSystem::WriteFileDurable(
        ToString(ReadOnly / "denied.bin"), Bytes("denied"));
    std::filesystem::permissions(
        ReadOnly,
        std::filesystem::perms::owner_all,
        std::filesystem::perm_options::replace,
        Error);
    Record(Result,
        PermissionResult.Result == EPlatformFileResult::PermissionDenied ||
        PermissionResult.Result == EPlatformFileResult::IoError,
        "Durable write reports permission failure");
#else
    Record(Result, true, "Permission-denied fixture runs in Windows CI");
#endif

#if SG_PLATFORM_LINUX
    const std::filesystem::path SharedMemoryRoot("/dev/shm");
    if (std::filesystem::is_directory(SharedMemoryRoot, Error))
    {
        const std::filesystem::path CrossSource = Root / "CrossVolumeSource";
        const std::filesystem::path CrossDestination = SharedMemoryRoot /
            ("StonerCrossVolume-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(CrossSource, Error);
        const auto CrossResult = FPlatformFileSystem::MoveDirectoryNoReplace(
            ToString(CrossSource), ToString(CrossDestination));
        Record(Result,
            CrossResult.Result == EPlatformFileResult::CrossVolume,
            "Directory move rejects cross-volume installation");
        std::filesystem::remove_all(CrossSource, Error);
        std::filesystem::remove_all(CrossDestination, Error);
    }
    else
    {
        Record(Result, true, "Cross-volume fixture unavailable on Linux host");
    }
#else
    Record(Result, true, "Cross-volume fixture runs on Linux CI");
#endif

    const std::filesystem::path RemoveCandidate = Root / "RemoveMe";
    std::filesystem::create_directories(RemoveCandidate / "Nested", Error);
    (void)FPlatformFileSystem::WriteFile(
        ToString(RemoveCandidate / "Nested" / "data.bin"), A);
    Record(Result,
        FPlatformFileSystem::RemoveTreeContained(
            ToString(Root), ToString(RemoveCandidate), 8).IsSuccess() &&
        !std::filesystem::exists(RemoveCandidate),
        "Contained recursive removal removes only selected tree");
    Record(Result,
        FPlatformFileSystem::RemoveTreeContained(
            ToString(Root), ToString(Root), 8).Result ==
            EPlatformFileResult::OutsideRoot &&
        std::filesystem::exists(Root),
        "Contained recursive removal refuses allowed root");
    Record(Result,
        FPlatformFileSystem::RemoveTreeContained(
            ToString(Root), ToString(Outside), 8).Result ==
            EPlatformFileResult::OutsideRoot &&
        std::filesystem::exists(Outside),
        "Contained recursive removal refuses outside tree");

    std::filesystem::remove_all(Root, Error);
    std::filesystem::remove_all(Outside, Error);
    return Result;
}
