#pragma once

#include "Renderer/FOutputTransformPlan.h"
#include "RHI/IRHIBuffer.h"
#include "RHI/IRHICommandBuffer.h"
#include "RHI/IRHIDevice.h"
#include "RHI/IRHIFence.h"
#include <array>
#include <functional>

namespace Stoner::Demo
{
enum class ELabCapturePurpose { SDRPreview, HDRNumeric };
enum class ELabCaptureStatus
{
    Pending, Success, Busy, InvalidRequest, GenerationMismatch,
    Cancelled, TimedOut, AllocationFailed, DeviceLost, ReadbackFailed
};

// Exact source identity, independent of the formal readback configuration.
struct FLabCaptureIdentity
{
    Core::uint64 SettingsGeneration = 0, DisplayGeneration = 0, OutputGeneration = 0;
    Core::uint32 Width = 0, Height = 0;
    RHI::ERHIFormat Format = RHI::ERHIFormat::Unknown;
    Core::FString OutputProfile, Stage;
    ELabCapturePurpose Purpose = ELabCapturePurpose::SDRPreview;
    bool bIncludeUI = false;
    bool operator==(const FLabCaptureIdentity&) const = default;
    [[nodiscard]] bool IsValid() const noexcept;
};
struct FLabCaptureRequest
{
    Core::uint64 RequestId = 0;
    FLabCaptureIdentity Target;
};
struct FLabCaptureFrame
{
    Core::uint64 FrameToken = 0;
    FLabCaptureIdentity Identity;
    Renderer::EFrameExecutionPurpose ExecutionPurpose = Renderer::EFrameExecutionPurpose::InteractivePreview;
    bool bStable = false;
};
struct FLabCapturePrepared
{
    Core::uint64 RequestId = 0, FrameToken = 0;
    RHI::FRHITextureBufferCopyRegion Region;
    Core::TSharedPtr<RHI::IRHIBuffer> Staging;
};
struct FLabCaptureCompletion
{
    FLabCaptureRequest Request;
    Core::uint64 FrameToken = 0;
    ELabCaptureStatus Status = ELabCaptureStatus::Pending;
};
struct FLabCaptureStatistics
{
    Core::uint32 Requests = 0, PeakRequests = 0;
    Core::uint64 StagingBytes = 0, PeakStagingBytes = 0;
    Core::uint64 Completed = 0, Rejected = 0, TimedOut = 0;
};

// Owned by the Demo frame context. No capture creates a zero-allocation queue.
// Prepared commands must be discarded, or submitted fences observed, before
// cancellation/timeout can release staging. Poll never waits or maps a buffer.
class FLabCaptureQueue
{
public:
    static constexpr Core::uint32 MaximumRequests = 2;
    static constexpr Core::uint64 MaximumStagingBytes = 128ull * 1024 * 1024;
    static constexpr Core::uint64 TimeoutMilliseconds = 5000;
    FLabCaptureQueue() = default;
    FLabCaptureQueue(const FLabCaptureQueue&) = delete;
    FLabCaptureQueue& operator=(const FLabCaptureQueue&) = delete;
    [[nodiscard]] ELabCaptureStatus Request(const FLabCaptureRequest&, Core::uint64 Now);
    [[nodiscard]] ELabCaptureStatus PrepareNext(const FLabCaptureFrame&,
        const Core::TSharedPtr<RHI::IRHIDevice>&,
        const Core::TSharedPtr<RHI::IRHICommandBuffer>& RecordingCommand,
        Core::uint64 Now, FLabCapturePrepared& Out);
    [[nodiscard]] bool Submit(Core::uint64 RequestId, Core::uint64 FrameToken,
        const Core::TSharedPtr<RHI::IRHIFence>& Fence);
    [[nodiscard]] bool Cancel(Core::uint64 RequestId);
    void CancelAll() noexcept;
    // Call before resetting a render fence or reusing a discarded command.
    void Poll(Core::uint64 Now) noexcept;
    // At most one terminal record per monotonically increasing service frame.
    // Reader runs only for successful, fence-complete explicit captures. The
    // borrowed staging reference must not escape this synchronous consumer.
    using FReadback = std::function<bool(const FLabCaptureCompletion&,
        const RHI::IRHIBuffer&, const RHI::FRHITextureBufferCopyRegion&)>;
    [[nodiscard]] bool ProcessOne(Core::uint64 ServiceFrame, Core::uint64 Now,
        const FReadback&, FLabCaptureCompletion& Out);
    [[nodiscard]] bool ReleaseAfterDeviceShutdown(const RHI::IRHIDevice&) noexcept;
    [[nodiscard]] const FLabCaptureStatistics& GetStatistics() const noexcept { return Statistics; }
private:
    enum class EState { Empty, Pending, Prepared, Submitted, Ready };
    struct FRecord
    {
        EState State = EState::Empty;
        FLabCaptureCompletion Completion;
        Core::uint64 Started = 0, Bytes = 0;
        RHI::FRHITextureBufferCopyRegion Region;
        Core::TSharedPtr<RHI::IRHIBuffer> Staging;
        Core::TSharedPtr<RHI::IRHICommandBuffer> Command;
        Core::TSharedPtr<RHI::IRHIFence> Fence;
    };
    std::array<FRecord,MaximumRequests> Records;
    Core::TSharedPtr<RHI::IRHIDevice> OwnerDevice;
    FLabCaptureStatistics Statistics;
    Core::uint64 LastRequest = 0, LastTime = 0, LastProcessedFrame = 0;
    bool bProcessing = false, bClosed = false;
    void RefreshStatistics() noexcept;
    void Stop(FRecord&, ELabCaptureStatus) noexcept;
};
}
