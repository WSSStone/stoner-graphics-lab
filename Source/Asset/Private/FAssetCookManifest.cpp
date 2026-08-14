#include "Asset/FAssetCookManifest.h"

#include "Asset/FAssetParticipant.h"

#include <algorithm>
#include <set>

namespace Stoner::Asset
{
namespace
{

bool IsToken(const Core::FString& Value, Core::usize Maximum = 128)
{
    if (Value.IsEmpty() || Value.Len() > Maximum)
        return false;
    for (const unsigned char Character : Value.View())
    {
        if (!((Character >= 'a' && Character <= 'z') ||
              (Character >= '0' && Character <= '9') ||
              Character == '.' || Character == '_' || Character == '-'))
            return false;
    }
    const unsigned char First = static_cast<unsigned char>(Value.View().front());
    return (First >= 'a' && First <= 'z') ||
        (First >= '0' && First <= '9');
}

bool IsSafeRelativeLocator(const Core::FString& Locator)
{
    const std::string_view Text = Locator.View();
    if (Text.empty() || Text.size() > 4096 || Text.front() == '/' ||
        Text.front() == '\\' || Text.find('\\') != std::string_view::npos ||
        Text.find(':') != std::string_view::npos || Text.back() == '/')
        return false;
    std::size_t Start = 0;
    while (Start < Text.size())
    {
        const std::size_t End = Text.find('/', Start);
        const std::string_view Segment = Text.substr(
            Start,
            End == std::string_view::npos ? Text.size() - Start : End - Start);
        if (Segment.empty() || Segment == "." || Segment == "..") return false;
        if (End == std::string_view::npos) break;
        Start = End + 1;
    }
    return true;
}

} // namespace

EAssetResult FAssetCookSelection::Validate() const noexcept
{
    if (Roots.empty() || SourceScopes.empty() ||
        !DiscoveryRulesVersion.IsAvailable())
        return EAssetResult::InvalidInput;
    for (Core::usize Index = 0; Index < Roots.size(); ++Index)
    {
        if (!Roots[Index].IsValid() ||
            (Index > 0 && !(Roots[Index - 1] < Roots[Index])))
            return EAssetResult::InvalidInput;
    }
    for (Core::usize Index = 0; Index < SourceScopes.size(); ++Index)
    {
        if (!IsSafeRelativeLocator(SourceScopes[Index]) ||
            (Index > 0 && !(SourceScopes[Index - 1] < SourceScopes[Index])))
            return EAssetResult::InvalidInput;
    }
    return EAssetResult::Success;
}

EAssetResult FAssetCookManifestSourceRecord::Validate() const noexcept
{
    return AssetId.IsValid() && Version.IsAvailable() && IsToken(Role)
        ? EAssetResult::Success : EAssetResult::InvalidInput;
}

EAssetResult FAssetCookManifestDependencyRecord::Validate() const noexcept
{
    return AssetId.IsValid() && IsToken(Role) &&
            (!RequiredVersion || RequiredVersion->IsAvailable())
        ? EAssetResult::Success : EAssetResult::InvalidInput;
}

EAssetResult FAssetCookManifestParticipant::Validate() const noexcept
{
    return Id.IsValid() && Version.IsValid()
        ? EAssetResult::Success : EAssetResult::InvalidInput;
}

EAssetResult FAssetCookManifestRecord::Validate() const noexcept
{
    if (!AssetId.IsValid() || AssetType != AssetId.GetAssetType() ||
        !SourceVersion.IsAvailable() ||
        Importer.Validate() != EAssetResult::Success ||
        Cooker.Validate() != EAssetResult::Success ||
        Codec.Validate() != EAssetResult::Success || !DerivedKey.IsValid() ||
        PayloadSchemaVersion == 0 || !IsSafeRelativeLocator(PayloadLocator) ||
        PayloadBytes == 0 || PayloadBytes > 1024ULL * 1024ULL * 1024ULL ||
        !EnvelopeDigest.IsAvailable())
        return EAssetResult::InvalidInput;
    for (Core::usize Index = 0; Index < SourceManifest.size(); ++Index)
    {
        if (SourceManifest[Index].Validate() != EAssetResult::Success ||
            (Index > 0 && !(SourceManifest[Index - 1].AssetId <
                            SourceManifest[Index].AssetId)))
            return EAssetResult::InvalidInput;
    }
    for (Core::usize Index = 0; Index < Dependencies.size(); ++Index)
    {
        if (Dependencies[Index].Validate() != EAssetResult::Success)
            return EAssetResult::InvalidInput;
        if (Index > 0)
        {
            const auto& Previous = Dependencies[Index - 1];
            const auto& Current = Dependencies[Index];
            if (Current.Role < Previous.Role ||
                (Previous.Role == Current.Role &&
                 !(Previous.AssetId < Current.AssetId)))
                return EAssetResult::InvalidInput;
        }
    }
    return EAssetResult::Success;
}

EAssetResult FAssetCookManifestLimits::Validate() const noexcept
{
    return MaxManifestBytes > 0 &&
            MaxManifestBytes <= 256ULL * 1024ULL * 1024ULL &&
            MaxRecords > 0 && MaxRecords <= 100000 &&
            MaxSourcesPerRecord <= 100000 &&
            MaxDependenciesPerRecord <= 100000 &&
            MaxRoots > 0 && MaxRoots <= 100000 &&
            MaxSourceScopes > 0 && MaxSourceScopes <= 1024 &&
            MaxExtensions <= 64 && MaxTextBytes > 0 && MaxTextBytes <= 4096
        ? EAssetResult::Success : EAssetResult::InvalidInput;
}

EAssetResult FAssetCookManifest::Validate(
    const FAssetCookManifestLimits& Limits) const noexcept
{
    if (Limits.Validate() != EAssetResult::Success ||
        Schema != Core::FString("stoner.asset-cook-manifest") ||
        SchemaVersion != CurrentSchemaVersion ||
        !GenerationId.IsAvailable() ||
        TargetProfile.Validate() != EAssetResult::Success ||
        Selection.Validate() != EAssetResult::Success ||
        !SnapshotDigest.IsAvailable() || !LimitsDigest.IsAvailable() ||
        Records.size() > Limits.MaxRecords ||
        Selection.Roots.size() > Limits.MaxRoots ||
        Selection.SourceScopes.size() > Limits.MaxSourceScopes ||
        RequiredExtensions.size() > Limits.MaxExtensions)
        return EAssetResult::InvalidInput;
    std::set<FAssetId> RecordIds;
    for (Core::usize Index = 0; Index < Records.size(); ++Index)
    {
        const auto& Record = Records[Index];
        if (Record.Validate() != EAssetResult::Success ||
            Record.AssetId.ToString().Len() > Limits.MaxTextBytes ||
            Record.AssetType.Len() > Limits.MaxTextBytes ||
            Record.PayloadLocator.Len() > Limits.MaxTextBytes ||
            Record.SourceManifest.size() > Limits.MaxSourcesPerRecord ||
            Record.Dependencies.size() > Limits.MaxDependenciesPerRecord ||
            (Index > 0 && !(Records[Index - 1].AssetId < Record.AssetId)))
            return EAssetResult::InvalidInput;
        for (const auto& Source : Record.SourceManifest)
        {
            if (Source.AssetId.ToString().Len() > Limits.MaxTextBytes ||
                Source.Role.Len() > Limits.MaxTextBytes)
                return EAssetResult::DefinitionLimitExceeded;
        }
        for (const auto& Dependency : Record.Dependencies)
        {
            if (Dependency.AssetId.ToString().Len() > Limits.MaxTextBytes ||
                Dependency.Role.Len() > Limits.MaxTextBytes)
                return EAssetResult::DefinitionLimitExceeded;
        }
        RecordIds.insert(Record.AssetId);
    }
    for (const auto& Record : Records)
    {
        for (const auto& Dependency : Record.Dependencies)
        {
            if (!RecordIds.contains(Dependency.AssetId))
                return EAssetResult::UnresolvedDependency;
        }
    }
    for (Core::usize Index = 0; Index < RequiredExtensions.size(); ++Index)
    {
        if (RequiredExtensions[Index].Len() > Limits.MaxTextBytes ||
            !IsToken(RequiredExtensions[Index]) ||
            (Index > 0 && !(RequiredExtensions[Index - 1] <
                            RequiredExtensions[Index])))
            return EAssetResult::InvalidInput;
    }
    for (const auto& Root : Selection.Roots)
    {
        if (Root.ToString().Len() > Limits.MaxTextBytes)
            return EAssetResult::DefinitionLimitExceeded;
    }
    for (const auto& Scope : Selection.SourceScopes)
    {
        if (Scope.Len() > Limits.MaxTextBytes)
            return EAssetResult::DefinitionLimitExceeded;
    }
    return EAssetResult::Success;
}

} // namespace Stoner::Asset
