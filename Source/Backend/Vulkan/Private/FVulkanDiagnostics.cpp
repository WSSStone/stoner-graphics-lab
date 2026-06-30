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

void MarkCommandAllocation(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept
{
    Diagnostics.CommandAllocationReason = Reason ? Reason : "";
}

void MarkCommandRecording(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept
{
    Diagnostics.CommandRecordingReason = Reason ? Reason : "";
}

void MarkRenderPass(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept
{
    Diagnostics.RenderPassReason = Reason ? Reason : "";
}

void MarkFramebuffer(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept
{
    Diagnostics.FramebufferReason = Reason ? Reason : "";
}

void MarkSubmission(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept
{
    Diagnostics.SubmissionReason = Reason ? Reason : "";
}

void MarkCompletion(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept
{
    Diagnostics.CompletionReason = Reason ? Reason : "";
}

void MarkUploadScheduling(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept
{
    Diagnostics.UploadSchedulingReason = Reason ? Reason : "";
}

void MarkShaderModule(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept
{
    Diagnostics.ShaderModuleReason = Reason ? Reason : "";
}

void MarkPipelineLayout(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept
{
    Diagnostics.PipelineLayoutReason = Reason ? Reason : "";
}

void MarkGraphicsPipeline(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept
{
    Diagnostics.GraphicsPipelineReason = Reason ? Reason : "";
}

void MarkComputePipeline(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept
{
    Diagnostics.ComputePipelineReason = Reason ? Reason : "";
}

void MarkPipelineCache(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept
{
    Diagnostics.PipelineCacheReason = Reason ? Reason : "";
}

void MarkPipelineBinding(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept
{
    Diagnostics.PipelineBindingReason = Reason ? Reason : "";
}

void MarkRuntimeMode(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept
{
    Diagnostics.RuntimeModeReason = Reason ? Reason : "";
}

} // namespace Stoner::Backend::Vulkan
