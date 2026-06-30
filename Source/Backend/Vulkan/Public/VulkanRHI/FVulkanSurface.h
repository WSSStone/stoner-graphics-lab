#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIResult.h"

namespace Stoner::Backend::Vulkan
{

class FVulkanSurface
{
public:
    [[nodiscard]] static Stoner::RHI::ERHIResult Create(const Stoner::Core::FPlatformWindow& Window, FVulkanSurface& OutSurface) noexcept;

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] void* GetNativeHandle() const noexcept;
    [[nodiscard]] const char* GetDiagnosticReason() const noexcept;
    void Invalidate() noexcept;

private:
    void* NativeHandle = nullptr;
    const char* DiagnosticReason = "";
};

} // namespace Stoner::Backend::Vulkan
