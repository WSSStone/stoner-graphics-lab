#include "Core/FPlatformFileSystem.h"
#include "Core/SGPlatform.h"

#include "FPlatformFileSystemInternal.h"

#include <filesystem>
#include <fstream>
#include <limits>
#include <algorithm>
#include <cctype>
#include <system_error>

namespace Stoner::Core
{

namespace
{

EPlatformFileResult ClassifyError(const std::error_code& Error)
{
    if (!Error)
    {
        return EPlatformFileResult::Success;
    }
    if (Error == std::errc::no_such_file_or_directory)
    {
        return EPlatformFileResult::NotFound;
    }
    if (Error == std::errc::file_exists)
    {
        return EPlatformFileResult::AlreadyExists;
    }
    if (Error == std::errc::permission_denied ||
        Error == std::errc::operation_not_permitted)
    {
        return EPlatformFileResult::PermissionDenied;
    }
    if (Error == std::errc::cross_device_link)
    {
        return EPlatformFileResult::CrossVolume;
    }
    if (Error == std::errc::device_or_resource_busy)
    {
        return EPlatformFileResult::Busy;
    }
    return EPlatformFileResult::IoError;
}

bool IsParentTraversal(const std::filesystem::path& Relative)
{
    if (Relative.empty())
    {
        return false;
    }
    const auto First = *Relative.begin();
    return First == std::filesystem::path("..");
}

bool IsContainedCanonical(
    const std::filesystem::path& Root,
    const std::filesystem::path& Candidate)
{
#if SG_PLATFORM_WINDOWS
    std::string RootText = Root.generic_string();
    std::string CandidateText = Candidate.generic_string();
    std::transform(RootText.begin(), RootText.end(), RootText.begin(),
        [](unsigned char Value) { return static_cast<char>(std::tolower(Value)); });
    std::transform(CandidateText.begin(), CandidateText.end(), CandidateText.begin(),
        [](unsigned char Value) { return static_cast<char>(std::tolower(Value)); });
    if (CandidateText == RootText)
    {
        return true;
    }
    if (!RootText.empty() && RootText.back() != '/')
    {
        RootText.push_back('/');
    }
    return CandidateText.starts_with(RootText);
#else
    const std::filesystem::path Relative = Candidate.lexically_relative(Root);
    return !Relative.empty() && !IsParentTraversal(Relative);
#endif
}

std::filesystem::path CanonicalExisting(
    const std::filesystem::path& Path,
    std::error_code& Error)
{
#if SG_PLATFORM_WINDOWS
    std::filesystem::path Result;
    const FPlatformFileStatus Status =
        Detail::PlatformCanonicalPath(Path, Result);
    if (!Status.IsSuccess())
    {
        Error = std::error_code(
            static_cast<int>(Status.NativeError), std::system_category());
        return {};
    }
    Error.clear();
    return Result;
#else
    return std::filesystem::weakly_canonical(Path, Error);
#endif
}

} // namespace

std::filesystem::path Detail::ToNativePath(const FString& Path)
{
    const std::string Utf8Path = Path.ToStdString();
    std::u8string NativeUtf8Path;
    NativeUtf8Path.reserve(Utf8Path.size());
    for (const unsigned char Byte : Utf8Path)
    {
        NativeUtf8Path.push_back(static_cast<char8_t>(Byte));
    }
    std::filesystem::path NativePath(NativeUtf8Path);
#if SG_PLATFORM_WINDOWS
    NativePath.make_preferred();
    // Keep ordinary absolute paths on the conventional Win32 spelling. Some
    // standard-library filesystem operations treat the extended namespace
    // differently even when the path is short; opt in only before directory
    // paths approach the legacy 248-character creation limit.
    if (NativePath.is_absolute() && NativePath.native().size() >= 248)
    {
        const std::wstring Native = NativePath.native();
        if (!Native.starts_with(L"\\\\?\\"))
        {
            if (Native.starts_with(L"\\\\"))
                return std::filesystem::path(
                    L"\\\\?\\UNC\\" + Native.substr(2));
            return std::filesystem::path(L"\\\\?\\" + Native);
        }
    }
#endif
    return NativePath;
}

FString Detail::FromNativePath(const std::filesystem::path& Path)
{
#if SG_PLATFORM_WINDOWS
    const std::wstring Native = Path.native();
    std::filesystem::path PortablePath = Path;
    if (Native.starts_with(L"\\\\?\\UNC\\"))
        PortablePath = std::filesystem::path(L"\\\\" + Native.substr(8));
    else if (Native.starts_with(L"\\\\?\\"))
        PortablePath = std::filesystem::path(Native.substr(4));
    const std::u8string Utf8 = PortablePath.generic_u8string();
#else
    const std::u8string Utf8 = Path.generic_u8string();
#endif
    std::string Text;
    Text.reserve(Utf8.size());
    for (const char8_t Byte : Utf8)
    {
        Text.push_back(static_cast<char>(Byte));
    }
    return FString(std::move(Text));
}

FPlatformFileStatus Detail::MakeFileStatus(
    EPlatformFileResult Result,
    int64 NativeError,
    const char* Context)
{
    FPlatformFileStatus Status;
    Status.Result = Result;
    Status.NativeError = NativeError;
    Status.Context = FString(Context);
    return Status;
}

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
    return std::filesystem::exists(Detail::ToNativePath(Path), Error) && !Error;
}

