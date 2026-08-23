#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FStaticModelRealization.h"

#include <functional>

namespace Stoner::Renderer::Private
{

struct FStaticModelOwnedResource
{
    Core::FString StableId;
    std::function<void()> Release;
};

class FStaticModelRealizationTransaction
{
public:
    explicit FStaticModelRealizationTransaction(
        FStaticModelRealizationInspection& Inspection);
    ~FStaticModelRealizationTransaction();
    FStaticModelRealizationTransaction(
        const FStaticModelRealizationTransaction&) = delete;
    FStaticModelRealizationTransaction& operator=(
        const FStaticModelRealizationTransaction&) = delete;

    void Add(FStaticModelOwnedResource Resource);
    [[nodiscard]] Core::TArray<FStaticModelOwnedResource> Commit();
    void Rollback() noexcept;

private:
    FStaticModelRealizationInspection* Inspection_ = nullptr;
    Core::TArray<FStaticModelOwnedResource> Resources_;
    bool bCommitted_ = false;
};

void ReleaseStaticModelResources(
    Core::TArray<FStaticModelOwnedResource>& Resources,
    FStaticModelRealizationInspection& Inspection) noexcept;

} // namespace Stoner::Renderer::Private
