#include "Core/FPlatformFileLease.h"

#include "FPlatformFileSystemInternal.h"
#include "Core/SGPlatform.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
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
std::unordered_map<std::string, std::weak_ptr<std::mutex>> GLeaseRegistry;

std::shared_ptr<std::mutex> GetProcessLeaseMutex(const FString& Path)
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
    auto Created = std::make_shared<std::mutex>();
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
    std::shared_ptr<std::mutex> ProcessMutex;
    std::unique_lock<std::mutex> ProcessLock;
#if SG_PLATFORM_WINDOWS
    HANDLE Handle = INVALID_HANDLE_VALUE;
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
    Candidate->ProcessMutex = GetProcessLeaseMutex(NormalizedPath);
    Candidate->ProcessLock =
        std::unique_lock<std::mutex>(*Candidate->ProcessMutex, std::defer_lock);
    while (!Candidate->ProcessLock.try_lock())
    {
        const auto Now = FClock::now();
        if (Now >= Deadline)
        {
            return LeaseStatus(
                EPlatformFileResult::TimedOut, 0, "lease:process-timeout");
        }
        std::this_thread::sleep_until(std::min(
            Deadline, Now + std::chrono::milliseconds(10)));
    }

#if SG_PLATFORM_WINDOWS
    for (;;)
    {
        Candidate->Handle = ::CreateFileW(
            CanonicalPath.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ,
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

    if (!::SetFilePointerEx(Candidate->Handle, {}, nullptr, FILE_BEGIN) ||
        !::SetEndOfFile(Candidate->Handle))
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
    if (!Metadata.empty() &&
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
    if (!::FlushFileBuffers(Candidate->Handle))
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
        if (::flock(Candidate->Descriptor, LOCK_EX | LOCK_NB) == 0)
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

    if (::ftruncate(Candidate->Descriptor, 0) != 0 ||
        ::lseek(Candidate->Descriptor, 0, SEEK_SET) < 0)
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
    while (Offset < Metadata.size())
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
    if (::fsync(Candidate->Descriptor) != 0)
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
