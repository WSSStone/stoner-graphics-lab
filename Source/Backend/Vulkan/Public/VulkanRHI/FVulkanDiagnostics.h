#pragma once

#include "Core/CoreMinimal.h"

namespace Stoner::Backend::Vulkan
{

enum class EVulkanBackendAvailability
{
    Available,
    UnsupportedRuntime,
    MissingRequiredCapability,
    FailedInitialization
};

enum class EVulkanValidationState
{
    Disabled,
    RequestedUnavailable,
    Enabled
};

struct FVulkanDiagnostics
{
    EVulkanBackendAvailability Availability = EVulkanBackendAvailability::UnsupportedRuntime;
    EVulkanValidationState Validation = EVulkanValidationState::Disabled;
    bool bValidationRequested = false;
    bool bUsedSdkHeaders = false;
    bool bUsedRuntimeFallback = false;
    const char* SelectedAdapterReason = "";
    const char* UnsupportedRuntimeReason = "";
    const char* ValidationUnavailableReason = "";
    const char* PresentationSkipReason = "";
    const char* QueueCapabilityReason = "";
    const char* ResourceAllocationReason = "";
    const char* AllocationFailureReason = "";
    const char* DescriptorPoolReason = "";
    const char* DescriptorUpdateReason = "";
    const char* UploadRejectionReason = "";
};

void MarkUnsupportedRuntime(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept;
void MarkValidationUnavailable(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept;
void MarkSelectedAdapter(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept;
void MarkPresentationSkipped(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept;
void MarkQueueCapability(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept;
void MarkResourceAllocation(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept;
void MarkAllocationFailure(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept;
void MarkDescriptorPool(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept;
void MarkDescriptorUpdate(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept;
void MarkUploadRejection(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept;

} // namespace Stoner::Backend::Vulkan
