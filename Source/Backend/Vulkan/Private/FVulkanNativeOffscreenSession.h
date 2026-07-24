#pragma once

#include "VulkanRHI/FVulkanNativeContext.h"

#include <memory>

namespace Stoner::Backend::Vulkan
{

class FVulkanNativeOffscreenSession
{
public:
    struct FImpl;

    explicit FVulkanNativeOffscreenSession(FVulkanNativeContext& InContext) noexcept;
    ~FVulkanNativeOffscreenSession();

    [[nodiscard]] Stoner::RHI::ERHIResult Execute(const Stoner::Core::FString& ShaderDirectory,
        FVulkanDeferredValidationReport& OutReport);
    [[nodiscard]] Stoner::RHI::ERHIResult Shutdown() noexcept;

private:
    FVulkanNativeContext& Context;
    std::unique_ptr<FImpl> Impl;
    bool bShutdown = false;
};

} // namespace Stoner::Backend::Vulkan
