#include "FLabCaptureQueue.h"
#include <algorithm>
#include <new>

namespace Stoner::Demo
{
using namespace Core;
using namespace RHI;
bool FLabCaptureIdentity::IsValid() const noexcept
{
    const auto Info=GetRHIFormatInfo(Format);
    return SettingsGeneration && DisplayGeneration && OutputGeneration &&
        Width && Height && Width<=4096 && Height<=4096 && uint64(Width)*Height<=7864320 &&
        Info.IsValid() && !Info.bCompressed &&
        !OutputProfile.IsEmpty() && OutputProfile.Len()<=128 &&
        !Stage.IsEmpty() && Stage.Len()<=128 &&
        (Purpose==ELabCapturePurpose::SDRPreview || Purpose==ELabCapturePurpose::HDRNumeric);
}
void FLabCaptureQueue::RefreshStatistics() noexcept
{
    Statistics.Requests=0; Statistics.StagingBytes=0;
    for (const auto& R : Records)
        if (R.State!=EState::Empty) { ++Statistics.Requests; Statistics.StagingBytes+=R.Bytes; }
    if (!Statistics.Requests) OwnerDevice.reset();
    Statistics.PeakRequests=std::max(Statistics.PeakRequests,Statistics.Requests);
    Statistics.PeakStagingBytes=std::max(Statistics.PeakStagingBytes,Statistics.StagingBytes);
}
void FLabCaptureQueue::Stop(FRecord& R,ELabCaptureStatus Status) noexcept
{
    if (R.Completion.Status!=ELabCaptureStatus::Pending) return;
    R.Completion.Status=Status;
    if (Status==ELabCaptureStatus::TimedOut) ++Statistics.TimedOut;
    if (R.State==EState::Pending) R.State=EState::Ready;
}
ELabCaptureStatus FLabCaptureQueue::Request(const FLabCaptureRequest& Request,uint64 Now)
{
    if (bClosed || bProcessing || !Request.RequestId || Request.RequestId<=LastRequest || !Request.Target.IsValid())
    { ++Statistics.Rejected; return ELabCaptureStatus::InvalidRequest; }
    Poll(Now);
    auto It=std::find_if(Records.begin(),Records.end(),[](const auto& R) { return R.State==EState::Empty; });
    if (It==Records.end()) { ++Statistics.Rejected; return ELabCaptureStatus::Busy; }
    It->Completion.Request=Request;
    It->Started=LastTime; It->State=EState::Pending;
    LastRequest=Request.RequestId;
    RefreshStatistics();
    return ELabCaptureStatus::Pending;
}
ELabCaptureStatus FLabCaptureQueue::PrepareNext(const FLabCaptureFrame& Frame,
    const TSharedPtr<IRHIDevice>& Device,const TSharedPtr<IRHICommandBuffer>& Command,
    uint64 Now,FLabCapturePrepared& Out)
{
    Out={};
    if (bProcessing || bClosed || Frame.ExecutionPurpose!=Renderer::EFrameExecutionPurpose::InteractivePreview)
        return ELabCaptureStatus::InvalidRequest;
    Poll(Now);
    FRecord* R=nullptr;
    for (auto& Candidate : Records)
        if (Candidate.State==EState::Pending &&
            (!R || Candidate.Completion.Request.RequestId<R->Completion.Request.RequestId)) R=&Candidate;
    if (!R || !Frame.bStable || !Frame.Identity.Width || !Frame.Identity.Height) return ELabCaptureStatus::Pending;
    if (!Frame.FrameToken || !Frame.Identity.IsValid() || !Device || !Device->IsActive() || !Command ||
        Command->GetState()!=ERHICommandBufferState::Recording ||
        (OwnerDevice && OwnerDevice!=Device)) return ELabCaptureStatus::InvalidRequest;
    if (!(R->Completion.Request.Target==Frame.Identity))
    { Stop(*R,ELabCaptureStatus::GenerationMismatch); return ELabCaptureStatus::GenerationMismatch; }
    FRHITextureBufferCopyRegion Region;
    Region.Width=Frame.Identity.Width; Region.Height=Frame.Identity.Height;
    uint64 Bytes=0;
    if (!TryGetRHITextureBufferCopyByteSize(Region,Frame.Identity.Format,Bytes) || !Bytes || Bytes>MaximumStagingBytes)
    { Stop(*R,ELabCaptureStatus::InvalidRequest); return ELabCaptureStatus::InvalidRequest; }
    if (Bytes>MaximumStagingBytes-Statistics.StagingBytes) return ELabCaptureStatus::Busy;
    TRHIObjectResult<IRHIBuffer> Buffer;
    try { Buffer=Device->CreateBuffer({Bytes,ERHIBufferUsage::CopyDestination,ERHIMemoryAccess::HostVisible}); }
    catch (const std::bad_alloc&) { Stop(*R,ELabCaptureStatus::AllocationFailed); return ELabCaptureStatus::AllocationFailed; }
    if (!Buffer.Succeeded() || Buffer.Object->GetSizeInBytes()!=Bytes)
    { Stop(*R,ELabCaptureStatus::AllocationFailed); return ELabCaptureStatus::AllocationFailed; }
    R->Bytes=Bytes; R->Region=Region; R->Staging=std::move(Buffer.Object); R->Command=Command;
    OwnerDevice=Device;
    R->Completion.FrameToken=Frame.FrameToken; R->State=EState::Prepared;
    Out={R->Completion.Request.RequestId,Frame.FrameToken,Region,R->Staging};
    RefreshStatistics();
    return ELabCaptureStatus::Success;
}
bool FLabCaptureQueue::Submit(uint64 Id,uint64 Frame,const TSharedPtr<IRHIFence>& Fence)
{
    if (bClosed || bProcessing || !Fence) return false;
    for (auto& R : Records)
        if (R.State==EState::Prepared && R.Completion.Request.RequestId==Id && R.Completion.FrameToken==Frame)
        {
            if (!R.Command || (R.Command->GetState()!=ERHICommandBufferState::Completed &&
                R.Command->GetState()!=ERHICommandBufferState::Submitted)) return false;
            // Cancellation racing a successful submission must still attach
            // the fence: its buffer is no longer safe to discard as unsubmitted.
            R.Fence=Fence; R.State=EState::Submitted;
            return true;
        }
    return false;
}
bool FLabCaptureQueue::Cancel(uint64 Id,ELabCaptureStatus Reason)
{
    if (bProcessing || (Reason!=ELabCaptureStatus::Cancelled &&
        Reason!=ELabCaptureStatus::InvalidRequest && Reason!=ELabCaptureStatus::ReadbackFailed)) return false;
    for (auto& R : Records)
        if (R.State!=EState::Empty && R.Completion.Request.RequestId==Id)
        { Stop(R,Reason); return true; }
    return false;
}
void FLabCaptureQueue::CancelAll() noexcept
{
    if (bProcessing) return;
    for (auto& R : Records) if (R.State!=EState::Empty) Stop(R,ELabCaptureStatus::Cancelled);
}
void FLabCaptureQueue::Poll(uint64 Now) noexcept
{
    if (bProcessing) return;
    LastTime=std::max(LastTime,Now);
    for (auto& R : Records)
    {
        if (R.State==EState::Empty) continue;
        if (LastTime-R.Started>=TimeoutMilliseconds) Stop(R,ELabCaptureStatus::TimedOut);
        if (R.State==EState::Submitted && R.Fence->IsSignaled())
        {
            R.State=EState::Ready;
            // Latch the proof before the frame owner resets its fence.
            R.Fence.reset();
        }
        if (R.State==EState::Prepared && R.Command->GetState()==ERHICommandBufferState::Idle)
        { Stop(R,ELabCaptureStatus::Cancelled); R.State=EState::Ready; }
        // Once reset is observed, the readback owns only its staging buffer.
        // Later reuse of the render command must not delay an already safe read.
        if (R.State==EState::Ready && R.Command && R.Command->GetState()==ERHICommandBufferState::Idle)
            R.Command.reset();
    }
}
bool FLabCaptureQueue::ProcessOne(uint64 ServiceFrame,uint64 Now,const FReadback& Readback,FLabCaptureCompletion& Out)
{
    Out={};
    if (bProcessing || !ServiceFrame || ServiceFrame<=LastProcessedFrame) return false;
    Poll(Now);
    FRecord* R=nullptr;
    for (auto& Candidate : Records)
    {
        if (Candidate.State!=EState::Ready ||
            (Candidate.Command && Candidate.Command->GetState()!=ERHICommandBufferState::Idle) ||
            (Candidate.Staging && Candidate.Staging.use_count()!=1)) continue;
        if (!R || Candidate.Completion.Request.RequestId<R->Completion.Request.RequestId) R=&Candidate;
    }
    if (!R) return false;
    LastProcessedFrame=ServiceFrame;
    bProcessing=true;
    if (R->Completion.Status==ELabCaptureStatus::Pending)
    {
        R->Completion.Status=ELabCaptureStatus::Success;
        bool OK=false;
        try { OK=Readback && R->Staging && Readback(R->Completion,R->Staging,R->Region); }
        catch (...) { OK=false; }
        if (!OK) R->Completion.Status=ELabCaptureStatus::ReadbackFailed;
    }
    bProcessing=false;
    // A backend consumer may retain the shared handle. Keep the completed
    // record charged until that final alias disappears, without reading again.
    if (R->Staging && R->Staging.use_count()!=1) return false;
    Out=R->Completion;
    *R={}; ++Statistics.Completed; RefreshStatistics();
    return true;
}
bool FLabCaptureQueue::ReleaseAfterDeviceShutdown(const IRHIDevice& Device) noexcept
{
    if (bProcessing || Device.GetState()!=ERHIDeviceState::Shutdown ||
        (OwnerDevice && OwnerDevice.get()!=&Device)) return false;
    bClosed=true;
    for (auto& R : Records)
    {
        if (R.State==EState::Empty) continue;
        Stop(R,ELabCaptureStatus::DeviceLost);
        R.State=EState::Ready;
        R.Command.reset(); R.Fence.reset(); R.Staging.reset(); R.Bytes=0;
    }
    RefreshStatistics();
    return true;
}
}
