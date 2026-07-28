#pragma once

#include "Asset/FAssetMetadata.h"

#include <memory>
#include <optional>

namespace Stoner::Asset
{

enum class EAssetMutationKind : Core::uint8
{
    Register,
    Replace,
    Remove
};

struct FAssetMutation
{
    EAssetMutationKind Kind = EAssetMutationKind::Register;
    FAssetMetadata Metadata;
    FAssetId Id;
};

class FAssetMutationBatch
{
public:
    void Register(FAssetMetadata Metadata);
    void Replace(FAssetMetadata Metadata);
    void Remove(FAssetId Id);
    [[nodiscard]] const Core::TArray<FAssetMutation>& GetMutations() const noexcept;

private:
    Core::TArray<FAssetMutation> Mutations_;
};

struct FAssetRegistrySnapshot
{
    Core::uint64 Revision = 0;
    Core::TArray<FAssetMetadata> Records;
};

class FAssetRegistry
{
public:
    FAssetRegistry();
    ~FAssetRegistry();
    FAssetRegistry(const FAssetRegistry&) = delete;
    FAssetRegistry& operator=(const FAssetRegistry&) = delete;

    [[nodiscard]] EAssetResult Apply(const FAssetMutationBatch& Batch);
    [[nodiscard]] std::optional<FAssetMetadata> Find(const FAssetId& Id) const;
    [[nodiscard]] Core::TArray<FAssetId> FindByType(const Core::FString& AssetType) const;
    [[nodiscard]] Core::TArray<FAssetId> FindBySource(const FAssetSourceLocator& Source) const;
    [[nodiscard]] Core::TArray<FAssetDependency> GetDependencies(const FAssetId& Id) const;
    [[nodiscard]] Core::TArray<FAssetId> GetDependents(const FAssetId& Id) const;
    [[nodiscard]] EAssetResult ValidateCompleteness(
        Core::TArray<FAssetDependency>& OutUnresolved) const;
    [[nodiscard]] FAssetRegistrySnapshot Snapshot() const;

private:
    struct FImpl;
    std::unique_ptr<FImpl> Impl_;
};

} // namespace Stoner::Asset
