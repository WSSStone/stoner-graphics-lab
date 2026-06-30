#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIFormat.h"

namespace Stoner::Backend::Vulkan
{

enum class EVulkanPhysicalDeviceType
{
    Unknown,
    Integrated,
    Discrete,
    Virtual,
    Cpu
};

struct FVulkanQueueSupport
{
    bool bGraphics = false;
    bool bCompute = false;
    bool bTransfer = false;
    bool bPresent = false;
};

struct FVulkanFormatSupport
{
    bool bColor = false;
    bool bDepth = false;
};

struct FVulkanAdapterCandidate
{
    const char* Name = "";
    EVulkanPhysicalDeviceType DeviceType = EVulkanPhysicalDeviceType::Unknown;
    bool bPassesRequiredGate = false;
    FVulkanQueueSupport Queues;
    bool bPresentationSupported = false;
    FVulkanFormatSupport Formats;
    Stoner::Core::int32 Score = 0;
    const char* RejectionReason = "";
};

struct FVulkanAdapterSelection
{
    bool bSucceeded = false;
    FVulkanAdapterCandidate Selected;
    Stoner::Core::TArray<FVulkanAdapterCandidate> Candidates;
    const char* Reason = "";
};

[[nodiscard]] bool PassesRequiredCapabilityGate(const FVulkanAdapterCandidate& Candidate) noexcept;
[[nodiscard]] Stoner::Core::int32 ScoreAdapterCandidate(const FVulkanAdapterCandidate& Candidate) noexcept;
[[nodiscard]] FVulkanAdapterSelection SelectBestAdapter(Stoner::Core::TArray<FVulkanAdapterCandidate> Candidates);
[[nodiscard]] Stoner::Core::TArray<Stoner::RHI::ERHIFormat> GetDefaultVulkanSupportedFormats();

} // namespace Stoner::Backend::Vulkan
