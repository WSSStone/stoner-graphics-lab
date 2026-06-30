#include "VulkanRHI/FVulkanSurface.h"

namespace Stoner::Backend::Vulkan
{

Stoner::RHI::ERHIResult FVulkanSurface::Create(const Stoner::Core::FPlatformWindow& Window, FVulkanSurface& OutSurface) noexcept
{
    OutSurface = {};
    if (!Window.IsValid())
    {
        OutSurface.DiagnosticReason = "invalid Core platform window wrapper";
        return Stoner::RHI::ERHIResult::InvalidState;
    }

    OutSurface.NativeHandle = Window.GetNativeHandle();
    return Stoner::RHI::ERHIResult::Success;
}

bool FVulkanSurface::IsValid() const noexcept
{
    return NativeHandle != nullptr;
}

void* FVulkanSurface::GetNativeHandle() const noexcept
{
    return NativeHandle;
}

const char* FVulkanSurface::GetDiagnosticReason() const noexcept
{
    return DiagnosticReason;
}

void FVulkanSurface::Invalidate() noexcept
{
    NativeHandle = nullptr;
    DiagnosticReason = "surface invalidated";
}

} // namespace Stoner::Backend::Vulkan
