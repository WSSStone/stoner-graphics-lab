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

} // namespace Stoner::Backend::Vulkan
