#include "FAssetCookGraph.h"

#include <algorithm>
#include <map>
#include <set>
#include <vector>

namespace Stoner::AssetCooker::Private
{
namespace
{

bool HasVersionEvidence(const Asset::FAssetVersion& Version)
{
    return Version.SourceDigest.IsAvailable() ||
        Version.ContentDigest.IsAvailable() || Version.CookDigest.IsAvailable();
}

bool ValidDependency(const Asset::FAssetDependency& Dependency)
{
    return static_cast<Core::uint8>(Dependency.Role) <=
            static_cast<Core::uint8>(Asset::EAssetDependencyRole::Runtime) &&
        static_cast<Core::uint8>(Dependency.Strength) <=
            static_cast<Core::uint8>(Asset::EAssetDependencyStrength::Soft) &&
        static_cast<Core::uint8>(Dependency.Resolution) <=
            static_cast<Core::uint8>(Asset::EAssetDependencyResolution::Resolved);
}

} // namespace

Asset::EAssetResult FAssetCookGraphLimits::Validate() const noexcept
{
    return MaxAssets > 0 && MaxAssets <= 100000 &&
            MaxDependencyEdges <= 1000000 && MaxDependencyDepth > 0 &&
            MaxDependencyDepth <= 256
        ? Asset::EAssetResult::Success
        : Asset::EAssetResult::InvalidInput;
}

Asset::EAssetResult FAssetCookGraphPlan::Validate() const noexcept
{
    if (Roots.empty() || Nodes.empty()) return Asset::EAssetResult::InvalidInput;
    Core::uint64 Edges = 0;
    for (Core::usize Index = 0; Index < Nodes.size(); ++Index)
    {
        const auto& Node = Nodes[Index];
        if (Node.PlanIndex != Index || !Node.Payload ||
            Node.Metadata.Validate() != Asset::EAssetResult::Success ||
            Node.Payload->GetAssetType() != Node.Metadata.Id.GetAssetType())
            return Asset::EAssetResult::InvalidInput;
        for (const Core::uint32 Dependency : Node.Dependencies)
        {
            if (Dependency >= Index) return Asset::EAssetResult::DependencyCycle;
            ++Edges;
        }
        if (!std::is_sorted(Node.Dependencies.begin(), Node.Dependencies.end()) ||
            !std::is_sorted(Node.Dependents.begin(), Node.Dependents.end()))
            return Asset::EAssetResult::InvalidInput;
    }
    return Edges == DependencyEdges
        ? Asset::EAssetResult::Success
        : Asset::EAssetResult::InvalidInput;
}

Asset::EAssetResult FAssetCookGraph::Build(
    const Core::TArray<Asset::FAssetImportOutput>& AvailableOutputs,
    Asset::EAssetCookSelectionMode SelectionMode,
    const Core::TArray<Asset::FAssetId>& ExplicitRoots,
    const FAssetCookGraphLimits& Limits,
    FAssetCookGraphPlan& OutPlan)
{
    OutPlan = {};
    if (Limits.Validate() != Asset::EAssetResult::Success ||
        AvailableOutputs.empty() ||
        (SelectionMode == Asset::EAssetCookSelectionMode::ExplicitRoots &&
         ExplicitRoots.empty()) ||
        (SelectionMode == Asset::EAssetCookSelectionMode::CookAll &&
         !ExplicitRoots.empty()))
        return Asset::EAssetResult::InvalidInput;

    std::map<Asset::FAssetId, const Asset::FAssetImportOutput*> Available;
    for (const auto& Output : AvailableOutputs)
    {
        if (!Output.Payload ||
            Output.Metadata.Validate() != Asset::EAssetResult::Success ||
            Output.Payload->GetAssetType() != Output.Metadata.Id.GetAssetType() ||
            !HasVersionEvidence(Output.Metadata.Version) ||
            !std::all_of(
                Output.Metadata.Dependencies.begin(),
                Output.Metadata.Dependencies.end(), ValidDependency))
            return Asset::EAssetResult::InvalidInput;
        if (!Available.emplace(Output.Metadata.Id, &Output).second)
            return Asset::EAssetResult::Conflict;
    }

    std::set<Asset::FAssetId> RootSet;
    if (SelectionMode == Asset::EAssetCookSelectionMode::CookAll)
    {
        for (const auto& [Id, Output] : Available)
        {
            (void)Output;
            RootSet.insert(Id);
        }
    }
    else
    {
        RootSet.insert(ExplicitRoots.begin(), ExplicitRoots.end());
        if (RootSet.size() != ExplicitRoots.size())
            return Asset::EAssetResult::Conflict;
    }

    std::set<Asset::FAssetId> Reachable;
    std::vector<Asset::FAssetId> Stack(RootSet.begin(), RootSet.end());
    Core::uint64 EdgeCount = 0;
    while (!Stack.empty())
    {
        const Asset::FAssetId Id = Stack.back();
        Stack.pop_back();
        if (!Reachable.insert(Id).second) continue;
        const auto Found = Available.find(Id);
        if (Found == Available.end()) return Asset::EAssetResult::UnresolvedDependency;
        if (Reachable.size() > Limits.MaxAssets)
            return Asset::EAssetResult::CapacityExceeded;
        for (const auto& Dependency : Found->second->Metadata.Dependencies)
        {
            if (Dependency.Strength != Asset::EAssetDependencyStrength::Required)
                continue;
            if (++EdgeCount > Limits.MaxDependencyEdges)
                return Asset::EAssetResult::CapacityExceeded;
            if (Dependency.TargetId == Id)
                return Asset::EAssetResult::DependencyCycle;
            Stack.push_back(Dependency.TargetId);
        }
    }

    std::map<Asset::FAssetId, Core::uint32> Indegree;
    std::map<Asset::FAssetId, Core::TArray<Asset::FAssetId>> Dependents;
    for (const auto& Id : Reachable) Indegree.emplace(Id, 0);
    for (const auto& Id : Reachable)
    {
        for (const auto& Dependency : Available.at(Id)->Metadata.Dependencies)
        {
            if (Dependency.Strength != Asset::EAssetDependencyStrength::Required)
                continue;
            if (!Reachable.contains(Dependency.TargetId))
                return Asset::EAssetResult::UnresolvedDependency;
            ++Indegree[Id];
            Dependents[Dependency.TargetId].push_back(Id);
        }
    }
    for (auto& [Id, Values] : Dependents)
    {
        (void)Id;
        std::sort(Values.begin(), Values.end());
    }

    std::set<Asset::FAssetId> Ready;
    for (const auto& [Id, Degree] : Indegree)
        if (Degree == 0) Ready.insert(Id);
    Core::TArray<Asset::FAssetId> Order;
    while (!Ready.empty())
    {
        const Asset::FAssetId Id = *Ready.begin();
        Ready.erase(Ready.begin());
        Order.push_back(Id);
        for (const auto& Dependent : Dependents[Id])
            if (--Indegree[Dependent] == 0) Ready.insert(Dependent);
    }
    if (Order.size() != Reachable.size())
        return Asset::EAssetResult::DependencyCycle;

    std::map<Asset::FAssetId, Core::uint32> PlanIndices;
    for (Core::uint32 Index = 0; Index < Order.size(); ++Index)
        PlanIndices.emplace(Order[Index], Index);
    Core::TArray<Core::uint32> Depth(Order.size(), 1);
    OutPlan.Nodes.reserve(Order.size());
    OutPlan.DependencyEdges = 0;
    for (Core::uint32 Index = 0; Index < Order.size(); ++Index)
    {
        const auto* Output = Available.at(Order[Index]);
        FAssetCookGraphNode Node;
        Node.PlanIndex = Index;
        Node.Metadata = Output->Metadata;
        Node.Payload = Output->Payload;
        for (const auto& Dependency : Output->Metadata.Dependencies)
        {
            if (Dependency.Strength != Asset::EAssetDependencyStrength::Required)
                continue;
            const Core::uint32 DependencyIndex = PlanIndices.at(Dependency.TargetId);
            Node.Dependencies.push_back(DependencyIndex);
            Depth[Index] = std::max(Depth[Index], Depth[DependencyIndex] + 1);
            ++OutPlan.DependencyEdges;
        }
        if (Depth[Index] > Limits.MaxDependencyDepth)
        {
            OutPlan = {};
            return Asset::EAssetResult::CapacityExceeded;
        }
        std::sort(Node.Dependencies.begin(), Node.Dependencies.end());
        OutPlan.Nodes.push_back(std::move(Node));
    }
    for (Core::uint32 Index = 0; Index < OutPlan.Nodes.size(); ++Index)
        for (const Core::uint32 Dependency : OutPlan.Nodes[Index].Dependencies)
            OutPlan.Nodes[Dependency].Dependents.push_back(Index);
    OutPlan.Roots.assign(RootSet.begin(), RootSet.end());
    if (OutPlan.Validate() != Asset::EAssetResult::Success)
    {
        OutPlan = {};
        return Asset::EAssetResult::ProcessingFailure;
    }
    return Asset::EAssetResult::Success;
}

} // namespace Stoner::AssetCooker::Private
