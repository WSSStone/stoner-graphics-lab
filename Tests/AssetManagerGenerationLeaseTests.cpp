#include "AssetManagerGenerationLeaseTests.h"

#include "Asset/FGenerationReaderLease.h"

#include <chrono>
#include <filesystem>
#include <iostream>

namespace
{
using namespace Stoner;
using namespace Stoner::Asset;

void Record(
    FAssetManagerGenerationLeaseTestResult& Result,
    bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}
} // namespace

FAssetManagerGenerationLeaseTestResult RunAssetManagerGenerationLeaseTests()
{
    FAssetManagerGenerationLeaseTestResult Result;
    const auto Token = std::chrono::steady_clock::now()
        .time_since_epoch().count();
    const auto Root = std::filesystem::temp_directory_path() /
        ("sg-generation-lease-contract-" + std::to_string(Token));
    const auto Publication = Root / "Published";
    const auto OtherPublication = Root / "OtherPublished";
    const auto Coordination = Root / "Coordination";
    std::error_code Error;
    std::filesystem::create_directories(Publication, Error);
    std::filesystem::create_directories(OtherPublication, Error);
    std::filesystem::create_directories(Coordination, Error);

    FAssetDigest First;
    FAssetDigest Alias;
    FAssetDigest Other;
    const auto AliasPath = Publication / ".";
    const bool Derived =
        FGenerationReaderLease::DerivePublicationNamespace(
            Core::FString(Publication.generic_string()), First) ==
            EAssetResult::Success &&
        FGenerationReaderLease::DerivePublicationNamespace(
            Core::FString(AliasPath.generic_string()), Alias) ==
            EAssetResult::Success &&
        FGenerationReaderLease::DerivePublicationNamespace(
            Core::FString(OtherPublication.generic_string()), Other) ==
            EAssetResult::Success;
    Record(Result,
        Derived && First == Alias && First != Other &&
            First.ToLowerHex().Len() == 64,
        "canonical publication aliases share one collision-resistant namespace");

    const FAssetDigest Generation = FAssetDigest::FromBytes(
        Core::TArray<Core::uint8>{2, 6});
    FGenerationReaderLease Reader;
    Record(Result,
        FGenerationReaderLease::Acquire(
            Core::FString(Publication.generic_string()),
            Core::FString(Coordination.generic_string()), Generation,
            1000, Reader) == EAssetResult::Success && Reader.IsHeld() &&
            Reader.GetPublicationNamespace() == First,
        "reader lease is scoped by canonical publication and generation");
    Reader.Release();
    Reader.Release();
    Record(Result, !Reader.IsHeld(), "generation reader release is idempotent");

    FGenerationReaderLease Missing;
    const auto MissingResult = FGenerationReaderLease::Acquire(
            Core::FString(Publication.generic_string()),
            Core::FString((Root / "Missing").generic_string()), Generation,
            10, Missing);
    Record(Result,
        MissingResult != EAssetResult::Success && !Missing.IsHeld(),
        "missing coordination root fails without partial reader ownership");
#if !defined(_WIN32)
    const auto ReadOnlyCoordination = Root / "ReadOnlyCoordination";
    std::filesystem::create_directories(ReadOnlyCoordination, Error);
    std::filesystem::permissions(ReadOnlyCoordination,
        std::filesystem::perms::owner_read |
            std::filesystem::perms::owner_exec,
        std::filesystem::perm_options::replace, Error);
    FGenerationReaderLease ReadOnly;
    const auto ReadOnlyResult = FGenerationReaderLease::Acquire(
        Core::FString(Publication.generic_string()),
        Core::FString(ReadOnlyCoordination.generic_string()), Generation,
        10, ReadOnly);
    Record(Result,
        ReadOnlyResult != EAssetResult::Success && !ReadOnly.IsHeld(),
        "read-only coordination root fails without modifying publication");
    std::filesystem::permissions(ReadOnlyCoordination,
        std::filesystem::perms::owner_all,
        std::filesystem::perm_options::replace, Error);
#endif
    std::filesystem::remove_all(Root, Error);
    return Result;
}
