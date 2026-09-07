#pragma once

#include "Core/CoreMinimal.h"
#include "FVulkanLabPresentationPolicy.h"
#include "RHI/ERHIResult.h"
#include "RHI/FRHIFormatInfo.h"

#include <array>

#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
#include <vulkan/vulkan.h>
#endif

namespace Stoner::Backend::Vulkan::Private
{

// The runtime owns only Vulkan presentation objects.  The renderer may copy
// this record while the acquisition token is live, but it must not retain a
// native handle after the token has been retired or cancelled.
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE

struct FVulkanLabSwapchainCreateDesc
{
    Stoner::Core::uint64 Generation = 0;
    Stoner::Core::uint32 Width = 0;
    Stoner::Core::uint32 Height = 0;
    Stoner::Core::uint32 MinImageCount = 2;
    Stoner::RHI::ERHIFormat ColorFormat =
        Stoner::RHI::ERHIFormat::Unknown;
    VkFormat VulkanFormat = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR ColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkImageUsageFlags ImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    VkPresentModeKHR PresentMode = VK_PRESENT_MODE_FIFO_KHR;
    VkSurfaceTransformFlagBitsKHR PreTransform =
        VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    VkCompositeAlphaFlagBitsKHR CompositeAlpha =
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    VkImageViewType ImageViewType = VK_IMAGE_VIEW_TYPE_2D;
    bool bClipped = true;
};

struct FVulkanLabNativeImageRecord
{
    Stoner::Core::uint64 Generation = 0;
    Stoner::Core::uint64 AcquisitionToken = 0;
    Stoner::Core::uint64 FrameToken = 0;
    Stoner::Core::uint32 FrameSlotIndex = 0;
    Stoner::Core::uint32 ImageIndex = 0;
    Stoner::Core::uint32 Width = 0;
    Stoner::Core::uint32 Height = 0;
    Stoner::RHI::ERHIFormat ColorFormat =
        Stoner::RHI::ERHIFormat::Unknown;
    VkSwapchainKHR Swapchain = VK_NULL_HANDLE;
    VkImage Image = VK_NULL_HANDLE;
    VkImageView ImageView = VK_NULL_HANDLE;
    // The graphics submission waits on this semaphore.  It is distinct from
    // the image-indexed render-finished semaphore below.
    VkSemaphore AcquireSemaphore = VK_NULL_HANDLE;
    VkSemaphore RenderFinishedSemaphore = VK_NULL_HANDLE;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return Generation != 0 && AcquisitionToken != 0 && FrameToken != 0 &&
            ImageIndex < FVulkanLabPresentationPolicy::MaxImagesPerGeneration &&
            Swapchain != VK_NULL_HANDLE && Image != VK_NULL_HANDLE &&
            ImageView != VK_NULL_HANDLE &&
            AcquireSemaphore != VK_NULL_HANDLE &&
            RenderFinishedSemaphore != VK_NULL_HANDLE;
    }
};

enum class EVulkanLabPresentationRetirementEvent : Stoner::Core::uint8
{
    PresentationProof,
    GenerationRetired
};

using FVulkanLabPresentationRetirementCallback = void (*) (
    void* UserData,
    Stoner::Core::uint8 Event,
    Stoner::Core::uint64 AcquisitionToken,
    Stoner::Core::uint64 Generation,
    Stoner::Core::uint32 ImageIndex) noexcept;

struct FVulkanLabSwapchainRuntimeSnapshot
{
    Stoner::RHI::ERHIPresentationRetirementMode RetirementMode =
        Stoner::RHI::ERHIPresentationRetirementMode::Unknown;
    Stoner::RHI::ERHIShutdownAssurance ShutdownAssurance =
        Stoner::RHI::ERHIShutdownAssurance::Unknown;
    Stoner::Core::uint64 ActiveGeneration = 0;
    Stoner::Core::uint64 RetiringGeneration = 0;
    Stoner::Core::uint32 ActiveImageCount = 0;
    Stoner::Core::uint32 RetiringImageCount = 0;
    Stoner::Core::uint32 OutstandingNativeRecordCount = 0;
    Stoner::Core::uint32 PendingAcquireCount = 0;
    Stoner::Core::uint64 EstimatedColorBytes = 0;
    VkResult FirstNativeFailure = VK_SUCCESS;
    Stoner::Core::uint32 AbandonedNativeOwnerCount = 0;
    Stoner::Core::uint32 ResidualNativeOwnerCount = 0;
    bool bPausedZeroExtent = false;
    bool bTransitionPending = false;
    bool bFailed = false;
    bool bDeviceLost = false;
    bool bTerminalCleanupStarted = false;
};

