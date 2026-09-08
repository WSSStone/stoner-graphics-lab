#pragma once
#include "RHI/IRHIDevice.h"
#include "RHI/FRHIShaderModuleDesc.h"
#include <functional>
#include <span>

using FUINativeReadback = std::function<Stoner::RHI::ERHIResult(
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer>&,
    Stoner::Core::uint64, Stoner::Core::TArray<Stoner::Core::uint8>&)>;
int RunUINativeRasterFixture(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIDevice>& Device,
    std::span<const Stoner::RHI::FRHIShaderModuleDesc> Draw,
    std::span<const Stoner::RHI::FRHIShaderModuleDesc> Copy,
    const FUINativeReadback& Readback,
    std::span<const Stoner::RHI::FRHIShaderModuleDesc> Diagnostic = {});
int RunVulkanUINativeTests(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIDevice>& Device,
    std::span<const Stoner::RHI::FRHIShaderModuleDesc> Draw,
    std::span<const Stoner::RHI::FRHIShaderModuleDesc> Copy,
    std::span<const Stoner::RHI::FRHIShaderModuleDesc> Diagnostic = {});
int RunMetalUINativeTests(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIDevice>& Device,
    std::span<const Stoner::RHI::FRHIShaderModuleDesc> Draw,
    std::span<const Stoner::RHI::FRHIShaderModuleDesc> Copy,
    std::span<const Stoner::RHI::FRHIShaderModuleDesc> Diagnostic = {});
