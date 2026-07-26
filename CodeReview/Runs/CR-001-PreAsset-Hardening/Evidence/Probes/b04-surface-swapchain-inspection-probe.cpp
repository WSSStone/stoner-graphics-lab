#include "RHI/FRHIPresentationSurfaceDesc.h"
#include "RHI/FRHISwapchainDesc.h"
#include "RHI/IRHIDevice.h"
#include "VulkanRHI/FVulkanDevice.h"
#include "VulkanRHI/FVulkanSurface.h"
#include "VulkanRHI/FVulkanSwapchain.h"

#include <iostream>
#include <memory>

using namespace Stoner::Backend::Vulkan;
using namespace Stoner::Core;
using namespace Stoner::RHI;

namespace
{

ERHIResult InitializeFallback(FVulkanDevice& Device)
{
    FVulkanInstanceDesc Desc;
    Desc.RuntimeMode = EVulkanInstanceRuntimeMode::DeterministicFallback;
    return Device.Initialize(Desc);
}

} // namespace

int main()
{
    FVulkanDevice DeviceA;
    FVulkanDevice DeviceB;
    const bool bDevicesReady =
        InitializeFallback(DeviceA) == ERHIResult::Success &&
        InitializeFallback(DeviceB) == ERHIResult::Success;

    int NativeToken = 7;
    const FPlatformWindow Window(&NativeToken);
    FVulkanSurface Surface;
    const bool bSurfaceReady = bDevicesReady &&
        DeviceA.CreateSurface(Window, Surface) == ERHIResult::Success &&
        Surface.IsValid();

    IRHIDevice& BaseDevice = DeviceB;
    FRHIPresentationSurfaceDesc SurfaceDesc;
    SurfaceDesc.Window = Window;
    const auto RhiSurface = BaseDevice.CreatePresentationSurface(SurfaceDesc);

    FRHISwapchainDesc SwapchainDesc;
    SwapchainDesc.Width = 64;
    SwapchainDesc.Height = 64;
    const auto RhiSwapchain = BaseDevice.CreateSwapchain(
        RhiSurface.Object, SwapchainDesc);
    const bool bBackendNeutralPresentationUnsupported =
        RhiSurface.Result == ERHIResult::Unsupported && !RhiSurface.Object &&
        RhiSwapchain.Result == ERHIResult::Unsupported && !RhiSwapchain.Object;

    const auto SurfaceFreeSwapchain = DeviceB.CreateSwapchain(2);
    const bool bLegacySurfaceFreeSwapchainAvailable =
        SurfaceFreeSwapchain.Succeeded();

    const auto OriginalSwapchain = DeviceA.CreateSwapchainForSurface(Surface, 2);
    const bool bOriginalSwapchainReady = OriginalSwapchain.Succeeded();
    const ERHIResult ShutdownA = DeviceA.Shutdown();
    const bool bSurfaceSurvivesOwnerShutdown =
        ShutdownA == ERHIResult::Success && Surface.IsValid();

    const auto CrossDeviceSwapchain =
        DeviceB.CreateSwapchainForSurface(Surface, 2);
    const bool bCrossDeviceStaleSurfaceAccepted =
        CrossDeviceSwapchain.Succeeded();

    auto ConcreteCrossDevice =
        std::dynamic_pointer_cast<FVulkanSwapchain>(CrossDeviceSwapchain.Object);
    Surface.Invalidate();
    if (ConcreteCrossDevice)
    {
        ConcreteCrossDevice->SetUnavailable();
    }
    const bool bRecreateIgnoresLostSurface = ConcreteCrossDevice &&
        ConcreteCrossDevice->Recreate(2) == ERHIResult::Success &&
        ConcreteCrossDevice->GetState() == ERHISwapchainState::Ready;

    FVulkanSurface PreservedOutput;
    const bool bPreparedOutput =
        FVulkanSurface::Create(Window, PreservedOutput) == ERHIResult::Success;
    FVulkanDevice InactiveDevice;
    const ERHIResult FailedCreate =
        InactiveDevice.CreateSurface(FPlatformWindow{}, PreservedOutput);
    const bool bFailedFactoryPreservesUsableOutput = bPreparedOutput &&
        FailedCreate == ERHIResult::InvalidState && PreservedOutput.IsValid();

    const auto DirectZeroFrameSwapchain = DeviceB.CreateSwapchain(0);
    const bool bZeroFrameClassifiedUnsupported =
        DirectZeroFrameSwapchain.Result == ERHIResult::Unsupported;

    std::cout
        << "devices_ready=" << bDevicesReady << '\n'
        << "surface_ready=" << bSurfaceReady << '\n'
        << "backend_neutral_presentation_unsupported="
        << bBackendNeutralPresentationUnsupported << '\n'
        << "legacy_surface_free_swapchain_available="
        << bLegacySurfaceFreeSwapchainAvailable << '\n'
        << "original_swapchain_ready=" << bOriginalSwapchainReady << '\n'
        << "surface_survives_owner_shutdown="
        << bSurfaceSurvivesOwnerShutdown << '\n'
        << "cross_device_stale_surface_accepted="
        << bCrossDeviceStaleSurfaceAccepted << '\n'
        << "recreate_ignores_lost_surface="
        << bRecreateIgnoresLostSurface << '\n'
        << "failed_factory_preserves_usable_output="
        << bFailedFactoryPreservesUsableOutput << '\n'
        << "zero_frame_classified_unsupported="
        << bZeroFrameClassifiedUnsupported << '\n';

    if (DeviceB.IsActive())
    {
        (void)DeviceB.Shutdown();
    }

    const bool bDefectsReproduced = bDevicesReady && bSurfaceReady &&
        bBackendNeutralPresentationUnsupported &&
        bLegacySurfaceFreeSwapchainAvailable && bOriginalSwapchainReady &&
        bSurfaceSurvivesOwnerShutdown && bCrossDeviceStaleSurfaceAccepted &&
        bRecreateIgnoresLostSurface && bFailedFactoryPreservesUsableOutput &&
        bZeroFrameClassifiedUnsupported;
    std::cout << "classification="
              << (bDefectsReproduced
                      ? "surface-swapchain-contract-defects"
                      : "unexpected")
              << '\n';
    return bDefectsReproduced ? 0 : 3;
}