class FVulkanLabSwapchainRuntime final
{
public:
    FVulkanLabSwapchainRuntime(
        VkPhysicalDevice InPhysicalDevice,
        VkDevice InDevice,
        VkSurfaceKHR InSurface,
        VkQueue InQueue,
        Stoner::Core::uint32 InQueueFamily,
        const FVulkanLabPresentationPolicySelection& InSelection);
    ~FVulkanLabSwapchainRuntime();

    FVulkanLabSwapchainRuntime(const FVulkanLabSwapchainRuntime&) = delete;
    FVulkanLabSwapchainRuntime& operator=(const FVulkanLabSwapchainRuntime&) = delete;

    [[nodiscard]] Stoner::RHI::ERHIResult Initialize(
        const FVulkanLabSwapchainCreateDesc& Desc) noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult RequestResize(
        const FVulkanLabSwapchainCreateDesc& Desc) noexcept;

    // Acquire uses timeout zero.  A NotReady return retains the pending
    // semaphore/fence and the same frame token for a subsequent call.
    [[nodiscard]] Stoner::RHI::ERHIResult Acquire(
        Stoner::Core::uint64 FrameToken,
        Stoner::Core::uint32 FrameSlotIndex,
        FVulkanLabNativeImageRecord& OutRecord) noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult CancelPendingAcquire(
        Stoner::Core::uint64 FrameToken,
        Stoner::Core::uint32 FrameSlotIndex) noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult Cancel(
        Stoner::Core::uint64 AcquisitionToken) noexcept;

    // The caller invokes this immediately after the real queue submission has
    // been accepted.  QueuePresent is kept separate so a failed present does
    // not get confused with a failed render submission.
    [[nodiscard]] Stoner::RHI::ERHIResult ReportRenderSubmitted(
        Stoner::Core::uint64 AcquisitionToken) noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult QueuePresent(
        const FVulkanLabNativeImageRecord& Record) noexcept;
    // This is an explicit report of the actual native render fence.  The
    // runtime never infers render completion from vkQueuePresentKHR.
    [[nodiscard]] Stoner::RHI::ERHIResult ReportRenderComplete(
        Stoner::Core::uint64 AcquisitionToken,
        bool bSucceeded = true) noexcept;

    // Poll performs only nonblocking fence status checks and timeout-zero
    // acquisition.  It is safe to call while input/event processing remains
    // live; it never performs queue/device idle waits.
    [[nodiscard]] Stoner::RHI::ERHIResult Poll() noexcept;

    [[nodiscard]] bool GetNativeImageRecord(
        Stoner::Core::uint64 AcquisitionToken,
        FVulkanLabNativeImageRecord& OutRecord) const noexcept;
    // The bridge consumes positive events while the corresponding native
    // owner is still addressable. The callback is synchronous and borrowed;
    // callers must clear it before its UserData lifetime ends.
    void SetPresentationRetirementCallback(
        FVulkanLabPresentationRetirementCallback Callback,
        void* UserData) noexcept;
    [[nodiscard]] FVulkanLabSwapchainRuntimeSnapshot GetSnapshot() const noexcept;
    [[nodiscard]] const FVulkanLabPresentationPolicySelection& GetSelection() const noexcept
    {
        return Selection_;
    }
    [[nodiscard]] bool IsFailed() const noexcept { return bFailed_; }
    [[nodiscard]] bool IsPausedZeroExtent() const noexcept
    {
        return bPausedZeroExtent_;
    }

    // This starts terminal ownership accounting only.  It never destroys a
    // pending owner and never calls vkDeviceWaitIdle.
    [[nodiscard]] Stoner::RHI::ERHIResult BeginTerminalCleanup() noexcept;
    // CallerConfirmedIdle is an external lifecycle proof.  For
    // AcquireHistory it authorizes the one compatibility cleanup; for the
    // presentation-fence path all leases still have to be observed retired.
    [[nodiscard]] Stoner::RHI::ERHIResult CompleteTerminalIdle(
        bool bCallerConfirmedIdle,
        bool bDeviceLost = false) noexcept;
    // Destruction is a separate explicit step so a caller can keep the
    // runtime and its residual owner diagnostics alive after a failed close.
    [[nodiscard]] Stoner::RHI::ERHIResult DestroyAfterTerminalProof() noexcept;

private:
    struct FNativeImage
    {
        VkImage Image = VK_NULL_HANDLE;
        VkImageView ImageView = VK_NULL_HANDLE;
        VkSemaphore RenderFinishedSemaphore = VK_NULL_HANDLE;
    };

