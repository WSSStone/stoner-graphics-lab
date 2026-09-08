#include "FUITextureRegistry.h"
#include "RHI/FRHIBufferUploadDesc.h"
#include "RHI/IRHITexture.h"
#include <algorithm>
#include <limits>
#include <new>
#include <utility>

namespace Stoner::Renderer
{
using namespace Stoner::Core;
using namespace Stoner::RHI;
struct FUITextureRecord
{
    FUITextureId Id;
    EUITextureColorDomain ColorDomain = EUITextureColorDomain::AlphaCoverage;
    EUITextureState State = EUITextureState::Prepared;
    bool bRetiring = false, bGpuOwned = false;
    FUIGpuTextureRegistration Gpu;
    FUIGpuTextureContext Identity;
    uint32 Width = 0, Height = 0, RowTexels = 0;
    TSharedPtr<IRHITexture> Texture;
    TSharedPtr<IRHIBuffer> Staging;
    TArray<uint8> Shadow;
    uint64 TextureBytes = 0, StagingBytes = 0;
    struct FUse
    {
        bool bReserved = false, bSubmitted = false, bUpload = false;
        TSharedPtr<IRHIFence> Fence;
    };
    std::array<FUse, 2> Uses{};
};
namespace
{
constexpr uint64 ByteBudget = 64ull * 1024 * 1024;
constexpr uint64 FramePayloadBudget = 16ull * 1024 * 1024;
bool HasUses(const FUITextureRecord& Record)
{
    return std::any_of(Record.Uses.begin(), Record.Uses.end(),
        [](const auto& Use) { return Use.bReserved; });
}
bool ValidGpuDependency(const FUIGpuTextureRegistration& Request, const FUIGpuTextureContext& Context)
{
    if (!Context.Graph || !Context.FrameId || !Context.SettingsRevision || !Context.DisplayGeneration ||
        Context.Graph->GetState()!=ERenderGraphState::Compiled || !Request.Texture) return false;
    const auto& Graph=*Context.Graph;
    const auto* Resource=Graph.FindResource(Request.Resource);
    const auto* Producer=Graph.FindPass(Request.Producer);
    const auto* Consumer=Graph.FindPass(Context.Consumer);
    if (!Resource || !Producer || !Consumer || Request.Producer==Context.Consumer) return false;
    const auto& Desc=Request.Texture->GetDesc();
    if (Desc.Dimension!=ERHITextureDimension::Texture2D || Desc.Width==0 || Desc.Height==0 ||
        Desc.Width>1024 || Desc.Height>1024 || Desc.Depth!=1 || Desc.MipLevels!=1 || Desc.ArrayLayers!=1 ||
        Desc.SampleCount!=ERHISampleCount::One || Desc.Format!=ERHIFormat::R8G8B8A8_sRGB ||
        !HasRHIFlag(Desc.Usage,ERHITextureUsage::Sampled) || !HasRHIFlag(Desc.Usage,ERHITextureUsage::ColorAttachment) ||
        Resource->Desc.Kind!=ERenderGraphResourceKind::Texture || Resource->Desc.Depth!=1 ||
        Resource->Desc.Width!=Desc.Width || Resource->Desc.Height!=Desc.Height ||
        Resource->Desc.Texture.SampleCount!=Desc.SampleCount ||
        !HasRHIFlag(Resource->Desc.Texture.Usage,ERHITextureUsage::Sampled) ||
        !HasRHIFlag(Resource->Desc.Texture.Usage,ERHITextureUsage::ColorAttachment) ||
        !Resource->Desc.Texture.bIsTyped || Resource->Desc.Texture.Format!=Desc.Format ||
        Resource->Desc.Texture.ColorDomain!=ERenderGraphColorDomain::EncodedSrgb) return false;
    const auto& Compiled=Graph.GetCompiledGraph();
    const auto Before=std::find(Compiled.ScheduledPasses.begin(),Compiled.ScheduledPasses.end(),Request.Producer.Index);
    const auto After=std::find(Compiled.ScheduledPasses.begin(),Compiled.ScheduledPasses.end(),Context.Consumer.Index);
    if (!Compiled.IsExecutable() || Before==Compiled.ScheduledPasses.end() || After==Compiled.ScheduledPasses.end() || Before>=After)
        return false;
    const auto Writes=std::any_of(Producer->Desc.Accesses.begin(),Producer->Desc.Accesses.end(),[&](const auto& A) {
        return A.Resource==Request.Resource && WritesResource(A.Access); });
    const auto Reads=std::any_of(Consumer->Desc.Accesses.begin(),Consumer->Desc.Accesses.end(),[&](const auto& A) {
        return A.Resource==Request.Resource && A.Access==ERenderGraphAccessType::Read; });
    if (!Writes || !Reads) return false;
    // A different writer between the selected producer and sample changes the
    // resource generation, even if the original producer still precedes UI.
    for (auto It=Before+1; It!=After; ++It)
        for (const auto& A : Graph.GetPasses()[*It].Desc.Accesses)
            if (A.Resource==Request.Resource && WritesResource(A.Access)) return false;
    return std::any_of(Compiled.DependencyEdges.begin(),Compiled.DependencyEdges.end(),[&](const auto& Edge) {
        return Edge.Resource==Request.Resource && Edge.FromPassIndex==Request.Producer.Index &&
            Edge.ToPassIndex==Context.Consumer.Index; });
}
FUITextureResult Result(const FUITextureRequest& Request, ERHIResult Code,
    EUITextureState State, FUITextureId Id = {})
{
    return {Request.RequestId, Code, State, Id, ToString(State)};
}
}
TSharedPtr<IRHITexture> FUITextureRegistry::ResolveTexture(const FUITextureLease& Lease) const noexcept
{
    if (!Lease.Record || Find(Lease.GetId()) != Lease.Record) return {};
    return Lease.Record->Texture;
}
bool FUITextureLease::IsValid() const noexcept { return Record && Record->Texture; }
FUITextureId FUITextureLease::GetId() const noexcept { return Record ? Record->Id : FUITextureId{}; }
FUITextureSubmission::~FUITextureSubmission() { Cancel(); }
FUITextureSubmission::FUITextureSubmission(FUITextureSubmission&& Other) noexcept
    : Entries(std::move(Other.Entries)), bCommitted(std::exchange(Other.bCommitted, false))
{
    Other.Entries.clear();
}
FUITextureSubmission& FUITextureSubmission::operator=(FUITextureSubmission&& Other) noexcept
{
    if (this != &Other)
    {
        Cancel(); Entries = std::move(Other.Entries);
        bCommitted = std::exchange(Other.bCommitted, false); Other.Entries.clear();
    }
    return *this;
}
void FUITextureSubmission::Cancel() noexcept
{
    if (!bCommitted)
        for (const auto& Entry : Entries) Entry.Lease.Record->Uses[Entry.UseIndex] = {};
    Entries.clear(); bCommitted = false;
}
ERHIResult FUITextureSubmission::Commit(const TSharedPtr<IRHIFence>& Fence) noexcept
{
    if (bCommitted || !Fence) return ERHIResult::InvalidState;
    for (const auto& Entry : Entries)
    {
        auto& Record = *Entry.Lease.Record;
        auto& Use = Record.Uses[Entry.UseIndex];
        Use.Fence = Fence; Use.bSubmitted = true;
        if (Use.bUpload) Record.State = EUITextureState::UploadQueued;
    }
    bCommitted = true;
    return ERHIResult::Success;
}
FUITextureRegistry::FUITextureRegistry(TSharedPtr<IRHIDevice> InDevice)
    : Device(std::move(InDevice)) {}
void FUITextureRegistry::BeginEligibleFrame(uint64 FrameId, bool bEligible) noexcept
{
    if (!bEligible)
    {
        bFrameEligible = false;
        LastFrame = std::max(LastFrame, FrameId);
        Poll();
        return;
    }
    if (FrameId == 0 || FrameId <= LastFrame) return;
    LastFrame = FrameId; bFrameEligible = bEligible;
    FrameRequests = 0; FramePayloadBytes = 0; Poll();
}
TSharedPtr<FUITextureRecord> FUITextureRegistry::Find(FUITextureId Id) const noexcept
{
    if (!Id.IsValid()) return {};
    for (const auto& Record : Records) if (Record && Record->Id == Id) return Record;
    return {};
}
FUITextureLease FUITextureRegistry::Acquire(FUITextureId Id) const noexcept
{
    FUITextureLease Lease;
    auto Record = Find(Id);
    if (Record && !Record->bRetiring) Lease.Record = std::move(Record);
    return Lease;
}
FUITextureRegistryStatistics FUITextureRegistry::GetStatistics() const noexcept
{
    FUITextureRegistryStatistics Stats;
    for (const auto& Record : Records)
        if (Record)
        {
            ++Stats.Generations; Stats.GPUBytes += Record->TextureBytes;
            Stats.CPUShadowBytes += Record->Shadow.size();
            Stats.StagingBytes += Record->StagingBytes;
        }
    return Stats;
}
void FUITextureRegistry::Poll() noexcept
{
    for (auto& Record : Records)
    {
        if (!Record) continue;
        for (auto& Use : Record->Uses)
            if (Use.bSubmitted && Use.Fence && Use.Fence->IsSignaled())
            {
                if (Use.bUpload)
                {
                    Record->State = EUITextureState::Ready;
                    Record->Staging.reset(); Record->StagingBytes = 0;
                }
                Use = {};
            }
        if (Record->bRetiring && Record.use_count() == 1 && !HasUses(*Record)) Record.reset();
    }
}
FUITextureResult FUITextureRegistry::Prepare(const FUITextureRequest& Request)
{
    if (!Request.IsValid() || !Device)
        return Result(Request, ERHIResult::InvalidState, EUITextureState::Rejected);
    Poll();
    const auto Slot = Request.Operation == EUITextureOperation::Create
        ? Request.LogicalSlot : Request.TextureId.Slot;
    auto Previous = Find(Request.TextureId);
    if (Previous && Previous->bGpuOwned)
        return Result(Request, ERHIResult::InvalidState, EUITextureState::Rejected);
    if (Request.Operation == EUITextureOperation::Destroy)
    {
        if (!Previous)
            return Request.TextureId.Generation <= LastGeneration[Slot]
                ? Result(Request, ERHIResult::Success, EUITextureState::Destroyed, Request.TextureId)
                : Result(Request, ERHIResult::InvalidState, EUITextureState::Rejected);
        Previous->bRetiring = true;
        if (Current[Slot] == Request.TextureId) Current[Slot] = {};
        Previous.reset(); Poll();
        return Find(Request.TextureId)
            ? Result(Request, ERHIResult::NotReady, EUITextureState::Retiring, Request.TextureId)
            : Result(Request, ERHIResult::Success, EUITextureState::Destroyed, Request.TextureId);
    }
    if ((Request.Operation == EUITextureOperation::Create && Current[Slot].IsValid()) ||
        (Request.Operation == EUITextureOperation::Update &&
            (!Previous || Current[Slot] != Request.TextureId || Previous->bRetiring)))
        return Result(Request, ERHIResult::InvalidState, EUITextureState::Rejected);
    if (!bFrameEligible || FrameRequests >= 64)
        return Result(Request, ERHIResult::NotReady, EUITextureState::Requested);
    ++FrameRequests;
    const uint64 Payload = Request.PixelBytes.size();
    const uint64 RowBytes = (static_cast<uint64>(Request.Width) * 4 + 255) & ~uint64{255};
    const uint64 ShadowBytes = RowBytes * Request.Height;
    const auto Stats = GetStatistics();
    const auto Empty = std::find(Records.begin(), Records.end(), nullptr);
    // Staging is separately reported and conservatively charged against the
    // GPU allocation ceiling, so alignment cannot create hidden growth.
    if (Empty == Records.end() || Payload > FramePayloadBudget - FramePayloadBytes ||
        ShadowBytes > ByteBudget - Stats.CPUShadowBytes ||
        Payload + ShadowBytes > ByteBudget - Stats.GPUBytes - Stats.StagingBytes)
        return Result(Request, ERHIResult::NotReady, EUITextureState::Requested);
    if (LastGeneration[Slot] == std::numeric_limits<uint64>::max())
        return Result(Request, ERHIResult::Unavailable, EUITextureState::Rejected);
    try
    {
        auto Record = MakeShared<FUITextureRecord>();
        Record->Id = {Slot, LastGeneration[Slot] + 1};
        Record->Width = Request.Width; Record->Height = Request.Height;
        Record->RowTexels = static_cast<uint32>(RowBytes / 4);
        Record->ColorDomain = Request.ColorDomain;
        Record->TextureBytes = Payload; Record->StagingBytes = ShadowBytes;
        Record->Shadow.assign(static_cast<std::size_t>(ShadowBytes), 0);
        for (uint32 Row = 0; Row < Request.Height; ++Row)
            std::copy_n(Request.PixelBytes.data() + static_cast<std::size_t>(Row) * Request.Width * 4,
                static_cast<std::size_t>(Request.Width) * 4,
                Record->Shadow.data() + static_cast<std::size_t>(Row) * RowBytes);
        if (Request.ColorDomain == EUITextureColorDomain::AlphaCoverage)
            for (uint32 Row = 0; Row < Request.Height; ++Row)
                for (uint32 X = 0; X < Request.Width; ++X)
                {
                    auto* Pixel = Record->Shadow.data() + static_cast<std::size_t>(Row) * RowBytes + X * 4;
                    Pixel[0] = Pixel[1] = Pixel[2] = 255;
                }
        FRHITextureDesc Desc;
        Desc.Width = Request.Width; Desc.Height = Request.Height;
        Desc.Format = Request.ColorDomain == EUITextureColorDomain::SRGBRec709
            ? ERHIFormat::R8G8B8A8_sRGB : ERHIFormat::R8G8B8A8_UNorm;
        Desc.Usage = ERHITextureUsage::Sampled | ERHITextureUsage::CopyDestination;
        auto Texture = Device->CreateTexture(Desc);
        if (!Texture.Succeeded()) return Result(Request, Texture.Result, EUITextureState::Rejected);
        Record->Texture = std::move(Texture.Object);
        auto Staging = Device->CreateBuffer({ShadowBytes, ERHIBufferUsage::CopySource, ERHIMemoryAccess::HostVisible});
        if (!Staging.Succeeded()) return Result(Request, Staging.Result, EUITextureState::Rejected);
        Record->Staging = std::move(Staging.Object);
        const auto Upload = Device->UploadBuffer(Record->Staging,
            {0, Record->Shadow.data(), ShadowBytes});
        if (Upload != ERHIResult::Success) return Result(Request, Upload, EUITextureState::Rejected);
        *Empty = Record; Current[Slot] = Record->Id; LastGeneration[Slot] = Record->Id.Generation;
        FramePayloadBytes += Payload;
        if (Previous) Previous->bRetiring = true;
        return Result(Request, ERHIResult::Success, EUITextureState::Prepared, Record->Id);
    }
    catch (const std::bad_alloc&)
    {
        return Result(Request, ERHIResult::Unavailable, EUITextureState::Rejected);
    }
}
ERHIResult FUITextureRegistry::RegisterGpuTexture(const FUIGpuTextureRegistration& Request,
    const FUIGpuTextureContext& Context, FUITextureId& OutId)
{
    if (!Device || Request.LogicalSlot==0 || Request.LogicalSlot>=Current.size() ||
        !ValidGpuDependency(Request,Context)) return ERHIResult::InvalidState;
    if (!bFrameEligible || Context.FrameId!=LastFrame || FrameRequests>=64) return ERHIResult::NotReady;
    Poll();
    const auto Slot=Request.LogicalSlot;
    const auto Previous=Find(Request.Previous);
    if (Current[Slot]!=Request.Previous || (Request.Previous.IsValid() &&
        (!Previous || !Previous->bGpuOwned || Previous->bRetiring))) return ERHIResult::InvalidState;
    // The target owner must allocate another target or wait for all old leases;
    // two identities must never authorize rewriting the same live texture.
    for (const auto& Record : Records)
        if (Record && Record->Texture==Request.Texture) return ERHIResult::NotReady;
    const auto Empty=std::find(Records.begin(),Records.end(),nullptr);
    const auto Stats=GetStatistics();
    const auto& Desc=Request.Texture->GetDesc();
    const uint64 Bytes=static_cast<uint64>(Desc.Width)*Desc.Height*4;
    if (Empty==Records.end() || Bytes>ByteBudget-Stats.GPUBytes-Stats.StagingBytes) return ERHIResult::NotReady;
    if (LastGeneration[Slot]==std::numeric_limits<uint64>::max()) return ERHIResult::Unavailable;
    try
    {
        auto Record=MakeShared<FUITextureRecord>();
        Record->Id={Slot,LastGeneration[Slot]+1}; Record->bGpuOwned=true;
        Record->Gpu=Request; Record->Identity=Context;
        // Keep values, never a borrowed pointer to the graph object.
        Record->Identity.Graph=nullptr;
        Record->Texture=Request.Texture; Record->TextureBytes=Bytes;
        Record->Width=Desc.Width; Record->Height=Desc.Height;
        Record->ColorDomain=EUITextureColorDomain::SRGBRec709; Record->State=EUITextureState::Ready;
        *Empty=Record; Current[Slot]=Record->Id; LastGeneration[Slot]=Record->Id.Generation; ++FrameRequests;
        if (Previous) Previous->bRetiring=true;
        OutId=Record->Id;
        return ERHIResult::Success;
    }
    catch (const std::bad_alloc&) { return ERHIResult::Unavailable; }
}
ERHIResult FUITextureRegistry::RetireGpuTexture(FUITextureId Id) noexcept
{
    auto Record=Find(Id);
    if (!Record || !Record->bGpuOwned) return ERHIResult::InvalidState;
    Record->bRetiring=true;
    if (Current[Id.Slot]==Id) Current[Id.Slot]={};
    Record.reset(); Poll();
    return Find(Id) ? ERHIResult::NotReady : ERHIResult::Success;
}
ERHIResult FUITextureRegistry::CanRecordSubmission(std::span<const FUITextureLease> Leases,
    const FUIGpuTextureContext* Context) const noexcept
{
    if (!bFrameEligible) return ERHIResult::NotReady;
    if (Leases.size() > Records.size()) return ERHIResult::InvalidState;
    for (std::size_t I = 0; I < Leases.size(); ++I)
    {
        const auto& Lease = Leases[I];
        if (!Lease.IsValid() || Find(Lease.GetId()) != Lease.Record) return ERHIResult::InvalidState;
        for (std::size_t J = 0; J < I; ++J)
            if (Leases[J].GetId() == Lease.GetId()) return ERHIResult::InvalidState;
        const auto& Record = *Lease.Record;
        if (Record.bGpuOwned && (!Context || Context->FrameId!=Record.Identity.FrameId ||
            Context->SettingsRevision!=Record.Identity.SettingsRevision || Context->DisplayGeneration!=Record.Identity.DisplayGeneration ||
            Context->Consumer!=Record.Identity.Consumer || !ValidGpuDependency(Record.Gpu,*Context))) return ERHIResult::InvalidState;
        if (Record.State == EUITextureState::UploadQueued ||
            (Record.State == EUITextureState::Prepared && HasUses(Record)) ||
            std::all_of(Record.Uses.begin(),Record.Uses.end(),[](const auto& Use) { return Use.bReserved; }))
            return ERHIResult::NotReady;
    }
    return ERHIResult::Success;
}
ERHIResult FUITextureRegistry::RecordSubmission(std::span<const FUITextureLease> Leases,
    const TSharedPtr<IRHICommandBuffer>& Command, FUITextureSubmission& OutSubmission,
    const FUIGpuTextureContext* Context)
{
    if (!bFrameEligible) return ERHIResult::NotReady;
    if (!Command || !OutSubmission.Entries.empty() || Leases.size() > Records.size())
        return ERHIResult::InvalidState;
    Poll();
    const auto CanRecord=CanRecordSubmission(Leases,Context);
    if (CanRecord!=ERHIResult::Success) return CanRecord;
    FUITextureSubmission Pending;
    try { Pending.Entries.reserve(Leases.size()); }
    catch (const std::bad_alloc&) { return ERHIResult::Unavailable; }
    for (const auto& Lease : Leases)
    {
        if (!Lease.IsValid() || Find(Lease.GetId()) != Lease.Record)
            return ERHIResult::InvalidState;
        auto& Record = *Lease.Record;
        if (Record.State == EUITextureState::UploadQueued ||
            (Record.State == EUITextureState::Prepared && HasUses(Record)))
            return ERHIResult::NotReady;
        for (const auto& Entry : Pending.Entries)
            if (Entry.Lease.GetId() == Lease.GetId()) return ERHIResult::InvalidState;
        uint32 Index = 0;
        while (Index < Record.Uses.size() && Record.Uses[Index].bReserved) ++Index;
        if (Index == Record.Uses.size()) return ERHIResult::NotReady;
        Record.Uses[Index].bReserved = true;
        Record.Uses[Index].bUpload = Record.State == EUITextureState::Prepared;
        Pending.Entries.push_back({Lease, Index});
    }
    // Once recording starts, even a failed partial command owns the reservations
    // until its caller discards that command and cancels this ticket.
    OutSubmission = std::move(Pending);
    for (const auto& Entry : OutSubmission.Entries)
    {
        const auto& Record = *Entry.Lease.Record;
        if (!Record.Uses[Entry.UseIndex].bUpload) continue;
        FRHIResourceBarrierDesc Transition;
        Transition.Texture = Record.Texture; Transition.After = ERHIResourceLayout::CopyDestination;
        FRHIBufferTextureCopyRegion Copy;
        Copy.Width = Record.Width; Copy.Height = Record.Height;
        Copy.SourceRowLengthTexels = Record.RowTexels;
        auto Status = Command->RecordLayoutTransition(Transition);
        if (Status == ERHIResult::Success)
            Status = Command->RecordBufferToTextureCopy(Record.Staging, Record.Texture, Copy);
        if (Status != ERHIResult::Success) return Status;
        Transition.Before = ERHIResourceLayout::CopyDestination;
        Transition.After = ERHIResourceLayout::ShaderReadOnly;
        Status = Command->RecordLayoutTransition(Transition);
        if (Status != ERHIResult::Success) return Status;
    }
    return ERHIResult::Success;
}
}
