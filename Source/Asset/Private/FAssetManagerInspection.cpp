#include "FAssetManagerInspection.h"

#include <algorithm>

namespace Stoner::Asset::Private
{

void NormalizeAssetManagerInspection(
    FAssetManagerInspection& Inspection,
    Core::uint32 MaximumRecords)
{
    std::sort(Inspection.Requests.begin(), Inspection.Requests.end(),
        [](const auto& Left, const auto& Right)
        {
            const auto& A = Left.Request.Handle;
            const auto& B = Right.Request.Handle;
            if (A.GetManagerLifetime() != B.GetManagerLifetime())
                return A.GetManagerLifetime() < B.GetManagerLifetime();
            if (A.GetSlot() != B.GetSlot()) return A.GetSlot() < B.GetSlot();
            return A.GetGeneration() < B.GetGeneration();
        });
    std::sort(Inspection.Operations.begin(), Inspection.Operations.end(),
        [](const auto& Left, const auto& Right)
        {
            if (Left.AssetId != Right.AssetId)
                return Left.AssetId < Right.AssetId;
            return Left.ExpectedType < Right.ExpectedType;
        });
    std::sort(Inspection.Cache.begin(), Inspection.Cache.end(),
        [](const auto& Left, const auto& Right)
        {
            if (Left.AssetId != Right.AssetId)
                return Left.AssetId < Right.AssetId;
            return Left.ExpectedType < Right.ExpectedType;
        });

    Core::uint64 Remaining = MaximumRecords;
    const auto Truncate = [&Remaining, &Inspection](auto& Records)
    {
        if (Records.size() > Remaining)
        {
            Records.resize(static_cast<Core::usize>(Remaining));
            Inspection.bInspectionTruncated = true;
        }
        Remaining -= std::min<Core::uint64>(Remaining, Records.size());
    };
    Truncate(Inspection.Requests);
    Truncate(Inspection.Operations);
    Truncate(Inspection.Cache);
}

} // namespace Stoner::Asset::Private
