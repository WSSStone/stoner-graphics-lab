#pragma once

#include "MetalRHI/FMetalBackendConfig.h"
#include "MetalRHI/FMetalBackendDiagnostics.h"
#include "MetalRHI/FMetalBackendInspection.h"
#include "RHI/IRHIDevice.h"

namespace Stoner::Backend::Metal
{

struct FMetalDeviceCreateResult
{
    RHI::ERHIResult Result = RHI::ERHIResult::Failed;
    Core::TSharedPtr<RHI::IRHIDevice> Device;
    FMetalBackendDiagnostics Diagnostics;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return Result == RHI::ERHIResult::Success && Device != nullptr;
    }
};

[[nodiscard]] FMetalDeviceCreateResult CreateMetalDevice(
    const FMetalBackendConfig& Config = {}) noexcept;

[[nodiscard]] bool InspectMetalDevice(
    const Core::TSharedPtr<RHI::IRHIDevice>& Device,
    FMetalBackendInspection& OutInspection) noexcept;

[[nodiscard]] bool InspectMetalDiagnostics(
    const Core::TSharedPtr<RHI::IRHIDevice>& Device,
    FMetalBackendDiagnostics& OutDiagnostics) noexcept;

[[nodiscard]] RHI::ERHIResult ReadMetalBufferForValidation(
    const Core::TSharedPtr<RHI::IRHIDevice>& Device,
    const Core::TSharedPtr<RHI::IRHIBuffer>& Buffer,
    Core::uint64 Offset,
    Core::uint64 Size,
    Core::TArray<Core::uint8>& OutBytes) noexcept;

} // namespace Stoner::Backend::Metal
