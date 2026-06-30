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
    const char* CommandAllocationReason = "";
    const char* CommandRecordingReason = "";
    const char* RenderPassReason = "";
    const char* FramebufferReason = "";
    const char* SubmissionReason = "";
    const char* CompletionReason = "";
    const char* UploadSchedulingReason = "";
    const char* ShaderModuleReason = "";
    const char* PipelineLayoutReason = "";
    const char* GraphicsPipelineReason = "";
    const char* ComputePipelineReason = "";
    const char* PipelineCacheReason = "";
    const char* PipelineBindingReason = "";
    const char* RuntimeModeReason = "";
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
void MarkCommandAllocation(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept;
void MarkCommandRecording(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept;
void MarkRenderPass(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept;
void MarkFramebuffer(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept;
void MarkSubmission(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept;
void MarkCompletion(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept;
void MarkUploadScheduling(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept;
void MarkShaderModule(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept;
void MarkPipelineLayout(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept;
void MarkGraphicsPipeline(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept;
void MarkComputePipeline(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept;
void MarkPipelineCache(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept;
void MarkPipelineBinding(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept;
void MarkRuntimeMode(FVulkanDiagnostics& Diagnostics, const char* Reason) noexcept;

} // namespace Stoner::Backend::Vulkan
