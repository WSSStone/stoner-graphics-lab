#include "VulkanRHI/FVulkanDiagnostics.h"

namespace Stoner::Backend::Vulkan
{

void MarkUnsupportedRuntime(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept
{
    Diagnostics.Availability = EVulkanBackendAvailability::UnsupportedRuntime;
    Diagnostics.UnsupportedRuntimeReason = Reason ? Reason : "";
}

void MarkValidationUnavailable(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept
{
    Diagnostics.Validation = EVulkanValidationState::RequestedUnavailable;
    Diagnostics.ValidationUnavailableReason = Reason ? Reason : "";
}

void MarkSelectedAdapter(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept
{
    Diagnostics.Availability = EVulkanBackendAvailability::Available;
    Diagnostics.SelectedAdapterReason = Reason ? Reason : "";
}

void MarkPresentationSkipped(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept
{
    Diagnostics.PresentationSkipReason = Reason ? Reason : "";
}

void MarkQueueCapability(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept
{
    Diagnostics.QueueCapabilityReason = Reason ? Reason : "";
}

void MarkResourceAllocation(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept
{
    Diagnostics.ResourceAllocationReason = Reason ? Reason : "";
}

void MarkAllocationFailure(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept
{
    Diagnostics.AllocationFailureReason = Reason ? Reason : "";
}

void MarkDescriptorPool(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept
{
    Diagnostics.DescriptorPoolReason = Reason ? Reason : "";
}

void MarkDescriptorUpdate(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept
{
    Diagnostics.DescriptorUpdateReason = Reason ? Reason : "";
}

void MarkUploadRejection(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept
{
    Diagnostics.UploadRejectionReason = Reason ? Reason : "";
}

} // namespace Stoner::Backend::Vulkan
