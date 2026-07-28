#pragma once

#include "Asset/FAssetId.h"

namespace Stoner::Asset
{

enum class EAssetDependencyRole : Core::uint8
{
    Source,
    Build,
    Runtime
};

enum class EAssetDependencyStrength : Core::uint8
{
    Required,
    Soft
};

enum class EAssetDependencyResolution : Core::uint8
{
    Unresolved,
    Resolved
};

struct FAssetDependency
{
    FAssetId TargetId;
    EAssetDependencyRole Role = EAssetDependencyRole::Runtime;
    EAssetDependencyStrength Strength = EAssetDependencyStrength::Required;
    EAssetDependencyResolution Resolution = EAssetDependencyResolution::Unresolved;

    [[nodiscard]] bool SameDeclaration(const FAssetDependency& Other) const noexcept
    {
        return TargetId == Other.TargetId && Role == Other.Role &&
            Strength == Other.Strength;
    }

    [[nodiscard]] bool operator==(const FAssetDependency&) const = default;
};

} // namespace Stoner::Asset
