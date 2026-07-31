#pragma once

#include "VulkanRHI/FVulkanNativeContext.h"

#include <memory>

namespace Stoner::Backend::Vulkan
{

struct FVulkanDeferredShaderSet
{
    Stoner::RHI::FRHIShaderModuleDesc SurfaceVertex;
    Stoner::RHI::FRHIShaderModuleDesc SurfaceFragment;
    Stoner::RHI::FRHIShaderModuleDesc FullscreenVertex;
    Stoner::RHI::FRHIShaderModuleDesc DirectionalFragment;
    Stoner::RHI::FRHIShaderModuleDesc PointVertex;
    Stoner::RHI::FRHIShaderModuleDesc PointFragment;
    Stoner::RHI::FRHIShaderModuleDesc SpotVertex;
    Stoner::RHI::FRHIShaderModuleDesc SpotFragment;
    Stoner::RHI::FRHIShaderModuleDesc CompositionFragment;
};

class FVulkanNativeOffscreenSession
{
public:
    struct FImpl;

    explicit FVulkanNativeOffscreenSession(FVulkanNativeContext& InContext) noexcept;
    ~FVulkanNativeOffscreenSession();

    [[nodiscard]] Stoner::RHI::ERHIResult Execute(
        const FVulkanDeferredShaderSet& Shaders,
        FVulkanDeferredValidationReport& OutReport,
        EVulkanDeferredFailurePoint FailurePoint = EVulkanDeferredFailurePoint::None,
        const FVulkanDeferredUniformPayload* UniformPayload = nullptr);
    [[nodiscard]] Stoner::RHI::ERHIResult Shutdown() noexcept;

private:
    FVulkanNativeContext& Context;
    std::unique_ptr<FImpl> Impl;
    bool bShutdown = false;
};

} // namespace Stoner::Backend::Vulkan