    struct FNativeGeneration
    {
        bool bOccupied = false;
        bool bActive = false;
        bool bRetiring = false;
        bool bCreationFailed = false;
        VkSwapchainKHR Swapchain = VK_NULL_HANDLE;
        FVulkanLabSwapchainCreateDesc Desc;
        Stoner::Core::uint32 ImageCount = 0;
        std::array<FNativeImage, FVulkanLabPresentationPolicy::MaxImagesPerGeneration>
            Images{};
    };

    struct FNativeAcquire
    {
        bool bOccupied = false;
        bool bPending = false;
        bool bAcquired = false;
        bool bCanceled = false;
        bool bRenderSubmitted = false;
        bool bRenderComplete = false;
        bool bRenderSucceeded = false;
        bool bPresentAttempted = false;
        bool bPresentQueued = false;
        bool bPresentationRetired = false;
        bool bAcquireFenceComplete = false;
        bool bAcquireSyncReported = false;
        bool bHasPredecessor = false;
        Stoner::Core::uint64 PredecessorToken = 0;
        // AcquireHistory uses the same bounded policy record for an old
        // presentation and its same-image reacquisition.  Keeping the
        // reacquisition sync here leaves all sixteen image-history records
        // available while an eighth-plus-eighth generation set retires.
        bool bHasReacquisition = false;
        bool bReacquisitionRenderSubmitted = false;
        bool bReacquisitionRenderComplete = false;
        bool bReacquisitionRenderSucceeded = false;
        bool bReacquisitionCanceled = false;
        bool bReacquisitionPresentAttempted = false;
        bool bReacquisitionPresentQueued = false;
        bool bReacquisitionAcquireFenceComplete = false;
        bool bReacquisitionAcquireSyncReported = false;
        bool bReacquisitionPresentationRetired = false;
        Stoner::Core::uint64 Generation = 0;
        Stoner::Core::uint64 FrameToken = 0;
        Stoner::Core::uint64 AcquisitionToken = 0;
        Stoner::Core::uint64 ReacquisitionToken = 0;
        Stoner::Core::uint64 ReacquisitionFrameToken = 0;
        Stoner::Core::uint32 FrameSlotIndex = 0;
        Stoner::Core::uint32 ReacquisitionFrameSlotIndex = 0;
        Stoner::Core::uint32 ImageIndex = 0;
        VkSemaphore AcquireSemaphore = VK_NULL_HANDLE;
        VkFence AcquireFence = VK_NULL_HANDLE;
        VkFence PresentationFence = VK_NULL_HANDLE;
        VkSemaphore ReacquisitionAcquireSemaphore = VK_NULL_HANDLE;
        VkFence ReacquisitionAcquireFence = VK_NULL_HANDLE;
        VkFence ReacquisitionPresentationFence = VK_NULL_HANDLE;
    };

