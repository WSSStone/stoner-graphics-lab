#include "FCookInputSnapshot.h"

#include <algorithm>
#include <limits>
#include <string>

namespace Stoner::AssetCooker::Private
{
namespace
{

Asset::FAssetDigest SnapshotDigest(const Core::TArray<FCookInputRecord>& Records)
{
    std::string Evidence = "stoner.cook-input-snapshot.v1\n";
    for (const auto& Record : Records)
    {
        const auto Append = [&Evidence](std::string_view Value)
        {
            Evidence += std::to_string(Value.size()) + ":";
            Evidence.append(Value);
            Evidence.push_back('\n');
        };
        Append(Record.Locator.ToString().View());
        Append(Record.Role.View());
        Append(std::to_string(Record.ByteSize));
        Append(Record.Digest.ToLowerHex().View());
    }
    return Asset::FAssetDigest::FromBytes(std::span<const Core::uint8>(
        reinterpret_cast<const Core::uint8*>(Evidence.data()), Evidence.size()));
}

} // namespace

Asset::EAssetResult FCookInputSnapshotBuilder::Pin(
    const Core::TArray<FCookInputSource>& Sources,
    Core::uint64 MaxSingleBytes,
    Core::uint64 MaxAggregateBytes,
    FCookInputSnapshot& OutSnapshot)
{
    OutSnapshot = {};
    if (Sources.empty() || MaxSingleBytes == 0 || MaxAggregateBytes == 0)
        return Asset::EAssetResult::InvalidInput;
    Core::TArray<FCookInputRecord> Records;
    Records.reserve(Sources.size());
    Core::uint64 Aggregate = 0;
    for (const auto& Source : Sources)
    {
        if (!Source.Descriptor.Location.IsValid() || !Source.Source.IsValid() ||
            Source.Role.IsEmpty())
            return Asset::EAssetResult::InvalidInput;
        Core::TArray<Core::uint8> Bytes;
        const Asset::EAssetResult Read = Source.Source.ReadBounded(
            MaxSingleBytes, Source.Descriptor.Size, Bytes);
        if (Read != Asset::EAssetResult::Success) return Read;
        if (Bytes.size() > MaxAggregateBytes - Aggregate)
            return Asset::EAssetResult::CapacityExceeded;
        Aggregate += Bytes.size();
        Records.push_back({
            Source.Descriptor.Location,
            Source.Role,
            Bytes.size(),
            Asset::FAssetDigest::FromBytes(Bytes),
            Core::MakeShared<const Core::TArray<Core::uint8>>(std::move(Bytes))});
    }
    std::sort(Records.begin(), Records.end(), [](const auto& Left, const auto& Right)
    {
        if (Left.Locator == Right.Locator) return Left.Role < Right.Role;
        return Left.Locator < Right.Locator;
    });
    for (Core::usize Index = 1; Index < Records.size(); ++Index)
        if (Records[Index - 1].Locator == Records[Index].Locator &&
            Records[Index - 1].Role == Records[Index].Role)
            return Asset::EAssetResult::Conflict;
    OutSnapshot.Records = std::move(Records);
    OutSnapshot.AggregateBytes = Aggregate;
    OutSnapshot.SnapshotDigest = SnapshotDigest(OutSnapshot.Records);
    return Asset::EAssetResult::Success;
}

Asset::EAssetResult FCookInputSnapshotBuilder::Revalidate(
    const FCookInputSnapshot& Snapshot,
    Core::uint64 MaxSingleBytes,
    const FCookInputResolve& Resolve)
{
    if (Snapshot.Records.empty() || !Snapshot.SnapshotDigest.IsAvailable() ||
        MaxSingleBytes == 0 || !Resolve)
        return Asset::EAssetResult::InvalidInput;
    for (const auto& Expected : Snapshot.Records)
    {
        Asset::FAssetResolveResult Resolved = Resolve(Expected.Locator);
        if (Resolved.Result != Asset::EAssetResult::Success ||
            !Resolved.Source.IsValid())
            return Asset::EAssetResult::TransientFailure;
        Core::TArray<Core::uint8> Bytes;
        const Asset::EAssetResult Read = Resolved.Source.ReadBounded(
            MaxSingleBytes, Resolved.Descriptor.Size, Bytes);
        if (Read != Asset::EAssetResult::Success ||
            Bytes.size() != Expected.ByteSize ||
            Asset::FAssetDigest::FromBytes(Bytes) != Expected.Digest)
            return Asset::EAssetResult::TransientFailure;
    }
    return Asset::EAssetResult::Success;
}

} // namespace Stoner::AssetCooker::Private
