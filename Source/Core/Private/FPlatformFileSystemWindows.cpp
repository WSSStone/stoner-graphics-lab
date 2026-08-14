#include "FPlatformFileSystemInternal.h"

#include "Core/SGPlatform.h"

#if SG_PLATFORM_WINDOWS

#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <limits>
#include <string>

namespace Stoner::Core::Detail
{

namespace
{

EPlatformFileResult ClassifyWindowsError(DWORD Error)
{
    switch (Error)
    {
    case ERROR_SUCCESS: return EPlatformFileResult::Success;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND: return EPlatformFileResult::NotFound;
    case ERROR_FILE_EXISTS:
    case ERROR_ALREADY_EXISTS: return EPlatformFileResult::AlreadyExists;
    case ERROR_ACCESS_DENIED:
    case ERROR_PRIVILEGE_NOT_HELD: return EPlatformFileResult::PermissionDenied;
    case ERROR_NOT_SAME_DEVICE: return EPlatformFileResult::CrossVolume;
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION: return EPlatformFileResult::Busy;
    default: return EPlatformFileResult::IoError;
    }
}

FPlatformFileStatus FromWindowsError(DWORD Error, const char* Context)
{
    return MakeFileStatus(
        ClassifyWindowsError(Error), static_cast<int64>(Error), Context);
}

bool IsAtomicReadWindow(DWORD Error)
{
    return Error == ERROR_FILE_NOT_FOUND ||
        Error == ERROR_PATH_NOT_FOUND ||
        Error == ERROR_SHARING_VIOLATION;
}

bool IsAtomicReplacementWindow(DWORD Error)
{
    return Error == ERROR_SHARING_VIOLATION ||
        Error == ERROR_ACCESS_DENIED ||
        Error == ERROR_UNABLE_TO_MOVE_REPLACEMENT ||
        Error == ERROR_UNABLE_TO_MOVE_REPLACEMENT_2;
}

} // namespace

bool PlatformReadFile(
    const std::filesystem::path& Path,
    TArray<uint8>& OutData)
{
    OutData.clear();
    HANDLE File = INVALID_HANDLE_VALUE;
    constexpr int MaxOpenAttempts = 64;
    for (int Attempt = 0; Attempt < MaxOpenAttempts; ++Attempt)
    {
        File = ::CreateFileW(
            Path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
            nullptr);
        if (File != INVALID_HANDLE_VALUE ||
            !IsAtomicReadWindow(::GetLastError()))
        {
            break;
        }
        ::Sleep(1);
    }
    if (File == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    LARGE_INTEGER Size{};
    if (!::GetFileSizeEx(File, &Size) || Size.QuadPart < 0 ||
        static_cast<unsigned long long>(Size.QuadPart) >
            static_cast<unsigned long long>(std::numeric_limits<usize>::max()))
    {
        ::CloseHandle(File);
        return false;
    }
    try
    {
        OutData.resize(static_cast<usize>(Size.QuadPart));
    }
    catch (...)
    {
        ::CloseHandle(File);
        OutData.clear();
        return false;
    }

    usize Offset = 0;
    while (Offset < OutData.size())
    {
        const DWORD Chunk = static_cast<DWORD>(std::min<usize>(
            OutData.size() - Offset,
            static_cast<usize>(std::numeric_limits<DWORD>::max())));
        DWORD Read = 0;
        if (!::ReadFile(File, OutData.data() + Offset, Chunk, &Read, nullptr) ||
            Read != Chunk)
        {
            ::CloseHandle(File);
            OutData.clear();
            return false;
        }
        Offset += static_cast<usize>(Read);
    }
    if (!::CloseHandle(File))
    {
        OutData.clear();
        return false;
    }
    return true;
}

FPlatformFileStatus PlatformCanonicalPath(
    const std::filesystem::path& Path,
    std::filesystem::path& OutPath)
{
    OutPath.clear();
    HANDLE File = ::CreateFileW(
        Path.c_str(),
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        nullptr);
    if (File == INVALID_HANDLE_VALUE)
        return FromWindowsError(GetLastError(), "canonical-path:open");

    const DWORD Required = ::GetFinalPathNameByHandleW(
        File, nullptr, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (Required == 0)
    {
        const DWORD Error = GetLastError();
        ::CloseHandle(File);
        return FromWindowsError(Error, "canonical-path:size");
    }
    std::wstring Buffer(static_cast<usize>(Required), L'\0');
    const DWORD Written = ::GetFinalPathNameByHandleW(
        File, Buffer.data(), Required,
        FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    const DWORD Error = Written == 0 || Written >= Required
        ? GetLastError() : ERROR_SUCCESS;
    ::CloseHandle(File);
    if (Error != ERROR_SUCCESS)
        return FromWindowsError(Error, "canonical-path:read");
    Buffer.resize(Written);
    OutPath = std::filesystem::path(std::move(Buffer));
    return {};
}

FPlatformFileStatus PlatformMoveDirectoryNoReplace(
    const std::filesystem::path& Source,
    const std::filesystem::path& Destination)
{
    if (!::MoveFileExW(
            Source.c_str(),
            Destination.c_str(),
            MOVEFILE_WRITE_THROUGH))
    {
        const DWORD Error = GetLastError();
        // MoveFileExW does not consistently use ERROR_ALREADY_EXISTS for an
        // existing directory destination. Preserve the no-replace contract so
        // callers can validate an existing winner regardless of that native
        // error-code variation.
        const DWORD DestinationAttributes =
            ::GetFileAttributesW(Destination.c_str());
        if (DestinationAttributes != INVALID_FILE_ATTRIBUTES)
        {
            return MakeFileStatus(
                EPlatformFileResult::AlreadyExists,
                static_cast<int64>(Error),
                "move-directory:no-replace");
        }
        return FromWindowsError(Error, "move-directory:no-replace");
    }
    return {};
}

FPlatformFileStatus PlatformReplaceFileAtomic(
    const std::filesystem::path& Source,
    const std::filesystem::path& Destination)
{
    constexpr int MaxReplaceAttempts = 64;
    DWORD LastError = ERROR_SUCCESS;
    for (int Attempt = 0; Attempt < MaxReplaceAttempts; ++Attempt)
    {
        if (::ReplaceFileW(
                Destination.c_str(), Source.c_str(), nullptr,
                REPLACEFILE_WRITE_THROUGH, nullptr, nullptr))
        {
            return {};
        }

        LastError = ::GetLastError();
        if (LastError == ERROR_FILE_NOT_FOUND ||
            LastError == ERROR_PATH_NOT_FOUND)
        {
            if (::MoveFileExW(
                    Source.c_str(), Destination.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                return {};
            }
            LastError = ::GetLastError();
        }
        if (!IsAtomicReplacementWindow(LastError))
        {
            return FromWindowsError(LastError, "replace-file:replace");
        }
        ::Sleep(1);
    }
    return FromWindowsError(LastError, "replace-file:retry-limit");
}

FPlatformFileStatus PlatformWriteFileDurable(
    const std::filesystem::path& Path,
    const TArray<uint8>& Data)
{
    HANDLE File = ::CreateFileW(
        Path.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
        nullptr);
    if (File == INVALID_HANDLE_VALUE)
    {
        return FromWindowsError(GetLastError(), "durable-write:open");
    }

    usize Offset = 0;
    while (Offset < Data.size())
    {
        const DWORD Chunk = static_cast<DWORD>(std::min<usize>(
            Data.size() - Offset,
            static_cast<usize>(std::numeric_limits<DWORD>::max())));
        DWORD Written = 0;
        if (!::WriteFile(File, Data.data() + Offset, Chunk, &Written, nullptr) ||
            Written != Chunk)
        {
            const DWORD Error = GetLastError();
            ::CloseHandle(File);
            return FromWindowsError(Error, "durable-write:write");
        }
        Offset += Written;
    }

    if (!::FlushFileBuffers(File))
    {
        const DWORD Error = GetLastError();
        ::CloseHandle(File);
        return FromWindowsError(Error, "durable-write:flush");
    }
    if (!::CloseHandle(File))
    {
        return FromWindowsError(GetLastError(), "durable-write:close");
    }
    return {};
}

} // namespace Stoner::Core::Detail

#endif
