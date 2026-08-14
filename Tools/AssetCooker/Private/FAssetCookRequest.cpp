#include "AssetCooker/FAssetCookRequest.h"

#include <algorithm>
#include <filesystem>
#include <set>

namespace Stoner::AssetCooker
{
namespace
{

bool CanonicalPath(const Core::FString& Input, std::filesystem::path& Out)
{
    if (Input.IsEmpty()) return false;
    std::error_code Error;
    Out = std::filesystem::absolute(
        std::filesystem::path(Input.ToStdString()), Error).lexically_normal();
    if (Error) return false;
    const auto Existing = std::filesystem::weakly_canonical(Out, Error);
    if (!Error) Out = Existing;
    return true;
}

bool Contains(const std::filesystem::path& Root, const std::filesystem::path& Path)
{
    auto RootIterator = Root.begin();
    auto PathIterator = Path.begin();
    for (; RootIterator != Root.end(); ++RootIterator, ++PathIterator)
        if (PathIterator == Path.end() || *RootIterator != *PathIterator) return false;
    return true;
}

} // namespace

Asset::EAssetResult FAssetCookRequest::Validate() const
{
    if (SourceRoots.empty() || TargetProfilePath.IsEmpty() ||
        OutputRoot.IsEmpty() || DerivedDataRoot.IsEmpty() || ScratchRoot.IsEmpty() ||
        WorkerCount == 0 || WorkerCount > 32 ||
        TargetProfile.Validate() != Asset::EAssetResult::Success ||
        LeaseTimeout.count() < 0 || LeaseTimeout > std::chrono::minutes(10) ||
        (Mode == EAssetCookRunMode::PlanOnly && LeaseTimeout.count() != 0) ||
        (SelectionMode == Asset::EAssetCookSelectionMode::ExplicitRoots &&
         ExplicitRoots.empty()) ||
        (SelectionMode == Asset::EAssetCookSelectionMode::CookAll &&
         !ExplicitRoots.empty()) ||
        !std::is_sorted(ExplicitRoots.begin(), ExplicitRoots.end()) ||
        std::adjacent_find(ExplicitRoots.begin(), ExplicitRoots.end()) !=
            ExplicitRoots.end())
        return Asset::EAssetResult::InvalidInput;

    Core::TArray<std::filesystem::path> Roots;
    Roots.reserve(SourceRoots.size() + 3);
    for (const auto& SourceRoot : SourceRoots)
    {
        std::filesystem::path Path;
        if (!CanonicalPath(SourceRoot, Path)) return Asset::EAssetResult::InvalidInput;
        Roots.push_back(std::move(Path));
    }
    for (const auto& Text : {OutputRoot, DerivedDataRoot, ScratchRoot})
    {
        std::filesystem::path Path;
        if (!CanonicalPath(Text, Path)) return Asset::EAssetResult::InvalidInput;
        Roots.push_back(std::move(Path));
    }
    for (Core::usize Left = 0; Left < Roots.size(); ++Left)
        for (Core::usize Right = Left + 1; Right < Roots.size(); ++Right)
            if (Contains(Roots[Left], Roots[Right]) || Contains(Roots[Right], Roots[Left]))
                return Asset::EAssetResult::Conflict;
    if (!ReportPath.IsEmpty())
    {
        std::filesystem::path Report;
        if (!CanonicalPath(ReportPath, Report)) return Asset::EAssetResult::InvalidInput;
        for (const auto& Root : Roots)
            if (Contains(Root, Report) || Report == Root)
                return Asset::EAssetResult::Conflict;
    }
    return Asset::EAssetResult::Success;
}

} // namespace Stoner::AssetCooker
