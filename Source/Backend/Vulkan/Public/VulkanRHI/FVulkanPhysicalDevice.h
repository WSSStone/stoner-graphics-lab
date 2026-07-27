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
    FVulkanFormatSupport() = default;
    FVulkanFormatSupport(bool bIncludeColorFormats, bool bIncludeDepthFormats);
    explicit FVulkanFormatSupport(Stoner::Core::TArray<Stoner::RHI::ERHIFormat> InSupportedFormats);

    [[nodiscard]] bool SupportsFormat(Stoner::RHI::ERHIFormat Format) const noexcept;
    [[nodiscard]] bool SupportsColor() const noexcept;
    [[nodiscard]] bool SupportsDepth() const noexcept;
    [[nodiscard]] const Stoner::Core::TArray<Stoner::RHI::ERHIFormat>& GetSupportedFormats() const noexcept;

private:
    Stoner::Core::TArray<Stoner::RHI::ERHIFormat> SupportedFormats;
};

struct FVulkanAdapterCandidate
{
    Stoner::Core::FString Name;
    EVulkanPhysicalDeviceType DeviceType = EVulkanPhysicalDeviceType::Unknown;
    bool bPassesRequiredGate = false;
    FVulkanQueueSupport Queues;
    bool bPresentationSupported = false;
    FVulkanFormatSupport Formats;
    Stoner::Core::int32 Score = 0;
    Stoner::Core::FString RejectionReason;
};

struct FVulkanAdapterSelection
{
    bool bSucceeded = false;
    FVulkanAdapterCandidate Selected;
    Stoner::Core::TArray<FVulkanAdapterCandidate> Candidates;
    Stoner::Core::FString Reason;
};

[[nodiscard]] bool PassesRequiredCapabilityGate(const FVulkanAdapterCandidate& Candidate) noexcept;
[[nodiscard]] Stoner::Core::int32 ScoreAdapterCandidate(const FVulkanAdapterCandidate& Candidate) noexcept;
[[nodiscard]] FVulkanAdapterSelection SelectBestAdapter(Stoner::Core::TArray<FVulkanAdapterCandidate> Candidates);

} // namespace Stoner::Backend::Vulkan
