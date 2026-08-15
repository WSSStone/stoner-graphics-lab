#pragma once

#include "Asset/EAssetResult.h"
#include "Asset/FAssetDigest.h"
#include "Core/FPlatformFileLease.h"
#include "Core/FString.h"

namespace Stoner::Asset
{

class FGenerationReaderLease
{
public:
    FGenerationReaderLease() = default;
    ~FGenerationReaderLease() = default;
    FGenerationReaderLease(const FGenerationReaderLease&) = delete;
    FGenerationReaderLease& operator=(const FGenerationReaderLease&) = delete;
    FGenerationReaderLease(FGenerationReaderLease&&) noexcept = default;
    FGenerationReaderLease& operator=(FGenerationReaderLease&&) noexcept = default;

    [[nodiscard]] static EAssetResult Acquire(
        const Core::FString& PublicationRoot,
        const Core::FString& CoordinationRoot,
        const FAssetDigest& GenerationId,
        Core::uint64 TimeoutMilliseconds,
        FGenerationReaderLease& OutLease);
    [[nodiscard]] static EAssetResult DerivePublicationNamespace(
        const Core::FString& PublicationRoot,
        FAssetDigest& OutDigest);

    [[nodiscard]] bool IsHeld() const noexcept { return Lease_.IsHeld(); }
    [[nodiscard]] const FAssetDigest& GetPublicationNamespace() const noexcept
    {
        return PublicationNamespace_;
    }
    [[nodiscard]] const FAssetDigest& GetGenerationId() const noexcept
    {
        return GenerationId_;
    }
    void Release() noexcept;

private:
    FAssetDigest PublicationNamespace_;
    FAssetDigest GenerationId_;
    Core::FPlatformFileLease Lease_;
};

} // namespace Stoner::Asset
