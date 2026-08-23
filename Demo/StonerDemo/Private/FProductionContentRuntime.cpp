#include "FProductionContentRuntime.h"

#include "Core/SGPlatform.h"
#include "VulkanRHI/FVulkanDevice.h"

#if SG_PLATFORM_MAC
#include "MetalRHI/FMetalDeviceFactory.h"
#endif

namespace Stoner::Demo
{

RHI::ERHIResult ReadProductionBuffer(
    EDemoGraphicsBackend Backend,
    const Core::TSharedPtr<RHI::IRHIDevice>& Device,
    const Core::TSharedPtr<RHI::IRHIBuffer>& Buffer,
    Core::uint64 ByteCount,
    Core::TArray<Core::uint8>& OutBytes)
{
    if (Backend == EDemoGraphicsBackend::Vulkan)
    {
        const auto VulkanDevice = std::dynamic_pointer_cast<
            Stoner::Backend::Vulkan::FVulkanDevice>(Device);
        return VulkanDevice
            ? VulkanDevice->ReadbackBufferForTesting(
                Buffer, 0, ByteCount, OutBytes)
            : RHI::ERHIResult::InvalidState;
    }
#if SG_PLATFORM_MAC
    return Stoner::Backend::Metal::ReadMetalBufferForValidation(
        Device, Buffer, 0, ByteCount, OutBytes);
#else
    (void)Device;
    (void)Buffer;
    (void)ByteCount;
    OutBytes.clear();
    return RHI::ERHIResult::Unsupported;
#endif
}

} // namespace Stoner::Demo
