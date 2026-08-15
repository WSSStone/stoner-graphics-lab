#include "FAssetDependencyScheduler.h"

#include "IAssetLoadingStrategy.h"
#include "FAssetNodeLoadCoordinator.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

namespace Stoner::Asset::Private
{
namespace
{
struct FNode
{
    FAssetMetadata Metadata;
    Core::TSharedPtr<const FAssetPayload> Payload;
    Core::uint64 PayloadBytes = 0;
};

struct FClosureState
{
    IAssetLoadingStrategy& Strategy;
    const FAssetRuntimeExecutionContext& Context;
    const FAssetManagerLimits& Limits;
    FAssetNodeLoadCoordinator* Coordinator = nullptr;
    std::map<FAssetId, FNode> Nodes;
    std::set<FAssetId> Processed;
    std::set<FAssetId> Visiting;
    Core::uint64 EdgeCount = 0;
    FAssetLoadScratchResult Result;
};

bool HasValidatedFallback(
    const FAssetLoadScratchResult& Result,
    const FAssetId& Owner,
    const FAssetId& Dependency)
{
    return std::any_of(Result.OptionalFallbacks.begin(),
        Result.OptionalFallbacks.end(),
        [&Owner, &Dependency](const FAssetOptionalFallback& Value)
        {
            return Value.Owner == Owner && Value.Dependency == Dependency &&
                Value.Decision ==
                    EAssetOptionalFallbackDecision::ValidatedFallback;
        });
}

EAssetResult Visit(
    FClosureState& State,
    const FAssetLoadKey& Key,
    Core::uint32 Depth,
    Core::TArray<FAssetId> Path)
{
    Path.push_back(Key.AssetId);
    const auto Fail = [&State, &Path](EAssetResult Result)
    {
        if (State.Result.FailurePath.empty())
            State.Result.FailurePath = Path;
        return Result;
    };
    if (State.Context.ShouldStop()) return Fail(EAssetResult::Cancelled);
    if (Depth > State.Limits.MaxDependencyDepth)
        return Fail(EAssetResult::CapacityExceeded);
    if (State.Processed.contains(Key.AssetId)) return EAssetResult::Success;
    if (State.Visiting.contains(Key.AssetId))
        return Fail(EAssetResult::Conflict);
    if (!State.Nodes.contains(Key.AssetId))
    {
        if (State.Nodes.size() >= State.Limits.MaxKnownAssets)
            return Fail(EAssetResult::CapacityExceeded);
        FAssetLoadScratchResult Loaded =
            Depth > 0 && State.Coordinator
            ? State.Coordinator->Load(Key, State.Strategy, State.Context)
            : State.Strategy.Load(Key, State.Context);
        State.Result.OptionalFallbacks.insert(
            State.Result.OptionalFallbacks.end(),
            Loaded.OptionalFallbacks.begin(), Loaded.OptionalFallbacks.end());
        State.Result.Diagnostics.insert(State.Result.Diagnostics.end(),
            Loaded.Diagnostics.begin(), Loaded.Diagnostics.end());
        State.Result.bExtensionContractViolation =
            State.Result.bExtensionContractViolation ||
            Loaded.bExtensionContractViolation;
        if (Loaded.Result != EAssetResult::Success)
            return Fail(Loaded.Result);
        if (Loaded.Metadata.empty() ||
            Loaded.Metadata.size() != Loaded.Payloads.size() ||
            Loaded.Metadata.size() != Loaded.PayloadBytes.size())
            return Fail(EAssetResult::ProcessingFailure);
        for (Core::usize Index = 0; Index < Loaded.Metadata.size(); ++Index)
        {
            const auto& Metadata = Loaded.Metadata[Index];
            const auto& Payload = Loaded.Payloads[Index];
            if (Metadata.Validate() != EAssetResult::Success || !Payload ||
                Payload->GetAssetType() != Metadata.Id.GetAssetType())
                return Fail(EAssetResult::ProcessingFailure);
            const auto [Found, Inserted] = State.Nodes.emplace(
                Metadata.Id,
                FNode{Metadata, Payload, Loaded.PayloadBytes[Index]});
            if (!Inserted &&
                (!Found->second.Metadata.IsCanonicallyEquivalent(Metadata) ||
                 Found->second.Payload->GetAssetType() != Payload->GetAssetType()))
                return Fail(EAssetResult::Conflict);
        }
    }
    const auto Owner = State.Nodes.find(Key.AssetId);
    if (Owner == State.Nodes.end() ||
        Owner->second.Payload->GetAssetType() != Key.ExpectedType)
    {
        State.Visiting.erase(Key.AssetId);
        return Fail(Owner == State.Nodes.end()
            ? EAssetResult::NotFound
            : EAssetResult::TypeMismatch);
    }

    State.Visiting.insert(Key.AssetId);
    const auto Dependencies = Owner->second.Metadata.Dependencies;
    for (const FAssetDependency& Dependency : Dependencies)
    {
        if (++State.EdgeCount > State.Limits.MaxDependencyEdges)
        {
            State.Visiting.erase(Key.AssetId);
            return Fail(EAssetResult::CapacityExceeded);
        }
        FAssetLoadKey DependencyKey = Key;
        DependencyKey.AssetId = Dependency.TargetId;
        DependencyKey.ExpectedType = Dependency.TargetId.GetAssetType();
        const EAssetResult DependencyResult =
            Visit(State, DependencyKey, Depth + 1, Path);
        if (DependencyResult != EAssetResult::Success &&
            !(Dependency.Strength == EAssetDependencyStrength::Soft &&
              HasValidatedFallback(
                  State.Result, Key.AssetId, Dependency.TargetId)))
        {
            State.Visiting.erase(Key.AssetId);
            return DependencyResult;
        }
    }
    State.Visiting.erase(Key.AssetId);
    State.Processed.insert(Key.AssetId);
    return EAssetResult::Success;
}
} // namespace

FAssetLoadScratchResult FAssetDependencyScheduler::LoadClosure(
    const FAssetLoadKey& Root,
    IAssetLoadingStrategy& Strategy,
    const FAssetRuntimeExecutionContext& Context,
    const FAssetManagerLimits& Limits,
    FAssetNodeLoadCoordinator* Coordinator)
{
    FClosureState State{
        Strategy, Context, Limits, Coordinator, {}, {}, {}, 0, {}};
    State.Result.Result = Visit(State, Root, 0, {});
    if (State.Result.Result != EAssetResult::Success) return State.Result;
    State.Result.Metadata.reserve(State.Nodes.size());
    State.Result.Payloads.reserve(State.Nodes.size());
    for (auto& [Id, Node] : State.Nodes)
    {
        (void)Id;
        State.Result.Metadata.push_back(std::move(Node.Metadata));
        State.Result.Payloads.push_back(std::move(Node.Payload));
        State.Result.PayloadBytes.push_back(Node.PayloadBytes);
    }
    return State.Result;
}

} // namespace Stoner::Asset::Private
