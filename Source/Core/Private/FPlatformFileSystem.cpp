#include "Core/FPlatformFileSystem.h"

#include <filesystem>
#include <fstream>
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

    std::ifstream File(FilePath, std::ios::binary);
    if (!File)
    {
        return false;
    }

    OutData.resize(static_cast<usize>(Size));
    if (!OutData.empty())
    {
        File.read(reinterpret_cast<char*>(OutData.data()), static_cast<std::streamsize>(OutData.size()));
    }

    return File.good() || File.eof();
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
