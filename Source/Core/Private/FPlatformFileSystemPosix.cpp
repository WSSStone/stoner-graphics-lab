#include "FPlatformFileSystemInternal.h"

#include "Core/SGPlatform.h"

#if SG_PLATFORM_MAC || SG_PLATFORM_LINUX

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

#if SG_PLATFORM_LINUX
#include <linux/fs.h>
#include <sys/syscall.h>
#endif

namespace Stoner::Core::Detail
{

namespace
{

EPlatformFileResult ClassifyErrno(int Error)
{
    switch (Error)
    {
    case 0: return EPlatformFileResult::Success;
    case ENOENT: return EPlatformFileResult::NotFound;
    case EEXIST: return EPlatformFileResult::AlreadyExists;
    case EACCES:
    case EPERM: return EPlatformFileResult::PermissionDenied;
    case EXDEV: return EPlatformFileResult::CrossVolume;
    case EBUSY: return EPlatformFileResult::Busy;
    default: return EPlatformFileResult::IoError;
    }
}

FPlatformFileStatus FromErrno(int Error, const char* Context)
{
    return MakeFileStatus(ClassifyErrno(Error), Error, Context);
}

FPlatformFileStatus SyncParentDirectory(
    const std::filesystem::path& Path,
    const char* Context)
{
    const std::filesystem::path Parent = Path.parent_path().empty()
        ? std::filesystem::path(".")
        : Path.parent_path();
    const int Descriptor = ::open(Parent.c_str(), O_RDONLY | O_CLOEXEC);
    if (Descriptor < 0)
    {
        return FromErrno(errno, Context);
    }
    const int SyncResult = ::fsync(Descriptor);
    const int SyncError = SyncResult == 0 ? 0 : errno;
    const int CloseResult = ::close(Descriptor);
    if (SyncError != 0)
    {
        return FromErrno(SyncError, Context);
    }
    if (CloseResult != 0)
    {
        return FromErrno(errno, Context);
    }
    return {};
}

} // namespace

FPlatformFileStatus PlatformMoveDirectoryNoReplace(
    const std::filesystem::path& Source,
    const std::filesystem::path& Destination)
{
    int Result = -1;
#if SG_PLATFORM_MAC
    Result = ::renamex_np(Source.c_str(), Destination.c_str(), RENAME_EXCL);
#elif SG_PLATFORM_LINUX
    Result = static_cast<int>(::syscall(
        SYS_renameat2,
        AT_FDCWD,
        Source.c_str(),
        AT_FDCWD,
        Destination.c_str(),
        RENAME_NOREPLACE));
#endif
    if (Result != 0)
    {
        return FromErrno(errno, "move-directory:no-replace");
    }
    return SyncParentDirectory(Destination, "move-directory:sync-parent");
}

FPlatformFileStatus PlatformReplaceFileAtomic(
    const std::filesystem::path& Source,
    const std::filesystem::path& Destination)
{
    if (::rename(Source.c_str(), Destination.c_str()) != 0)
    {
        return FromErrno(errno, "replace-file:rename");
    }
    return SyncParentDirectory(Destination, "replace-file:sync-parent");
}

FPlatformFileStatus PlatformWriteFileDurable(
    const std::filesystem::path& Path,
    const TArray<uint8>& Data)
{
    const int Descriptor = ::open(
        Path.c_str(),
        O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
        0666);
    if (Descriptor < 0)
    {
        return FromErrno(errno, "durable-write:open");
    }

    usize Offset = 0;
    while (Offset < Data.size())
    {
        const ssize_t Written = ::write(
            Descriptor,
            Data.data() + Offset,
            Data.size() - Offset);
        if (Written < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            const int Error = errno;
            ::close(Descriptor);
            return FromErrno(Error, "durable-write:write");
        }
        Offset += static_cast<usize>(Written);
    }

    if (::fsync(Descriptor) != 0)
    {
        const int Error = errno;
        ::close(Descriptor);
        return FromErrno(Error, "durable-write:sync-file");
    }
    if (::close(Descriptor) != 0)
    {
        return FromErrno(errno, "durable-write:close");
    }
    return SyncParentDirectory(Path, "durable-write:sync-parent");
}

} // namespace Stoner::Core::Detail

#endif
