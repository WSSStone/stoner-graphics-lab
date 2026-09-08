#pragma once

#include "FMetalDeviceOwnerState.h"
#include "Core/FPlatformWindow.h"
#include "RHI/ERHIFormat.h"
#include "RHI/ERHIResult.h"
#include "RHI/FRHIPresentationCapabilities.h"
#include "RHI/FRHIPresentationFrame.h"
#include "RHI/FRHIPresentationSurfaceDesc.h"
#include "RHI/FRHIResolvedPresentationState.h"
#include "RHI/FRHISwapchainDesc.h"
#include "RHI/IRHITexture.h"

#include <array>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>

namespace Stoner::Backend::Metal::Private
{

class FMetalSemaphore;
class FMetalFence;
class FMetalTexture;

struct FMetalPresentationLayerPolicy
{
    Core::uint64 PixelFormat = 0;
    RHI::ERHIPresentationColorSpace ColorSpace =
        RHI::ERHIPresentationColorSpace::Unknown;
    RHI::ERHIPresentationDisplayAdaptation DisplayAdaptation =
        RHI::ERHIPresentationDisplayAdaptation::None;
    bool bWantsExtendedDynamicRangeContent = false;
    bool bHasEDRMetadata = false;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return PixelFormat != 0 &&
            RHI::IsValidPresentationColorSpace(ColorSpace);
    }
};

struct FMetalPresentationLayerSnapshot
{
    FMetalPresentationLayerPolicy Policy;
    RHI::FRHINativePresentationStatistics NativeStatistics;
    Core::uint64 ModeGeneration = 0;
    Core::uint32 Width = 0;
    Core::uint32 Height = 0;
    float NativeReferenceWhiteNits = 0.0f;
    float CurrentHeadroom = 1.0f;
    float PotentialHeadroom = 1.0f;
    Core::FString MetadataDigest;
    Core::uint64 LastAcquiredFrameToken = 0;
    Core::uint64 LastSubmittedFrameToken = 0;
    Core::uint64 LastPresentedFrameToken = 0;
};

// Backend-private image-indexed ownership shared by the native presentation
// callback and deterministic lifecycle tests.  It is separate from the two
// render-slot records and never represents physical scanout by itself.
class FMetalPresentationTracker final
{
public:
    [[nodiscard]] bool TryReserve(Core::uint32& OutImageIndex) noexcept;
    void Reset() noexcept;
    void Release(Core::uint32 ImageIndex) noexcept;
    void Complete(
        Core::uint32 ImageIndex,
        Core::uint64 FrameToken,
        bool bActuallyPresented) noexcept;
    [[nodiscard]] bool IsEmpty() const noexcept;
    [[nodiscard]] bool HasOtherLeases() const noexcept;
    [[nodiscard]] bool WaitForZero(
        std::chrono::milliseconds Timeout) noexcept;
    [[nodiscard]] Core::uint64 GetLastPresentedFrameToken() const noexcept;
    [[nodiscard]] bool HasPresentedFrame() const noexcept;
    [[nodiscard]] Core::uint32 GetPendingCount() const noexcept;

private:
    mutable std::mutex Mutex;
    std::condition_variable Condition;
    Core::uint32 PendingCount = 0;
    Core::uint64 LastPresentedFrameToken = 0;
    bool bHasPresentedFrame = false;
    std::array<bool, RHI::MaxRHIPresentationImageLeases>
        ActiveImageLeases{};
};

[[nodiscard]] FMetalPresentationLayerPolicy ResolveMetalPresentationLayerPolicy(
    const RHI::FRHISwapchainDesc& Request) noexcept;

