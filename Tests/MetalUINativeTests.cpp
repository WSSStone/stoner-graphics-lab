#include "UINativeRasterFixture.h"
#include "MetalRHI/FMetalDeviceFactory.h"
#include <iostream>
int RunMetalUINativeTests(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIDevice>& Device,
    std::span<const Stoner::RHI::FRHIShaderModuleDesc> Draw,
    std::span<const Stoner::RHI::FRHIShaderModuleDesc> Copy)
{
    if (!Device || !Device->GetRuntimeSnapshot().NativeOperations.bAvailable)
    { std::cout << "[UNSUPPORTED] Metal native UI capability unavailable\n"; return 1; }
    return RunUINativeRasterFixture(Device,Draw,Copy,[Device](const auto& Buffer,auto Bytes,auto& Out) {
        return Stoner::Backend::Metal::ReadMetalBufferForValidation(Device,Buffer,0,Bytes,Out);
    });
}
