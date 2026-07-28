#include "FAssetDependencyGraph.h"

#include <unordered_map>

namespace Stoner::Asset::Private
{
namespace
{

bool Visit(
    const FAssetId& Id,
    const FRecordMap& Records,
    std::unordered_map<FAssetId, Core::uint8>& Marks)
{
    Core::uint8& Mark = Marks[Id];
    if (Mark == 1)
    {
        return false;
    }
    if (Mark == 2)
    {
        return true;
    }
    Mark = 1;
    const auto Found = Records.find(Id);
    if (Found != Records.end())
    {
        for (const FAssetDependency& Dependency : Found->second.Dependencies)
        {
            if (Dependency.Strength == EAssetDependencyStrength::Required &&
                Records.contains(Dependency.TargetId) &&
                !Visit(Dependency.TargetId, Records, Marks))
            {
                return false;
            }
        }
    }
    Mark = 2;
    return true;
}

} // namespace

EAssetResult NormalizeAndValidateDependencyGraph(FRecordMap& Records)
{
    for (auto& [Id, Metadata] : Records)
    {
        (void)Id;
        for (FAssetDependency& Dependency : Metadata.Dependencies)
        {
            Dependency.Resolution = Records.contains(Dependency.TargetId)
                ? EAssetDependencyResolution::Resolved
                : EAssetDependencyResolution::Unresolved;
        }
    }

    std::unordered_map<FAssetId, Core::uint8> Marks;
    for (const auto& [Id, Metadata] : Records)
    {
        (void)Metadata;
        if (!Visit(Id, Records, Marks))
        {
            return EAssetResult::DependencyCycle;
        }
    }
    return EAssetResult::Success;
}

} // namespace Stoner::Asset::Private