bool FPlatformFileSystem::CreateDirectory(const FString& Path)
{
    std::error_code Error;
    const std::filesystem::path Directory = Detail::ToNativePath(Path);
    if (std::filesystem::exists(Directory, Error))
    {
        return !Error && std::filesystem::is_directory(Directory, Error) && !Error;
    }

    std::filesystem::create_directories(Directory, Error);
    return !Error && std::filesystem::is_directory(Directory, Error) && !Error;
}

FPlatformFileStatus FPlatformFileSystem::CanonicalizeExistingPath(
    const FString& Path,
    FString& OutCanonicalPath)
{
    OutCanonicalPath = {};
    if (Path.IsEmpty())
        return Detail::MakeFileStatus(
            EPlatformFileResult::InvalidArgument, 0, "canonicalize:arguments");
    std::error_code Error;
    auto Canonical = CanonicalExisting(Detail::ToNativePath(Path), Error);
    if (Error)
        return Detail::MakeFileStatus(
            ClassifyError(Error), Error.value(), "canonicalize:path");
    std::string Text = Canonical.generic_string();
#if SG_PLATFORM_WINDOWS
    std::transform(Text.begin(), Text.end(), Text.begin(),
        [](unsigned char Value)
        {
            return static_cast<char>(std::tolower(Value));
        });
#endif
    OutCanonicalPath = FString(std::move(Text));
    return {};
}

bool FPlatformFileSystem::ReadFile(const FString& Path, TArray<uint8>& OutData)
{
    OutData.clear();

    const std::filesystem::path FilePath = Detail::ToNativePath(Path);
#if SG_PLATFORM_WINDOWS
    return Detail::PlatformReadFile(FilePath, OutData);
#else
    std::error_code Error;
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
#endif
}

FPlatformFileStatus FPlatformFileSystem::ReadRegularFileBounded(
    const FString& Path,
    uint64 MaxBytes,
    TArray<uint8>& OutData)
{
    OutData.clear();
    if (Path.IsEmpty() || MaxBytes == 0)
        return Detail::MakeFileStatus(
            EPlatformFileResult::InvalidArgument, 0,
            "read-regular-file:arguments");
    return Detail::PlatformReadRegularFileBounded(
        Detail::ToNativePath(Path), MaxBytes, OutData);
}

