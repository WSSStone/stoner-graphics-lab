#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIRuntimeMode.h"
#include "RHI/ERHIFormat.h"
#include "RHI/ERHIPresentationColorSpace.h"

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
    Stoner::Core::uint64 PresentationModeGeneration = 0;
    Stoner::Core::uint32 PresentationWidth = 0;
    Stoner::Core::uint32 PresentationHeight = 0;
    ERHIFormat PresentationFormat = ERHIFormat::Unknown;
    ERHIPresentationColorSpace PresentationColorSpace =
        ERHIPresentationColorSpace::Unknown;
    ERHIPresentationNativeEncoding PresentationNativeEncoding =
        ERHIPresentationNativeEncoding::Unknown;
    ERHIPresentationDisplayAdaptation PresentationDisplayAdaptation =
        ERHIPresentationDisplayAdaptation::None;
    Stoner::Core::uint64 LastAcquiredFrameToken = 0;
    Stoner::Core::uint64 LastSubmittedFrameToken = 0;
    Stoner::Core::uint64 LastPresentedFrameToken = 0;
    Stoner::Core::FString PresentationMetadataDigest;

    [[nodiscard]] Stoner::Core::uint64 GetTotalLiveObjectCount() const noexcept
    {
        return static_cast<Stoner::Core::uint64>(LiveInstances) + LiveDevices + LiveSurfaces + LiveSwapchains + LiveBuffers + LiveTextures +
            LiveShaderModules + LivePipelines + LiveCommandBuffers + LiveSynchronizationObjects;
    }

    [[nodiscard]] bool ProvesNativeExecution() const noexcept
    {
        const bool bNativeRequested =
            RequestedMode == ERHIRuntimeMode::Native || RequestedMode == ERHIRuntimeMode::NativeHeadless;
        return bNativeRequested && ObjectMode == ERHIRuntimeObjectMode::RealRuntime &&
            LiveInstances > 0 && LiveDevices > 0;
    }
};

} // namespace Stoner::RHI
