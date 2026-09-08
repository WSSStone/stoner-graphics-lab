#include "FProductionContentSession.h"
#include <map>
#include <set>

namespace Stoner::Demo
{
Asset::EAssetResult BuildProductionSourceIdentity(
    const Asset::FAssetCookManifest& Manifest, const Asset::FAssetId& Root,
    Asset::FAssetDigest& OutDigest)
{
    using namespace Asset;
    constexpr Core::usize MaximumEntries = 100000;
    constexpr Core::usize MaximumBytes = 64 * 1024 * 1024;
    if (!Root.IsValid() || Manifest.Records.empty()) return EAssetResult::InvalidInput;
    if (Manifest.Records.size() > MaximumEntries) return EAssetResult::CapacityExceeded;
    std::map<FAssetId, const FAssetCookManifestRecord*> Records;
    for (const auto& Record : Manifest.Records)
        if (!Record.AssetId.IsValid() || !Records.emplace(Record.AssetId, &Record).second)
            return EAssetResult::InvalidIdentity;
    std::map<FAssetId, FAssetDigest> Sources;
    const auto Add = [&](const FAssetId& Id, const FAssetDigest& Version) {
        if (!Id.IsValid() || !Version.IsAvailable()) return EAssetResult::InvalidIdentity;
        const auto [It, Inserted] = Sources.emplace(Id, Version);
        if (!Inserted && It->second != Version) return EAssetResult::SourceChanged;
        return Sources.size() > MaximumEntries ? EAssetResult::CapacityExceeded : EAssetResult::Success;
    };
    std::set<FAssetId> Visited{Root};
    Core::TArray<FAssetId> Pending{Root};
    while (!Pending.empty())
    {
        const auto Id = Pending.back(); Pending.pop_back();
        const auto It = Records.find(Id);
        if (It == Records.end()) return EAssetResult::UnresolvedDependency;
        const auto& Record = *It->second;
        auto Result = Add(Record.AssetId, Record.SourceVersion);
        if (Result != EAssetResult::Success) return Result;
        if (Record.SourceManifest.size() > MaximumEntries || Record.Dependencies.size() > MaximumEntries)
            return EAssetResult::CapacityExceeded;
        for (const auto& Source : Record.SourceManifest)
        {
            Result = Add(Source.AssetId, Source.Version);
            if (Result != EAssetResult::Success) return Result;
        }
        for (const auto& Dependency : Record.Dependencies)
        {
            if (!Dependency.AssetId.IsValid()) return EAssetResult::InvalidIdentity;
            if (Visited.insert(Dependency.AssetId).second) Pending.push_back(Dependency.AssetId);
            if (Visited.size() > MaximumEntries) return EAssetResult::CapacityExceeded;
        }
    }
    // Versioned, length-prefixed UTF-8 identities and fixed-width hex digests
    // avoid delimiter ambiguity without depending on a JSON writer or target.
    std::string Canonical = "stoner.production-source-closure.v1\n";
    for (const auto& [Id, Version] : Sources)
    {
        const auto Text = Id.ToString().ToStdString();
        if (Text.size() > 4096) return EAssetResult::IdentityTooLong;
        if (Canonical.size() + Text.size() + 96 > MaximumBytes) return EAssetResult::CapacityExceeded;
        Canonical += std::to_string(Text.size()) + ":" + Text + ":" + Version.ToLowerHex().ToStdString() + "\n";
    }
    OutDigest = FAssetDigest::FromBytes(std::span<const Core::uint8>(
        reinterpret_cast<const Core::uint8*>(Canonical.data()), Canonical.size()));
    return EAssetResult::Success;
}
}
