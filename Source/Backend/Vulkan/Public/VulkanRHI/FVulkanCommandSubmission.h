#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIResult.h"

namespace Stoner::Backend::Vulkan
{

class FVulkanCommandBuffer;
class FVulkanQueue;

enum class EVulkanSubmissionMode
{
    RealRuntime,
    DeterministicFallback,
    Failed
};

enum class EVulkanCompletionState
{
    Pending,
    Completed,
    NotReady,
    Timeout,
    Invalidated,
    Failed
};

struct FVulkanCompletionInjectionConfig
{
    bool bForceNotReady = false;
    bool bForceTimeout = false;
};

class FVulkanCommandSubmission final
{
public:
    ~FVulkanCommandSubmission() = default;

    [[nodiscard]] EVulkanSubmissionMode GetMode() const noexcept;
    [[nodiscard]] EVulkanCompletionState GetCompletionState() const noexcept;
    [[nodiscard]] Stoner::Core::TSharedPtr<FVulkanCommandBuffer> GetCommandBuffer() const noexcept;
    [[nodiscard]] bool IsValid() const noexcept;

    Stoner::RHI::ERHIResult ObserveCompletion(Stoner::Core::uint64 TimeoutMicroseconds = 0) noexcept;
    Stoner::RHI::ERHIResult CompleteForWaitIdle() noexcept;
    void Invalidate() noexcept;

private:
    friend class FVulkanQueue;

    FVulkanCommandSubmission(
        Stoner::Core::TSharedPtr<FVulkanCommandBuffer> InCommandBuffer,
        EVulkanSubmissionMode InMode,
        FVulkanCompletionInjectionConfig InInjection = {}) noexcept;

    Stoner::Core::TSharedPtr<FVulkanCommandBuffer> CommandBuffer;
    EVulkanSubmissionMode Mode = EVulkanSubmissionMode::DeterministicFallback;
    EVulkanCompletionState CompletionState = EVulkanCompletionState::Pending;
    FVulkanCompletionInjectionConfig Injection;
    bool bValid = true;
};

} // namespace Stoner::Backend::Vulkan
