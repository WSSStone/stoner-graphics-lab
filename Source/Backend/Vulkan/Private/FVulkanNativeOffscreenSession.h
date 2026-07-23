#pragma once

#include "VulkanRHI/FVulkanNativeContext.h"

namespace Stoner::Backend::Vulkan
{

class FVulkanNativeOffscreenSession
{
public:
    explicit FVulkanNativeOffscreenSession(FVulkanNativeContext& InContext) noexcept;
    ~FVulkanNativeOffscreenSession();

    [[nodiscard]] Stoner::RHI::ERHIResult Execute(const Stoner::Core::FString& ShaderDirectory,
        FVulkanDeferredValidationReport& OutReport);
    [[nodiscard]] Stoner::RHI::ERHIResult Shutdown() noexcept;

private:
    void AddReferenceProbes(const char* Convention, float FarDepth,
        FVulkanDeferredValidationReport& Report);
    void TrackCreate(Stoner::Core::uint32 Count = 1) noexcept;
    void TrackReleaseAll() noexcept;

    FVulkanNativeContext& Context;
    Stoner::Core::uint32 LiveObjects = 0;
    Stoner::Core::uint32 PeakLiveObjects = 0;
    bool bShutdown = false;
};

} // namespace Stoner::Backend::Vulkan
