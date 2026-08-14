#include "FPlatformFileSystemInternal.h"

#include "Core/SGPlatform.h"

#if SG_PLATFORM_WINDOWS

#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <limits>

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

} // namespace

bool PlatformReadFile(
    const std::filesystem::path& Path,
    TArray<uint8>& OutData)
{
    OutData.clear();
    HANDLE File = ::CreateFileW(
        Path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr);
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
    const DWORD Attributes = ::GetFileAttributesW(Destination.c_str());
    if (Attributes != INVALID_FILE_ATTRIBUTES)
    {
        if (!::ReplaceFileW(
                Destination.c_str(), Source.c_str(), nullptr,
                REPLACEFILE_WRITE_THROUGH, nullptr, nullptr))
        {
            return FromWindowsError(GetLastError(), "replace-file:replace");
        }
        return {};
    }
    if (GetLastError() != ERROR_FILE_NOT_FOUND &&
        GetLastError() != ERROR_PATH_NOT_FOUND)
    {
        return FromWindowsError(GetLastError(), "replace-file:query");
    }
    if (!::MoveFileExW(
            Source.c_str(), Destination.c_str(), MOVEFILE_WRITE_THROUGH))
    {
        return FromWindowsError(GetLastError(), "replace-file:move");
    }
    return {};
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
