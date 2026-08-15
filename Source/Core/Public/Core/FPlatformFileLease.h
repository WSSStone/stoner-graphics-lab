#pragma once

#include "Core/FPlatformFileSystem.h"
#include "Core/FString.h"
#include "Core/TUniquePtr.h"

namespace Stoner::Core
{

enum class EPlatformFileLeaseMode : uint8
{
    Shared,
    Exclusive
};

class FPlatformFileLease
{
public:
    FPlatformFileLease() noexcept;
    ~FPlatformFileLease();

    FPlatformFileLease(const FPlatformFileLease&) = delete;
    FPlatformFileLease& operator=(const FPlatformFileLease&) = delete;
    FPlatformFileLease(FPlatformFileLease&& Other) noexcept;
    FPlatformFileLease& operator=(FPlatformFileLease&& Other) noexcept;

    [[nodiscard]] static FPlatformFileStatus Acquire(
        const FString& LeasePath,
        uint64 TimeoutMilliseconds,
        const FString& OwnerMetadata,
        FPlatformFileLease& OutLease);
    [[nodiscard]] static FPlatformFileStatus Acquire(
        const FString& LeasePath,
        EPlatformFileLeaseMode Mode,
        uint64 TimeoutMilliseconds,
        const FString& OwnerMetadata,
        FPlatformFileLease& OutLease);

    [[nodiscard]] bool IsHeld() const noexcept;
    [[nodiscard]] const FString& GetPath() const noexcept;
    void Release() noexcept;

private:
    struct FImpl;
    TUniquePtr<FImpl> Impl;
};

} // namespace Stoner::Core