class FMetalPresentationContext final
    : public std::enable_shared_from_this<FMetalPresentationContext>
{
public:
    FMetalPresentationContext(
        Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
        void* NativeDevice,
        void* NativeQueue);
    ~FMetalPresentationContext();

    [[nodiscard]] RHI::ERHIResult Attach(
        const Core::FPlatformWindow& Window,
        RHI::ERHIFormat Format,
        Core::uint32 MaximumDrawableCount,
        bool bVSync) noexcept;
    [[nodiscard]] RHI::ERHIResult Attach(
        const Core::FPlatformWindow& Window,
        const RHI::FRHISwapchainDesc& Request) noexcept;
    [[nodiscard]] RHI::ERHIResult Reconfigure(
        const Core::FPlatformWindow& Window,
        const RHI::FRHISwapchainDesc& Request) noexcept;
    [[nodiscard]] RHI::ERHIResult QueryCapabilities(
        const RHI::FRHIPresentationSurfaceDesc& Surface,
        Core::uint64 CapabilityGeneration,
        RHI::FRHIPresentationCapabilities& OutCapabilities) const noexcept;
    [[nodiscard]] bool IsAttached() const noexcept;
    [[nodiscard]] RHI::ERHIResult Acquire(
        Core::uint32 FrameSlot,
        Core::uint64 FrameToken,
        Core::TSharedPtr<RHI::IRHITexture>& OutTexture,
        Core::uint64& OutGeneration) noexcept;
    [[nodiscard]] RHI::ERHIResult AcquireBorrowed(
        Core::uint32 FrameSlot,
        Core::uint64 FrameToken,
        Core::TSharedPtr<RHI::IRHITexture>& OutTexture,
        Core::uint64& OutGeneration,
        Core::uint32& OutImageIndex) noexcept;
    [[nodiscard]] RHI::ERHIResult Present(
        Core::uint32 FrameSlot,
        Core::uint64 Generation,
        Core::uint64 FrameToken,
        const Core::TSharedPtr<FMetalSemaphore>& WaitSemaphore) noexcept;
    [[nodiscard]] RHI::ERHIResult PresentBorrowed(
        const RHI::FRHIBorrowedAcquiredTarget& Target,
        const Core::TSharedPtr<FMetalSemaphore>& RenderFinishedSemaphore,
        RHI::FRHIPresentationLease& OutPresentationLease) noexcept;
    [[nodiscard]] RHI::ERHIResult PresentBorrowedAfterRender(
        const RHI::FRHIBorrowedAcquiredTarget& Target,
        const Core::TSharedPtr<FMetalFence>& RenderCompletionFence,
        RHI::FRHIPresentationLease& OutPresentationLease) noexcept;
    [[nodiscard]] RHI::ERHIResult ReleaseBorrowed(
        const RHI::FRHIBorrowedAcquiredTarget& Target,
        const Core::TSharedPtr<RHI::IRHIFence>& RenderCompletionFence) noexcept;
    void ReleaseBorrowedAcquire(
        Core::uint32 FrameSlot,
        Core::uint64 Generation,
        Core::uint64 FrameToken) noexcept;
    [[nodiscard]] bool HasPendingBorrowedAcquire(
        Core::uint32 FrameSlot, Core::uint64 FrameToken) const noexcept;
    [[nodiscard]] RHI::ERHIResult CancelPendingBorrowedAcquire(
        Core::uint32 FrameSlot, Core::uint64 FrameToken) noexcept;
    void CancelAcquire(
        Core::uint32 FrameSlot,
        Core::uint64 FrameToken) noexcept;
    void CancelAcquire(
        Core::uint32 FrameSlot,
        Core::uint64 Generation,
        Core::uint64 FrameToken) noexcept;
    // Cancel every unpublished async acquisition owned by this presentation
    // context.  Published borrowed targets and legacy formal acquisitions
    // remain untouched; running workers retain their layer/job state until
    // their native nextDrawable call has actually returned.
    void CancelAllUnpublishedBorrowedAcquires() noexcept;
    [[nodiscard]] RHI::ERHIResult Shutdown() noexcept;
    [[nodiscard]] Core::uint64 GetGeneration() const noexcept;
    [[nodiscard]] RHI::FRHIResolvedPresentationState
    GetResolvedPresentationState() const noexcept;
    [[nodiscard]] FMetalPresentationLayerSnapshot
    GetLayerSnapshot() const noexcept;
    [[nodiscard]] Core::uint32
    GetPendingPresentationLeaseCount() const noexcept;
    [[nodiscard]] Core::uint32
    GetPendingDrawableAcquireCount() const noexcept;

private:
    [[nodiscard]] RHI::ERHIResult AcquireInternal(
        Core::uint32 FrameSlot,
        Core::uint64 FrameToken,
        Core::TSharedPtr<RHI::IRHITexture>& OutTexture,
        Core::uint64& OutGeneration,
        Core::uint32& OutImageIndex,
        bool bBorrowed) noexcept;
    [[nodiscard]] RHI::ERHIResult PresentBorrowedInternal(
        const RHI::FRHIBorrowedAcquiredTarget& Target,
        const Core::TSharedPtr<FMetalSemaphore>& RenderFinishedSemaphore,
        const Core::TSharedPtr<FMetalFence>& RenderCompletionFence,
        RHI::FRHIPresentationLease& OutPresentationLease) noexcept;
    void PollCompletedUnpublishedBorrowedAcquires() noexcept;
    struct FImpl;
    Core::TUniquePtr<FImpl> Impl_;
    Core::TSharedPtr<FMetalDeviceOwnerState> Owner_;
    void* NativeDevice_ = nullptr;
    void* NativeQueue_ = nullptr;
};

} // namespace Stoner::Backend::Metal::Private
