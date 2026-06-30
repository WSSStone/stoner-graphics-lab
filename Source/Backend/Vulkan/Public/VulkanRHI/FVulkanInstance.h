#pragma once

#include "VulkanRHI/FVulkanDiagnostics.h"
#include "VulkanRHI/FVulkanPhysicalDevice.h"
#include "RHI/ERHIResult.h"

namespace Stoner::Backend::Vulkan
{

struct FVulkanInstanceDesc
{
    bool bRequestValidation = true;
    bool bForceUnsupportedRuntime = false;
    bool bForceValidationUnavailable = false;
    Stoner::Core::TArray<FVulkanAdapterCandidate> SyntheticCandidates;
};

class FVulkanInstance
{
public:
    [[nodiscard]] bool IsInitialized() const noexcept;
    [[nodiscard]] const FVulkanDiagnostics& GetDiagnostics() const noexcept;
    [[nodiscard]] const FVulkanAdapterSelection& GetAdapterSelection() const noexcept;

    Stoner::RHI::ERHIResult Initialize(const FVulkanInstanceDesc& Desc = {});
    Stoner::RHI::ERHIResult Shutdown() noexcept;

private:
    bool bInitialized = false;
    FVulkanDiagnostics Diagnostics;
    FVulkanAdapterSelection AdapterSelection;
};

} // namespace Stoner::Backend::Vulkan
