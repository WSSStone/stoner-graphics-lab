#include "Renderer/FUIRenderSession.h"
#include "FUICompositionExecutor.h"
#include "RHI/IRHITexture.h"
#include <array>
#include <algorithm>
#include <new>

namespace Stoner::Renderer
{
using namespace Stoner::Core;
using namespace Stoner::RHI;
struct FUIRenderFrame::FImpl
{
    TSharedPtr<FUITextureRegistry> Registry;
    TSharedPtr<IRHITexture> Scene;
    FUICompositionFrame Composition;
    FUICompositionSettings Settings;
    FUITextureSubmission Submission;
    TSharedPtr<IRHIFence> Fence;
    bool bRecordAttempted = false, bRecorded = false, bSubmitted = false, bRetired = false;
    void Release() noexcept
    {
        Composition = {};
        Submission = {};
        Scene.reset(); Fence.reset(); bRetired = true;
        Registry->Poll();
    }
};
struct FUIRenderSession::FImpl
{
    TSharedPtr<IRHIDevice> Device;
    TSharedPtr<FUITextureRegistry> Registry;
    uint64 SessionId = 0, LastPreparedFrameId = 0;
    std::array<std::weak_ptr<FUIRenderFrame::FImpl>,2> Frames;
};
FUIRenderSession::FUIRenderSession(TSharedPtr<IRHIDevice> Device, uint64 SessionId)
    : Impl(std::make_unique<FImpl>())
{
    Impl->Device = std::move(Device); Impl->SessionId = SessionId;
    Impl->Registry = MakeShared<FUITextureRegistry>(Impl->Device);
}
FUIRenderSession::~FUIRenderSession() = default;
void FUIRenderSession::BeginEligibleFrame(uint64 FrameId,bool bEligible) noexcept
{ Impl->Registry->BeginEligibleFrame(FrameId,bEligible); }
FUITextureResult FUIRenderSession::PrepareTexture(const FUITextureRequest& Request)
{ return Impl->Registry->Prepare(Request); }
FUITextureLease FUIRenderSession::AcquireTexture(FUITextureId Id) const noexcept
{ return Impl->Registry->Acquire(Id); }
ERHIResult FUIRenderSession::PrepareFrame(const FUIDrawSnapshot& Snapshot,
    const FUICompositionSettings& Settings, uint64 Revision, uint64 LastSubmittedFrameId,
    const TSharedPtr<IRHITexture>& Scene, std::span<const FRHIShaderModuleDesc> DrawShaders,
    std::span<const FRHIShaderModuleDesc> CopyShaders,TSharedPtr<FUIRenderFrame>& OutFrame)
{
    if (!Scene || Impl->SessionId == 0) return ERHIResult::InvalidState;
    std::size_t Slot = 0;
    while (Slot < Impl->Frames.size())
    {
        auto Frame = Impl->Frames[Slot].lock();
        if (!Frame || Frame->bRetired) break;
        ++Slot;
    }
    if (Slot == Impl->Frames.size()) return ERHIResult::NotReady;
    try
    {
        auto Candidate = MakeShared<FUIRenderFrame>();
        Candidate->Impl = MakeShared<FUIRenderFrame::FImpl>();
        Candidate->Impl->Registry = Impl->Registry; Candidate->Impl->Scene = Scene;
        FUIDrawValidationContext Context{Impl->SessionId,Revision,Settings.DisplayGeneration,
            std::max(LastSubmittedFrameId,Impl->LastPreparedFrameId),
            Scene->GetDesc().Width,Scene->GetDesc().Height,{}};
        const auto Result = FUICompositionExecutor::Prepare(Impl->Device,Snapshot,Context,Settings,
            *Impl->Registry,Scene,DrawShaders,CopyShaders,Candidate->Impl->Composition);
        if (Result != ERHIResult::Success) return Result;
        Candidate->Impl->Settings = Settings;
        Impl->Frames[Slot] = Candidate->Impl;
        Impl->LastPreparedFrameId = Snapshot.GetFrameId();
        OutFrame = std::move(Candidate);
        return ERHIResult::Success;
    }
    catch (const std::bad_alloc&) { return ERHIResult::Unavailable; }
}
TSharedPtr<IRHITexture> FUIRenderFrame::GetInput() const noexcept
{ return Impl && !Impl->bRetired ? Impl->Scene : nullptr; }
TSharedPtr<IRHITexture> FUIRenderFrame::GetOutput() const noexcept
{ return Impl && !Impl->bRetired ? Impl->Composition.GetOutput() : nullptr; }
const FUICompositionSettings* FUIRenderFrame::GetSettings() const noexcept
{ return Impl && !Impl->bRetired ? &Impl->Settings : nullptr; }
bool FUIRenderFrame::HasDraws() const noexcept
{ return Impl && !Impl->bRetired && Impl->Composition.HasDraws(); }
bool FUIRenderFrame::CanRecord() const noexcept
{ return Impl && !Impl->bRetired && !Impl->bRecordAttempted && Impl->Composition.CanRecord(*Impl->Registry); }
ERHIResult FUIRenderFrame::Record(const TSharedPtr<IRHICommandBuffer>& Command)
{
    if (!Impl || Impl->bRetired || Impl->bRecordAttempted) return ERHIResult::InvalidState;
    Impl->bRecordAttempted = true;
    const auto Result = FUICompositionExecutor::Record(Impl->Composition,*Impl->Registry,Command,Impl->Submission);
    Impl->bRecorded = Result == ERHIResult::Success;
    return Result;
}
ERHIResult FUIRenderFrame::Commit(const TSharedPtr<IRHIFence>& Fence) noexcept
{
    if (!Impl || Impl->bRetired || !Impl->bRecorded || Impl->bSubmitted || !Fence) return ERHIResult::InvalidState;
    const auto Result = Impl->Composition.HasDraws() ? Impl->Submission.Commit(Fence) : ERHIResult::Success;
    if (Result == ERHIResult::Success) { Impl->Fence = Fence; Impl->bSubmitted = true; }
    return Result;
}
ERHIResult FUIRenderFrame::ReleaseCompleted() noexcept
{
    if (!Impl || Impl->bRetired || !Impl->bSubmitted) return ERHIResult::InvalidState;
    if (!Impl->Fence || !Impl->Fence->IsSignaled()) return ERHIResult::NotReady;
    Impl->Release();
    return ERHIResult::Success;
}
ERHIResult FUIRenderFrame::CancelAfterCommandDiscard() noexcept
{
    if (!Impl || Impl->bRetired || Impl->bSubmitted) return ERHIResult::InvalidState;
    Impl->Release();
    return ERHIResult::Success;
}
}
