#include "FStaticModelRealizationTransaction.h"

#include <utility>

namespace Stoner::Renderer::Private
{

FStaticModelRealizationTransaction::FStaticModelRealizationTransaction(
    FStaticModelRealizationInspection& Inspection)
    : Inspection_(&Inspection)
{
}

FStaticModelRealizationTransaction::~FStaticModelRealizationTransaction()
{
    if (!bCommitted_) Rollback();
}

void FStaticModelRealizationTransaction::Add(
    FStaticModelOwnedResource Resource)
{
    if (bCommitted_ || Resource.StableId.IsEmpty() || !Resource.Release)
        return;
    Inspection_->OrderedResourceIds.push_back(Resource.StableId);
    ++Inspection_->CreatedResourceCount;
    Resources_.push_back(std::move(Resource));
}

Core::TArray<FStaticModelOwnedResource>
FStaticModelRealizationTransaction::Commit()
{
    bCommitted_ = true;
    Inspection_->bCommitted = true;
    return std::move(Resources_);
}

void FStaticModelRealizationTransaction::Rollback() noexcept
{
    if (bCommitted_ || Resources_.empty()) return;
    Inspection_->bRolledBack = true;
    Inspection_->Stage = EStaticModelRealizationStage::Rollback;
    ReleaseStaticModelResources(Resources_, *Inspection_);
}

void ReleaseStaticModelResources(
    Core::TArray<FStaticModelOwnedResource>& Resources,
    FStaticModelRealizationInspection& Inspection) noexcept
{
    for (auto It = Resources.rbegin(); It != Resources.rend(); ++It)
    {
        if (!It->Release) continue;
        try
        {
            It->Release();
        }
        catch (...)
        {
        }
        Inspection.ReverseReleaseIds.push_back(It->StableId);
        ++Inspection.ReleasedResourceCount;
        It->Release = {};
    }
    Resources.clear();
}

} // namespace Stoner::Renderer::Private
