#include "Renderer/FDeferredFrameUniformResources.h"

#include "RHI/FRHIBufferDesc.h"
#include "RHI/FRHIBufferUploadDesc.h"
#include "RHI/IRHIBuffer.h"
#include "RHI/IRHIDescriptorSet.h"
#include "RHI/IRHIDevice.h"
#include "RHI/IRHIPipelineLayout.h"
#include "RHI/IRHITexture.h"

#include <algorithm>
#include <limits>
#include <new>
#include <utility>

namespace Stoner::Renderer
{
namespace
{

using namespace Stoner::Core;
using namespace Stoner::RHI;

void Fail(FString* OutReason, const char* Reason) noexcept
{
    if (OutReason)
    {
        try
        {
            *OutReason = Reason;
        }
        catch (...)
        {
            OutReason->Clear();
        }
    }
}

const FStaticModelDescriptorBindingResource* FindDescriptorBinding(
    const FStaticModelDrawResources& Resources,
    uint32 SetIndex, uint32 BindingSlot, uint32 ArrayIndex) noexcept
{
    const auto Found = std::find_if(
        Resources.DescriptorBindings.begin(), Resources.DescriptorBindings.end(),
        [SetIndex, BindingSlot, ArrayIndex](
            const FStaticModelDescriptorBindingResource& Candidate)
        {
            return Candidate.SetIndex == SetIndex &&
                Candidate.BindingSlot == BindingSlot &&
                Candidate.ArrayIndex == ArrayIndex;
        });
    return Found == Resources.DescriptorBindings.end() ? nullptr : &*Found;
}

struct FBufferClone
{
    const IRHIBuffer* Source = nullptr;
    uint32 DrawSlot = 0;
    bool bFrame = false;
    TSharedPtr<IRHIBuffer> Clone;
};

struct FBuildResult
{
    TArray<FDeferredSurfaceDrawBinding> SurfaceDraws;
    TArray<TSharedPtr<IRHIBuffer>> OwnedBuffers;
    TArray<TSharedPtr<IRHIDescriptorSet>> OwnedDescriptors;
    TArray<const IRHIBuffer*> MutableSourceBuffers;
    TArray<uint32> MutableDrawSlots;
    TArray<bool> MutableIsFrame;
    TArray<uint32> DrawSlots;
    TArray<FDeferredEntityIdentity> DrawIdentities;
    TSharedPtr<IRHIBuffer> FrameUniformBuffer;
};

bool IsValidBuffer(const TSharedPtr<IRHIBuffer>& Buffer) noexcept
{
    return Buffer && Buffer->GetLifecycleState() ==
        ERHIResourceLifecycleState::Valid && Buffer->GetSizeInBytes() > 0;
}

bool IsValidDescriptor(const TSharedPtr<IRHIDescriptorSet>& Descriptor) noexcept
{
    return Descriptor && Descriptor->GetLifecycleState() ==
        ERHIResourceLifecycleState::Valid &&
        Descriptor->GetPipelineLayout() &&
        Descriptor->GetPipelineLayout()->GetLifecycleState() ==
            ERHIResourceLifecycleState::Valid;
}

bool IsBufferDescriptorType(ERHIDescriptorType Type) noexcept
{
    return Type == ERHIDescriptorType::UniformBuffer ||
        Type == ERHIDescriptorType::StorageBuffer;
}

bool IsTextureDescriptorType(ERHIDescriptorType Type) noexcept
{
    return Type == ERHIDescriptorType::SampledTexture ||
        Type == ERHIDescriptorType::StorageTexture;
}

ERHIResult CreateClone(
    IRHIDevice& Device, const TSharedPtr<IRHIBuffer>& Source,
    uint32 RequiredBytes, TSharedPtr<IRHIBuffer>& OutClone,
    FString* OutReason)
{
    if (!IsValidBuffer(Source) || Source->GetSizeInBytes() < RequiredBytes)
    {
        Fail(OutReason, "source uniform buffer is invalid or too small");
        return ERHIResult::InvalidState;
    }

    FRHIBufferDesc Desc = Source->GetDesc();
    if (Desc.SizeInBytes < RequiredBytes || !IsValidRHIBufferDesc(Desc))
    {
        Fail(OutReason, "source uniform buffer descriptor is invalid");
        return ERHIResult::InvalidState;
    }
    Desc.Usage |= ERHIBufferUsage::CopyDestination;
    if (!IsValidRHIBufferDesc(Desc))
    {
        Fail(OutReason, "slot uniform buffer descriptor is invalid");
        return ERHIResult::InvalidState;
    }

    const auto Created = Device.CreateBuffer(Desc);
    if (!Created.Succeeded())
    {
        Fail(OutReason, "slot uniform buffer allocation failed");
        return Created.Result;
    }
    OutClone = Created.Object;
    return ERHIResult::Success;
}

ERHIResult UploadFrame(
    IRHIDevice& Device, const TSharedPtr<IRHIBuffer>& Buffer,
    const FDeferredFrameViewUniform& Uniform, FString* OutReason)
{
    const ERHIResult Result = Device.UploadBuffer(
        Buffer, {0, &Uniform, sizeof(Uniform)});
    if (Result != ERHIResult::Success)
        Fail(OutReason, "slot frame uniform upload failed");
    return Result;
}

ERHIResult UploadDraw(
    IRHIDevice& Device, const TSharedPtr<IRHIBuffer>& Buffer,
    const FDeferredDrawMaterialUniform& Uniform, FString* OutReason)
{
    const ERHIResult Result = Device.UploadBuffer(
        Buffer, {0, &Uniform, sizeof(Uniform)});
    if (Result != ERHIResult::Success)
        Fail(OutReason, "slot draw uniform upload failed");
    return Result;
}

ERHIResult GetOrCreateBuffer(
    IRHIDevice& Device,
    const TSharedPtr<IRHIBuffer>& Source,
    uint32 DrawSlot,
    bool bFrame,
    const FDeferredFrameViewUniform& FrameUniform,
    const FDeferredDrawMaterialUniform& DrawUniform,
    TArray<FBufferClone>& InOutClones,
    TArray<TSharedPtr<IRHIBuffer>>& InOutOwnedBuffers,
    TArray<const IRHIBuffer*>& InOutMutableSourceBuffers,
    TArray<uint32>& InOutMutableDrawSlots,
    TArray<bool>& InOutMutableIsFrame,
    TSharedPtr<IRHIBuffer>& OutClone,
    FString* OutReason)
{
    if (!IsValidBuffer(Source))
    {
        Fail(OutReason, "source descriptor buffer is invalid");
        return ERHIResult::InvalidState;
    }
    const auto Found = std::find_if(
        InOutClones.begin(), InOutClones.end(),
        [Source, DrawSlot, bFrame](const FBufferClone& Candidate)
        {
            return Candidate.Source == Source.get() &&
                Candidate.bFrame == bFrame &&
                (bFrame || Candidate.DrawSlot == DrawSlot);
        });
    if (Found != InOutClones.end())
    {
        OutClone = Found->Clone;
        return ERHIResult::Success;
    }

    TSharedPtr<IRHIBuffer> Clone;
    const uint32 RequiredBytes = bFrame
        ? static_cast<uint32>(sizeof(FDeferredFrameViewUniform))
        : static_cast<uint32>(sizeof(FDeferredDrawMaterialUniform));
    ERHIResult Result = CreateClone(
        Device, Source, RequiredBytes, Clone, OutReason);
    if (Result != ERHIResult::Success)
        return Result;
    Result = bFrame
        ? UploadFrame(Device, Clone, FrameUniform, OutReason)
        : UploadDraw(Device, Clone, DrawUniform, OutReason);
    if (Result != ERHIResult::Success)
    {
        (void)Clone->Invalidate();
        return Result;
    }
    bool bOwned = false;
    try
    {
        InOutOwnedBuffers.push_back(Clone);
        bOwned = true;
        InOutClones.push_back({Source.get(), bFrame ? 0U : DrawSlot,
            bFrame, Clone});
        InOutMutableSourceBuffers.push_back(Source.get());
        InOutMutableDrawSlots.push_back(DrawSlot);
        InOutMutableIsFrame.push_back(bFrame);
    }
    catch (const std::bad_alloc&)
    {
        if (!bOwned)
            (void)Clone->Invalidate();
        Fail(OutReason, "slot uniform ownership allocation failed");
        return ERHIResult::Unavailable;
    }
    catch (const std::length_error&)
    {
        if (!bOwned)
            (void)Clone->Invalidate();
        Fail(OutReason, "slot uniform ownership capacity exceeded");
        return ERHIResult::Unavailable;
    }
    OutClone = std::move(Clone);
    return ERHIResult::Success;
}

ERHIResult ResolveBindingResource(
    const FStaticModelDrawResources& Resources,
    const FRHIDescriptorBinding& LayoutBinding,
    uint32 ArrayIndex,
    TSharedPtr<IRHIBuffer>& OutSourceBuffer,
    TSharedPtr<IRHITexture>& OutTexture,
    TSharedPtr<IRHISampler>& OutSampler,
    FString* OutReason)
{
    const FStaticModelDescriptorBindingResource* Record =
        FindDescriptorBinding(Resources, LayoutBinding.SetIndex,
            LayoutBinding.BindingSlot, ArrayIndex);
    if (!Record)
    {
        Fail(OutReason, "descriptor binding record is missing");
        return ERHIResult::InvalidState;
    }
    if (Record->Kind == ERHIDescriptorResourceKind::Buffer)
        OutSourceBuffer = Record->Buffer;
    else if (Record->Kind == ERHIDescriptorResourceKind::Texture)
        OutTexture = Record->Texture;
    else if (Record->Kind == ERHIDescriptorResourceKind::Sampler)
        OutSampler = Record->Sampler;
    else if (Record->Kind == ERHIDescriptorResourceKind::CombinedTextureSampler)
    {
        OutTexture = Record->Texture;
        OutSampler = Record->Sampler;
    }
    if (Record->Kind == ERHIDescriptorResourceKind::None)
    {
        Fail(OutReason, "descriptor binding record is empty");
        return ERHIResult::InvalidState;
    }
    return ERHIResult::Success;
}

ERHIResult CloneDescriptor(
    IRHIDevice& Device,
    const FStaticModelDrawResources& Resources,
    const TSharedPtr<IRHIDescriptorSet>& Source,
    uint32 DrawSlot,
    const FDeferredFrameViewUniform& FrameUniform,
    const FDeferredDrawMaterialUniform& DrawUniform,
    TArray<FBufferClone>& InOutClones,
    TArray<TSharedPtr<IRHIBuffer>>& InOutOwnedBuffers,
    TArray<const IRHIBuffer*>& InOutMutableSourceBuffers,
    TArray<uint32>& InOutMutableDrawSlots,
    TArray<bool>& InOutMutableIsFrame,
    TArray<TSharedPtr<IRHIDescriptorSet>>& InOutOwnedDescriptors,
    TSharedPtr<IRHIDescriptorSet>& OutDescriptor,
    TSharedPtr<IRHIBuffer>& InOutFrameUniformBuffer,
    FString* OutReason)
{
    if (!IsValidDescriptor(Source))
    {
        Fail(OutReason, "source descriptor set is invalid");
        return ERHIResult::InvalidState;
    }
    const auto Layout = Source->GetPipelineLayout();
    const auto Created = Device.CreateDescriptorSet(Layout,
        Source->GetSetIndex());
    if (!Created.Succeeded())
    {
        Fail(OutReason, "slot descriptor set allocation failed");
        return Created.Result;
    }
    OutDescriptor = Created.Object;
    try
    {
        InOutOwnedDescriptors.push_back(OutDescriptor);
    }
    catch (const std::bad_alloc&)
    {
        (void)OutDescriptor->Invalidate();
        OutDescriptor.reset();
        Fail(OutReason, "slot descriptor ownership allocation failed");
        return ERHIResult::Unavailable;
    }
    catch (const std::length_error&)
    {
        (void)OutDescriptor->Invalidate();
        OutDescriptor.reset();
        Fail(OutReason, "slot descriptor ownership capacity exceeded");
        return ERHIResult::Unavailable;
    }

    for (const FRHIDescriptorBinding& Binding : Layout->GetDesc().Bindings)
    {
        if (Binding.SetIndex != Source->GetSetIndex())
            continue;
        for (uint32 ArrayIndex = 0; ArrayIndex < Binding.ArrayCount;
             ++ArrayIndex)
        {
            TSharedPtr<IRHIBuffer> SourceBuffer;
            TSharedPtr<IRHITexture> Texture;
            TSharedPtr<IRHISampler> Sampler;
            const ERHIResult ResolveResult = ResolveBindingResource(
                Resources, Binding, ArrayIndex, SourceBuffer,
                Texture, Sampler, OutReason);
            if (ResolveResult != ERHIResult::Success)
                return ResolveResult;

            ERHIResult UpdateResult = ERHIResult::Success;
            if (IsBufferDescriptorType(Binding.DescriptorType))
            {
                if (!SourceBuffer)
                {
                    Fail(OutReason, "mutable descriptor buffer is missing");
                    return ERHIResult::InvalidState;
                }
                const bool bFrame = Binding.SetIndex == 0 &&
                    Binding.BindingSlot == 0 &&
                    Binding.DescriptorType == ERHIDescriptorType::UniformBuffer;
                const bool bDraw = Binding.SetIndex == 1 &&
                    Binding.BindingSlot == 0 &&
                    Binding.DescriptorType == ERHIDescriptorType::UniformBuffer;
                if ((!bFrame && !bDraw) || Binding.ArrayCount != 1 ||
                    ArrayIndex != 0)
                {
                    Fail(OutReason,
                        "slot helper only supports single uniform buffers at set 0/1 binding 0");
                    return ERHIResult::Unsupported;
                }
                TSharedPtr<IRHIBuffer> Clone;
                UpdateResult = GetOrCreateBuffer(
                    Device, SourceBuffer, DrawSlot, bFrame, FrameUniform,
                    DrawUniform, InOutClones, InOutOwnedBuffers,
                    InOutMutableSourceBuffers, InOutMutableDrawSlots,
                    InOutMutableIsFrame, Clone, OutReason);
                if (UpdateResult == ERHIResult::Success)
                {
                    if (bFrame && !InOutFrameUniformBuffer)
                        InOutFrameUniformBuffer = Clone;
                    UpdateResult = OutDescriptor->UpdateBuffer(
                        Binding.BindingSlot, ArrayIndex, Clone);
                }
            }
            else if (IsTextureDescriptorType(Binding.DescriptorType))
            {
                if (!Texture)
                {
                    Fail(OutReason, "immutable texture descriptor is missing");
                    return ERHIResult::InvalidState;
                }
                UpdateResult = OutDescriptor->UpdateTexture(
                    Binding.BindingSlot, ArrayIndex, Texture);
            }
            else if (Binding.DescriptorType == ERHIDescriptorType::Sampler)
            {
                if (!Sampler)
                {
                    Fail(OutReason, "immutable sampler descriptor is missing");
                    return ERHIResult::InvalidState;
                }
                UpdateResult = OutDescriptor->UpdateSampler(
                    Binding.BindingSlot, ArrayIndex, Sampler);
            }
            else if (Binding.DescriptorType ==
                     ERHIDescriptorType::CombinedTextureSampler)
            {
                if (!Texture || !Sampler)
                {
                    Fail(OutReason,
                        "immutable combined descriptor is missing");
                    return ERHIResult::InvalidState;
                }
                UpdateResult = OutDescriptor->UpdateCombinedTextureSampler(
                    Binding.BindingSlot, ArrayIndex, Texture, Sampler);
            }
            else
            {
                Fail(OutReason, "descriptor type is unsupported");
                return ERHIResult::Unsupported;
            }
            if (UpdateResult != ERHIResult::Success)
            {
                Fail(OutReason, "slot descriptor binding update failed");
                return UpdateResult;
            }
        }
    }
    return ERHIResult::Success;
}

ERHIResult InspectDescriptorSetBindings(
    const FStaticModelDrawResources& Resources,
    const TSharedPtr<IRHIDescriptorSet>& Descriptor,
    bool& OutHasMutableBuffer,
    FString* OutReason) noexcept
{
    OutHasMutableBuffer = false;
    if (!Descriptor)
        return ERHIResult::InvalidState;
    const auto Layout = Descriptor->GetPipelineLayout();
    if (!Layout)
        return ERHIResult::InvalidState;
    for (const auto& Binding : Layout->GetDesc().Bindings)
    {
        if (Binding.SetIndex != Descriptor->GetSetIndex() ||
            !IsBufferDescriptorType(Binding.DescriptorType))
            continue;
        for (uint32 ArrayIndex = 0; ArrayIndex < Binding.ArrayCount;
             ++ArrayIndex)
        {
            const auto* Record = FindDescriptorBinding(Resources,
                Binding.SetIndex, Binding.BindingSlot, ArrayIndex);
            if (!Record || Record->Kind != ERHIDescriptorResourceKind::Buffer ||
                !IsValidBuffer(Record->Buffer))
            {
                Fail(OutReason, "descriptor buffer binding record is missing");
                return ERHIResult::InvalidState;
            }
            const bool bSupported =
                Binding.DescriptorType == ERHIDescriptorType::UniformBuffer &&
                Binding.BindingSlot == 0 &&
                (Binding.SetIndex == 0 || Binding.SetIndex == 1) &&
                Binding.ArrayCount == 1 && ArrayIndex == 0;
            if (!bSupported)
            {
                Fail(OutReason,
                    "slot helper cannot replace this descriptor buffer binding");
                return ERHIResult::Unsupported;
            }
            OutHasMutableBuffer = true;
        }
    }
    return ERHIResult::Success;
}

ERHIResult BuildUnchecked(
    const TSharedPtr<IRHIDevice>& Device,
    const TSharedPtr<const FStaticModelRenderSnapshot>& Snapshot,
    const FDeferredFramePlan& Plan,
    FBuildResult& Out,
    FString* OutReason)
{
    Out = {};
    if (!Device || !Device->IsActive() || !Snapshot)
    {
        Fail(OutReason, "slot helper requires an active device and snapshot");
        return ERHIResult::InvalidState;
    }
    if (!Plan.IsValid() || Plan.FrameId.IsEmpty() ||
        !Plan.View.IsValid() || !Plan.Output.IsValid(Plan.View) ||
        Plan.AcceptedDraws.empty())
    {
        Fail(OutReason, "deferred plan is invalid or has no accepted draws");
        return ERHIResult::InvalidState;
    }
    const auto& SnapshotDraws = Snapshot->GetDraws();
    const auto& Meshes = Snapshot->GetMeshes();
    const auto& Materials = Snapshot->GetMaterialResources();
    const auto& DrawResources = Snapshot->GetDrawResources();
    if (SnapshotDraws.empty() || SnapshotDraws.size() != DrawResources.size())
    {
        Fail(OutReason, "snapshot draw resource tables are inconsistent");
        return ERHIResult::InvalidState;
    }

    const FDeferredFrameViewUniform FrameUniform =
        BuildDeferredFrameViewUniform(Plan.View);
    TArray<FBufferClone> Clones;
    try
    {
        Out.SurfaceDraws.reserve(Plan.AcceptedDraws.size());
        Clones.reserve(Plan.AcceptedDraws.size() * 2);
        Out.OwnedBuffers.reserve(Plan.AcceptedDraws.size() * 2);
        Out.OwnedDescriptors.reserve(Plan.AcceptedDraws.size() * 2);
        Out.MutableSourceBuffers.reserve(Plan.AcceptedDraws.size() * 2);
        Out.MutableDrawSlots.reserve(Plan.AcceptedDraws.size() * 2);
        Out.MutableIsFrame.reserve(Plan.AcceptedDraws.size() * 2);
        Out.DrawSlots.reserve(Plan.AcceptedDraws.size());
        Out.DrawIdentities.reserve(Plan.AcceptedDraws.size());
    }
    catch (const std::bad_alloc&)
    {
        Fail(OutReason, "slot helper capacity reservation failed");
        return ERHIResult::Unavailable;
    }
    catch (const std::length_error&)
    {
        Fail(OutReason, "slot helper capacity limit exceeded");
        return ERHIResult::Unavailable;
    }

    TArray<uint32> SeenSlots;
    try
    {
        SeenSlots.reserve(Plan.AcceptedDraws.size());
    }
    catch (const std::bad_alloc&)
    {
        Fail(OutReason, "slot helper identity reservation failed");
        return ERHIResult::Unavailable;
    }

    for (const FDeferredDrawRecord& PlanDraw : Plan.AcceptedDraws)
    {
        const uint32 DrawSlot = PlanDraw.Candidate.Identity.Slot;
        if (!PlanDraw.bAccepted || !PlanDraw.Candidate.Identity.IsValid() ||
            DrawSlot == 0 || DrawSlot > SnapshotDraws.size() ||
            std::find(SeenSlots.begin(), SeenSlots.end(), DrawSlot) !=
                SeenSlots.end())
        {
            Fail(OutReason, "deferred plan draw identity is invalid or duplicated");
            return ERHIResult::InvalidState;
        }
        SeenSlots.push_back(DrawSlot);
        Out.DrawSlots.push_back(DrawSlot);
        Out.DrawIdentities.push_back(PlanDraw.Candidate.Identity);

        const FStaticModelRenderDraw& SnapshotDraw =
            SnapshotDraws[DrawSlot - 1];
        if (SnapshotDraw.MeshIndex >= Meshes.size() ||
            SnapshotDraw.PipelineIndex >= Materials.size() ||
            SnapshotDraw.SectionIndex >=
                Meshes[SnapshotDraw.MeshIndex].Sections.size())
        {
            Fail(OutReason, "snapshot draw identity resolves to invalid resources");
            return ERHIResult::InvalidState;
        }
        const auto& Mesh = Meshes[SnapshotDraw.MeshIndex];
        const auto& Material = Materials[SnapshotDraw.PipelineIndex];
        const auto& Resources = DrawResources[DrawSlot - 1];
        if (!Mesh.VertexBuffer || !Mesh.IndexBuffer || !Material.Pipeline ||
            !Material.PipelineLayout ||
            Material.Pipeline->GetLifecycleState() !=
                ERHIResourceLifecycleState::Valid ||
            Material.PipelineLayout->GetLifecycleState() !=
                ERHIResourceLifecycleState::Valid ||
            Material.Pipeline->GetPipelineLayout().get() !=
                Material.PipelineLayout.get())
        {
            Fail(OutReason, "snapshot draw resource is incomplete");
            return ERHIResult::InvalidState;
        }

        FDeferredSurfaceDrawBinding Binding;
        Binding.VertexBuffer = Mesh.VertexBuffer;
        Binding.IndexBuffer = Mesh.IndexBuffer;
        Binding.IndexType = Mesh.IndexType;
        Binding.Draw = MakeStaticMeshSectionDrawArguments(
            Mesh.Sections[SnapshotDraw.SectionIndex]);
        Binding.Pipeline = Material.Pipeline;
        if (!IsValidRHIIndexedDrawArguments(Binding.Draw))
        {
            Fail(OutReason, "snapshot draw arguments are invalid");
            return ERHIResult::InvalidState;
        }

        const FDeferredDrawMaterialUniform DrawUniform =
            BuildDeferredDrawMaterialUniform(PlanDraw);
        for (const auto& SourceDescriptor : Resources.DescriptorSets)
        {
            if (!IsValidDescriptor(SourceDescriptor))
            {
                Fail(OutReason, "snapshot draw descriptor is incomplete");
                return ERHIResult::InvalidState;
            }
            bool bHasMutableBuffer = false;
            const ERHIResult InspectionResult = InspectDescriptorSetBindings(
                Resources, SourceDescriptor, bHasMutableBuffer, OutReason);
            if (InspectionResult != ERHIResult::Success)
                return InspectionResult;
            if (!bHasMutableBuffer)
            {
                Binding.DescriptorSets.push_back(SourceDescriptor);
                continue;
            }

            TSharedPtr<IRHIDescriptorSet> SlotDescriptor;
            const ERHIResult DescriptorResult = CloneDescriptor(
                *Device, Resources, SourceDescriptor, DrawSlot, FrameUniform,
                DrawUniform, Clones, Out.OwnedBuffers,
                Out.MutableSourceBuffers, Out.MutableDrawSlots,
                Out.MutableIsFrame, Out.OwnedDescriptors, SlotDescriptor,
                Out.FrameUniformBuffer, OutReason);
            if (DescriptorResult != ERHIResult::Success)
                return DescriptorResult;
            Binding.DescriptorSets.push_back(std::move(SlotDescriptor));
        }
        if (Binding.DescriptorSets.empty())
        {
            Fail(OutReason, "snapshot draw has no descriptor sets");
            return ERHIResult::InvalidState;
        }
        Out.SurfaceDraws.push_back(std::move(Binding));
    }
    if (!Out.FrameUniformBuffer)
    {
        Fail(OutReason, "snapshot draw descriptors have no frame uniform binding");
        return ERHIResult::InvalidState;
    }
    return ERHIResult::Success;
}

void InvalidateBuildResources(FBuildResult& Resources) noexcept
{
    for (auto It = Resources.OwnedDescriptors.rbegin();
         It != Resources.OwnedDescriptors.rend(); ++It)
    {
        if (*It)
        {
            try
            {
                (void)(*It)->Invalidate();
            }
            catch (...)
            {
            }
        }
    }
    Resources.SurfaceDraws.clear();
    for (auto It = Resources.OwnedBuffers.rbegin();
         It != Resources.OwnedBuffers.rend(); ++It)
    {
        if (*It)
        {
            try
            {
                (void)(*It)->Invalidate();
            }
            catch (...)
            {
            }
        }
    }
    Resources.OwnedDescriptors.clear();
    Resources.OwnedBuffers.clear();
    Resources.MutableSourceBuffers.clear();
    Resources.MutableDrawSlots.clear();
    Resources.MutableIsFrame.clear();
    Resources.DrawSlots.clear();
    Resources.DrawIdentities.clear();
    Resources.FrameUniformBuffer.reset();
}

ERHIResult Build(
    const TSharedPtr<IRHIDevice>& Device,
    const TSharedPtr<const FStaticModelRenderSnapshot>& Snapshot,
    const FDeferredFramePlan& Plan,
    FBuildResult& Out,
    FString* OutReason) noexcept
{
    try
    {
        const ERHIResult Result = BuildUnchecked(
            Device, Snapshot, Plan, Out, OutReason);
        if (Result != ERHIResult::Success)
            InvalidateBuildResources(Out);
        return Result;
    }
    catch (const std::bad_alloc&)
    {
        InvalidateBuildResources(Out);
        Fail(OutReason, "slot helper allocation failed");
        return ERHIResult::Unavailable;
    }
    catch (const std::length_error&)
    {
        InvalidateBuildResources(Out);
        Fail(OutReason, "slot helper capacity exceeded");
        return ERHIResult::Unavailable;
    }
    catch (...)
    {
        InvalidateBuildResources(Out);
        Fail(OutReason, "slot helper build failed");
        return ERHIResult::Unavailable;
    }
}

bool HasBufferSource(
    const FStaticModelDrawResources& Resources,
    uint32 SetIndex, uint32 BindingSlot, const IRHIBuffer* Source) noexcept
{
    if (!Source)
        return false;
    for (const auto& Binding : Resources.DescriptorBindings)
    {
        if (Binding.SetIndex == SetIndex &&
            Binding.BindingSlot == BindingSlot &&
            Binding.Kind == ERHIDescriptorResourceKind::Buffer &&
            Binding.Buffer.get() == Source)
            return true;
    }
    return false;
}

ERHIResult ValidateUpdatePlan(
    const TSharedPtr<const FStaticModelRenderSnapshot>& Snapshot,
    const FDeferredFramePlan& Plan,
    const TArray<FDeferredSurfaceDrawBinding>& SurfaceDraws,
    const TArray<uint32>& DrawSlots,
    const TArray<FDeferredEntityIdentity>& DrawIdentities,
    const TArray<const IRHIBuffer*>& MutableSourceBuffers,
    const TArray<uint32>& MutableDrawSlots,
    const TArray<bool>& MutableIsFrame,
    const TArray<TSharedPtr<IRHIBuffer>>& OwnedBuffers,
    FString* OutReason) noexcept
{
    if (!Snapshot || !Plan.IsValid() || Plan.FrameId.IsEmpty() ||
        !Plan.View.IsValid() || !Plan.Output.IsValid(Plan.View) ||
        Plan.AcceptedDraws.empty() ||
        Plan.AcceptedDraws.size() != SurfaceDraws.size() ||
        Plan.AcceptedDraws.size() != DrawSlots.size() ||
        Plan.AcceptedDraws.size() != DrawIdentities.size())
    {
        Fail(OutReason, "slot update plan is invalid or changes the draw mapping");
        return ERHIResult::InvalidState;
    }
    const auto& SnapshotDraws = Snapshot->GetDraws();
    const auto& Meshes = Snapshot->GetMeshes();
    const auto& Materials = Snapshot->GetMaterialResources();
    const auto& DrawResources = Snapshot->GetDrawResources();
    if (SnapshotDraws.empty() || SnapshotDraws.size() != DrawResources.size() ||
        MutableSourceBuffers.size() != OwnedBuffers.size() ||
        MutableSourceBuffers.size() != MutableDrawSlots.size() ||
        MutableSourceBuffers.size() != MutableIsFrame.size())
    {
        Fail(OutReason, "slot update ownership mapping is inconsistent");
        return ERHIResult::InvalidState;
    }

    for (std::size_t Index = 0; Index < Plan.AcceptedDraws.size(); ++Index)
    {
        const FDeferredDrawRecord& PlanDraw = Plan.AcceptedDraws[Index];
        const uint32 DrawSlot = PlanDraw.Candidate.Identity.Slot;
        if (!PlanDraw.bAccepted || !PlanDraw.Candidate.Identity.IsValid() ||
            DrawSlot == 0 || DrawSlot > SnapshotDraws.size() ||
            DrawSlot != DrawSlots[Index] ||
            !(PlanDraw.Candidate.Identity == DrawIdentities[Index]))
        {
            Fail(OutReason, "slot update draw identity does not match initialization");
            return ERHIResult::InvalidState;
        }
        for (std::size_t Previous = 0; Previous < Index; ++Previous)
        {
            if (DrawSlots[Previous] == DrawSlot)
            {
                Fail(OutReason, "slot update draw identity is duplicated");
                return ERHIResult::InvalidState;
            }
        }
        const FStaticModelRenderDraw& SnapshotDraw =
            SnapshotDraws[DrawSlot - 1];
        if (SnapshotDraw.MeshIndex >= Meshes.size() ||
            SnapshotDraw.PipelineIndex >= Materials.size() ||
            SnapshotDraw.SectionIndex >=
                Meshes[SnapshotDraw.MeshIndex].Sections.size())
        {
            Fail(OutReason, "slot update resolves to an invalid snapshot draw");
            return ERHIResult::InvalidState;
        }
        const auto& Mesh = Meshes[SnapshotDraw.MeshIndex];
        const auto& Material = Materials[SnapshotDraw.PipelineIndex];
        const auto& Existing = SurfaceDraws[Index];
        if (!Mesh.VertexBuffer || !Mesh.IndexBuffer || !Material.Pipeline ||
            Existing.VertexBuffer != Mesh.VertexBuffer ||
            Existing.IndexBuffer != Mesh.IndexBuffer ||
            Existing.Pipeline != Material.Pipeline ||
            Existing.DescriptorSets.empty())
        {
            Fail(OutReason, "slot update immutable draw mapping changed");
            return ERHIResult::InvalidState;
        }
        for (const auto& Descriptor : Existing.DescriptorSets)
        {
            if (!IsValidDescriptor(Descriptor))
            {
                Fail(OutReason, "slot update descriptor ownership is invalid");
                return ERHIResult::InvalidState;
            }
        }
    }

    for (std::size_t Index = 0; Index < MutableSourceBuffers.size(); ++Index)
    {
        const IRHIBuffer* Source = MutableSourceBuffers[Index];
        const uint32 DrawSlot = MutableDrawSlots[Index];
        if (!Source || !IsValidBuffer(OwnedBuffers[Index]))
        {
            Fail(OutReason, "slot update mutable buffer ownership is invalid");
            return ERHIResult::InvalidState;
        }
        if (MutableIsFrame[Index])
        {
            bool bFound = false;
            for (const auto& Resources : DrawResources)
                bFound = bFound || HasBufferSource(Resources, 0, 0, Source);
            if (!bFound)
            {
                Fail(OutReason, "slot update frame buffer mapping changed");
                return ERHIResult::InvalidState;
            }
        }
        else
        {
            const auto SlotIt = std::find(
                DrawSlots.begin(), DrawSlots.end(), DrawSlot);
            if (SlotIt == DrawSlots.end() || DrawSlot == 0 ||
                DrawSlot > DrawResources.size() ||
                !HasBufferSource(DrawResources[DrawSlot - 1], 1, 0, Source))
            {
                Fail(OutReason, "slot update draw buffer mapping changed");
                return ERHIResult::InvalidState;
            }
        }
    }
    return ERHIResult::Success;
}

} // namespace

FDeferredFrameUniformResources::~FDeferredFrameUniformResources()
{
    Release();
}

ERHIResult FDeferredFrameUniformResources::Initialize(
    const TSharedPtr<RHI::IRHIDevice>& Device,
    const TSharedPtr<const FStaticModelRenderSnapshot>& Snapshot,
    const FDeferredFramePlan& Plan,
    Core::FString* OutReason)
{
    if (OutReason)
        OutReason->Clear();
    FBuildResult Candidate;
    const ERHIResult Result = Build(Device, Snapshot, Plan, Candidate, OutReason);
    if (Result != ERHIResult::Success)
        return Result;
    try
    {
        InvalidateMutableResources();
        Device_ = Device;
        Snapshot_ = Snapshot;
        SurfaceDraws_ = std::move(Candidate.SurfaceDraws);
        OwnedBuffers_ = std::move(Candidate.OwnedBuffers);
        OwnedDescriptors_ = std::move(Candidate.OwnedDescriptors);
        MutableSourceBuffers_ = std::move(Candidate.MutableSourceBuffers);
        MutableDrawSlots_ = std::move(Candidate.MutableDrawSlots);
        MutableIsFrame_ = std::move(Candidate.MutableIsFrame);
        DrawSlots_ = std::move(Candidate.DrawSlots);
        DrawIdentities_ = std::move(Candidate.DrawIdentities);
        FrameUniformBuffer_ = std::move(Candidate.FrameUniformBuffer);
        FrameId_ = Plan.FrameId;
        bValid_ = true;
    }
    catch (const std::bad_alloc&)
    {
        InvalidateMutableResources();
        InvalidateBuildResources(Candidate);
        Fail(OutReason, "slot helper publication allocation failed");
        return ERHIResult::Unavailable;
    }
    catch (const std::length_error&)
    {
        InvalidateMutableResources();
        InvalidateBuildResources(Candidate);
        Fail(OutReason, "slot helper publication capacity exceeded");
        return ERHIResult::Unavailable;
    }
    catch (...)
    {
        InvalidateMutableResources();
        InvalidateBuildResources(Candidate);
        Fail(OutReason, "slot helper publication failed");
        return ERHIResult::Unavailable;
    }
    return ERHIResult::Success;
}

ERHIResult FDeferredFrameUniformResources::Update(
    const TSharedPtr<RHI::IRHIDevice>& Device,
    const FDeferredFramePlan& Plan,
    Core::FString* OutReason)
{
    if (OutReason)
        OutReason->Clear();
    if (!bValid_ || !Device || !Device->IsActive() || !Device_ ||
        !Snapshot_ || Device.get() != Device_.get())
    {
        Fail(OutReason, "slot helper update device does not match initialization");
        return ERHIResult::InvalidState;
    }
    const ERHIResult Result = ValidateUpdatePlan(
        Snapshot_, Plan, SurfaceDraws_, DrawSlots_, DrawIdentities_,
        MutableSourceBuffers_, MutableDrawSlots_, MutableIsFrame_,
        OwnedBuffers_, OutReason);
    if (Result != ERHIResult::Success)
        return Result;

    try
    {
        const FDeferredFrameViewUniform FrameUniform =
            BuildDeferredFrameViewUniform(Plan.View);
        for (std::size_t Index = 0;
             Index < MutableSourceBuffers_.size(); ++Index)
        {
            const bool bFrame = MutableIsFrame_[Index];
            ERHIResult UploadResult = ERHIResult::Success;
            if (bFrame)
            {
                UploadResult = UploadFrame(
                    *Device, OwnedBuffers_[Index], FrameUniform, OutReason);
            }
            else
            {
                const auto DrawIt = std::find_if(
                    Plan.AcceptedDraws.begin(), Plan.AcceptedDraws.end(),
                    [Slot = MutableDrawSlots_[Index]](const auto& Draw)
                    { return Draw.Candidate.Identity.Slot == Slot; });
                if (DrawIt == Plan.AcceptedDraws.end())
                {
                    Fail(OutReason, "slot update draw buffer mapping changed");
                    return ERHIResult::InvalidState;
                }
                const FDeferredDrawMaterialUniform DrawUniform =
                    BuildDeferredDrawMaterialUniform(*DrawIt);
                UploadResult = UploadDraw(
                    *Device, OwnedBuffers_[Index], DrawUniform, OutReason);
            }
            if (UploadResult != ERHIResult::Success)
            {
                InvalidateMutableResources();
                return UploadResult;
            }
        }
        FrameId_ = Plan.FrameId;
    }
    catch (const std::bad_alloc&)
    {
        InvalidateMutableResources();
        Fail(OutReason, "slot helper update publication allocation failed");
        return ERHIResult::Unavailable;
    }
    catch (const std::length_error&)
    {
        InvalidateMutableResources();
        Fail(OutReason, "slot helper update publication capacity exceeded");
        return ERHIResult::Unavailable;
    }
    catch (...)
    {
        InvalidateMutableResources();
        Fail(OutReason, "slot helper update failed");
        return ERHIResult::Unavailable;
    }
    return ERHIResult::Success;
}

bool FDeferredFrameUniformResources::IsValid() const noexcept
{
    return bValid_ && Device_ && Snapshot_ && !FrameId_.IsEmpty() &&
        FrameUniformBuffer_ && !SurfaceDraws_.empty();
}

const Core::TArray<FDeferredSurfaceDrawBinding>&
FDeferredFrameUniformResources::GetSurfaceDraws() const noexcept
{
    return SurfaceDraws_;
}

const Core::TArray<Core::TSharedPtr<RHI::IRHIBuffer>>&
FDeferredFrameUniformResources::GetOwnedBuffers() const noexcept
{
    return OwnedBuffers_;
}

const Core::TSharedPtr<RHI::IRHIBuffer>&
FDeferredFrameUniformResources::GetFrameUniformBuffer() const noexcept
{
    return FrameUniformBuffer_;
}

void FDeferredFrameUniformResources::Release() noexcept
{
    InvalidateMutableResources();
    Snapshot_.reset();
    Device_.reset();
}

void FDeferredFrameUniformResources::InvalidateMutableResources() noexcept
{
    bValid_ = false;
    for (auto It = OwnedDescriptors_.rbegin();
         It != OwnedDescriptors_.rend(); ++It)
    {
        if (*It)
        {
            try
            {
                (void)(*It)->Invalidate();
            }
            catch (...)
            {
            }
        }
    }
    OwnedDescriptors_.clear();
    SurfaceDraws_.clear();
    for (auto It = OwnedBuffers_.rbegin();
         It != OwnedBuffers_.rend(); ++It)
    {
        if (*It)
        {
            try
            {
                (void)(*It)->Invalidate();
            }
            catch (...)
            {
            }
        }
    }
    OwnedBuffers_.clear();
    MutableSourceBuffers_.clear();
    MutableDrawSlots_.clear();
    MutableIsFrame_.clear();
    DrawSlots_.clear();
    DrawIdentities_.clear();
    FrameUniformBuffer_.reset();
    FrameId_.Clear();
}

} // namespace Stoner::Renderer
