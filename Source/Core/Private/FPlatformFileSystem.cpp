#include "Core/FPlatformFileSystem.h"

#include "FPlatformFileSystemInternal.h"

#include <filesystem>
#include <fstream>
#include <limits>
#include <system_error>

namespace Stoner::Core
{

namespace
{

std::filesystem::path ToPath(const FString& Path)
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

} // namespace

bool Detail::ReadExactBytes(
    std::istream& Stream,
    usize ByteCount,
    TArray<uint8>& OutData) noexcept
{
    OutData.clear();
    if (ByteCount > static_cast<usize>(std::numeric_limits<std::streamsize>::max()))
    {
        return false;
    }

    try
    {
        OutData.resize(ByteCount);
        if (ByteCount == 0)
        {
            return true;
        }

        const std::streamsize Requested = static_cast<std::streamsize>(ByteCount);
        Stream.read(reinterpret_cast<char*>(OutData.data()), Requested);
        if (Stream.gcount() != Requested)
        {
            OutData.clear();
            return false;
        }
        return true;
    }
    catch (...)
    {
        OutData.clear();
        return false;
    }
}

bool FPlatformFileSystem::Exists(const FString& Path)
{
    std::error_code Error;
    return std::filesystem::exists(ToPath(Path), Error) && !Error;
}

bool FPlatformFileSystem::CreateDirectory(const FString& Path)
{
    std::error_code Error;
    const std::filesystem::path Directory = ToPath(Path);
    if (std::filesystem::exists(Directory, Error))
    {
        return !Error && std::filesystem::is_directory(Directory, Error) && !Error;
    }

    std::filesystem::create_directories(Directory, Error);
    return !Error && std::filesystem::is_directory(Directory, Error) && !Error;
}

bool FPlatformFileSystem::ReadFile(const FString& Path, TArray<uint8>& OutData)
{
    OutData.clear();

    std::error_code Error;
    const std::filesystem::path FilePath = ToPath(Path);
    if (!std::filesystem::is_regular_file(FilePath, Error) || Error)
    {
        return false;
    }

    const auto Size = std::filesystem::file_size(FilePath, Error);
    if (Error)
    {
        return false;
    }
    if (Size > static_cast<std::uintmax_t>(std::numeric_limits<usize>::max()))
    {
        return false;
    }

    std::ifstream File(FilePath, std::ios::binary);
    if (!File)
    {
        return false;
    }

    return Detail::ReadExactBytes(File, static_cast<usize>(Size), OutData);
}

bool FPlatformFileSystem::WriteFile(const FString& Path, const TArray<uint8>& Data)
{
    std::ofstream File(ToPath(Path), std::ios::binary | std::ios::trunc);
    if (!File)
    {
        return false;
    }

    if (!Data.empty())
    {
        File.write(reinterpret_cast<const char*>(Data.data()), static_cast<std::streamsize>(Data.size()));
    }

    return File.good();
}

} // namespace Stoner::Core
