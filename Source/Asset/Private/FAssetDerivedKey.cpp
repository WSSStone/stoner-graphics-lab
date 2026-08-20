#include "Asset/FAssetDerivedKey.h"

#include <algorithm>

namespace Stoner::Asset
{

EAssetResult FAssetDerivedKey::ParseLowerHex(
    const Core::FString& Text,
    FAssetDerivedKey& OutKey) noexcept
{
    OutKey = {};
    FAssetDigest Digest;
    const EAssetResult Result = FAssetDigest::ParseLowerHex(Text, Digest);
    if (Result == EAssetResult::Success)
    {
        OutKey = FAssetDerivedKey(std::move(Digest));
    }
    return Result;
}

EAssetResult FAssetDerivedKeyEvidence::Validate() const noexcept
{
    if ((KeyFormatVersion != LegacyKeyFormatVersion &&
         KeyFormatVersion != CurrentKeyFormatVersion) ||
        !AssetId.IsValid() ||
        !SourceVersion.IsAvailable() || !ImporterId.IsValid() ||
        !ImporterVersion.IsValid() || !CookerId.IsValid() ||
        !CookerVersion.IsValid() || !CodecId.IsValid() ||
        !CodecVersion.IsValid() || PayloadSchemaVersion == 0 ||
        !EffectiveSettingsDigest.IsAvailable() ||
        !RelevantProfileDigest.IsAvailable())
    {
        return EAssetResult::InvalidInput;
    }
    if ((KeyFormatVersion == LegacyKeyFormatVersion &&
         !AdditionalEvidence.empty()) ||
        (KeyFormatVersion == CurrentKeyFormatVersion &&
         AdditionalEvidence.empty()))
    {
        return EAssetResult::InvalidInput;
    }
    for (Core::usize Index = 0; Index < AdditionalEvidence.size(); ++Index)
    {
        const auto& Evidence = AdditionalEvidence[Index];
        if (Evidence.Name.IsEmpty() || Evidence.Name.Len() > 128 ||
            !Evidence.Digest.IsAvailable() ||
            (Index > 0 &&
             !(AdditionalEvidence[Index - 1].Name < Evidence.Name)))
        {
            return EAssetResult::InvalidInput;
        }
    }
    for (Core::usize Index = 0; Index < SourceManifest.size(); ++Index)
    {
        const auto& Source = SourceManifest[Index];
        if (!Source.Locator.IsValid() || !Source.Version.IsAvailable() ||
            (Index > 0 && !(SourceManifest[Index - 1].Locator < Source.Locator)))
        {
            return EAssetResult::InvalidInput;
        }
    }
    for (Core::usize Index = 0; Index < Dependencies.size(); ++Index)
    {
        const auto& Dependency = Dependencies[Index];
        if (!Dependency.Id.IsValid() ||
            (!Dependency.Version.SourceDigest.IsAvailable() &&
             !Dependency.Version.ContentDigest.IsAvailable() &&
             !Dependency.Version.CookDigest.IsAvailable()))
        {
            return EAssetResult::InvalidInput;
        }
        if (Index > 0)
        {
            const auto& Previous = Dependencies[Index - 1];
            if (static_cast<Core::uint8>(Previous.Role) >
                    static_cast<Core::uint8>(Dependency.Role) ||
                (Previous.Role == Dependency.Role &&
                 !(Previous.Id < Dependency.Id)))
            {
                return EAssetResult::InvalidInput;
            }
        }
    }
    return EAssetResult::Success;
}

} // namespace Stoner::Asset
