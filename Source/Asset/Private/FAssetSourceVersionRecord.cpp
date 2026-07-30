#include "Asset/FAssetSourceVersionRecord.h"

#include <algorithm>

namespace Stoner::Asset
{

EAssetResult NormalizeSourceManifest(
    Core::TArray<FAssetSourceVersionRecord>& Manifest)
{
    for (const FAssetSourceVersionRecord& Record : Manifest)
    {
        if (!Record.Id.IsValid() ||
            Record.Version.Validate() != EAssetResult::Success)
        {
            return EAssetResult::InvalidInput;
        }
    }
    std::sort(
        Manifest.begin(),
        Manifest.end(),
        [](const auto& Left, const auto& Right)
        {
            return Left.Id < Right.Id;
        });
    auto Write = Manifest.begin();
    for (auto Read = Manifest.begin(); Read != Manifest.end(); ++Read)
    {
        if (Write != Manifest.begin() && (Write - 1)->Id == Read->Id)
        {
            if (!((Write - 1)->Version == Read->Version))
            {
                return EAssetResult::Conflict;
            }
            continue;
        }
        if (Write != Read)
        {
            *Write = std::move(*Read);
        }
        ++Write;
    }
    Manifest.erase(Write, Manifest.end());
    return EAssetResult::Success;
}

} // namespace Stoner::Asset
