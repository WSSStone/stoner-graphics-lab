#include "AssetCookerInputSnapshotTests.h"

#include "AssetCookerTestSupport.h"
#include "FCookInputSnapshot.h"

#include <iostream>
#include <map>

namespace
{
using namespace Stoner;
using namespace Stoner::Asset;
using namespace Stoner::AssetCooker::Private;
using namespace Stoner::Tests::AssetCooker;

void Record(
    FAssetCookerInputSnapshotTestResult& Result,
    bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

FCookInputSource Source(const char* Path, Core::TArray<Core::uint8> Bytes)
{
    FCookInputSource Value;
    Value.Descriptor = Descriptor(Path, Bytes.size());
    Value.Source = FAssetSourceLease(Core::MakeShared<FMemorySource>(std::move(Bytes)));
    Value.Role = Core::FString("primary");
    return Value;
}

FAssetResolveResult Resolve(
    const FAssetSourceLocator& Locator,
    Core::TArray<Core::uint8> Bytes)
{
    FAssetResolveResult Result;
    Result.Result = EAssetResult::Success;
    Result.Descriptor.Location = Locator;
    Result.Descriptor.Size = Bytes.size();
    Result.Source = FAssetSourceLease(Core::MakeShared<FMemorySource>(std::move(Bytes)));
    return Result;
}

void TestPinAndRevalidate(FAssetCookerInputSnapshotTestResult& Result)
{
    FCookInputSnapshot First;
    const EAssetResult Pinned = FCookInputSnapshotBuilder::Pin(
        {Source("Z.bin", {3, 4}), Source("A.bin", {1, 2})}, 16, 32, First);
    FCookInputSnapshot Second;
    (void)FCookInputSnapshotBuilder::Pin(
        {Source("A.bin", {1, 2}), Source("Z.bin", {3, 4})}, 16, 32, Second);
    Record(Result,
        Pinned == EAssetResult::Success && First.Records.size() == 2 &&
            First.SnapshotDigest == Second.SnapshotDigest &&
            First.Records[0].Locator < First.Records[1].Locator,
        "input snapshot pins complete bytes in canonical locator order");

    const EAssetResult Stable = FCookInputSnapshotBuilder::Revalidate(
        First, 16, [](const FAssetSourceLocator& Locator)
        {
            return Resolve(
                Locator,
                Locator.GetLocator() == Core::FString("A.bin")
                    ? Core::TArray<Core::uint8>{1, 2}
                    : Core::TArray<Core::uint8>{3, 4});
        });
    const EAssetResult Changed = FCookInputSnapshotBuilder::Revalidate(
        First, 16, [](const FAssetSourceLocator& Locator)
        {
            return Resolve(Locator, {9, 9});
        });
    Record(Result, Stable == EAssetResult::Success,
        "snapshot re-resolution accepts the exact pinned source state");
    Record(Result, Changed == EAssetResult::TransientFailure,
        "snapshot re-resolution reports source change without retry");
}

void TestLimits(FAssetCookerInputSnapshotTestResult& Result)
{
    FCookInputSnapshot Snapshot;
    Record(Result,
        FCookInputSnapshotBuilder::Pin(
            {Source("large.bin", {1, 2, 3})}, 2, 16, Snapshot) !=
                EAssetResult::Success && Snapshot.Records.empty(),
        "snapshot byte limit fails without partial pinned records");
}
} // namespace

FAssetCookerInputSnapshotTestResult RunAssetCookerInputSnapshotTests()
{
    FAssetCookerInputSnapshotTestResult Result;
    TestPinAndRevalidate(Result);
    TestLimits(Result);
    return Result;
}
