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

FRHIPresentationSurfaceDesc MakeSurfaceDesc(void* NativeHandle)
{
    FRHIPresentationSurfaceDesc Desc;
    Desc.Window = FPlatformWindow(NativeHandle);
    Desc.DebugName = "FixProbeSurface";
    return Desc;
}

FRHISwapchainDesc MakeSwapchainDesc()
{
    FRHISwapchainDesc Desc;
    Desc.Width = 80;
    Desc.Height = 48;
    Desc.FramesInFlight = 2;
    Desc.PreferredFormat = ERHIFormat::B8G8R8A8_UNorm;
    return Desc;
}

} // namespace

int main()
{
    FVulkanDevice OwnerDevice;
    FVulkanDevice ForeignDevice;
    const bool bDevicesReady =
        InitializeFallback(OwnerDevice) == ERHIResult::Success &&
        InitializeFallback(ForeignDevice) == ERHIResult::Success;

    int NativeToken = 17;
    IRHIDevice& OwnerRhi = OwnerDevice;
    const auto Surface =
        OwnerRhi.CreatePresentationSurface(MakeSurfaceDesc(&NativeToken));
    const auto Swapchain =
        OwnerRhi.CreateSwapchain(Surface.Object, MakeSwapchainDesc());
    const auto Image0 = Swapchain.Object ? Swapchain.Object->GetImage(0) : nullptr;
    const bool bBackendNeutralContract =
        bDevicesReady && Surface.Succeeded() && Swapchain.Succeeded() &&
        Image0 && Swapchain.Object->GetImage(1) &&
        !Swapchain.Object->GetImage(2) &&
        Image0->GetDesc().Width == 80 &&
        Image0->GetDesc().Height == 48 &&
        HasRHIFlag(Image0->GetUsage(), ERHITextureUsage::Present);

    const auto Signal = OwnerDevice.CreateSemaphore();
    uint32 FrameIndex = 99;
    const bool bSynchronizedTransitions =
        Signal.Succeeded() &&
        Swapchain.Object->AcquireNextFrame(FrameIndex, Signal.Object) ==
            ERHIResult::Success &&
        FrameIndex == 0 && Signal.Object->IsSignaled() &&
        Swapchain.Object->Present(FrameIndex + 1, Signal.Object) ==
            ERHIResult::InvalidState &&
        Signal.Object->IsSignaled() &&
        Swapchain.Object->GetState() == ERHISwapchainState::Acquired &&
        Swapchain.Object->Present(FrameIndex, Signal.Object) ==
            ERHIResult::Success &&
        Signal.Object->GetState() == ERHISemaphoreState::Consumed &&
        Swapchain.Object->GetState() == ERHISwapchainState::Ready;

    const bool bForeignSurfaceRejected =
        ForeignDevice.CreateSwapchain(Surface.Object, MakeSwapchainDesc()).Result ==
        ERHIResult::InvalidState;

    FVulkanSurface LegacyOutput;
    const bool bLegacySurfaceReady =
        OwnerDevice.CreateSurface(FPlatformWindow(&NativeToken), LegacyOutput) ==
            ERHIResult::Success &&
        LegacyOutput.IsValid();
    FVulkanDevice InactiveDevice;
    const bool bFailedFactoryClearsOutput =
        bLegacySurfaceReady &&
        InactiveDevice.CreateSurface(FPlatformWindow{}, LegacyOutput) ==
            ERHIResult::InvalidState &&
        !LegacyOutput.IsValid();

    FVulkanSwapchain InvalidConcrete(0);
    uint32 PreservedIndex = 41;
    const bool bInvalidInputClassification =
        OwnerDevice.CreateSwapchain(0).Result == ERHIResult::InvalidState &&
        InvalidConcrete.AcquireNextFrame(PreservedIndex) ==
            ERHIResult::InvalidState &&
        PreservedIndex == 41;

    const auto ConcreteSurface =
        std::dynamic_pointer_cast<FVulkanSurface>(Surface.Object);
    const auto ConcreteSwapchain =
        std::dynamic_pointer_cast<FVulkanSwapchain>(Swapchain.Object);
    const bool bSurfaceLossBlocksRecovery =
        ConcreteSurface && ConcreteSwapchain &&
        ConcreteSurface->Invalidate() == ERHIResult::Success &&
        ConcreteSwapchain->GetState() == ERHISwapchainState::Unavailable &&
        ConcreteSwapchain->Recreate(2) == ERHIResult::Unavailable &&
        !ConcreteSwapchain->GetImage(0);

    const auto ShutdownSurface =
        OwnerRhi.CreatePresentationSurface(MakeSurfaceDesc(&NativeToken));
    const auto ShutdownSwapchain =
        OwnerRhi.CreateSwapchain(ShutdownSurface.Object, MakeSwapchainDesc());
    const auto ShutdownImage =
        ShutdownSwapchain.Object ? ShutdownSwapchain.Object->GetImage(0) : nullptr;
    const bool bShutdownPrepared =
        ShutdownSurface.Succeeded() && ShutdownSwapchain.Succeeded() &&
        ShutdownImage;
    const bool bShutdownInvalidatesOwnership =
        bShutdownPrepared && OwnerDevice.Shutdown() == ERHIResult::Success &&
        !ShutdownSurface.Object->IsValid() &&
        ShutdownSwapchain.Object->AcquireNextFrame(PreservedIndex) ==
            ERHIResult::InvalidState &&
        ShutdownImage->GetLifecycleState() ==
            ERHIResourceLifecycleState::Invalidated;

    if (ForeignDevice.IsActive())
    {
        (void)ForeignDevice.Shutdown();
    }

    std::cout
        << "backend_neutral_contract=" << bBackendNeutralContract << '\n'
        << "synchronized_transitions=" << bSynchronizedTransitions << '\n'
        << "foreign_surface_rejected=" << bForeignSurfaceRejected << '\n'
        << "failed_factory_clears_output=" << bFailedFactoryClearsOutput << '\n'
        << "invalid_input_classification=" << bInvalidInputClassification
        << '\n'
        << "surface_loss_blocks_recovery=" << bSurfaceLossBlocksRecovery << '\n'
        << "shutdown_invalidates_ownership="
        << bShutdownInvalidatesOwnership << '\n';

    const bool bPassed = bBackendNeutralContract && bSynchronizedTransitions &&
        bForeignSurfaceRejected && bFailedFactoryClearsOutput &&
        bInvalidInputClassification && bSurfaceLossBlocksRecovery &&
        bShutdownInvalidatesOwnership;
    std::cout << "classification="
              << (bPassed ? "surface-swapchain-contracts-fixed" : "unexpected")
              << '\n';
    return bPassed ? 0 : 3;
}