bool FPlatformFileSystem::WriteFile(const FString& Path, const TArray<uint8>& Data)
{
    std::ofstream File(Detail::ToNativePath(Path), std::ios::binary | std::ios::trunc);
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

FPlatformFileStatus FPlatformFileSystem::QueryRegularFile(
    const FString& Path,
    uint64 MaxBytes,
    FPlatformFileInfo& OutInfo)
{
    OutInfo = {};
    if (Path.IsEmpty())
    {
        return Detail::MakeFileStatus(
            EPlatformFileResult::InvalidArgument, 0, "query-regular-file:path");
    }

    const std::filesystem::path NativePath = Detail::ToNativePath(Path);
#if SG_PLATFORM_WINDOWS
    return Detail::PlatformQueryRegularFile(NativePath, MaxBytes, OutInfo);
#else
    std::error_code Error;
    const auto Symlink = std::filesystem::symlink_status(NativePath, Error);
    if (Error)
    {
        return Detail::MakeFileStatus(
            ClassifyError(Error), Error.value(), "query-regular-file:status");
    }
    if (!std::filesystem::is_regular_file(Symlink))
    {
        return Detail::MakeFileStatus(
            EPlatformFileResult::NotRegularFile, 0, "query-regular-file:type");
    }

    const auto Size = std::filesystem::file_size(NativePath, Error);
    if (Error)
    {
        return Detail::MakeFileStatus(
            ClassifyError(Error), Error.value(), "query-regular-file:size");
    }
    if (Size > MaxBytes)
    {
        return Detail::MakeFileStatus(
            EPlatformFileResult::LimitExceeded, 0, "query-regular-file:bytes");
    }

    const std::filesystem::path Canonical =
        CanonicalExisting(NativePath, Error);
    if (Error)
    {
        return Detail::MakeFileStatus(
            ClassifyError(Error), Error.value(), "query-regular-file:canonical");
    }
    OutInfo.Path = Detail::FromNativePath(Canonical);
    OutInfo.ByteSize = static_cast<uint64>(Size);
    return {};
#endif
}

FPlatformFileStatus FPlatformFileSystem::EnumerateRegularFiles(
    const FString& Root,
    const FPlatformFileEnumerationOptions& Options,
    TArray<FPlatformFileInfo>& OutFiles)
{
    OutFiles.clear();
    if (Root.IsEmpty() || Options.MaxFiles == 0 || Options.MaxDepth == 0 ||
        Options.MaxPathBytes == 0)
    {
        return Detail::MakeFileStatus(
            EPlatformFileResult::InvalidArgument, 0, "enumerate:options");
    }

    std::error_code Error;
    const std::filesystem::path CanonicalRoot =
        CanonicalExisting(Detail::ToNativePath(Root), Error);
    if (Error || !std::filesystem::is_directory(CanonicalRoot, Error))
    {
        const EPlatformFileResult Result = Error
            ? ClassifyError(Error)
            : EPlatformFileResult::NotFound;
        return Detail::MakeFileStatus(Result, Error.value(), "enumerate:root");
    }

    std::filesystem::recursive_directory_iterator Iterator(CanonicalRoot, Error);
    const std::filesystem::recursive_directory_iterator End;
    while (!Error && Iterator != End)
    {
        const auto& Entry = *Iterator;
        const usize Depth = static_cast<usize>(Iterator.depth()) + 1;
        const auto LinkStatus = Entry.symlink_status(Error);
        if (Error)
        {
            break;
        }
        if (std::filesystem::is_symlink(LinkStatus))
        {
            if (std::filesystem::is_directory(Entry.status(Error)))
            {
                Iterator.disable_recursion_pending();
            }
            if (Error)
            {
                break;
            }
            Iterator.increment(Error);
            continue;
        }
        if (Depth > Options.MaxDepth)
        {
            OutFiles.clear();
            return Detail::MakeFileStatus(
                EPlatformFileResult::LimitExceeded, 0, "enumerate:depth");
        }
        if (std::filesystem::is_regular_file(LinkStatus))
        {
            if (OutFiles.size() >= Options.MaxFiles)
            {
                OutFiles.clear();
                return Detail::MakeFileStatus(
                    EPlatformFileResult::LimitExceeded, 0, "enumerate:files");
            }
            const std::filesystem::path CanonicalPath =
                CanonicalExisting(Entry.path(), Error);
            if (Error)
            {
                break;
            }
            const FString Normalized = Detail::FromNativePath(CanonicalPath);
            if (Normalized.Len() > Options.MaxPathBytes)
            {
                OutFiles.clear();
                return Detail::MakeFileStatus(
                    EPlatformFileResult::LimitExceeded, 0, "enumerate:path");
            }
            const auto Size = Entry.file_size(Error);
            if (Error)
            {
                break;
            }
            OutFiles.push_back({Normalized, static_cast<uint64>(Size)});
        }
        Iterator.increment(Error);
    }
    if (Error)
    {
        OutFiles.clear();
        return Detail::MakeFileStatus(
            ClassifyError(Error), Error.value(), "enumerate:iteration");
    }

    std::sort(OutFiles.begin(), OutFiles.end(),
        [](const FPlatformFileInfo& Left, const FPlatformFileInfo& Right)
        {
            return Left.Path < Right.Path;
        });
    return {};
}

FPlatformFileStatus FPlatformFileSystem::CheckContainedPath(
    const FString& Root,
    const FString& Candidate,
    bool& OutContained)
{
    OutContained = false;
    if (Root.IsEmpty() || Candidate.IsEmpty())
    {
        return Detail::MakeFileStatus(
            EPlatformFileResult::InvalidArgument, 0, "containment:path");
    }
    std::error_code Error;
    const auto CanonicalRoot =
        CanonicalExisting(Detail::ToNativePath(Root), Error);
    if (Error)
    {
        return Detail::MakeFileStatus(
            ClassifyError(Error), Error.value(), "containment:root");
    }
    const auto CanonicalCandidate =
        CanonicalExisting(Detail::ToNativePath(Candidate), Error);
    if (Error)
    {
        return Detail::MakeFileStatus(
            ClassifyError(Error), Error.value(), "containment:candidate");
    }
    OutContained = IsContainedCanonical(CanonicalRoot, CanonicalCandidate);
    return {};
}

FPlatformFileStatus FPlatformFileSystem::MoveDirectoryNoReplace(
    const FString& Source,
    const FString& Destination)
{
    if (Source.IsEmpty() || Destination.IsEmpty())
    {
        return Detail::MakeFileStatus(
            EPlatformFileResult::InvalidArgument, 0, "move-directory:path");
    }
    return Detail::PlatformMoveDirectoryNoReplace(
        Detail::ToNativePath(Source), Detail::ToNativePath(Destination));
}

FPlatformFileStatus FPlatformFileSystem::ReplaceFileAtomic(
    const FString& Source,
    const FString& Destination)
{
    if (Source.IsEmpty() || Destination.IsEmpty())
    {
        return Detail::MakeFileStatus(
            EPlatformFileResult::InvalidArgument, 0, "replace-file:path");
    }
    return Detail::PlatformReplaceFileAtomic(
        Detail::ToNativePath(Source), Detail::ToNativePath(Destination));
}

FPlatformFileStatus FPlatformFileSystem::WriteFileDurable(
    const FString& Path,
    const TArray<uint8>& Data)
{
    if (Path.IsEmpty())
    {
        return Detail::MakeFileStatus(
            EPlatformFileResult::InvalidArgument, 0, "durable-write:path");
    }
    return Detail::PlatformWriteFileDurable(Detail::ToNativePath(Path), Data);
}

FPlatformFileStatus FPlatformFileSystem::RemoveTreeContained(
    const FString& AllowedRoot,
    const FString& Candidate,
    usize MaxEntries)
{
    if (AllowedRoot.IsEmpty() || Candidate.IsEmpty() || MaxEntries == 0)
    {
        return Detail::MakeFileStatus(
            EPlatformFileResult::InvalidArgument, 0, "remove-tree:arguments");
    }
    bool bContained = false;
    FPlatformFileStatus Containment =
        CheckContainedPath(AllowedRoot, Candidate, bContained);
    if (!Containment.IsSuccess())
    {
        return Containment;
    }

    std::error_code Error;
    const auto Root =
        CanonicalExisting(Detail::ToNativePath(AllowedRoot), Error);
    const auto Tree =
        CanonicalExisting(Detail::ToNativePath(Candidate), Error);
    if (Error)
    {
        return Detail::MakeFileStatus(
            ClassifyError(Error), Error.value(), "remove-tree:canonical");
    }
    if (!bContained || Tree == Root)
    {
        return Detail::MakeFileStatus(
            EPlatformFileResult::OutsideRoot, 0, "remove-tree:containment");
    }
    if (!std::filesystem::exists(Tree, Error))
    {
        return Detail::MakeFileStatus(
            EPlatformFileResult::NotFound, Error.value(), "remove-tree:not-found");
    }

    usize EntryCount = 0;
    std::filesystem::recursive_directory_iterator Iterator(Tree, Error);
    const std::filesystem::recursive_directory_iterator End;
    while (!Error && Iterator != End)
    {
        if (++EntryCount > MaxEntries)
        {
            return Detail::MakeFileStatus(
                EPlatformFileResult::LimitExceeded, 0, "remove-tree:entries");
        }
        if (Iterator->is_symlink(Error))
        {
            Iterator.disable_recursion_pending();
        }
        Iterator.increment(Error);
    }
    if (Error)
    {
        return Detail::MakeFileStatus(
            ClassifyError(Error), Error.value(), "remove-tree:enumerate");
    }

    std::filesystem::remove_all(Tree, Error);
    if (Error)
    {
        return Detail::MakeFileStatus(
            ClassifyError(Error), Error.value(), "remove-tree:remove");
    }
    return {};
}

} // namespace Stoner::Core
