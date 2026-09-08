#include "UINativeRasterFixture.h"
#include "VulkanRHI/FVulkanDevice.h"
#include <iostream>
int RunVulkanUINativeTests(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIDevice>& Device,
    std::span<const Stoner::RHI::FRHIShaderModuleDesc> Draw,
    std::span<const Stoner::RHI::FRHIShaderModuleDesc> Copy)
{
    const auto Vulkan = std::dynamic_pointer_cast<Stoner::Backend::Vulkan::FVulkanDevice>(Device);
    if (!Vulkan || !Vulkan->GetRuntimeSnapshot().NativeOperations.bAvailable)
    { std::cout << "[UNSUPPORTED] Vulkan native UI capability unavailable\n"; return 1; }
    return RunUINativeRasterFixture(Device,Draw,Copy,[Vulkan](const auto& Buffer,auto Bytes,auto& Out) {
        return Vulkan->ReadbackBufferForTesting(Buffer,0,Bytes,Out);
    });
}

int RunInteractiveLabNativeShaderTests(bool Metal);
int RunVulkanUINativeTests() { return RunInteractiveLabNativeShaderTests(false); }
