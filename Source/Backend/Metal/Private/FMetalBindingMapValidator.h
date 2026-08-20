#pragma once

#include "RHI/ERHIResult.h"
#include "RHI/FRHIDeviceCapabilities.h"
#include "RHI/FRHINativeBindingMap.h"
#include "RHI/FRHIPipelineLayoutDesc.h"

namespace Stoner::Backend::Metal::Private
{

[[nodiscard]] RHI::ERHIResult ValidateMetalBindingMap(
    const RHI::FRHINativeBindingMap& Map,
    const RHI::FRHIShaderInterfaceMetadata& Interface,
    const RHI::FRHIPipelineLayoutDesc& Layout,
    const RHI::FRHIDeviceCapabilities& Capabilities) noexcept;

[[nodiscard]] const RHI::FRHINativeBindingEntry* FindMetalNativeBinding(
    const RHI::FRHINativeBindingMap& Map,
    RHI::ERHIShaderStage Stage,
    Core::uint32 SetIndex,
    Core::uint32 BindingSlot,
    RHI::ERHIDescriptorType DescriptorType,
    Core::uint32 ArrayElement) noexcept;

} // namespace Stoner::Backend::Metal::Private
