#pragma once

#include "Core/FPlatformTypes.h"
#include "Core/FString.h"
#include "Core/TArray.h"

#include <limits>

namespace Stoner::Core
{

enum class EPlatformFileResult : uint8
{
    Success,
    InvalidArgument,
    NotFound,
    AlreadyExists,
    NotRegularFile,
    OutsideRoot,
    LimitExceeded,
    CrossVolume,
    PermissionDenied,
    Busy,
    TimedOut,
    IoError,
    Unsupported
};

struct FPlatformFileStatus
{
    EPlatformFileResult Result = EPlatformFileResult::Success;
    int64 NativeError = 0;
    FString Context;

    [[nodiscard]] bool IsSuccess() const noexcept
    {
        return Result == EPlatformFileResult::Success;
    }
};

struct FPlatformFileInfo
{
    FString Path;
    uint64 ByteSize = 0;
};

struct FPlatformFileEnumerationOptions
{
    usize MaxFiles = 100000;
    usize MaxDepth = 256;
    usize MaxPathBytes = 4096;
};

struct FPlatformFileSystem
{
    [[nodiscard]] static bool Exists(const FString& Path);
    [[nodiscard]] static bool CreateDirectory(const FString& Path);
    [[nodiscard]] static FPlatformFileStatus CanonicalizeExistingPath(
        const FString& Path,
        FString& OutCanonicalPath);
    [[nodiscard]] static bool ReadFile(const FString& Path, TArray<uint8>& OutData);
    [[nodiscard]] static FPlatformFileStatus ReadRegularFileBounded(
        const FString& Path,
        uint64 MaxBytes,
        TArray<uint8>& OutData);
    [[nodiscard]] static bool WriteFile(const FString& Path, const TArray<uint8>& Data);

    [[nodiscard]] static FPlatformFileStatus QueryRegularFile(
        const FString& Path,
        uint64 MaxBytes,
        FPlatformFileInfo& OutInfo);
    [[nodiscard]] static FPlatformFileStatus EnumerateRegularFiles(
        const FString& Root,
        const FPlatformFileEnumerationOptions& Options,
        TArray<FPlatformFileInfo>& OutFiles);
    [[nodiscard]] static FPlatformFileStatus CheckContainedPath(
        const FString& Root,
        const FString& Candidate,
        bool& OutContained);
    [[nodiscard]] static FPlatformFileStatus MoveDirectoryNoReplace(
        const FString& Source,
        const FString& Destination);
    [[nodiscard]] static FPlatformFileStatus ReplaceFileAtomic(
        const FString& Source,
        const FString& Destination);
    [[nodiscard]] static FPlatformFileStatus WriteFileDurable(
        const FString& Path,
        const TArray<uint8>& Data);
    [[nodiscard]] static FPlatformFileStatus RemoveTreeContained(
        const FString& AllowedRoot,
        const FString& Candidate,
        usize MaxEntries = 100000);
};

} // namespace Stoner::Core
