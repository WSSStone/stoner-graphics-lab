#pragma once

#include "Core/FPlatformFileSystem.h"
#include "Core/FString.h"
#include "Core/TUniquePtr.h"

namespace Stoner::Core
{

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

    [[nodiscard]] bool IsHeld() const noexcept;
    [[nodiscard]] const FString& GetPath() const noexcept;
    void Release() noexcept;

private:
    struct FImpl;
    TUniquePtr<FImpl> Impl;
};

} // namespace Stoner::Core
