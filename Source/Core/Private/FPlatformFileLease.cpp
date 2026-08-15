#include "Core/FPlatformFileLease.h"

#include "FPlatformFileSystemInternal.h"
#include "Core/SGPlatform.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>

#if SG_PLATFORM_WINDOWS
#define NOMINMAX
#include <Windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace Stoner::Core
{

namespace
{

using FClock = std::chrono::steady_clock;

std::mutex GLeaseRegistryMutex;
std::unordered_map<std::string, std::weak_ptr<std::shared_timed_mutex>>
    GLeaseRegistry;

std::shared_ptr<std::shared_timed_mutex> GetProcessLeaseMutex(
    const FString& Path)
{
    std::lock_guard<std::mutex> Guard(GLeaseRegistryMutex);
    const std::string Key = Path.ToStdString();
    if (const auto Found = GLeaseRegistry.find(Key); Found != GLeaseRegistry.end())
    {
        if (auto Existing = Found->second.lock())
        {
            return Existing;
        }
    }
    auto Created = std::make_shared<std::shared_timed_mutex>();
    GLeaseRegistry[Key] = Created;
    return Created;
}

FPlatformFileStatus LeaseStatus(
    EPlatformFileResult Result,
    int64 NativeError,
    const char* Context)
{
    return Detail::MakeFileStatus(Result, NativeError, Context);
}

} // namespace

struct FPlatformFileLease::FImpl
{
    FString Path;
    EPlatformFileLeaseMode Mode = EPlatformFileLeaseMode::Exclusive;
    std::shared_ptr<std::shared_timed_mutex> ProcessMutex;
    std::unique_lock<std::shared_timed_mutex> ExclusiveProcessLock;
    std::shared_lock<std::shared_timed_mutex> SharedProcessLock;
#if SG_PLATFORM_WINDOWS
    HANDLE Handle = INVALID_HANDLE_VALUE;
    OVERLAPPED LockRange{};
#else
    int Descriptor = -1;
#endif
};

FPlatformFileLease::FPlatformFileLease() noexcept = default;
FPlatformFileLease::~FPlatformFileLease()
{
    Release();
}

FPlatformFileLease::FPlatformFileLease(FPlatformFileLease&& Other) noexcept = default;

FPlatformFileLease& FPlatformFileLease::operator=(
    FPlatformFileLease&& Other) noexcept
{
    if (this != &Other)
    {
        Release();
        Impl = std::move(Other.Impl);
    }
    return *this;
}

FPlatformFileStatus FPlatformFileLease::Acquire(
    const FString& LeasePath,
    uint64 TimeoutMilliseconds,
    const FString& OwnerMetadata,
    FPlatformFileLease& OutLease)
{
    return Acquire(
        LeasePath, EPlatformFileLeaseMode::Exclusive, TimeoutMilliseconds,
        OwnerMetadata, OutLease);
}

FPlatformFileStatus FPlatformFileLease::Acquire(
    const FString& LeasePath,
    EPlatformFileLeaseMode Mode,
    uint64 TimeoutMilliseconds,
    const FString& OwnerMetadata,
    FPlatformFileLease& OutLease)
{
    if (LeasePath.IsEmpty() || TimeoutMilliseconds > 600000 || OutLease.IsHeld())
    {
        return LeaseStatus(EPlatformFileResult::InvalidArgument, 0, "lease:arguments");
    }

    std::error_code Error;
    const auto NativePath = Detail::ToNativePath(LeasePath);
    const auto Parent = NativePath.parent_path();
    if (Parent.empty() || !std::filesystem::is_directory(Parent, Error) || Error)
    {
        return LeaseStatus(
            Error ? EPlatformFileResult::IoError : EPlatformFileResult::NotFound,
            Error.value(),
            "lease:parent");
    }
    const auto CanonicalPath = std::filesystem::weakly_canonical(NativePath, Error);
    if (Error)
    {
        return LeaseStatus(EPlatformFileResult::IoError, Error.value(), "lease:path");
    }
    const FString NormalizedPath = Detail::FromNativePath(CanonicalPath);

    const FClock::time_point Deadline =
        FClock::now() + std::chrono::milliseconds(TimeoutMilliseconds);
    auto Candidate = TUniquePtr<FImpl>(new FImpl());
    Candidate->Path = NormalizedPath;
    Candidate->Mode = Mode;
    Candidate->ProcessMutex = GetProcessLeaseMutex(NormalizedPath);
    bool ProcessAcquired = false;
    if (Mode == EPlatformFileLeaseMode::Shared)
    {
        Candidate->SharedProcessLock = std::shared_lock<std::shared_timed_mutex>(
            *Candidate->ProcessMutex, std::defer_lock);
        ProcessAcquired = Candidate->SharedProcessLock.try_lock_until(Deadline);
    }
    else
    {
        Candidate->ExclusiveProcessLock =
            std::unique_lock<std::shared_timed_mutex>(
                *Candidate->ProcessMutex, std::defer_lock);
        ProcessAcquired = Candidate->ExclusiveProcessLock.try_lock_until(Deadline);
    }
    if (!ProcessAcquired)
        return LeaseStatus(
            EPlatformFileResult::TimedOut, 0, "lease:process-timeout");

#if SG_PLATFORM_WINDOWS
    for (;;)
    {
        Candidate->Handle = ::CreateFileW(
            CanonicalPath.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (Candidate->Handle != INVALID_HANDLE_VALUE)
        {
            break;
        }
        const DWORD NativeError = GetLastError();
        if (NativeError != ERROR_SHARING_VIOLATION &&
            NativeError != ERROR_LOCK_VIOLATION)
        {
            return LeaseStatus(
                NativeError == ERROR_ACCESS_DENIED
                    ? EPlatformFileResult::PermissionDenied
                    : EPlatformFileResult::IoError,
                static_cast<int64>(NativeError),
                "lease:open");
        }
        if (FClock::now() >= Deadline)
        {
            return LeaseStatus(
                EPlatformFileResult::TimedOut,
                static_cast<int64>(NativeError),
                "lease:native-timeout");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    for (;;)
    {
        DWORD Flags = LOCKFILE_FAIL_IMMEDIATELY;
        if (Mode == EPlatformFileLeaseMode::Exclusive)
            Flags |= LOCKFILE_EXCLUSIVE_LOCK;
        if (::LockFileEx(
                Candidate->Handle, Flags, 0, 1, 0, &Candidate->LockRange))
            break;
        const DWORD NativeError = GetLastError();
        if (NativeError != ERROR_LOCK_VIOLATION &&
            NativeError != ERROR_SHARING_VIOLATION)
        {
            ::CloseHandle(Candidate->Handle);
            Candidate->Handle = INVALID_HANDLE_VALUE;
            return LeaseStatus(EPlatformFileResult::IoError,
                static_cast<int64>(NativeError), "lease:lock");
        }
        if (FClock::now() >= Deadline)
        {
            ::CloseHandle(Candidate->Handle);
            Candidate->Handle = INVALID_HANDLE_VALUE;
            return LeaseStatus(EPlatformFileResult::TimedOut,
                static_cast<int64>(NativeError), "lease:native-timeout");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (Mode == EPlatformFileLeaseMode::Exclusive &&
        (!::SetFilePointerEx(Candidate->Handle, {}, nullptr, FILE_BEGIN) ||
         !::SetEndOfFile(Candidate->Handle)))
    {
        const DWORD NativeError = GetLastError();
        ::CloseHandle(Candidate->Handle);
        Candidate->Handle = INVALID_HANDLE_VALUE;
        return LeaseStatus(
            EPlatformFileResult::IoError,
            static_cast<int64>(NativeError),
            "lease:metadata-truncate");
    }
    const std::string& Metadata = OwnerMetadata.ToStdString();
    DWORD Written = 0;
    if (Mode == EPlatformFileLeaseMode::Exclusive && !Metadata.empty() &&
        (!::WriteFile(
             Candidate->Handle,
             Metadata.data(),
             static_cast<DWORD>(Metadata.size()),
             &Written,
             nullptr) ||
         Written != Metadata.size()))
    {
        const DWORD NativeError = GetLastError();
        ::CloseHandle(Candidate->Handle);
        Candidate->Handle = INVALID_HANDLE_VALUE;
        return LeaseStatus(
            EPlatformFileResult::IoError,
            static_cast<int64>(NativeError),
            "lease:metadata-write");
    }
    if (Mode == EPlatformFileLeaseMode::Exclusive &&
        !::FlushFileBuffers(Candidate->Handle))
    {
        const DWORD NativeError = GetLastError();
        ::CloseHandle(Candidate->Handle);
        Candidate->Handle = INVALID_HANDLE_VALUE;
        return LeaseStatus(
            EPlatformFileResult::IoError,
            static_cast<int64>(NativeError),
            "lease:metadata-flush");
    }
#else
    Candidate->Descriptor = ::open(
        CanonicalPath.c_str(),
        O_RDWR | O_CREAT | O_CLOEXEC,
        0666);
    if (Candidate->Descriptor < 0)
    {
        return LeaseStatus(
            errno == EACCES ? EPlatformFileResult::PermissionDenied
                            : EPlatformFileResult::IoError,
            errno,
            "lease:open");
    }

    for (;;)
    {
        const int LockMode = Mode == EPlatformFileLeaseMode::Shared
            ? LOCK_SH
            : LOCK_EX;
        if (::flock(Candidate->Descriptor, LockMode | LOCK_NB) == 0)
        {
            break;
        }
        const int NativeError = errno;
        if (NativeError != EACCES && NativeError != EAGAIN)
        {
            ::close(Candidate->Descriptor);
            Candidate->Descriptor = -1;
            return LeaseStatus(
                EPlatformFileResult::IoError,
                NativeError,
                "lease:lock");
        }
        if (FClock::now() >= Deadline)
        {
            ::close(Candidate->Descriptor);
            Candidate->Descriptor = -1;
            return LeaseStatus(
                EPlatformFileResult::TimedOut,
                NativeError,
                "lease:native-timeout");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (Mode == EPlatformFileLeaseMode::Exclusive &&
        (::ftruncate(Candidate->Descriptor, 0) != 0 ||
         ::lseek(Candidate->Descriptor, 0, SEEK_SET) < 0))
    {
        const int NativeError = errno;
        ::close(Candidate->Descriptor);
        Candidate->Descriptor = -1;
        return LeaseStatus(
            EPlatformFileResult::IoError,
            NativeError,
            "lease:metadata-truncate");
    }
    const std::string& Metadata = OwnerMetadata.ToStdString();
    usize Offset = 0;
    while (Mode == EPlatformFileLeaseMode::Exclusive &&
           Offset < Metadata.size())
    {
        const ssize_t Written = ::write(
            Candidate->Descriptor,
            Metadata.data() + Offset,
            Metadata.size() - Offset);
        if (Written < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            const int NativeError = errno;
            ::close(Candidate->Descriptor);
            Candidate->Descriptor = -1;
            return LeaseStatus(
                EPlatformFileResult::IoError,
                NativeError,
                "lease:metadata-write");
        }
        Offset += static_cast<usize>(Written);
    }
    if (Mode == EPlatformFileLeaseMode::Exclusive &&
        ::fsync(Candidate->Descriptor) != 0)
    {
        const int NativeError = errno;
        ::close(Candidate->Descriptor);
        Candidate->Descriptor = -1;
        return LeaseStatus(
            EPlatformFileResult::IoError,
            NativeError,
            "lease:metadata-sync");
    }
#endif

    OutLease.Impl = std::move(Candidate);
    return {};
}

bool FPlatformFileLease::IsHeld() const noexcept
{
    if (!Impl)
    {
        return false;
    }
#if SG_PLATFORM_WINDOWS
    return Impl->Handle != INVALID_HANDLE_VALUE;
#else
    return Impl->Descriptor >= 0;
#endif
}

const FString& FPlatformFileLease::GetPath() const noexcept
{
    static const FString Empty;
    return Impl ? Impl->Path : Empty;
}

void FPlatformFileLease::Release() noexcept
{
    if (!Impl)
    {
        return;
    }
#if SG_PLATFORM_WINDOWS
    if (Impl->Handle != INVALID_HANDLE_VALUE)
    {
        (void)::UnlockFileEx(Impl->Handle, 0, 1, 0, &Impl->LockRange);
        ::CloseHandle(Impl->Handle);
        Impl->Handle = INVALID_HANDLE_VALUE;
    }
#else
    if (Impl->Descriptor >= 0)
    {
        (void)::flock(Impl->Descriptor, LOCK_UN);
        (void)::close(Impl->Descriptor);
        Impl->Descriptor = -1;
    }
#endif
    Impl.reset();
}

} // namespace Stoner::Core
