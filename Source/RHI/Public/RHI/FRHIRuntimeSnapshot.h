#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIRuntimeMode.h"

namespace Stoner::RHI
{

struct FRHIRuntimeSnapshot
{
    ERHIRuntimeMode RequestedMode = ERHIRuntimeMode::Deterministic;
    ERHIRuntimeObjectMode ObjectMode = ERHIRuntimeObjectMode::DeterministicFallback;
    Stoner::Core::FString AdapterName = "Deterministic";
    bool bSoftwareDevice = false;
    Stoner::Core::uint32 LiveInstances = 0;
    Stoner::Core::uint32 LiveDevices = 0;
    Stoner::Core::uint32 LiveSurfaces = 0;
    Stoner::Core::uint32 LiveSwapchains = 0;
    Stoner::Core::uint32 LiveBuffers = 0;
    Stoner::Core::uint32 LiveTextures = 0;
    Stoner::Core::uint32 LiveShaderModules = 0;
    Stoner::Core::uint32 LivePipelines = 0;
    Stoner::Core::uint32 LiveCommandBuffers = 0;
    Stoner::Core::uint32 LiveSynchronizationObjects = 0;

    [[nodiscard]] Stoner::Core::uint32 GetTotalLiveObjectCount() const noexcept
    {
        return LiveInstances + LiveDevices + LiveSurfaces + LiveSwapchains + LiveBuffers + LiveTextures +
            LiveShaderModules + LivePipelines + LiveCommandBuffers + LiveSynchronizationObjects;
    }

    [[nodiscard]] bool ProvesNativeExecution() const noexcept
    {
        return ObjectMode == ERHIRuntimeObjectMode::RealRuntime && LiveInstances > 0 && LiveDevices > 0;
    }
};

} // namespace Stoner::RHI