    [[nodiscard]] Stoner::RHI::ERHIResult CreateGeneration(
        const FVulkanLabSwapchainCreateDesc& Desc,
        VkSwapchainKHR OldSwapchain,
        FNativeGeneration& OutGeneration) noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult ValidateSurfaceCompatibility(
        const FVulkanLabSwapchainCreateDesc& Desc,
        VkFormat& OutFormat,
        VkColorSpaceKHR& OutColorSpace) noexcept;
    void DestroyGeneration(FNativeGeneration& Generation) noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult CreateAcquireSync(
        FNativeAcquire& Acquire) noexcept;
    void DestroyAcquireSync(FNativeAcquire& Acquire) noexcept;
    [[nodiscard]] FNativeGeneration* FindGeneration(
        Stoner::Core::uint64 Generation) noexcept;
    [[nodiscard]] const FNativeGeneration* FindGeneration(
        Stoner::Core::uint64 Generation) const noexcept;
    [[nodiscard]] FNativeAcquire* FindAcquire(
        Stoner::Core::uint64 AcquisitionToken) noexcept;
    [[nodiscard]] const FNativeAcquire* FindAcquire(
        Stoner::Core::uint64 AcquisitionToken) const noexcept;
    [[nodiscard]] FNativeAcquire* FindPendingAcquire(
        Stoner::Core::uint64 FrameToken,
        Stoner::Core::uint32 FrameSlotIndex) noexcept;
    [[nodiscard]] FNativeAcquire* FindFreeAcquire() noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult TryAcquire(
        FNativeAcquire& Acquire) noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult PublishPendingAcquire(
        FNativeAcquire& Pending) noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult FillNativeImageRecord(
        const FNativeAcquire& Acquire,
        Stoner::Core::uint64 AcquisitionToken,
        FVulkanLabNativeImageRecord& OutRecord) const noexcept;
    [[nodiscard]] FNativeAcquire* FindTokenRecord(
        Stoner::Core::uint64 AcquisitionToken,
        bool& bOutReacquisition) noexcept;
    [[nodiscard]] const FNativeAcquire* FindTokenRecord(
        Stoner::Core::uint64 AcquisitionToken,
        bool& bOutReacquisition) const noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult PollAcquire(
        FNativeAcquire& Acquire) noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult PollPresentation(
        FNativeAcquire& Acquire) noexcept;
    void ReleaseCompletedAcquire(FNativeAcquire& Acquire) noexcept;
    void PromoteReacquisition(FNativeAcquire& Acquire) noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult TryRetirePredecessor() noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult BeginReplacementIfReady() noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult DestroyRetiredGeneration() noexcept;
    [[nodiscard]] bool IsGenerationRenderDrained(
        Stoner::Core::uint64 Generation) const noexcept;
    [[nodiscard]] bool IsTerminalProofComplete() const noexcept;
    [[nodiscard]] bool HasTerminalNativeProof() const noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult MapNativeResult(VkResult Result) noexcept;
    void LatchFailure(
        Stoner::RHI::ERHIResult Result,
        VkResult NativeResult = VK_SUCCESS) noexcept;
    void NotifyPresentationRetirement(
        EVulkanLabPresentationRetirementEvent Event,
        Stoner::Core::uint64 AcquisitionToken,
        Stoner::Core::uint64 Generation,
        Stoner::Core::uint32 ImageIndex) noexcept;

    VkPhysicalDevice PhysicalDevice_ = VK_NULL_HANDLE;
    VkDevice Device_ = VK_NULL_HANDLE;
    VkSurfaceKHR Surface_ = VK_NULL_HANDLE;
    VkQueue Queue_ = VK_NULL_HANDLE;
    Stoner::Core::uint32 QueueFamily_ = 0;
    FVulkanLabPresentationPolicySelection Selection_;
    FVulkanLabPresentationPolicy Policy_;
    std::array<FNativeGeneration, FVulkanLabPresentationPolicy::MaxGenerationCount>
        Generations_{};
    std::array<FNativeAcquire, FVulkanLabPresentationPolicy::MaxPresentationRecords>
        Acquires_{};
    // A pending timeout-zero acquire is an operation, not an image-history
    // record. Two bounded attempts allow both render slots to keep polling
    // while the sixteen per-image records remain available for retirement.
    std::array<FNativeAcquire, 2> PendingAcquires_{};
    FVulkanLabSwapchainCreateDesc PendingResize_;
    bool bHasPendingResize_ = false;
    bool bPausedZeroExtent_ = false;
    bool bInitialized_ = false;
    bool bFailed_ = false;
    bool bTerminalCleanupStarted_ = false;
    bool bTerminalIdleConfirmed_ = false;
    bool bDeviceLost_ = false;
    bool bClosed_ = false;
    FVulkanLabPresentationRetirementCallback RetirementCallback_ = nullptr;
    void* RetirementCallbackUserData_ = nullptr;
    Stoner::Core::uint32 AbandonedNativeOwnerCount_ = 0;
    VkResult FirstNativeFailure_ = VK_SUCCESS;
    Stoner::Core::uint64 NextAcquisitionToken_ = 1;
    Stoner::RHI::ERHIResult FailureResult_ =
        Stoner::RHI::ERHIResult::InvalidState;
};

#else

// Keep non-native Vulkan builds source-compatible without introducing fake
// handles or a fallback presentation claim.
class FVulkanLabSwapchainRuntime final
{
public:
    FVulkanLabSwapchainRuntime() = default;
};

#endif

} // namespace Stoner::Backend::Vulkan::Private
