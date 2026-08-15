#include "Asset/FGenerationReaderLease.h"

#include "Core/FPlatformFileSystem.h"
#include "Core/SGPlatform.h"

#include <filesystem>
#include <span>
#include <string>
#include <utility>

namespace Stoner::Asset
{
namespace
{
constexpr std::string_view NamespaceDomain =
    "stoner.asset-publication-namespace.v1";
}

EAssetResult FGenerationReaderLease::DerivePublicationNamespace(
    const Core::FString& PublicationRoot,
    FAssetDigest& OutDigest)
{
    OutDigest = {};
    Core::FString Canonical;
    if (!Core::FPlatformFileSystem::CanonicalizeExistingPath(
            PublicationRoot, Canonical).IsSuccess())
        return EAssetResult::InvalidInput;
    std::string Evidence(NamespaceDomain);
    Evidence += Canonical.ToStdString();
    OutDigest = FAssetDigest::FromBytes(std::span<const Core::uint8>(
        reinterpret_cast<const Core::uint8*>(Evidence.data()), Evidence.size()));
    return EAssetResult::Success;
}

EAssetResult FGenerationReaderLease::Acquire(
    const Core::FString& PublicationRoot,
    const Core::FString& CoordinationRoot,
    const FAssetDigest& GenerationId,
    Core::uint64 TimeoutMilliseconds,
    FGenerationReaderLease& OutLease)
{
    if (OutLease.IsHeld() || CoordinationRoot.IsEmpty() ||
        !GenerationId.IsAvailable())
        return EAssetResult::InvalidInput;
    std::error_code RootError;
    if (!std::filesystem::is_directory(
            std::filesystem::path(CoordinationRoot.ToStdString()), RootError) ||
        RootError)
        return EAssetResult::AccessDenied;
    Core::FString CanonicalCoordination;
    if (!Core::FPlatformFileSystem::CanonicalizeExistingPath(
            CoordinationRoot, CanonicalCoordination).IsSuccess())
        return EAssetResult::AccessDenied;
    FAssetDigest Namespace;
    const EAssetResult Derived =
        DerivePublicationNamespace(PublicationRoot, Namespace);
    if (Derived != EAssetResult::Success) return Derived;

    const auto NamespacePath = std::filesystem::path(
        CanonicalCoordination.ToStdString()) /
        Namespace.ToLowerHex().ToStdString();
    const Core::FString NamespaceString(NamespacePath.generic_string());
    if (!Core::FPlatformFileSystem::CreateDirectory(NamespaceString))
        return EAssetResult::AccessDenied;
    bool Contained = false;
    if (!Core::FPlatformFileSystem::CheckContainedPath(
            CanonicalCoordination, NamespaceString, Contained).IsSuccess() ||
        !Contained)
        return EAssetResult::AccessDenied;

    const auto LeasePath = NamespacePath /
        (GenerationId.ToLowerHex().ToStdString() + ".lease");
    Core::FPlatformFileLease Native;
    const auto Status = Core::FPlatformFileLease::Acquire(
        Core::FString(LeasePath.generic_string()),
        Core::EPlatformFileLeaseMode::Shared,
        TimeoutMilliseconds,
        Core::FString(),
        Native);
    if (!Status.IsSuccess())
        return Status.Result == Core::EPlatformFileResult::TimedOut
            ? EAssetResult::TransientFailure
            : EAssetResult::AccessDenied;
    OutLease.PublicationNamespace_ = Namespace;
    OutLease.GenerationId_ = GenerationId;
    OutLease.Lease_ = std::move(Native);
    return EAssetResult::Success;
}

void FGenerationReaderLease::Release() noexcept
{
    Lease_.Release();
    PublicationNamespace_ = {};
    GenerationId_ = {};
}

} // namespace Stoner::Asset
