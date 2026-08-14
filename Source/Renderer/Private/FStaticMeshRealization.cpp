#include "Renderer/FStaticMeshRealization.h"

#include "Asset/FStaticMeshAsset.h"
#include "RHI/FRHIBufferDesc.h"
#include "RHI/FRHIBufferUploadDesc.h"
#include "RHI/IRHIBuffer.h"
#include "RHI/IRHIDevice.h"

#include <new>
#include <stdexcept>
#include <utility>

namespace Stoner::Renderer
{
namespace
{

FStaticMeshRealizationResult Failure(
    EStaticMeshRealizationStage Stage,
    Stoner::RHI::ERHIResult Result,
    const Stoner::Core::FString& Identity,
    const char* Code,
    const Stoner::Core::FString& Reason,
    std::optional<Stoner::Core::FString> Primitive = std::nullopt)
{
    FStaticMeshRealizationResult Value;
    Value.Result = Result;
    Value.Diagnostic = {
        Stage, Result, Identity, std::move(Primitive),
        Stoner::Core::FString(Code), Reason};
    return Value;
}

void Rollback(
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer>& Vertex,
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer>& Index)
{
    if (Index)
    {
        (void)Index->Invalidate();
    }
    if (Vertex)
    {
        (void)Vertex->Invalidate();
    }
}

} // namespace

FStaticMeshRealizationResult FStaticMeshRealizer::Realize(
    const FStaticMeshRealizationRequest& Request)
{
    using namespace Stoner::RHI;
    const Stoner::Core::FString Identity = Request.Asset
        ? Request.Asset->GetDesc().Id.ToString()
        : Stoner::Core::FString("<missing>");
    if (!Request.Device || !Request.Device->IsActive())
    {
        return Failure(
            EStaticMeshRealizationStage::ValidateAsset,
            ERHIResult::InvalidState, Identity,
            "StaticMeshRealization.DeviceInactive",
            Stoner::Core::FString("device is missing or inactive"));
    }
    if (!Request.Asset || Request.Asset->GetDesc().Primitives.empty() ||
        Request.Asset->GetDesc().SourceManifest.empty())
    {
        return Failure(
            EStaticMeshRealizationStage::ValidateAsset,
            ERHIResult::InvalidState, Identity,
            "StaticMeshRealization.AssetInvalid",
            Stoner::Core::FString("mesh payload or source evidence is missing"));
    }

    FStaticMeshPackingPlan Plan;
    Stoner::Core::FString PlanReason;
    const ERHIResult PlanResult = BuildStaticMeshPackingPlan(
        *Request.Asset, Request.Profile, Plan, &PlanReason);
    if (PlanResult != ERHIResult::Success)
    {
        return Failure(
            EStaticMeshRealizationStage::Plan, PlanResult, Identity,
            "StaticMeshRealization.PlanFailed", PlanReason);
    }

    FRHIBufferDesc VertexDesc{
        Plan.VertexBytes.size(),
        ERHIBufferUsage::Vertex | ERHIBufferUsage::CopyDestination,
        ERHIMemoryAccess::DeviceLocal};
    FRHIBufferDesc IndexDesc{
        Plan.IndexBytes.size(),
        ERHIBufferUsage::Index | ERHIBufferUsage::CopyDestination,
        ERHIMemoryAccess::DeviceLocal};
    auto Vertex = Request.Device->CreateBuffer(VertexDesc);
    if (!Vertex.Succeeded())
    {
        return Failure(
            EStaticMeshRealizationStage::Allocate, Vertex.Result, Identity,
            "StaticMeshRealization.VertexAllocateFailed",
            Stoner::Core::FString("RHI vertex buffer allocation failed"));
    }
    auto Index = Request.Device->CreateBuffer(IndexDesc);
    if (!Index.Succeeded())
    {
        Rollback(Vertex.Object, nullptr);
        return Failure(
            EStaticMeshRealizationStage::Allocate, Index.Result, Identity,
            "StaticMeshRealization.IndexAllocateFailed",
            Stoner::Core::FString("RHI index buffer allocation failed"));
    }

    ERHIResult Transfer = Request.Device->UploadBuffer(
        Vertex.Object,
        {0, Plan.VertexBytes.data(), Plan.VertexBytes.size()});
    if (Transfer != ERHIResult::Success)
    {
        Rollback(Vertex.Object, Index.Object);
        return Failure(
            EStaticMeshRealizationStage::Upload, Transfer, Identity,
            "StaticMeshRealization.VertexUploadFailed",
            Stoner::Core::FString("synchronous vertex upload failed"));
    }
    Transfer = Request.Device->UploadBuffer(
        Index.Object,
        {0, Plan.IndexBytes.data(), Plan.IndexBytes.size()});
    if (Transfer != ERHIResult::Success)
    {
        Rollback(Vertex.Object, Index.Object);
        return Failure(
            EStaticMeshRealizationStage::Upload, Transfer, Identity,
            "StaticMeshRealization.IndexUploadFailed",
            Stoner::Core::FString("synchronous index upload failed"));
    }

    if (Vertex.Object->GetLifecycleState() !=
            ERHIResourceLifecycleState::Valid ||
        Index.Object->GetLifecycleState() !=
            ERHIResourceLifecycleState::Valid ||
        !IsValidRHIVertexInputDesc(Plan.VertexInput) ||
        Plan.Sections.empty() || !Plan.ProfileDigest.IsAvailable())
    {
        Rollback(Vertex.Object, Index.Object);
        return Failure(
            EStaticMeshRealizationStage::Finalize,
            ERHIResult::Failed, Identity,
            "StaticMeshRealization.FinalizeFailed",
            Stoner::Core::FString("uploaded resources or snapshot evidence are invalid"));
    }

    Stoner::Core::TSharedPtr<FStaticMeshAssetSnapshot> Snapshot;
    try
    {
        Snapshot = Stoner::Core::MakeShared<FStaticMeshAssetSnapshot>();
        Snapshot->SourceManifest = Request.Asset->GetDesc().SourceManifest;
    }
    catch (const std::bad_alloc&)
    {
        Rollback(Vertex.Object, Index.Object);
        return Failure(
            EStaticMeshRealizationStage::Finalize,
            ERHIResult::Unavailable, Identity,
            "StaticMeshRealization.SnapshotAllocationFailed",
            Stoner::Core::FString("snapshot allocation failed"));
    }
    catch (const std::length_error&)
    {
        Rollback(Vertex.Object, Index.Object);
        return Failure(
            EStaticMeshRealizationStage::Finalize,
            ERHIResult::Unavailable, Identity,
            "StaticMeshRealization.SnapshotCapacityExceeded",
            Stoner::Core::FString("snapshot source evidence exceeds container limits"));
    }
    if (Stoner::Asset::NormalizeSourceManifest(Snapshot->SourceManifest) !=
        Stoner::Asset::EAssetResult::Success)
    {
        Rollback(Vertex.Object, Index.Object);
        return Failure(
            EStaticMeshRealizationStage::Finalize,
            ERHIResult::InvalidState, Identity,
            "StaticMeshRealization.ManifestInvalid",
            Stoner::Core::FString("source manifest is not normalized"));
    }
    Snapshot->VertexBuffer = std::move(Vertex.Object);
    Snapshot->IndexBuffer = std::move(Index.Object);
    Snapshot->VertexInput = std::move(Plan.VertexInput);
    Snapshot->IndexType = Plan.IndexType;
    Snapshot->Sections = std::move(Plan.Sections);
    Snapshot->Bounds = Request.Asset->GetDesc().Bounds;
    Snapshot->RealizationProfileDigest = Plan.ProfileDigest;

    FStaticMeshRealizationResult Result;
    Result.Result = ERHIResult::Success;
    Result.Snapshot = std::move(Snapshot);
    Result.Diagnostic = {
        EStaticMeshRealizationStage::Publish,
        ERHIResult::Success,
        Identity,
        std::nullopt,
        Stoner::Core::FString("StaticMeshRealization.Success"),
        Stoner::Core::FString("draw-ready snapshot published")};
    return Result;
}

bool FStaticMeshRealizationResult::Succeeded() const noexcept
{
    return Result == Stoner::RHI::ERHIResult::Success && Snapshot != nullptr;
}

const char* ToString(EStaticMeshRealizationStage Stage) noexcept
{
    switch (Stage)
    {
    case EStaticMeshRealizationStage::ValidateAsset: return "ValidateAsset";
    case EStaticMeshRealizationStage::Plan: return "Plan";
    case EStaticMeshRealizationStage::Allocate: return "Allocate";
    case EStaticMeshRealizationStage::Upload: return "Upload";
    case EStaticMeshRealizationStage::Finalize: return "Finalize";
    case EStaticMeshRealizationStage::Publish: return "Publish";
    }
    return "Unknown";
}

} // namespace Stoner::Renderer
