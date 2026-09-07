#include "FVulkanLabSwapchainRuntime.h"

#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE

#include "FVulkanStruct.h"

#include <algorithm>
#include <array>
#include <limits>

namespace Stoner::Backend::Vulkan::Private
{

namespace
{

using Stoner::Core::uint32;
using Stoner::Core::uint64;
using Stoner::RHI::ERHIResult;

constexpr uint32 MaxSurfaceFormats = 256;
constexpr uint32 MaxPresentModes = 256;
constexpr uint32 MaxImageCount =
    FVulkanLabPresentationPolicy::MaxImagesPerGeneration;
constexpr uint32 MaxDrawableAxis = 4096;
constexpr uint64 MaxDrawablePixels = 7'864'320;
[[nodiscard]] ERHIResult MapPolicyResult(
    EVulkanLabPresentationPolicyResult Result) noexcept
{
    switch (Result)
    {
    case EVulkanLabPresentationPolicyResult::Accepted:
    case EVulkanLabPresentationPolicyResult::Coalesced:
        return ERHIResult::Success;
    case EVulkanLabPresentationPolicyResult::BudgetExceeded:
    case EVulkanLabPresentationPolicyResult::GenerationLimit:
        return ERHIResult::Unsupported;
    case EVulkanLabPresentationPolicyResult::ImageOwnershipPending:
    case EVulkanLabPresentationPolicyResult::NoActiveGeneration:
    case EVulkanLabPresentationPolicyResult::NoPendingReplacement:
        return ERHIResult::NotReady;
    case EVulkanLabPresentationPolicyResult::CreationFailed:
        return ERHIResult::Failed;
    case EVulkanLabPresentationPolicyResult::Invalid:
    case EVulkanLabPresentationPolicyResult::StateConflict:
    case EVulkanLabPresentationPolicyResult::NotFound:
        return ERHIResult::InvalidState;
    }
    return ERHIResult::Failed;
}

[[nodiscard]] bool IsPresentSuccess(VkResult Result) noexcept
{
    return Result == VK_SUCCESS || Result == VK_SUBOPTIMAL_KHR;
}

[[nodiscard]] bool IsAcquireRetry(VkResult Result) noexcept
{
    return Result == VK_NOT_READY || Result == VK_TIMEOUT;
}

[[nodiscard]] bool IsZeroExtent(const FVulkanLabSwapchainCreateDesc& Desc) noexcept
{
    return Desc.Width == 0 || Desc.Height == 0;
}

[[nodiscard]] bool TryEstimateColorBytes(
    const FVulkanLabSwapchainCreateDesc& Desc,
    uint64& OutBytes) noexcept
{
    OutBytes = 0;
    const Stoner::RHI::FRHIFormatInfo Info =
        Stoner::RHI::GetRHIFormatInfo(Desc.ColorFormat);
    if (!Info.IsValid() || Info.bCompressed || Info.bDepthStencil ||
        Info.BytesPerBlock == 0 || Desc.Width == 0 || Desc.Height == 0 ||
        Desc.Width > MaxDrawableAxis || Desc.Height > MaxDrawableAxis ||
        Desc.MinImageCount == 0 || Desc.MinImageCount > MaxImageCount)
    {
        return false;
    }
    constexpr uint64 MaxValue = std::numeric_limits<uint64>::max();
    if (static_cast<uint64>(Desc.Width) > MaxValue / Desc.Height)
    {
        return false;
    }
    const uint64 Pixels = static_cast<uint64>(Desc.Width) * Desc.Height;
    if (Pixels > MaxDrawablePixels ||
        Pixels > MaxValue / Info.BytesPerBlock ||
        Pixels * Info.BytesPerBlock > MaxValue / Desc.MinImageCount)
    {
        return false;
    }
    OutBytes = Pixels * Info.BytesPerBlock * Desc.MinImageCount;
    return OutBytes <= FVulkanLabPresentationPolicy::MaxEstimatedColorBytes;
}

} // namespace

FVulkanLabSwapchainRuntime::FVulkanLabSwapchainRuntime(
    VkPhysicalDevice InPhysicalDevice,
    VkDevice InDevice,
    VkSurfaceKHR InSurface,
    VkQueue InQueue,
    uint32 InQueueFamily,
    const FVulkanLabPresentationPolicySelection& InSelection)
    : PhysicalDevice_(InPhysicalDevice)
    , Device_(InDevice)
    , Surface_(InSurface)
    , Queue_(InQueue)
    , QueueFamily_(InQueueFamily)
    , Selection_(InSelection)
    , Policy_(InSelection.Mode)
{
}

FVulkanLabSwapchainRuntime::~FVulkanLabSwapchainRuntime()
{
    // Native destruction is deliberately explicit.  A destructor cannot
    // establish presentation retirement, and it must not race queue work or
    // silently discard pending image owners.
}

void FVulkanLabSwapchainRuntime::SetPresentationRetirementCallback(
    FVulkanLabPresentationRetirementCallback Callback,
    void* UserData) noexcept
{
    RetirementCallback_ = Callback;
    RetirementCallbackUserData_ = UserData;
}

void FVulkanLabSwapchainRuntime::NotifyPresentationRetirement(
    EVulkanLabPresentationRetirementEvent Event,
    uint64 AcquisitionToken,
    uint64 Generation,
    uint32 ImageIndex) noexcept
{
    if (RetirementCallback_ != nullptr)
    {
        RetirementCallback_(
            RetirementCallbackUserData_, static_cast<Stoner::Core::uint8>(Event),
            AcquisitionToken,
            Generation, ImageIndex);
    }
}

ERHIResult FVulkanLabSwapchainRuntime::MapNativeResult(VkResult Result) noexcept
{
    const bool bRecoverableStatus =
        Result == VK_NOT_READY || Result == VK_TIMEOUT ||
        Result == VK_SUBOPTIMAL_KHR || Result == VK_ERROR_OUT_OF_DATE_KHR ||
        Result == VK_ERROR_SURFACE_LOST_KHR;
    if (Result != VK_SUCCESS && !bRecoverableStatus &&
        FirstNativeFailure_ == VK_SUCCESS)
    {
        FirstNativeFailure_ = Result;
    }
    if (Result == VK_ERROR_DEVICE_LOST)
    {
        // Device loss is terminal even when the caller has not yet reached
        // the explicit cleanup phase. Preserve that fact in both layers.
        bDeviceLost_ = true;
        bFailed_ = true;
        FailureResult_ = ERHIResult::Failed;
        Policy_.MarkDeviceLost();
    }
    switch (Result)
    {
    case VK_SUCCESS:
        return ERHIResult::Success;
    case VK_NOT_READY:
    case VK_TIMEOUT:
        return ERHIResult::NotReady;
    case VK_SUBOPTIMAL_KHR:
        return ERHIResult::Success;
    case VK_ERROR_OUT_OF_DATE_KHR:
        return ERHIResult::ResizeRequired;
    case VK_ERROR_SURFACE_LOST_KHR:
        return ERHIResult::Unavailable;
    case VK_ERROR_DEVICE_LOST:
    case VK_ERROR_OUT_OF_HOST_MEMORY:
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
    default:
        return ERHIResult::Failed;
    }
}

void FVulkanLabSwapchainRuntime::LatchFailure(
    ERHIResult Result,
    VkResult NativeResult) noexcept
{
    const bool bRecoverableStatus =
        NativeResult == VK_NOT_READY || NativeResult == VK_TIMEOUT ||
        NativeResult == VK_SUBOPTIMAL_KHR ||
        NativeResult == VK_ERROR_OUT_OF_DATE_KHR ||
        NativeResult == VK_ERROR_SURFACE_LOST_KHR;
    if (NativeResult != VK_SUCCESS && !bRecoverableStatus &&
        FirstNativeFailure_ == VK_SUCCESS)
    {
        FirstNativeFailure_ = NativeResult;
    }
    if (NativeResult == VK_ERROR_DEVICE_LOST)
    {
        bDeviceLost_ = true;
        Policy_.MarkDeviceLost();
    }
    if (Result == ERHIResult::Success || Result == ERHIResult::NotReady ||
        Result == ERHIResult::ResizeRequired)
    {
        return;
    }
    if (!bFailed_)
    {
        bFailed_ = true;
        FailureResult_ = Result;
    }
}

FVulkanLabSwapchainRuntime::FNativeGeneration*
FVulkanLabSwapchainRuntime::FindGeneration(uint64 Generation) noexcept
{
    for (FNativeGeneration& Candidate : Generations_)
    {
        if (Candidate.bOccupied && Candidate.Desc.Generation == Generation)
        {
            return &Candidate;
        }
    }
    return nullptr;
}

const FVulkanLabSwapchainRuntime::FNativeGeneration*
FVulkanLabSwapchainRuntime::FindGeneration(uint64 Generation) const noexcept
{
    for (const FNativeGeneration& Candidate : Generations_)
    {
        if (Candidate.bOccupied && Candidate.Desc.Generation == Generation)
        {
            return &Candidate;
        }
    }
    return nullptr;
}

FVulkanLabSwapchainRuntime::FNativeAcquire*
FVulkanLabSwapchainRuntime::FindAcquire(uint64 AcquisitionToken) noexcept
{
    for (FNativeAcquire& Candidate : Acquires_)
    {
        if (Candidate.bOccupied &&
            Candidate.AcquisitionToken == AcquisitionToken)
        {
            return &Candidate;
        }
    }
    return nullptr;
}

const FVulkanLabSwapchainRuntime::FNativeAcquire*
FVulkanLabSwapchainRuntime::FindAcquire(uint64 AcquisitionToken) const noexcept
{
    for (const FNativeAcquire& Candidate : Acquires_)
    {
        if (Candidate.bOccupied &&
            Candidate.AcquisitionToken == AcquisitionToken)
        {
            return &Candidate;
        }
    }
    return nullptr;
}

FVulkanLabSwapchainRuntime::FNativeAcquire*
FVulkanLabSwapchainRuntime::FindPendingAcquire(
    uint64 FrameToken,
    uint32 FrameSlotIndex) noexcept
{
    for (FNativeAcquire& Candidate : PendingAcquires_)
    {
        if (Candidate.bOccupied && Candidate.bPending &&
            Candidate.FrameToken == FrameToken &&
            Candidate.FrameSlotIndex == FrameSlotIndex)
        {
            return &Candidate;
        }
    }
    return nullptr;
}

FVulkanLabSwapchainRuntime::FNativeAcquire*
FVulkanLabSwapchainRuntime::FindTokenRecord(
    uint64 AcquisitionToken,
    bool& bOutReacquisition) noexcept
{
    bOutReacquisition = false;
    for (FNativeAcquire& Candidate : Acquires_)
    {
        if (!Candidate.bOccupied)
        {
            continue;
        }
        if (Candidate.AcquisitionToken == AcquisitionToken)
        {
            return &Candidate;
        }
        if (Candidate.bHasReacquisition &&
            Candidate.ReacquisitionToken == AcquisitionToken)
        {
            bOutReacquisition = true;
            return &Candidate;
        }
    }
    return nullptr;
}

const FVulkanLabSwapchainRuntime::FNativeAcquire*
FVulkanLabSwapchainRuntime::FindTokenRecord(
    uint64 AcquisitionToken,
    bool& bOutReacquisition) const noexcept
{
    bOutReacquisition = false;
    for (const FNativeAcquire& Candidate : Acquires_)
    {
        if (!Candidate.bOccupied)
        {
            continue;
        }
        if (Candidate.AcquisitionToken == AcquisitionToken)
        {
            return &Candidate;
        }
        if (Candidate.bHasReacquisition &&
            Candidate.ReacquisitionToken == AcquisitionToken)
        {
            bOutReacquisition = true;
            return &Candidate;
        }
    }
    return nullptr;
}

FVulkanLabSwapchainRuntime::FNativeAcquire*
FVulkanLabSwapchainRuntime::FindFreeAcquire() noexcept
{
    for (FNativeAcquire& Candidate : Acquires_)
    {
        if (!Candidate.bOccupied)
        {
            return &Candidate;
        }
    }
    return nullptr;
}

ERHIResult FVulkanLabSwapchainRuntime::CreateAcquireSync(
    FNativeAcquire& Acquire) noexcept
{
    if (Device_ == VK_NULL_HANDLE ||
        Acquire.AcquireSemaphore != VK_NULL_HANDLE ||
        Acquire.AcquireFence != VK_NULL_HANDLE)
    {
        return ERHIResult::InvalidState;
    }

    VkSemaphoreCreateInfo SemaphoreInfo = MakeVulkanStruct<VkSemaphoreCreateInfo>(
        VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
    VkFenceCreateInfo FenceInfo = MakeVulkanStruct<VkFenceCreateInfo>(
        VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
    VkResult NativeResult = vkCreateSemaphore(
        Device_, &SemaphoreInfo, nullptr, &Acquire.AcquireSemaphore);
    if (NativeResult != VK_SUCCESS)
    {
        Acquire.AcquireSemaphore = VK_NULL_HANDLE;
        return MapNativeResult(NativeResult);
    }
    NativeResult = vkCreateFence(
        Device_, &FenceInfo, nullptr, &Acquire.AcquireFence);
    if (NativeResult != VK_SUCCESS)
    {
        vkDestroySemaphore(Device_, Acquire.AcquireSemaphore, nullptr);
        Acquire.AcquireSemaphore = VK_NULL_HANDLE;
        Acquire.AcquireFence = VK_NULL_HANDLE;
        return MapNativeResult(NativeResult);
    }
    return ERHIResult::Success;
}

void FVulkanLabSwapchainRuntime::DestroyAcquireSync(
    FNativeAcquire& Acquire) noexcept
{
    const bool bHadNativeSync =
        Acquire.PresentationFence != VK_NULL_HANDLE ||
        Acquire.AcquireFence != VK_NULL_HANDLE ||
        Acquire.AcquireSemaphore != VK_NULL_HANDLE ||
        Acquire.ReacquisitionPresentationFence != VK_NULL_HANDLE ||
        Acquire.ReacquisitionAcquireFence != VK_NULL_HANDLE ||
        Acquire.ReacquisitionAcquireSemaphore != VK_NULL_HANDLE;
    if (bDeviceLost_ && bHadNativeSync)
    {
        ++AbandonedNativeOwnerCount_;
    }
    if (Device_ != VK_NULL_HANDLE && !bDeviceLost_)
    {
        if (Acquire.PresentationFence != VK_NULL_HANDLE)
        {
            vkDestroyFence(Device_, Acquire.PresentationFence, nullptr);
        }
        if (Acquire.AcquireFence != VK_NULL_HANDLE)
        {
            vkDestroyFence(Device_, Acquire.AcquireFence, nullptr);
        }
        if (Acquire.AcquireSemaphore != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(Device_, Acquire.AcquireSemaphore, nullptr);
        }
        if (Acquire.ReacquisitionPresentationFence != VK_NULL_HANDLE)
        {
            vkDestroyFence(
                Device_, Acquire.ReacquisitionPresentationFence, nullptr);
        }
        if (Acquire.ReacquisitionAcquireFence != VK_NULL_HANDLE)
        {
            vkDestroyFence(
                Device_, Acquire.ReacquisitionAcquireFence, nullptr);
        }
        if (Acquire.ReacquisitionAcquireSemaphore != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(
                Device_, Acquire.ReacquisitionAcquireSemaphore, nullptr);
        }
    }
    Acquire.PresentationFence = VK_NULL_HANDLE;
    Acquire.AcquireFence = VK_NULL_HANDLE;
    Acquire.AcquireSemaphore = VK_NULL_HANDLE;
    Acquire.ReacquisitionPresentationFence = VK_NULL_HANDLE;
    Acquire.ReacquisitionAcquireFence = VK_NULL_HANDLE;
    Acquire.ReacquisitionAcquireSemaphore = VK_NULL_HANDLE;
}

ERHIResult FVulkanLabSwapchainRuntime::ValidateSurfaceCompatibility(
    const FVulkanLabSwapchainCreateDesc& Desc,
    VkFormat& OutFormat,
    VkColorSpaceKHR& OutColorSpace) noexcept
{
    if (PhysicalDevice_ == VK_NULL_HANDLE || Surface_ == VK_NULL_HANDLE ||
        Desc.Width == 0 || Desc.Height == 0 ||
        Desc.Width > MaxDrawableAxis || Desc.Height > MaxDrawableAxis ||
        static_cast<uint64>(Desc.Width) * Desc.Height > MaxDrawablePixels)
    {
        return ERHIResult::InvalidState;
    }

    VkSurfaceCapabilitiesKHR Capabilities{};
    VkResult NativeResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        PhysicalDevice_, Surface_, &Capabilities);
    if (NativeResult != VK_SUCCESS)
    {
        return MapNativeResult(NativeResult);
    }
    if (Capabilities.minImageCount > MaxImageCount ||
        Desc.MinImageCount < Capabilities.minImageCount ||
        (Capabilities.maxImageCount != 0 &&
            Desc.MinImageCount > Capabilities.maxImageCount))
    {
        return ERHIResult::Unsupported;
    }
    if (Capabilities.currentExtent.width !=
            std::numeric_limits<uint32>::max() &&
        (Capabilities.currentExtent.width != Desc.Width ||
            Capabilities.currentExtent.height != Desc.Height))
    {
        return ERHIResult::ResizeRequired;
    }
    if (Desc.Width < Capabilities.minImageExtent.width ||
        Desc.Width > Capabilities.maxImageExtent.width ||
        Desc.Height < Capabilities.minImageExtent.height ||
        Desc.Height > Capabilities.maxImageExtent.height)
    {
        return ERHIResult::Unsupported;
    }
    if ((Capabilities.supportedUsageFlags & Desc.ImageUsage) !=
        Desc.ImageUsage ||
        (Capabilities.supportedTransforms & Desc.PreTransform) == 0 ||
        (Capabilities.supportedCompositeAlpha & Desc.CompositeAlpha) == 0)
    {
        return ERHIResult::Unsupported;
    }

    OutFormat = Desc.VulkanFormat == VK_FORMAT_UNDEFINED
        ? ToVulkanPresentationFormat(Desc.ColorFormat) : Desc.VulkanFormat;
    OutColorSpace = Desc.ColorSpace == VK_COLOR_SPACE_MAX_ENUM_KHR
        ? ToVulkanPresentationColorSpace(
            Stoner::RHI::ERHIPresentationColorSpace::SrgbNonlinear)
        : Desc.ColorSpace;
    if (OutFormat == VK_FORMAT_UNDEFINED ||
        OutColorSpace == VK_COLOR_SPACE_MAX_ENUM_KHR)
    {
        return ERHIResult::InvalidState;
    }

    uint32 SurfaceFormatCount = 0;
    NativeResult = vkGetPhysicalDeviceSurfaceFormatsKHR(
        PhysicalDevice_, Surface_, &SurfaceFormatCount, nullptr);
    if (NativeResult != VK_SUCCESS || SurfaceFormatCount == 0)
    {
        return MapNativeResult(NativeResult == VK_SUCCESS
            ? VK_ERROR_FORMAT_NOT_SUPPORTED : NativeResult);
    }
    if (SurfaceFormatCount > MaxSurfaceFormats)
    {
        return ERHIResult::Unsupported;
    }
    std::array<VkSurfaceFormatKHR, MaxSurfaceFormats> SurfaceFormats{};
    uint32 EnumeratedFormatCount = SurfaceFormatCount;
    NativeResult = vkGetPhysicalDeviceSurfaceFormatsKHR(
        PhysicalDevice_, Surface_, &EnumeratedFormatCount,
        SurfaceFormats.data());
    if (NativeResult != VK_SUCCESS || EnumeratedFormatCount == 0 ||
        EnumeratedFormatCount > SurfaceFormats.size())
    {
        return MapNativeResult(NativeResult == VK_SUCCESS
            ? VK_INCOMPLETE : NativeResult);
    }
    bool bFoundFormat = false;
    for (uint32 Index = 0; Index < EnumeratedFormatCount; ++Index)
    {
        if (SurfaceFormats[Index].format == OutFormat &&
            SurfaceFormats[Index].colorSpace == OutColorSpace)
        {
            bFoundFormat = true;
            break;
        }
    }
    if (!bFoundFormat)
    {
        return ERHIResult::Unsupported;
    }

    uint32 PresentModeCount = 0;
    NativeResult = vkGetPhysicalDeviceSurfacePresentModesKHR(
        PhysicalDevice_, Surface_, &PresentModeCount, nullptr);
    if (NativeResult != VK_SUCCESS || PresentModeCount == 0)
    {
        return MapNativeResult(NativeResult == VK_SUCCESS
            ? VK_ERROR_INITIALIZATION_FAILED : NativeResult);
    }
    if (PresentModeCount > MaxPresentModes)
    {
        return ERHIResult::Unsupported;
    }
    std::array<VkPresentModeKHR, MaxPresentModes> PresentModes{};
    uint32 EnumeratedPresentModeCount = PresentModeCount;
    NativeResult = vkGetPhysicalDeviceSurfacePresentModesKHR(
        PhysicalDevice_, Surface_, &EnumeratedPresentModeCount,
        PresentModes.data());
    if (NativeResult != VK_SUCCESS || EnumeratedPresentModeCount == 0 ||
        EnumeratedPresentModeCount > PresentModes.size())
    {
        return MapNativeResult(NativeResult == VK_SUCCESS
            ? VK_INCOMPLETE : NativeResult);
    }
    for (uint32 Index = 0; Index < EnumeratedPresentModeCount; ++Index)
    {
        if (PresentModes[Index] == Desc.PresentMode)
        {
            return ERHIResult::Success;
        }
    }
    return ERHIResult::Unsupported;
}

ERHIResult FVulkanLabSwapchainRuntime::CreateGeneration(
    const FVulkanLabSwapchainCreateDesc& Desc,
    VkSwapchainKHR OldSwapchain,
    FNativeGeneration& OutGeneration) noexcept
{
    OutGeneration = {};
    if (Device_ == VK_NULL_HANDLE || PhysicalDevice_ == VK_NULL_HANDLE ||
        Surface_ == VK_NULL_HANDLE || Desc.Generation == 0 ||
        IsZeroExtent(Desc) || Desc.MinImageCount == 0 ||
        Desc.MinImageCount > MaxImageCount ||
        Desc.ColorFormat == Stoner::RHI::ERHIFormat::Unknown)
    {
        return ERHIResult::InvalidState;
    }

    VkFormat RequestedFormat = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR RequestedColorSpace = VK_COLOR_SPACE_MAX_ENUM_KHR;
    const ERHIResult Compatibility = ValidateSurfaceCompatibility(
        Desc, RequestedFormat, RequestedColorSpace);
    if (Compatibility != ERHIResult::Success)
    {
        return Compatibility;
    }
    VkResult NativeResult = VK_SUCCESS;

    VkSwapchainCreateInfoKHR SwapchainInfo = MakeVulkanStruct<
        VkSwapchainCreateInfoKHR>(VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR);
    SwapchainInfo.surface = Surface_;
    SwapchainInfo.minImageCount = Desc.MinImageCount;
    SwapchainInfo.imageFormat = RequestedFormat;
    SwapchainInfo.imageColorSpace = RequestedColorSpace;
    SwapchainInfo.imageExtent = {Desc.Width, Desc.Height};
    SwapchainInfo.imageArrayLayers = 1;
    SwapchainInfo.imageUsage = Desc.ImageUsage;
    SwapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SwapchainInfo.preTransform = Desc.PreTransform;
    SwapchainInfo.compositeAlpha = Desc.CompositeAlpha;
    SwapchainInfo.presentMode = Desc.PresentMode;
    SwapchainInfo.clipped = Desc.bClipped ? VK_TRUE : VK_FALSE;
    SwapchainInfo.oldSwapchain = OldSwapchain;
    NativeResult = vkCreateSwapchainKHR(
        Device_, &SwapchainInfo, nullptr, &OutGeneration.Swapchain);
    if (NativeResult != VK_SUCCESS)
    {
        OutGeneration.Swapchain = VK_NULL_HANDLE;
        return MapNativeResult(NativeResult);
    }

    // The implementation may return a count different from minImageCount.
    // Keep the actual count and let policy admission/completion validate the
    // bounded image and aggregate-color budgets before publishing records.
    uint32 ImageCount = 0;
    NativeResult = vkGetSwapchainImagesKHR(
        Device_, OutGeneration.Swapchain, &ImageCount, nullptr);
    if (NativeResult != VK_SUCCESS || ImageCount == 0 ||
        ImageCount > MaxImageCount)
    {
        DestroyGeneration(OutGeneration);
        if (NativeResult != VK_SUCCESS)
        {
            return MapNativeResult(NativeResult);
        }
        // A successful enumeration with no images is an invalid native
        // result, and must not be mapped back to ERHIResult::Success.
        return ImageCount > MaxImageCount
            ? ERHIResult::Unsupported : ERHIResult::Failed;
    }
    std::array<VkImage, MaxImageCount> Images{};
    uint32 EnumeratedImageCount = ImageCount;
    NativeResult = vkGetSwapchainImagesKHR(
        Device_, OutGeneration.Swapchain, &EnumeratedImageCount, Images.data());
    if (NativeResult != VK_SUCCESS || EnumeratedImageCount != ImageCount)
    {
        DestroyGeneration(OutGeneration);
        return NativeResult != VK_SUCCESS
            ? MapNativeResult(NativeResult) : ERHIResult::Failed;
    }

    OutGeneration.Desc = Desc;
    OutGeneration.Desc.VulkanFormat = RequestedFormat;
    OutGeneration.Desc.ColorSpace = RequestedColorSpace;
    OutGeneration.ImageCount = ImageCount;
    for (uint32 Index = 0; Index < ImageCount; ++Index)
    {
        OutGeneration.Images[Index].Image = Images[Index];
        VkImageViewCreateInfo ViewInfo = MakeVulkanStruct<VkImageViewCreateInfo>(
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
        ViewInfo.image = Images[Index];
        ViewInfo.viewType = Desc.ImageViewType;
        ViewInfo.format = RequestedFormat;
        ViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ViewInfo.subresourceRange.baseMipLevel = 0;
        ViewInfo.subresourceRange.levelCount = 1;
        ViewInfo.subresourceRange.baseArrayLayer = 0;
        ViewInfo.subresourceRange.layerCount = 1;
        NativeResult = vkCreateImageView(
            Device_, &ViewInfo, nullptr,
            &OutGeneration.Images[Index].ImageView);
        if (NativeResult != VK_SUCCESS)
        {
            DestroyGeneration(OutGeneration);
            return MapNativeResult(NativeResult);
        }

        VkSemaphoreCreateInfo SemaphoreInfo = MakeVulkanStruct<
            VkSemaphoreCreateInfo>(VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
        NativeResult = vkCreateSemaphore(
            Device_, &SemaphoreInfo, nullptr,
            &OutGeneration.Images[Index].RenderFinishedSemaphore);
        if (NativeResult != VK_SUCCESS)
        {
            DestroyGeneration(OutGeneration);
            return MapNativeResult(NativeResult);
        }
    }

    uint64 TotalBytes = 0;
    auto CreatedDesc = OutGeneration.Desc;
    CreatedDesc.MinImageCount = OutGeneration.ImageCount;
    (void)TryEstimateColorBytes(CreatedDesc, TotalBytes);
    for (const auto& Existing : Generations_)
    {
        if (!Existing.bOccupied) continue;
        auto ExistingDesc = Existing.Desc;
        ExistingDesc.MinImageCount = Existing.ImageCount;
        uint64 Bytes = 0;
        (void)TryEstimateColorBytes(ExistingDesc, Bytes);
        TotalBytes += Bytes;
    }
    PeakEstimatedColorBytes_ = std::max(PeakEstimatedColorBytes_, TotalBytes);
    OutGeneration.bOccupied = true;
    return ERHIResult::Success;
}

void FVulkanLabSwapchainRuntime::DestroyGeneration(
    FNativeGeneration& Generation) noexcept
{
    const bool bHadNativeGeneration =
        Generation.bOccupied || Generation.Swapchain != VK_NULL_HANDLE;
    if (bDeviceLost_ && bHadNativeGeneration)
    {
        ++AbandonedNativeOwnerCount_;
    }
    if (Device_ != VK_NULL_HANDLE && !bDeviceLost_)
    {
        for (uint32 Index = 0; Index < Generation.ImageCount &&
            Index < Generation.Images.size(); ++Index)
        {
            if (Generation.Images[Index].RenderFinishedSemaphore !=
                VK_NULL_HANDLE)
            {
                vkDestroySemaphore(
                    Device_, Generation.Images[Index].RenderFinishedSemaphore,
                    nullptr);
            }
            if (Generation.Images[Index].ImageView != VK_NULL_HANDLE)
            {
                vkDestroyImageView(
                    Device_, Generation.Images[Index].ImageView, nullptr);
            }
        }
        if (Generation.Swapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(Device_, Generation.Swapchain, nullptr);
        }
    }
    Generation = {};
}

ERHIResult FVulkanLabSwapchainRuntime::Initialize(
    const FVulkanLabSwapchainCreateDesc& Desc) noexcept
{
    if (bInitialized_ || bTerminalCleanupStarted_ || bClosed_)
    {
        return ERHIResult::InvalidState;
    }
    if (PhysicalDevice_ == VK_NULL_HANDLE || Device_ == VK_NULL_HANDLE ||
        Surface_ == VK_NULL_HANDLE || Queue_ == VK_NULL_HANDLE ||
        !Selection_.IsUsable() ||
        (Selection_.Mode != Stoner::RHI::ERHIPresentationRetirementMode::PresentationFence &&
            Selection_.Mode != Stoner::RHI::ERHIPresentationRetirementMode::AcquireHistory))
    {
        return ERHIResult::InvalidState;
    }
    (void)QueueFamily_;
    if (IsZeroExtent(Desc))
    {
        bPausedZeroExtent_ = true;
        return ERHIResult::Unavailable;
    }
    if (Desc.Generation == 0 || Desc.MinImageCount == 0 ||
        Desc.MinImageCount > MaxImageCount || Desc.Width == 0 ||
        Desc.Height == 0 || Desc.ColorFormat == Stoner::RHI::ERHIFormat::Unknown)
    {
        return ERHIResult::InvalidState;
    }

    uint64 EstimatedBytes = 0;
    if (!TryEstimateColorBytes(Desc, EstimatedBytes))
    {
        return ERHIResult::Unsupported;
    }

    FNativeGeneration Created;
    const ERHIResult NativeResult = CreateGeneration(Desc, VK_NULL_HANDLE, Created);
    if (NativeResult != ERHIResult::Success)
    {
        // No borrowed record was published.  Keep policy/native state
        // coherent by entering the sticky failure path; explicit terminal
        // cleanup still retains any owners created before the error.
        LatchFailure(NativeResult == ERHIResult::ResizeRequired
            ? ERHIResult::Failed : NativeResult);
        bInitialized_ = true;
        return NativeResult;
    }
    FVulkanLabPresentationGenerationDesc PolicyDesc;
    PolicyDesc.Generation = Desc.Generation;
    // Use the count returned by vkGetSwapchainImagesKHR for authoritative
    // policy budget/admission; it may differ from the requested minimum.
    PolicyDesc.ImageCount = Created.ImageCount;
    PolicyDesc.Width = Created.Desc.Width;
    PolicyDesc.Height = Created.Desc.Height;
    PolicyDesc.ColorFormat = Created.Desc.ColorFormat;
    const EVulkanLabPresentationPolicyResult Admission =
        Policy_.AdmitInitialGeneration(PolicyDesc);
    if (Admission != EVulkanLabPresentationPolicyResult::Accepted)
    {
        DestroyGeneration(Created);
        const ERHIResult Mapped = MapPolicyResult(Admission);
        LatchFailure(Mapped);
        bInitialized_ = true;
        return Mapped;
    }
    FNativeGeneration* Destination = nullptr;
    for (FNativeGeneration& Candidate : Generations_)
    {
        if (!Candidate.bOccupied)
        {
            Destination = &Candidate;
            break;
        }
    }
    if (Destination == nullptr)
    {
        DestroyGeneration(Created);
        LatchFailure(ERHIResult::Unsupported);
        bInitialized_ = true;
        return ERHIResult::Unsupported;
    }
    *Destination = Created;
    Destination->bActive = true;
    bInitialized_ = true;
    bPausedZeroExtent_ = false;
    return ERHIResult::Success;
}

ERHIResult FVulkanLabSwapchainRuntime::RequestResize(
    const FVulkanLabSwapchainCreateDesc& Desc) noexcept
{
    if (!bInitialized_ || bTerminalCleanupStarted_ || bClosed_ || bFailed_)
    {
        return bFailed_ ? FailureResult_ : ERHIResult::InvalidState;
    }
    if (IsZeroExtent(Desc))
    {
        // The active and retiring generations remain owned.  No swapchain is
        // allocated and no acquired image is cancelled by a zero extent.
        bPausedZeroExtent_ = true;
        bHasPendingResize_ = false;
        PendingResize_ = {};
        return ERHIResult::Success;
    }
    if (Desc.Generation == 0 || Desc.MinImageCount == 0 ||
        Desc.MinImageCount > MaxImageCount || Desc.Width == 0 ||
        Desc.Height == 0 || Desc.ColorFormat == Stoner::RHI::ERHIFormat::Unknown)
    {
        return ERHIResult::InvalidState;
    }

    FVulkanLabPresentationGenerationDesc PolicyDesc;
    PolicyDesc.Generation = Desc.Generation;
    PolicyDesc.ImageCount = Desc.MinImageCount;
    PolicyDesc.Width = Desc.Width;
    PolicyDesc.Height = Desc.Height;
    PolicyDesc.ColorFormat = Desc.ColorFormat;
    const EVulkanLabPresentationPolicyResult Result =
        Policy_.QueueReplacement(PolicyDesc);
    if (Result == EVulkanLabPresentationPolicyResult::GenerationLimit)
    {
        // Keep the existing generation usable and let the caller continue
        // servicing events while the predecessor cap drains.
        return ERHIResult::NotReady;
    }
    const ERHIResult Mapped = MapPolicyResult(Result);
    if (Mapped == ERHIResult::Success)
    {
        PendingResize_ = Desc;
        bHasPendingResize_ = true;
        bPausedZeroExtent_ = false;
    }
    return Mapped;
}

ERHIResult FVulkanLabSwapchainRuntime::TryAcquire(
    FNativeAcquire& Acquire) noexcept
{
    FNativeGeneration* Generation = FindGeneration(Acquire.Generation);
    if (Acquire.bAcquired)
    {
        // A timeout-zero acquire may be held while a preferred-mode
        // predecessor fence is proved. Do not issue vkAcquireNextImageKHR a
        // second time for that already-owned image.
        return Generation != nullptr && Generation->bActive
            ? ERHIResult::Success : ERHIResult::InvalidState;
    }
    if (Generation == nullptr || !Generation->bActive ||
        Generation->Swapchain == VK_NULL_HANDLE ||
        Acquire.AcquireSemaphore == VK_NULL_HANDLE ||
        Acquire.AcquireFence == VK_NULL_HANDLE)
    {
        return ERHIResult::InvalidState;
    }
    uint32 ImageIndex = 0;
    const VkResult NativeResult = vkAcquireNextImageKHR(
        Device_, Generation->Swapchain, 0, Acquire.AcquireSemaphore,
        Acquire.AcquireFence, &ImageIndex);
    if (IsAcquireRetry(NativeResult))
    {
        Acquire.bPending = true;
        Acquire.bAcquired = false;
        return ERHIResult::NotReady;
    }
    if (NativeResult != VK_SUCCESS && NativeResult != VK_SUBOPTIMAL_KHR)
    {
        Acquire.bPending = false;
        Acquire.bAcquired = false;
        return MapNativeResult(NativeResult);
    }
    if (ImageIndex >= Generation->ImageCount || ImageIndex >= MaxImageCount)
    {
        Acquire.bPending = false;
        Acquire.bAcquired = true;
        return ERHIResult::Failed;
    }

    Acquire.bPending = false;
    Acquire.bAcquired = true;
    Acquire.ImageIndex = ImageIndex;
    return ERHIResult::Success;
}

ERHIResult FVulkanLabSwapchainRuntime::PublishPendingAcquire(
    FNativeAcquire& Pending) noexcept
{
    FNativeGeneration* Generation = FindGeneration(Pending.Generation);
    if (Generation == nullptr || !Generation->bActive ||
        !Pending.bAcquired || Pending.ImageIndex >= Generation->ImageCount)
    {
        return ERHIResult::InvalidState;
    }

    FNativeAcquire* Predecessor = nullptr;
    for (FNativeAcquire& Candidate : Acquires_)
    {
        if (Candidate.bOccupied && Candidate.bAcquired &&
            Candidate.Generation == Pending.Generation &&
            Candidate.ImageIndex == Pending.ImageIndex &&
            Candidate.bPresentQueued)
        {
            Predecessor = &Candidate;
            break;
        }
    }

    if (Predecessor != nullptr)
    {
        if (Selection_.Mode ==
            Stoner::RHI::ERHIPresentationRetirementMode::PresentationFence)
        {
            // Preferred mode does not use acquire-history proof. If Vulkan
            // returns the same image before the predecessor fence was polled,
            // keep the acquired owner in its pending slot until that actual
            // presentation fence and the predecessor render report agree.
            if (!Predecessor->bRenderComplete ||
                !Predecessor->bRenderSucceeded)
            {
                Pending.bPending = true;
                return ERHIResult::NotReady;
            }
            if (!Predecessor->bPresentAttempted ||
                !Predecessor->bPresentQueued ||
                Predecessor->PresentationFence == VK_NULL_HANDLE)
            {
                LatchFailure(ERHIResult::Failed);
                return ERHIResult::Failed;
            }
            const VkResult FenceResult = vkGetFenceStatus(
                Device_, Predecessor->PresentationFence);
            if (FenceResult == VK_NOT_READY)
            {
                Pending.bPending = true;
                return ERHIResult::NotReady;
            }
            if (FenceResult != VK_SUCCESS)
            {
                const ERHIResult Mapped = MapNativeResult(FenceResult);
                LatchFailure(Mapped, FenceResult);
                return Mapped;
            }
            const EVulkanLabPresentationPolicyResult FencePolicyResult =
                Policy_.MarkPresentationFenceComplete(
                    Predecessor->Generation, Predecessor->ImageIndex,
                    Predecessor->AcquisitionToken);
            if (FencePolicyResult !=
                EVulkanLabPresentationPolicyResult::Accepted)
            {
                const ERHIResult Mapped = MapPolicyResult(FencePolicyResult);
                LatchFailure(Mapped);
                return Mapped;
            }
            Predecessor->bPresentationRetired = true;
            NotifyPresentationRetirement(
                EVulkanLabPresentationRetirementEvent::PresentationProof,
                Predecessor->AcquisitionToken, Predecessor->Generation,
                Predecessor->ImageIndex);
            ReleaseCompletedAcquire(*Predecessor);
            Predecessor = nullptr;
        }
    }

    const EVulkanLabPresentationPolicyResult PolicyResult =
        Policy_.RecordAcquisition(
            Pending.Generation, Pending.ImageIndex, Pending.AcquisitionToken);
    if (PolicyResult != EVulkanLabPresentationPolicyResult::Accepted)
    {
        // Vulkan has transferred image ownership already. Keep this pending
        // object alive and fail closed instead of recycling its semaphore.
        LatchFailure(ERHIResult::Failed);
        return ERHIResult::Failed;
    }

    if (Predecessor != nullptr)
    {
        // Merge the new operation into the predecessor's one native image
        // record. This mirrors the policy's reacquisition fields and avoids a
        // seventeenth history record at the 8+8 generation boundary.
        if (Predecessor->bHasReacquisition)
        {
            LatchFailure(ERHIResult::Failed);
            return ERHIResult::Failed;
        }
        Predecessor->bHasReacquisition = true;
        Predecessor->ReacquisitionToken = Pending.AcquisitionToken;
        Predecessor->ReacquisitionFrameToken = Pending.FrameToken;
        Predecessor->ReacquisitionFrameSlotIndex = Pending.FrameSlotIndex;
        Predecessor->ReacquisitionAcquireSemaphore =
            Pending.AcquireSemaphore;
        Predecessor->ReacquisitionAcquireFence = Pending.AcquireFence;
        Pending.AcquireSemaphore = VK_NULL_HANDLE;
        Pending.AcquireFence = VK_NULL_HANDLE;
        Predecessor->bReacquisitionAcquireFenceComplete = false;
        Predecessor->bReacquisitionAcquireSyncReported = false;
        // The pending slot is an operation slot, not the history record that
        // now owns the native synchronization. Clear every moved-from bit so
        // a later call cannot rediscover this token and retry its acquire.
        Pending = {};
        return ERHIResult::Success;
    }

    FNativeAcquire* Destination = FindFreeAcquire();
    if (Destination == nullptr)
    {
        // This is only possible when Vulkan returned an image for which no
        // fixed policy record exists. Keep the acquired owner and fail closed.
        LatchFailure(ERHIResult::Failed);
        return ERHIResult::Failed;
    }
    *Destination = Pending;
    Destination->bPending = false;
    Destination->bOccupied = true;
    Pending = {};
    return ERHIResult::Success;
}

ERHIResult FVulkanLabSwapchainRuntime::FillNativeImageRecord(
    const FNativeAcquire& Acquire,
    uint64 AcquisitionToken,
    FVulkanLabNativeImageRecord& OutRecord) const noexcept
{
    OutRecord = {};
    const FNativeGeneration* Generation = FindGeneration(Acquire.Generation);
    if (Generation == nullptr || !Acquire.bOccupied || !Acquire.bAcquired ||
        Acquire.ImageIndex >= Generation->ImageCount)
    {
        return ERHIResult::InvalidState;
    }
    bool bReacquisition = AcquisitionToken == Acquire.ReacquisitionToken &&
        Acquire.bHasReacquisition;
    if ((AcquisitionToken != Acquire.AcquisitionToken && !bReacquisition) ||
        (bReacquisition ? Acquire.bReacquisitionCanceled : Acquire.bCanceled))
    {
        // Cancellation retains native ownership but ends the caller's right
        // to acquire, submit or present this logical target again.
        return ERHIResult::InvalidState;
    }
    const FNativeImage& Image = Generation->Images[Acquire.ImageIndex];
    OutRecord.Generation = Acquire.Generation;
    OutRecord.AcquisitionToken = AcquisitionToken;
    OutRecord.FrameToken = bReacquisition
        ? Acquire.ReacquisitionFrameToken : Acquire.FrameToken;
    OutRecord.FrameSlotIndex = bReacquisition
        ? Acquire.ReacquisitionFrameSlotIndex : Acquire.FrameSlotIndex;
    OutRecord.ImageIndex = Acquire.ImageIndex;
    OutRecord.Width = Generation->Desc.Width;
    OutRecord.Height = Generation->Desc.Height;
    OutRecord.ColorFormat = Generation->Desc.ColorFormat;
    OutRecord.Swapchain = Generation->Swapchain;
    OutRecord.Image = Image.Image;
    OutRecord.ImageView = Image.ImageView;
    OutRecord.AcquireSemaphore = bReacquisition
        ? Acquire.ReacquisitionAcquireSemaphore : Acquire.AcquireSemaphore;
    OutRecord.RenderFinishedSemaphore = Image.RenderFinishedSemaphore;
    return OutRecord.IsValid() ? ERHIResult::Success : ERHIResult::InvalidState;
}

ERHIResult FVulkanLabSwapchainRuntime::Acquire(
    uint64 FrameToken,
    uint32 FrameSlotIndex,
    FVulkanLabNativeImageRecord& OutRecord) noexcept
{
    OutRecord = {};
    if (!bInitialized_ || bTerminalCleanupStarted_ || bClosed_ ||
        bFailed_ || FrameToken == 0 || FrameSlotIndex >= 2)
    {
        return bFailed_ ? FailureResult_ : ERHIResult::InvalidState;
    }
    if (bPausedZeroExtent_)
    {
        return ERHIResult::NotReady;
    }
    FNativeGeneration* Active = nullptr;
    for (FNativeGeneration& Candidate : Generations_)
    {
        if (Candidate.bOccupied && Candidate.bActive)
        {
            Active = &Candidate;
            break;
        }
    }
    if (Active == nullptr)
    {
        return ERHIResult::Unavailable;
    }

    FNativeAcquire* AcquireRecord =
        FindPendingAcquire(FrameToken, FrameSlotIndex);
    if (AcquireRecord == nullptr)
    {
        // An already-published image is returned idempotently for the same
        // frame token, including a reacquisition record held in-place.
        for (FNativeAcquire& Candidate : Acquires_)
        {
            if (Candidate.bOccupied && Candidate.bAcquired &&
                ((Candidate.FrameToken == FrameToken &&
                    Candidate.FrameSlotIndex == FrameSlotIndex) ||
                 (Candidate.bHasReacquisition &&
                    Candidate.ReacquisitionFrameToken == FrameToken &&
                    Candidate.ReacquisitionFrameSlotIndex == FrameSlotIndex)))
            {
                const uint64 Token = Candidate.FrameToken == FrameToken
                    ? Candidate.AcquisitionToken : Candidate.ReacquisitionToken;
                return FillNativeImageRecord(Candidate, Token, OutRecord);
            }
        }
    }
    if (AcquireRecord == nullptr)
    {
        // Pending acquisition attempts are independent from per-image
        // history. Two slots can therefore poll while all sixteen history
        // records remain available for same-image retirement proof.
        for (FNativeAcquire& Candidate : PendingAcquires_)
        {
            if (!Candidate.bOccupied)
            {
                AcquireRecord = &Candidate;
                break;
            }
        }
        if (AcquireRecord == nullptr)
        {
            return ERHIResult::NotReady;
        }
        *AcquireRecord = {};
        AcquireRecord->bOccupied = true;
        AcquireRecord->Generation = Active->Desc.Generation;
        AcquireRecord->FrameToken = FrameToken;
        AcquireRecord->FrameSlotIndex = FrameSlotIndex;
        AcquireRecord->AcquisitionToken = NextAcquisitionToken_++;
        if (AcquireRecord->AcquisitionToken == 0)
        {
            AcquireRecord->AcquisitionToken = NextAcquisitionToken_++;
        }
        const ERHIResult SyncResult = CreateAcquireSync(*AcquireRecord);
        if (SyncResult != ERHIResult::Success)
        {
            *AcquireRecord = {};
            LatchFailure(SyncResult);
            return SyncResult;
        }
    }

    ERHIResult Result = TryAcquire(*AcquireRecord);
    if (Result == ERHIResult::NotReady)
    {
        return Result;
    }
    if (Result != ERHIResult::Success)
    {
        if (!AcquireRecord->bAcquired)
        {
            // No image was returned, and the timeout-zero native call has
            // completed. Its private acquire synchronization can be retired
            // without making an image-release claim.
            DestroyAcquireSync(*AcquireRecord);
            *AcquireRecord = {};
        }
        if (Result != ERHIResult::ResizeRequired &&
            Result != ERHIResult::Unavailable)
        {
            LatchFailure(Result);
        }
        return Result;
    }

    const uint64 AcquiredToken = AcquireRecord->AcquisitionToken;
    Result = PublishPendingAcquire(*AcquireRecord);
    if (Result != ERHIResult::Success)
    {
        return Result;
    }
    bool bReacquisition = false;
    FNativeAcquire* Published =
        FindTokenRecord(AcquiredToken, bReacquisition);
    // PublishPendingAcquire clears a standalone pending object. Preserve the
    // token before clearing and find it in the merged/per-image history.
    if (Published == nullptr)
    {
        for (FNativeAcquire& Candidate : Acquires_)
        {
            if (Candidate.bOccupied &&
                ((Candidate.AcquisitionToken == AcquiredToken) ||
                 (Candidate.bHasReacquisition &&
                    Candidate.ReacquisitionToken == AcquiredToken)))
            {
                Published = &Candidate;
                bReacquisition =
                    Candidate.ReacquisitionToken == AcquiredToken;
                break;
            }
        }
    }
    if (Published == nullptr)
    {
        // The source pending record may have been moved into a free history
        // slot. Find it by frame token as the final bounded lookup.
        for (FNativeAcquire& Candidate : Acquires_)
        {
            if (Candidate.bOccupied && Candidate.FrameToken == FrameToken &&
                Candidate.FrameSlotIndex == FrameSlotIndex)
            {
                Published = &Candidate;
                break;
            }
        }
    }
    if (Published == nullptr)
    {
        LatchFailure(ERHIResult::Failed);
        return ERHIResult::Failed;
    }
    const uint64 PublishedToken = bReacquisition
        ? Published->ReacquisitionToken :
        (Published->FrameToken == FrameToken
            ? Published->AcquisitionToken : Published->ReacquisitionToken);
    return FillNativeImageRecord(*Published, PublishedToken, OutRecord);
}

void FVulkanLabSwapchainRuntime::PromoteReacquisition(
    FNativeAcquire& Acquire) noexcept
{
    if (!Acquire.bHasReacquisition || !Acquire.bRenderComplete ||
        !Acquire.bRenderSucceeded ||
        !Acquire.bReacquisitionAcquireSyncReported ||
        (Acquire.bReacquisitionRenderComplete &&
            !Acquire.bReacquisitionRenderSucceeded))
    {
        return;
    }

    // The predecessor render fence is complete and the acquire fence proved
    // the presentation engine waited for that image. Recycle only the old
    // acquire semaphore/fence, then make the in-place record describe the new
    // acquisition. Its render-finished semaphore remains image-indexed.
    if (Device_ != VK_NULL_HANDLE && !bDeviceLost_)
    {
        if (Acquire.PresentationFence != VK_NULL_HANDLE)
        {
            vkDestroyFence(Device_, Acquire.PresentationFence, nullptr);
        }
        if (Acquire.AcquireFence != VK_NULL_HANDLE)
        {
            vkDestroyFence(Device_, Acquire.AcquireFence, nullptr);
        }
        if (Acquire.AcquireSemaphore != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(Device_, Acquire.AcquireSemaphore, nullptr);
        }
    }
    Acquire.PresentationFence = Acquire.ReacquisitionPresentationFence;
    Acquire.AcquireFence = Acquire.ReacquisitionAcquireFence;
    Acquire.AcquireSemaphore = Acquire.ReacquisitionAcquireSemaphore;
    Acquire.ReacquisitionPresentationFence = VK_NULL_HANDLE;
    Acquire.ReacquisitionAcquireFence = VK_NULL_HANDLE;
    Acquire.ReacquisitionAcquireSemaphore = VK_NULL_HANDLE;
    Acquire.AcquisitionToken = Acquire.ReacquisitionToken;
    Acquire.FrameToken = Acquire.ReacquisitionFrameToken;
    Acquire.FrameSlotIndex = Acquire.ReacquisitionFrameSlotIndex;
    Acquire.bCanceled = Acquire.bReacquisitionCanceled;
    Acquire.bRenderSubmitted = Acquire.bReacquisitionRenderSubmitted;
    Acquire.bRenderComplete = Acquire.bReacquisitionRenderComplete;
    Acquire.bRenderSucceeded = Acquire.bReacquisitionRenderComplete &&
        Acquire.bReacquisitionRenderSucceeded;
    Acquire.bPresentAttempted =
        Acquire.bReacquisitionPresentAttempted;
    Acquire.bPresentQueued = Acquire.bReacquisitionPresentQueued;
    // The acquire-history proof retired the predecessor. The promoted
    // acquisition is still unpublished and must establish its own present
    // proof later.
    Acquire.bPresentationRetired = false;
    Acquire.bAcquireFenceComplete = Acquire.bReacquisitionAcquireFenceComplete;
    Acquire.bAcquireSyncReported =
        Acquire.bReacquisitionAcquireSyncReported;
    Acquire.bHasPredecessor = false;
    Acquire.PredecessorToken = 0;
    Acquire.bHasReacquisition = false;
    Acquire.bReacquisitionRenderSubmitted = false;
    Acquire.bReacquisitionRenderComplete = false;
    Acquire.bReacquisitionRenderSucceeded = false;
    Acquire.bReacquisitionCanceled = false;
    Acquire.bReacquisitionPresentAttempted = false;
    Acquire.bReacquisitionPresentQueued = false;
    Acquire.bReacquisitionAcquireFenceComplete = false;
    Acquire.bReacquisitionAcquireSyncReported = false;
    Acquire.bReacquisitionPresentationRetired = false;
    Acquire.ReacquisitionToken = 0;
    Acquire.ReacquisitionFrameToken = 0;
    Acquire.ReacquisitionFrameSlotIndex = 0;
}

void FVulkanLabSwapchainRuntime::ReleaseCompletedAcquire(
    FNativeAcquire& Acquire) noexcept
{
    if (!Acquire.bOccupied)
    {
        return;
    }
    if (Acquire.bHasReacquisition)
    {
        PromoteReacquisition(Acquire);
        return;
    }
    if (!Acquire.bAcquired || Acquire.bCanceled ||
        (Acquire.bPresentAttempted && !Acquire.bPresentQueued) ||
        !Acquire.bRenderComplete || !Acquire.bRenderSucceeded ||
        !Acquire.bPresentationRetired)
    {
        return;
    }
    DestroyAcquireSync(Acquire);
    Acquire = {};
}

ERHIResult FVulkanLabSwapchainRuntime::CancelUnpublishedAcquire(
    uint64 FrameToken, uint32 FrameSlotIndex) noexcept
{
    if (!bInitialized_ || bClosed_ || FrameToken == 0 || FrameSlotIndex >= 2)
        return ERHIResult::InvalidState;
    if (FindPendingAcquire(FrameToken, FrameSlotIndex))
        return CancelPendingAcquire(FrameToken, FrameSlotIndex);
    for (auto& A : Acquires_)
    {
        if (!A.bOccupied) continue;
        if (A.bHasReacquisition && A.ReacquisitionFrameToken == FrameToken &&
            A.ReacquisitionFrameSlotIndex == FrameSlotIndex)
        {
            if (A.bReacquisitionCanceled) return ERHIResult::Success;
            return Cancel(A.ReacquisitionToken);
        }
        if (A.FrameToken == FrameToken && A.FrameSlotIndex == FrameSlotIndex)
        {
            if (A.bCanceled) return ERHIResult::Success;
            return Cancel(A.AcquisitionToken);
        }
    }
    // The timeout-zero attempt already returned without acquiring an image.
    // The swapchain wrapper retains the exact attempted identity until here.
    return ERHIResult::Success;
}

ERHIResult FVulkanLabSwapchainRuntime::CancelPendingAcquire(
    uint64 FrameToken,
    uint32 FrameSlotIndex) noexcept
{
    if (!bInitialized_ || bClosed_ || FrameToken == 0 || FrameSlotIndex >= 2)
    {
        return ERHIResult::InvalidState;
    }
    FNativeAcquire* Pending = FindPendingAcquire(FrameToken, FrameSlotIndex);
    if (Pending == nullptr || !Pending->bPending)
    {
        return ERHIResult::InvalidState;
    }
    if (Pending->bAcquired)
    {
        // A preferred-mode same-image reacquisition can be held after
        // vkAcquireNextImageKHR has transferred ownership but before its old
        // presentation fence is proved. Cancellation cannot recycle that
        // native owner; stop retrying it and retain its fence for polling or
        // terminal cleanup.
        Pending->bCanceled = true;
        Pending->bPending = false;
        return ERHIResult::Success;
    }
    // vkAcquireNextImageKHR returned before this call, so the bounded
    // timeout-zero attempt has no asynchronous native worker to cancel.
    DestroyAcquireSync(*Pending);
    *Pending = {};
    return ERHIResult::Success;
}

ERHIResult FVulkanLabSwapchainRuntime::Cancel(uint64 AcquisitionToken) noexcept
{
    if (!bInitialized_ || bClosed_ || AcquisitionToken == 0)
    {
        return ERHIResult::InvalidState;
    }
    bool bReacquisition = false;
    FNativeAcquire* Acquire = FindTokenRecord(
        AcquisitionToken, bReacquisition);
    if (Acquire == nullptr || !Acquire->bAcquired)
    {
        return ERHIResult::InvalidState;
    }
    if (bReacquisition)
    {
        if (Acquire->bReacquisitionRenderSubmitted && !Acquire->bReacquisitionRenderComplete)
            return ERHIResult::NotReady;
        if (Acquire->bReacquisitionPresentQueued ||
            Acquire->bReacquisitionCanceled)
        {
            LatchFailure(ERHIResult::Failed);
            return ERHIResult::Failed;
        }
        const EVulkanLabPresentationPolicyResult PolicyResult =
            Policy_.CancelAcquisition(
                Acquire->Generation, Acquire->ImageIndex, AcquisitionToken);
        if (PolicyResult != EVulkanLabPresentationPolicyResult::Accepted)
        {
            return MapPolicyResult(PolicyResult);
        }
        Acquire->bReacquisitionCanceled = true;
        return ERHIResult::Success;
    }
    if (Acquire->bPresentQueued)
    {
        return ERHIResult::InvalidState;
    }
    if (Acquire->bRenderSubmitted && !Acquire->bRenderComplete)
        return ERHIResult::NotReady;
    const EVulkanLabPresentationPolicyResult PolicyResult =
        Policy_.CancelAcquisition(
            Acquire->Generation, Acquire->ImageIndex, AcquisitionToken);
    if (PolicyResult != EVulkanLabPresentationPolicyResult::Accepted)
    {
        return MapPolicyResult(PolicyResult);
    }
    Acquire->bCanceled = true;
    // Completed rendering permits logical cancellation. Keep the native
    // acquisition against its generation budget until real retirement; this
    // does not return an unpresented image or prove presentation completion.
    return ERHIResult::Success;
}

ERHIResult FVulkanLabSwapchainRuntime::ReportRenderSubmitted(
    uint64 AcquisitionToken) noexcept
{
    if (!bInitialized_ || bClosed_ || AcquisitionToken == 0)
    {
        return ERHIResult::InvalidState;
    }
    bool bReacquisition = false;
    FNativeAcquire* Acquire = FindTokenRecord(
        AcquisitionToken, bReacquisition);
    if (Acquire == nullptr || !Acquire->bAcquired || Acquire->bCanceled ||
        (bReacquisition && Acquire->bReacquisitionCanceled))
    {
        return ERHIResult::InvalidState;
    }
    const EVulkanLabPresentationPolicyResult PolicyResult =
        Policy_.MarkRenderSubmitted(
            Acquire->Generation, Acquire->ImageIndex, AcquisitionToken);
    if (PolicyResult != EVulkanLabPresentationPolicyResult::Accepted)
    {
        return MapPolicyResult(PolicyResult);
    }
    if (bReacquisition)
    {
        Acquire->bReacquisitionRenderSubmitted = true;
    }
    else
    {
        Acquire->bRenderSubmitted = true;
    }
    return ERHIResult::Success;
}

ERHIResult FVulkanLabSwapchainRuntime::QueuePresent(
    const FVulkanLabNativeImageRecord& Record) noexcept
{
    if (!bInitialized_ || bTerminalCleanupStarted_ || bClosed_ ||
        bFailed_ || !Record.IsValid())
    {
        return bFailed_ ? FailureResult_ : ERHIResult::InvalidState;
    }
    bool bReacquisition = false;
    FNativeAcquire* Acquire = FindTokenRecord(
        Record.AcquisitionToken, bReacquisition);
    if (Acquire == nullptr || !Acquire->bAcquired || Acquire->bCanceled)
    {
        return ERHIResult::InvalidState;
    }
    FVulkanLabNativeImageRecord Expected;
    if (FillNativeImageRecord(
            *Acquire, Record.AcquisitionToken, Expected) != ERHIResult::Success ||
        Expected.Generation != Record.Generation ||
        Expected.FrameToken != Record.FrameToken ||
        Expected.FrameSlotIndex != Record.FrameSlotIndex ||
        Expected.ImageIndex != Record.ImageIndex ||
        Expected.Swapchain != Record.Swapchain ||
        Expected.Image != Record.Image || Expected.ImageView != Record.ImageView ||
        Expected.AcquireSemaphore != Record.AcquireSemaphore ||
        Expected.RenderFinishedSemaphore != Record.RenderFinishedSemaphore)
    {
        return ERHIResult::InvalidState;
    }
    const bool bSubmitted = bReacquisition
        ? Acquire->bReacquisitionRenderSubmitted
        : Acquire->bRenderSubmitted;
    const bool bAlreadyAttempted = bReacquisition
        ? Acquire->bReacquisitionPresentAttempted
        : Acquire->bPresentAttempted;
    const bool bAlreadyPresented = bReacquisition
        ? Acquire->bReacquisitionPresentQueued : Acquire->bPresentQueued;
    if (!bSubmitted || bAlreadyAttempted || bAlreadyPresented)
    {
        return ERHIResult::InvalidState;
    }

    VkFence PresentationFence = VK_NULL_HANDLE;
#if defined(VK_EXT_swapchain_maintenance1)
    if (Selection_.Mode ==
        Stoner::RHI::ERHIPresentationRetirementMode::PresentationFence)
    {
        VkFenceCreateInfo FenceInfo = MakeVulkanStruct<VkFenceCreateInfo>(
            VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
        const VkResult FenceResult = vkCreateFence(
            Device_, &FenceInfo, nullptr, &PresentationFence);
        if (FenceResult != VK_SUCCESS)
        {
            LatchFailure(MapNativeResult(FenceResult));
            return MapNativeResult(FenceResult);
        }
    }
#else
    if (Selection_.Mode ==
        Stoner::RHI::ERHIPresentationRetirementMode::PresentationFence)
    {
        LatchFailure(ERHIResult::Unsupported);
        return ERHIResult::Unsupported;
    }
#endif

    VkSwapchainKHR Swapchain = Acquire->Generation == Record.Generation
        ? Record.Swapchain : VK_NULL_HANDLE;
    const uint32 ImageIndex = Record.ImageIndex;
    VkPresentInfoKHR Present = MakeVulkanStruct<VkPresentInfoKHR>(
        VK_STRUCTURE_TYPE_PRESENT_INFO_KHR);
    Present.waitSemaphoreCount = 1;
    Present.pWaitSemaphores = &Record.RenderFinishedSemaphore;
    Present.swapchainCount = 1;
    Present.pSwapchains = &Swapchain;
    Present.pImageIndices = &ImageIndex;
#if defined(VK_EXT_swapchain_maintenance1)
    VkSwapchainPresentFenceInfoEXT FenceInfo = MakeVulkanStruct<
        VkSwapchainPresentFenceInfoEXT>(
            VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT);
    if (PresentationFence != VK_NULL_HANDLE)
    {
        FenceInfo.swapchainCount = 1;
        FenceInfo.pFences = &PresentationFence;
        Present.pNext = &FenceInfo;
    }
#endif
    // Set this before entering the driver. An error return may still mean the
    // wait operation was accepted, so the token must never be retried.
    if (bReacquisition)
    {
        Acquire->bReacquisitionPresentAttempted = true;
    }
    else
    {
        Acquire->bPresentAttempted = true;
    }
    const VkResult NativeResult = vkQueuePresentKHR(Queue_, &Present);
    if (!IsPresentSuccess(NativeResult))
    {
        // The operation remains occupied. In particular, do not destroy the
        // image-indexed render semaphore after a present call returned an
        // error; a native driver may have accepted part of the operation.
        if (bReacquisition)
        {
            Acquire->ReacquisitionPresentationFence = PresentationFence;
        }
        else
        {
            Acquire->PresentationFence = PresentationFence;
        }
        const ERHIResult Mapped = MapNativeResult(NativeResult);
        if (Mapped != ERHIResult::ResizeRequired &&
            Mapped != ERHIResult::Unavailable)
        {
            LatchFailure(Mapped);
        }
        return Mapped;
    }

    const EVulkanLabPresentationPolicyResult PolicyResult =
        Policy_.RecordPresentationQueued(
            Acquire->Generation, Acquire->ImageIndex,
            Record.AcquisitionToken);
    if (PolicyResult != EVulkanLabPresentationPolicyResult::Accepted)
    {
        // Queue presentation already transferred native ownership, so the
        // fence/semaphore stays retained while the helper enters failure.
        if (bReacquisition)
        {
            Acquire->ReacquisitionPresentationFence = PresentationFence;
        }
        else
        {
            Acquire->PresentationFence = PresentationFence;
        }
        LatchFailure(ERHIResult::Failed);
        return ERHIResult::Failed;
    }
    if (bReacquisition)
    {
        Acquire->bReacquisitionPresentQueued = true;
        Acquire->ReacquisitionPresentationFence = PresentationFence;
    }
    else
    {
        Acquire->bPresentQueued = true;
        Acquire->PresentationFence = PresentationFence;
    }
    return ERHIResult::Success;
}

ERHIResult FVulkanLabSwapchainRuntime::ReportRenderComplete(
    uint64 AcquisitionToken,
    bool bSucceeded) noexcept
{
    if (!bInitialized_ || bClosed_ || AcquisitionToken == 0)
    {
        return ERHIResult::InvalidState;
    }
    bool bReacquisition = false;
    FNativeAcquire* Acquire = FindTokenRecord(
        AcquisitionToken, bReacquisition);
    if (Acquire == nullptr || !Acquire->bAcquired)
    {
        return ERHIResult::InvalidState;
    }
    if ((!bReacquisition && !Acquire->bRenderSubmitted) ||
        (bReacquisition && !Acquire->bReacquisitionRenderSubmitted))
    {
        return ERHIResult::InvalidState;
    }
    if (!bSucceeded)
    {
        if (bReacquisition)
        {
            Acquire->bReacquisitionRenderComplete = true;
            Acquire->bReacquisitionRenderSucceeded = false;
        }
        else
        {
            Acquire->bRenderComplete = true;
            Acquire->bRenderSucceeded = false;
        }
        LatchFailure(ERHIResult::Failed);
        return ERHIResult::Failed;
    }

    const EVulkanLabPresentationPolicyResult PolicyResult =
        Policy_.MarkRenderComplete(
            Acquire->Generation, Acquire->ImageIndex, AcquisitionToken);
    if (PolicyResult != EVulkanLabPresentationPolicyResult::Accepted)
    {
        const ERHIResult Mapped = MapPolicyResult(PolicyResult);
        LatchFailure(Mapped);
        return Mapped;
    }
    if (bReacquisition)
    {
        Acquire->bReacquisitionRenderComplete = true;
        Acquire->bReacquisitionRenderSucceeded = true;
    }
    else
    {
        Acquire->bRenderComplete = true;
        Acquire->bRenderSucceeded = true;
    }
    ReleaseCompletedAcquire(*Acquire);
    return ERHIResult::Success;
}

ERHIResult FVulkanLabSwapchainRuntime::PollAcquire(
    FNativeAcquire& Acquire) noexcept
{
    if (!Acquire.bOccupied)
    {
        return ERHIResult::Success;
    }
    if (Acquire.bPending)
    {
        const ERHIResult RetryResult = TryAcquire(Acquire);
        if (RetryResult == ERHIResult::NotReady)
        {
            return RetryResult;
        }
        if (RetryResult != ERHIResult::Success)
        {
            if (!Acquire.bAcquired)
            {
                DestroyAcquireSync(Acquire);
                Acquire = {};
            }
            if (RetryResult != ERHIResult::ResizeRequired &&
                RetryResult != ERHIResult::Unavailable)
            {
                LatchFailure(RetryResult);
            }
            return RetryResult;
        }
        const uint64 Token = Acquire.AcquisitionToken;
        const ERHIResult PublishResult = PublishPendingAcquire(Acquire);
        if (PublishResult != ERHIResult::Success)
        {
            if (PublishResult != ERHIResult::NotReady)
            {
                LatchFailure(PublishResult);
            }
            return PublishResult;
        }
        // Standalone pending records are moved/cleared by PublishPending.
        // The per-image record is found using the saved token below.
        bool bIgnoredReacquisition = false;
        FNativeAcquire* Published = FindTokenRecord(
            Token, bIgnoredReacquisition);
        if (Published == nullptr)
        {
            return ERHIResult::Failed;
        }
        return PollAcquire(*Published);
    }
    if (!Acquire.bAcquired || Acquire.AcquireFence == VK_NULL_HANDLE)
    {
        return ERHIResult::InvalidState;
    }
    if (!Acquire.bAcquireFenceComplete)
    {
        const VkResult FenceResult = vkGetFenceStatus(
            Device_, Acquire.AcquireFence);
        if (FenceResult == VK_NOT_READY)
        {
            return ERHIResult::NotReady;
        }
        if (FenceResult != VK_SUCCESS)
        {
            const ERHIResult Mapped = MapNativeResult(FenceResult);
            LatchFailure(Mapped);
            return Mapped;
        }
        Acquire.bAcquireFenceComplete = true;
    }

    if (Acquire.bHasReacquisition &&
        Acquire.ReacquisitionAcquireFence != VK_NULL_HANDLE &&
        !Acquire.bReacquisitionAcquireFenceComplete)
    {
        const VkResult FenceResult = vkGetFenceStatus(
            Device_, Acquire.ReacquisitionAcquireFence);
        if (FenceResult == VK_NOT_READY)
        {
            return ERHIResult::NotReady;
        }
        if (FenceResult != VK_SUCCESS)
        {
            const ERHIResult Mapped = MapNativeResult(FenceResult);
            LatchFailure(Mapped);
            return Mapped;
        }
        Acquire.bReacquisitionAcquireFenceComplete = true;
    }

    // Delay the policy proof until the predecessor render report arrives.
    // The acquire fence proves image availability, while the render report
    // proves the predecessor acquire semaphore was actually consumed.
    if (Acquire.bHasReacquisition &&
        Acquire.bRenderComplete && Acquire.bRenderSucceeded &&
        Acquire.bReacquisitionAcquireFenceComplete &&
        !Acquire.bReacquisitionAcquireSyncReported)
    {
        const EVulkanLabPresentationPolicyResult PolicyResult =
            Policy_.MarkAcquireSynchronizationComplete(
                Acquire.Generation, Acquire.ImageIndex,
                Acquire.ReacquisitionToken);
        if (PolicyResult != EVulkanLabPresentationPolicyResult::Accepted)
        {
            const ERHIResult Mapped = MapPolicyResult(PolicyResult);
            LatchFailure(Mapped);
            return Mapped;
        }
        Acquire.bReacquisitionAcquireSyncReported = true;
        // The acquire-history signal proves retirement of the old presented
        // token. The newly acquired token remains owned and is not itself a
        // presentation proof.
        if (Acquire.bPresentQueued)
        {
            NotifyPresentationRetirement(
                EVulkanLabPresentationRetirementEvent::PresentationProof,
                Acquire.AcquisitionToken, Acquire.Generation,
                Acquire.ImageIndex);
        }
        Acquire.bPresentationRetired = true;
        ReleaseCompletedAcquire(Acquire);
    }
    return ERHIResult::Success;
}

ERHIResult FVulkanLabSwapchainRuntime::PollPresentation(
    FNativeAcquire& Acquire) noexcept
{
    if (!Acquire.bOccupied || Selection_.Mode !=
        Stoner::RHI::ERHIPresentationRetirementMode::PresentationFence ||
        !Acquire.bPresentQueued || Acquire.bPresentationRetired)
    {
        return ERHIResult::Success;
    }
    if (Acquire.PresentationFence == VK_NULL_HANDLE)
    {
        LatchFailure(ERHIResult::Failed);
        return ERHIResult::Failed;
    }
    const VkResult FenceResult = vkGetFenceStatus(
        Device_, Acquire.PresentationFence);
    if (FenceResult == VK_NOT_READY)
    {
        return ERHIResult::NotReady;
    }
    if (FenceResult != VK_SUCCESS)
    {
        const ERHIResult Mapped = MapNativeResult(FenceResult);
        LatchFailure(Mapped);
        return Mapped;
    }
    const EVulkanLabPresentationPolicyResult PolicyResult =
        Policy_.MarkPresentationFenceComplete(
            Acquire.Generation, Acquire.ImageIndex,
            Acquire.AcquisitionToken);
    if (PolicyResult != EVulkanLabPresentationPolicyResult::Accepted)
    {
        const ERHIResult Mapped = MapPolicyResult(PolicyResult);
        LatchFailure(Mapped);
        return Mapped;
    }
    Acquire.bPresentationRetired = true;
    NotifyPresentationRetirement(
        EVulkanLabPresentationRetirementEvent::PresentationProof,
        Acquire.AcquisitionToken, Acquire.Generation, Acquire.ImageIndex);
    ReleaseCompletedAcquire(Acquire);
    return ERHIResult::Success;
}

bool FVulkanLabSwapchainRuntime::IsGenerationRenderDrained(
    uint64 Generation) const noexcept
{
    for (const FNativeAcquire& Pending : PendingAcquires_)
    {
        if (Pending.bOccupied && Pending.Generation == Generation &&
            Pending.bAcquired &&
            (!Pending.bAcquireFenceComplete || Pending.bPending))
        {
            return false;
        }
    }
    for (const FNativeAcquire& Acquire : Acquires_)
    {
        if (!Acquire.bOccupied || Acquire.Generation != Generation)
        {
            continue;
        }
        if ((Acquire.bRenderSubmitted && !Acquire.bRenderComplete) ||
            (Acquire.bHasReacquisition &&
                Acquire.bReacquisitionRenderSubmitted &&
                !Acquire.bReacquisitionRenderComplete))
        {
            return false;
        }
    }
    return true;
}

ERHIResult FVulkanLabSwapchainRuntime::BeginReplacementIfReady() noexcept
{
    if (!bHasPendingResize_ || bPausedZeroExtent_ || bFailed_)
    {
        return ERHIResult::Success;
    }
    FNativeGeneration* Active = nullptr;
    FNativeGeneration* Free = nullptr;
    for (FNativeGeneration& Generation : Generations_)
    {
        if (Generation.bOccupied && Generation.bActive)
        {
            Active = &Generation;
        }
        else if (!Generation.bOccupied && Free == nullptr)
        {
            Free = &Generation;
        }
    }
    if (Active == nullptr || Free == nullptr)
    {
        return ERHIResult::NotReady;
    }
    // Validate all surface compatibility without passing oldSwapchain or
    // changing policy generation state. A current-extent mismatch is a
    // retryable resize condition while the existing owner remains usable.
    VkFormat IgnoredFormat = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR IgnoredColorSpace = VK_COLOR_SPACE_MAX_ENUM_KHR;
    const ERHIResult Compatibility = ValidateSurfaceCompatibility(
        PendingResize_, IgnoredFormat, IgnoredColorSpace);
    if (Compatibility != ERHIResult::Success)
    {
        return Compatibility;
    }
    if (!IsGenerationRenderDrained(Active->Desc.Generation))
    {
        return ERHIResult::NotReady;
    }

    const EVulkanLabPresentationPolicyResult BeginResult =
        Policy_.BeginPendingReplacement();
    if (BeginResult != EVulkanLabPresentationPolicyResult::Accepted)
    {
        return MapPolicyResult(BeginResult);
    }

    // A timeout-zero pending acquire has completed its Vulkan call and owns
    // no image. It is cancelled here before the old handle is retired; an
    // acquired operation stays in Acquires_ until policy retirement.
    for (FNativeAcquire& Pending : PendingAcquires_)
    {
        if (Pending.bOccupied && Pending.Generation == Active->Desc.Generation &&
            !Pending.bAcquired)
        {
            DestroyAcquireSync(Pending);
            Pending = {};
        }
    }
    Active->bActive = false;
    Active->bRetiring = true;
    const VkSwapchainKHR OldSwapchain = Active->Swapchain;
    FNativeGeneration Created;
    const ERHIResult CreateResult = CreateGeneration(
        PendingResize_, OldSwapchain, Created);
    if (CreateResult != ERHIResult::Success)
    {
        // Passing oldSwapchain to vkCreateSwapchainKHR invalidates the
        // predecessor's usability even when creation fails. Never reactivate
        // it; leave its owners for terminal cleanup.
        (void)Policy_.CompletePendingReplacement(false);
        Active->bCreationFailed = true;
        bHasPendingResize_ = false;
        PendingResize_ = {};
        LatchFailure(ERHIResult::Failed);
        return CreateResult;
    }

    // The native image count is checked by CreateGeneration before views or
    // borrowed records are published. The policy's typed completion then
    // performs the authoritative actual-count and aggregate-budget check.
    const EVulkanLabPresentationPolicyResult CompleteResult =
        Policy_.CompletePendingReplacement(Created.ImageCount);
    if (CompleteResult != EVulkanLabPresentationPolicyResult::Accepted)
    {
        DestroyGeneration(Created);
        Active->bCreationFailed = true;
        bHasPendingResize_ = false;
        PendingResize_ = {};
        const ERHIResult Mapped = MapPolicyResult(CompleteResult);
        LatchFailure(Mapped == ERHIResult::NotReady ? ERHIResult::Failed : Mapped);
        return Mapped;
    }
    *Free = Created;
    Free->bActive = true;
    Free->bRetiring = false;
    bHasPendingResize_ = false;
    PendingResize_ = {};
    return ERHIResult::Success;
}

ERHIResult FVulkanLabSwapchainRuntime::TryRetirePredecessor() noexcept
{
    FNativeGeneration* Retiring = nullptr;
    FNativeGeneration* Active = nullptr;
    for (FNativeGeneration& Generation : Generations_)
    {
        if (Generation.bOccupied && Generation.bRetiring)
        {
            Retiring = &Generation;
        }
        if (Generation.bOccupied && Generation.bActive)
        {
            Active = &Generation;
        }
    }
    if (Retiring == nullptr || Active == nullptr)
    {
        return ERHIResult::Success;
    }
    const EVulkanLabPresentationPolicyResult DrainResult =
        Policy_.MarkRetiringGenerationRenderUsesComplete(
            Retiring->Desc.Generation);
    if (DrainResult != EVulkanLabPresentationPolicyResult::Accepted)
    {
        return MapPolicyResult(DrainResult);
    }
    if (!Policy_.CanRetirePredecessor())
    {
        return ERHIResult::NotReady;
    }
    const EVulkanLabPresentationPolicyResult RetireResult =
        Policy_.RetirePredecessor();
    if (RetireResult != EVulkanLabPresentationPolicyResult::Accepted)
    {
        return MapPolicyResult(RetireResult);
    }
    const uint64 RetiringGeneration = Retiring->Desc.Generation;
    NotifyPresentationRetirement(
        EVulkanLabPresentationRetirementEvent::GenerationRetired, 0,
        RetiringGeneration, 0);
    // RetirePredecessor is the proof boundary for old acquired images. It is
    // now legal to release their native sync and destroy the old swapchain.
    for (FNativeAcquire& Acquire : Acquires_)
    {
        if (Acquire.bOccupied && Acquire.Generation == RetiringGeneration)
        {
            DestroyAcquireSync(Acquire);
            Acquire = {};
        }
    }
    for (FNativeAcquire& Pending : PendingAcquires_)
    {
        if (Pending.bOccupied && Pending.Generation == RetiringGeneration)
        {
            DestroyAcquireSync(Pending);
            Pending = {};
        }
    }
    DestroyGeneration(*Retiring);
    return ERHIResult::Success;
}

ERHIResult FVulkanLabSwapchainRuntime::DestroyRetiredGeneration() noexcept
{
    return TryRetirePredecessor();
}

ERHIResult FVulkanLabSwapchainRuntime::Poll() noexcept
{
    if (!bInitialized_ || bClosed_)
    {
        return ERHIResult::InvalidState;
    }
    ERHIResult FirstResult = ERHIResult::Success;
    for (FNativeAcquire& Pending : PendingAcquires_)
    {
        const ERHIResult Result = PollAcquire(Pending);
        if (Result != ERHIResult::Success &&
            FirstResult == ERHIResult::Success)
        {
            FirstResult = Result;
        }
    }
    for (FNativeAcquire& Acquire : Acquires_)
    {
        const ERHIResult AcquireResult = PollAcquire(Acquire);
        if (AcquireResult != ERHIResult::Success &&
            AcquireResult != ERHIResult::NotReady &&
            FirstResult == ERHIResult::Success)
        {
            FirstResult = AcquireResult;
        }
        const ERHIResult PresentResult = PollPresentation(Acquire);
        if (PresentResult != ERHIResult::Success &&
            PresentResult != ERHIResult::NotReady &&
            FirstResult == ERHIResult::Success)
        {
            FirstResult = PresentResult;
        }
    }
    if (!bPausedZeroExtent_ && !bFailed_ && bHasPendingResize_)
    {
        const ERHIResult ReplacementResult = BeginReplacementIfReady();
        if (ReplacementResult != ERHIResult::Success &&
            ReplacementResult != ERHIResult::NotReady &&
            FirstResult == ERHIResult::Success)
        {
            FirstResult = ReplacementResult;
        }
    }
    const ERHIResult RetirementResult = TryRetirePredecessor();
    if (RetirementResult != ERHIResult::Success &&
        RetirementResult != ERHIResult::NotReady &&
        FirstResult == ERHIResult::Success)
    {
        FirstResult = RetirementResult;
    }
    if (bFailed_)
    {
        return FailureResult_;
    }
    return FirstResult;
}

bool FVulkanLabSwapchainRuntime::GetNativeImageRecord(
    uint64 AcquisitionToken,
    FVulkanLabNativeImageRecord& OutRecord) const noexcept
{
    bool bReacquisition = false;
    const FNativeAcquire* Acquire = FindTokenRecord(
        AcquisitionToken, bReacquisition);
    if (Acquire == nullptr)
    {
        OutRecord = {};
        return false;
    }
    return FillNativeImageRecord(*Acquire, AcquisitionToken, OutRecord) ==
        ERHIResult::Success;
}

FVulkanLabSwapchainRuntimeSnapshot
FVulkanLabSwapchainRuntime::GetSnapshot() const noexcept
{
    FVulkanLabSwapchainRuntimeSnapshot Snapshot;
    Snapshot.RetirementMode = Selection_.Mode;
    Snapshot.ShutdownAssurance = Policy_.GetShutdownAssurance();
    Snapshot.bPausedZeroExtent = bPausedZeroExtent_;
    Snapshot.bTransitionPending = bHasPendingResize_ ||
        Policy_.HasPendingReplacement();
    Snapshot.bFailed = bFailed_;
    Snapshot.bDeviceLost = bDeviceLost_;
    Snapshot.FirstNativeFailure = FirstNativeFailure_;
    Snapshot.AbandonedNativeOwnerCount = AbandonedNativeOwnerCount_;
    Snapshot.bTerminalCleanupStarted = bTerminalCleanupStarted_;
    Snapshot.OutstandingNativeRecordCount = 0;
    Snapshot.PendingAcquireCount = 0;
    for (const FNativeAcquire& Acquire : Acquires_)
    {
        if (Acquire.bOccupied)
        {
            ++Snapshot.OutstandingNativeRecordCount;
        }
    }
    for (const FNativeAcquire& Pending : PendingAcquires_)
    {
        if (Pending.bOccupied)
        {
            ++Snapshot.PendingAcquireCount;
        }
    }
    Snapshot.ResidualNativeOwnerCount =
        Snapshot.AbandonedNativeOwnerCount +
        Snapshot.OutstandingNativeRecordCount + Snapshot.PendingAcquireCount;
    // Native gauges describe native owners, not policy history. Preferred
    // cleanup can retain its proven policy identity after destroying objects.
    for (const auto& Generation : Generations_)
    {
        if (!Generation.bOccupied) continue;
        if (Generation.bRetiring)
        {
            Snapshot.RetiringGeneration = Generation.Desc.Generation;
            Snapshot.RetiringImageCount = Generation.ImageCount;
        }
        else
        {
            Snapshot.ActiveGeneration = Generation.Desc.Generation;
            Snapshot.ActiveImageCount = Generation.ImageCount;
        }
        auto Estimate = Generation.Desc;
        Estimate.MinImageCount = Generation.ImageCount;
        uint64 Bytes = 0;
        (void)TryEstimateColorBytes(Estimate, Bytes);
        Snapshot.EstimatedColorBytes += Bytes;
    }
    Snapshot.PeakEstimatedColorBytes = PeakEstimatedColorBytes_;
    return Snapshot;
}

ERHIResult FVulkanLabSwapchainRuntime::BeginTerminalCleanup() noexcept
{
    if (!bInitialized_ || bClosed_ || bTerminalCleanupStarted_)
    {
        return ERHIResult::InvalidState;
    }
    bTerminalCleanupStarted_ = true;
    Policy_.BeginTerminalCleanup();
    return ERHIResult::Success;
}

bool FVulkanLabSwapchainRuntime::IsTerminalProofComplete() const noexcept
{
    if (!bTerminalCleanupStarted_)
    {
        return false;
    }
    if (bDeviceLost_)
    {
        return true;
    }
    if (!bTerminalIdleConfirmed_)
    {
        return false;
    }
    const Stoner::RHI::ERHIShutdownAssurance Assurance =
        Policy_.GetShutdownAssurance();
    return Assurance == Stoner::RHI::ERHIShutdownAssurance::IdleAssumed ||
        Assurance == Stoner::RHI::ERHIShutdownAssurance::Proven;
}

bool FVulkanLabSwapchainRuntime::HasTerminalNativeProof() const noexcept
{
    const bool bPreferredMode =
        Selection_.Mode ==
        Stoner::RHI::ERHIPresentationRetirementMode::PresentationFence;
    const auto HasOperationProof = [](const FNativeAcquire& Acquire,
                                      bool bReacquisition,
                                      bool bRejectUnresolvedPresent) noexcept
    {
        const bool bOccupied = Acquire.bOccupied;
        if (!bOccupied || !Acquire.bAcquired)
        {
            return true;
        }

        const bool bCanceled = bReacquisition
            ? Acquire.bReacquisitionCanceled : Acquire.bCanceled;
        const bool bSubmitted = bReacquisition
            ? Acquire.bReacquisitionRenderSubmitted
            : Acquire.bRenderSubmitted;
        const bool bComplete = bReacquisition
            ? Acquire.bReacquisitionRenderComplete
            : Acquire.bRenderComplete;
        const bool bSucceeded = bReacquisition
            ? Acquire.bReacquisitionRenderSucceeded
            : Acquire.bRenderSucceeded;
        const VkFence AcquireFence = bReacquisition
            ? Acquire.ReacquisitionAcquireFence : Acquire.AcquireFence;
        const bool bFenceComplete = bReacquisition
            ? Acquire.bReacquisitionAcquireFenceComplete
            : Acquire.bAcquireFenceComplete;
        const bool bPresentAttempted = bReacquisition
            ? Acquire.bReacquisitionPresentAttempted
            : Acquire.bPresentAttempted;
        const bool bPresentQueued = bReacquisition
            ? Acquire.bReacquisitionPresentQueued : Acquire.bPresentQueued;

        if (AcquireFence == VK_NULL_HANDLE || !bFenceComplete ||
            (bRejectUnresolvedPresent && bPresentAttempted &&
                !bPresentQueued) ||
            (bCanceled
                ? (bSubmitted && (!bComplete || !bSucceeded))
                : (!bSubmitted || !bComplete || !bSucceeded)))
        {
            return false;
        }
        return true;
    };

    for (const FNativeAcquire& Pending : PendingAcquires_)
    {
        if (!HasOperationProof(Pending, false, bPreferredMode))
        {
            return false;
        }
        if (Pending.bHasReacquisition &&
            !HasOperationProof(Pending, true, bPreferredMode))
        {
            return false;
        }
    }
    for (const FNativeAcquire& Acquire : Acquires_)
    {
        if (!HasOperationProof(Acquire, false, bPreferredMode))
        {
            return false;
        }
        if (Acquire.bHasReacquisition &&
            !HasOperationProof(Acquire, true, bPreferredMode))
        {
            return false;
        }
    }
    return true;
}

ERHIResult FVulkanLabSwapchainRuntime::CompleteTerminalIdle(
    bool bCallerConfirmedIdle,
    bool bDeviceLost) noexcept
{
    if (!bTerminalCleanupStarted_ || bClosed_)
    {
        return ERHIResult::InvalidState;
    }
    if (bDeviceLost)
    {
        bDeviceLost_ = true;
        bFailed_ = true;
        FailureResult_ = ERHIResult::Failed;
        if (FirstNativeFailure_ == VK_SUCCESS)
        {
            FirstNativeFailure_ = VK_ERROR_DEVICE_LOST;
        }
        Policy_.MarkDeviceLost();
        return ERHIResult::Failed;
    }
    if (!bCallerConfirmedIdle)
    {
        return ERHIResult::InvalidState;
    }
    if (!HasTerminalNativeProof())
    {
        return bFailed_ ? FailureResult_ : ERHIResult::NotReady;
    }
    if (Selection_.Mode ==
        Stoner::RHI::ERHIPresentationRetirementMode::PresentationFence)
    {
        // Once native acquire/render uses are proven, terminal cleanup may
        // discard only canceled/unpresented policy owners. Queued presents
        // and unresolved presentation fences remain outstanding.
        const EVulkanLabPresentationPolicyResult ResolveResult =
            Policy_.ResolveTerminalNonPresentedOwners();
        if (ResolveResult !=
            EVulkanLabPresentationPolicyResult::Accepted)
        {
            return bFailed_ ? FailureResult_ : MapPolicyResult(ResolveResult);
        }
    }
    if (bFailed_)
    {
        // A known failed execution still needs the policy's terminal
        // accounting before native owners may be destroyed, but its public
        // result must remain the original failure.
        bTerminalIdleConfirmed_ = true;
        const EVulkanLabPresentationPolicyResult FailedPolicyResult =
            Policy_.CompleteTerminalIdle(true);
        (void)FailedPolicyResult;
        return FailureResult_;
    }
    const EVulkanLabPresentationPolicyResult PolicyResult =
        Policy_.CompleteTerminalIdle(true);
    if (PolicyResult != EVulkanLabPresentationPolicyResult::Accepted)
    {
        return MapPolicyResult(PolicyResult);
    }
    bTerminalIdleConfirmed_ = true;
    return ERHIResult::Success;
}

ERHIResult FVulkanLabSwapchainRuntime::DestroyAfterTerminalProof() noexcept
{
    if (!IsTerminalProofComplete())
    {
        return ERHIResult::InvalidState;
    }
    if (!bDeviceLost_ &&
        Selection_.Mode == Stoner::RHI::ERHIPresentationRetirementMode::AcquireHistory)
    {
        const EVulkanLabPresentationPolicyResult ResolveResult =
            Policy_.ResolveTerminalOwnersForCompatibility();
        if (ResolveResult != EVulkanLabPresentationPolicyResult::Accepted)
        {
            return MapPolicyResult(ResolveResult);
        }
    }
    for (FNativeAcquire& Pending : PendingAcquires_)
    {
        DestroyAcquireSync(Pending);
        Pending = {};
    }
    for (FNativeAcquire& Acquire : Acquires_)
    {
        DestroyAcquireSync(Acquire);
        Acquire = {};
    }
    for (FNativeGeneration& Generation : Generations_)
    {
        DestroyGeneration(Generation);
    }
    bClosed_ = true;
    bInitialized_ = false;
    return bDeviceLost_ || bFailed_ ? FailureResult_ : ERHIResult::Success;
}

} // namespace Stoner::Backend::Vulkan::Private

#endif
