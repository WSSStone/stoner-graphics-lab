#include "Asset/FAssetDerivedDataEntry.h"

#include <algorithm>

namespace Stoner::Asset
{

EAssetResult FAssetDerivedDataEntry::Validate() const noexcept
{
    if (Schema != Core::FString("stoner.asset-derived-entry") ||
        SchemaVersion != CurrentSchemaVersion || !DerivedKey.IsValid() ||
        Evidence.Validate() != EAssetResult::Success ||
        AssetId != Evidence.AssetId || !CodecId.IsValid() ||
        !CodecVersion.IsValid() || PayloadSchemaVersion == 0 ||
        CodecId != Evidence.CodecId || CodecVersion != Evidence.CodecVersion ||
        PayloadSchemaVersion != Evidence.PayloadSchemaVersion ||
        RelevantProfileDigest != Evidence.RelevantProfileDigest ||
        PayloadLocator != Core::FString("Payload.sgasset") || PayloadBytes == 0 ||
        !EnvelopeDigest.IsAvailable() ||
        !std::is_sorted(RequiredExtensions.begin(), RequiredExtensions.end()) ||
        std::adjacent_find(RequiredExtensions.begin(), RequiredExtensions.end()) !=
            RequiredExtensions.end())
        return EAssetResult::InvalidInput;
    for (const auto& Extension : RequiredExtensions)
        if (Extension.IsEmpty()) return EAssetResult::InvalidInput;
    return EAssetResult::Success;
}

} // namespace Stoner::Asset
