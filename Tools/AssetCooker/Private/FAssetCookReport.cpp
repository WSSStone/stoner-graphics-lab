#include "AssetCooker/FAssetCookReport.h"

namespace Stoner::AssetCooker
{

Asset::EAssetResult FAssetCookReport::Validate() const noexcept
{
    if (!EffectiveProfileDigest.IsAvailable() ||
        Counts.ReachableAssets != Assets.size() ||
        Counts.ReuseEligible + Counts.ReuseIneligible != Counts.ReachableAssets ||
        Counts.Cooked + Counts.Reused + Counts.Failed > Counts.ReachableAssets ||
        Counts.Reused > Counts.ReuseEligible ||
        Counts.CacheMisses + Counts.Reused > Counts.ReuseEligible ||
        Counts.Invalidated > Counts.CacheMisses ||
        Counts.Quarantined > Counts.Invalidated ||
        Counts.Rebuilt > Counts.Invalidated)
        return Asset::EAssetResult::InvalidInput;
    for (Core::usize Index = 0; Index < Assets.size(); ++Index)
        if (Assets[Index].PlanIndex != Index || !Assets[Index].AssetId.IsValid() ||
            (Assets[Index].Decision != EAssetCookDecision::Planned &&
             (!Assets[Index].EffectiveSettingsDigest.IsAvailable() ||
              !Assets[Index].RelevantProfileDigest.IsAvailable() ||
              Assets[Index].TargetDecision.IsEmpty())))
            return Asset::EAssetResult::InvalidInput;
    return Asset::EAssetResult::Success;
}

} // namespace Stoner::AssetCooker
