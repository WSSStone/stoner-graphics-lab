#pragma once

#include "Core/FPlatformTypes.h"
#include "Core/FPlatformFileSystem.h"
#include "Core/TArray.h"

#include <filesystem>
#include <istream>

namespace Stoner::Core::Detail
{

[[nodiscard]] bool ReadExactBytes(
    std::istream& Stream,
    usize ByteCount,
    TArray<uint8>& OutData) noexcept;

[[nodiscard]] std::filesystem::path ToNativePath(const FString& Path);
[[nodiscard]] FString FromNativePath(const std::filesystem::path& Path);
[[nodiscard]] FPlatformFileStatus MakeFileStatus(
    EPlatformFileResult Result,
    int64 NativeError,
    const char* Context);

[[nodiscard]] FPlatformFileStatus PlatformMoveDirectoryNoReplace(
    const std::filesystem::path& Source,
    const std::filesystem::path& Destination);
[[nodiscard]] FPlatformFileStatus PlatformReplaceFileAtomic(
    const std::filesystem::path& Source,
    const std::filesystem::path& Destination);
[[nodiscard]] FPlatformFileStatus PlatformWriteFileDurable(
    const std::filesystem::path& Path,
    const TArray<uint8>& Data);

} // namespace Stoner::Core::Detail
