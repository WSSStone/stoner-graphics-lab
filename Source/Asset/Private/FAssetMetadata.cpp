#include "Asset/FAssetMetadata.h"

#include <algorithm>
#include <set>
#include <tuple>

namespace Stoner::Asset
{
namespace
{

auto DependencyKey(const FAssetDependency& Dependency)
{
    return std::tuple(
        Dependency.TargetId,
        Dependency.Role,
        Dependency.Strength);
}

} // namespace

EAssetResult FAssetMetadata::Validate() const
{
    if (!Id.IsValid() || !Source.IsValid() || !Producer.IsValid() ||
        !ProducerVersion.IsValid() || Version.Validate() != EAssetResult::Success)
    {
        return EAssetResult::InvalidInput;
    }

    std::set<Core::FString> AttributeKeys;
    for (const FAssetAttribute& Attribute : Attributes)
    {
        if (Attribute.first.IsEmpty() || !AttributeKeys.insert(Attribute.first).second)
        {
            return EAssetResult::InvalidInput;
        }
    }

    std::set<decltype(DependencyKey(FAssetDependency{}))> DependencyKeys;
    for (const FAssetDependency& Dependency : Dependencies)
    {
        if (!Dependency.TargetId.IsValid() ||
            !DependencyKeys.insert(DependencyKey(Dependency)).second)
        {
            return EAssetResult::InvalidInput;
        }
    }
    return EAssetResult::Success;
}

bool FAssetMetadata::IsCanonicallyEquivalent(const FAssetMetadata& Other) const
{
    if (Id != Other.Id || Version != Other.Version || Source != Other.Source ||
        Producer != Other.Producer || ProducerVersion != Other.ProducerVersion)
    {
        return false;
    }
    auto LeftAttributes = Attributes;
    auto RightAttributes = Other.Attributes;
    const auto AttributeLess = [](const FAssetAttribute& Left, const FAssetAttribute& Right)
    {
        return Left.first != Right.first ? Left.first < Right.first : Left.second < Right.second;
    };
    std::sort(LeftAttributes.begin(), LeftAttributes.end(), AttributeLess);
    std::sort(RightAttributes.begin(), RightAttributes.end(), AttributeLess);
    if (LeftAttributes != RightAttributes)
    {
        return false;
    }

    auto LeftDependencies = Dependencies;
    auto RightDependencies = Other.Dependencies;
    const auto DependencyLess = [](const FAssetDependency& Left, const FAssetDependency& Right)
    {
        return DependencyKey(Left) < DependencyKey(Right);
    };
    std::sort(LeftDependencies.begin(), LeftDependencies.end(), DependencyLess);
    std::sort(RightDependencies.begin(), RightDependencies.end(), DependencyLess);
    if (LeftDependencies.size() != RightDependencies.size())
    {
        return false;
    }
    for (std::size_t Index = 0; Index < LeftDependencies.size(); ++Index)
    {
        if (!LeftDependencies[Index].SameDeclaration(RightDependencies[Index]))
        {
            return false;
        }
    }
    return true;
}

} // namespace Stoner::Asset
