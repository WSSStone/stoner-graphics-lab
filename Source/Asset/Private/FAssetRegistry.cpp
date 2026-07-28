#include "Asset/FAssetRegistry.h"

#include "FAssetDependencyGraph.h"

#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>

namespace Stoner::Asset
{

struct FAssetRegistry::FImpl
{
    mutable std::shared_mutex Mutex;
    Private::FRecordMap Records;
    std::unordered_map<std::string, Core::TArray<FAssetId>> TypeIndex;
    std::unordered_map<std::string, Core::TArray<FAssetId>> SourceIndex;
    std::unordered_map<FAssetId, Core::TArray<FAssetId>> ReverseIndex;
    Core::uint64 Revision = 0;

    void RebuildIndexes()
    {
        TypeIndex.clear();
        SourceIndex.clear();
        ReverseIndex.clear();
        for (const auto& [Id, Metadata] : Records)
        {
            TypeIndex[Id.GetAssetType().ToStdString()].push_back(Id);
            SourceIndex[Metadata.Source.ToString().ToStdString()].push_back(Id);
            for (const FAssetDependency& Dependency : Metadata.Dependencies)
            {
                ReverseIndex[Dependency.TargetId].push_back(Id);
            }
        }
        const auto SortValues = [](auto& Index)
        {
            for (auto& [Key, Values] : Index)
            {
                (void)Key;
                std::sort(Values.begin(), Values.end());
            }
        };
        SortValues(TypeIndex);
        SortValues(SourceIndex);
        SortValues(ReverseIndex);
    }
};

void FAssetMutationBatch::Register(FAssetMetadata Metadata)
{
    Mutations_.push_back({EAssetMutationKind::Register, std::move(Metadata), {}});
}

void FAssetMutationBatch::Replace(FAssetMetadata Metadata)
{
    Mutations_.push_back({EAssetMutationKind::Replace, std::move(Metadata), {}});
}

void FAssetMutationBatch::Remove(FAssetId Id)
{
    Mutations_.push_back({EAssetMutationKind::Remove, {}, std::move(Id)});
}

const Core::TArray<FAssetMutation>& FAssetMutationBatch::GetMutations() const noexcept
{
    return Mutations_;
}

FAssetRegistry::FAssetRegistry()
    : Impl_(std::make_unique<FImpl>())
{
}

FAssetRegistry::~FAssetRegistry() = default;

EAssetResult FAssetRegistry::Apply(const FAssetMutationBatch& Batch)
{
    if (Batch.GetMutations().empty())
    {
        return EAssetResult::Success;
    }
    std::unique_lock Lock(Impl_->Mutex);
    Private::FRecordMap Proposed = Impl_->Records;
    std::unordered_set<FAssetId> Touched;
    bool Changed = false;
    for (const FAssetMutation& Mutation : Batch.GetMutations())
    {
        const FAssetId& Id = Mutation.Kind == EAssetMutationKind::Remove
            ? Mutation.Id
            : Mutation.Metadata.Id;
        if (!Id.IsValid() || !Touched.insert(Id).second)
        {
            return EAssetResult::Conflict;
        }

        auto Existing = Proposed.find(Id);
        if (Mutation.Kind == EAssetMutationKind::Remove)
        {
            if (Existing == Proposed.end())
            {
                return EAssetResult::NotFound;
            }
            Proposed.erase(Existing);
            Changed = true;
            continue;
        }
        if (Mutation.Metadata.Validate() != EAssetResult::Success)
        {
            return EAssetResult::InvalidInput;
        }
        if (Mutation.Kind == EAssetMutationKind::Register)
        {
            if (Existing != Proposed.end())
            {
                if (!Existing->second.IsCanonicallyEquivalent(Mutation.Metadata))
                {
                    return EAssetResult::AlreadyExists;
                }
                continue;
            }
            Proposed.emplace(Id, Mutation.Metadata);
            Changed = true;
        }
        else
        {
            if (Existing == Proposed.end())
            {
                return EAssetResult::NotFound;
            }
            Existing->second = Mutation.Metadata;
            Changed = true;
        }
    }

    const EAssetResult GraphResult =
        Private::NormalizeAndValidateDependencyGraph(Proposed);
    if (GraphResult != EAssetResult::Success)
    {
        return GraphResult;
    }
    if (Changed)
    {
        Impl_->Records = std::move(Proposed);
        Impl_->RebuildIndexes();
        ++Impl_->Revision;
    }
    return EAssetResult::Success;
}

std::optional<FAssetMetadata> FAssetRegistry::Find(const FAssetId& Id) const
{
    std::shared_lock Lock(Impl_->Mutex);
    const auto Found = Impl_->Records.find(Id);
    return Found != Impl_->Records.end()
        ? std::optional<FAssetMetadata>(Found->second)
        : std::nullopt;
}

Core::TArray<FAssetId> FAssetRegistry::FindByType(const Core::FString& AssetType) const
{
    std::shared_lock Lock(Impl_->Mutex);
    const auto Found = Impl_->TypeIndex.find(AssetType.ToStdString());
    return Found == Impl_->TypeIndex.end() ? Core::TArray<FAssetId>{} : Found->second;
}

Core::TArray<FAssetId> FAssetRegistry::FindBySource(const FAssetSourceLocator& Source) const
{
    std::shared_lock Lock(Impl_->Mutex);
    const auto Found = Impl_->SourceIndex.find(Source.ToString().ToStdString());
    return Found == Impl_->SourceIndex.end() ? Core::TArray<FAssetId>{} : Found->second;
}

Core::TArray<FAssetDependency> FAssetRegistry::GetDependencies(const FAssetId& Id) const
{
    const auto Metadata = Find(Id);
    if (!Metadata)
    {
        return {};
    }
    auto Result = Metadata->Dependencies;
    std::sort(
        Result.begin(),
        Result.end(),
        [](const FAssetDependency& Left, const FAssetDependency& Right)
        {
            return Left.TargetId < Right.TargetId;
        });
    return Result;
}

Core::TArray<FAssetId> FAssetRegistry::GetDependents(const FAssetId& Id) const
{
    std::shared_lock Lock(Impl_->Mutex);
    const auto Found = Impl_->ReverseIndex.find(Id);
    return Found == Impl_->ReverseIndex.end() ? Core::TArray<FAssetId>{} : Found->second;
}

EAssetResult FAssetRegistry::ValidateCompleteness(
    Core::TArray<FAssetDependency>& OutUnresolved) const
{
    std::shared_lock Lock(Impl_->Mutex);
    OutUnresolved.clear();
    for (const auto& [Id, Metadata] : Impl_->Records)
    {
        (void)Id;
        for (const FAssetDependency& Dependency : Metadata.Dependencies)
        {
            if (Dependency.Strength == EAssetDependencyStrength::Required &&
                Dependency.Resolution == EAssetDependencyResolution::Unresolved)
            {
                OutUnresolved.push_back(Dependency);
            }
        }
    }
    std::sort(
        OutUnresolved.begin(),
        OutUnresolved.end(),
        [](const FAssetDependency& Left, const FAssetDependency& Right)
        {
            return Left.TargetId < Right.TargetId;
        });
    return OutUnresolved.empty()
        ? EAssetResult::Success
        : EAssetResult::IncompleteRegistry;
}

FAssetRegistrySnapshot FAssetRegistry::Snapshot() const
{
    std::shared_lock Lock(Impl_->Mutex);
    FAssetRegistrySnapshot Snapshot;
    Snapshot.Revision = Impl_->Revision;
    Snapshot.Records.reserve(Impl_->Records.size());
    for (const auto& [Id, Metadata] : Impl_->Records)
    {
        (void)Id;
        Snapshot.Records.push_back(Metadata);
    }
    std::sort(
        Snapshot.Records.begin(),
        Snapshot.Records.end(),
        [](const FAssetMetadata& Left, const FAssetMetadata& Right)
        {
            return Left.Id < Right.Id;
        });
    return Snapshot;
}

} // namespace Stoner::Asset
