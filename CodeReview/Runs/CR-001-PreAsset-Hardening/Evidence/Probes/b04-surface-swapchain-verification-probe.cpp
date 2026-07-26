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

bool InitializeFallback(FVulkanDevice& Device)
{
    FVulkanInstanceDesc Desc;
    Desc.RuntimeMode = EVulkanInstanceRuntimeMode::DeterministicFallback;
    return Device.Initialize(Desc) == ERHIResult::Success;
}

FRHIPresentationSurfaceDesc MakeSurfaceDesc(void* NativeHandle, const char* Name)
{
    FRHIPresentationSurfaceDesc Desc;
    Desc.Window = FPlatformWindow(NativeHandle);
    Desc.DebugName = Name;
    return Desc;
}

FRHISwapchainDesc MakeSwapchainDesc()
{
    FRHISwapchainDesc Desc;
    Desc.Width = 96;
    Desc.Height = 54;
    Desc.FramesInFlight = 2;
    Desc.PreferredFormat = ERHIFormat::B8G8R8A8_UNorm;
    return Desc;
}

} // namespace

int main()
{
    FVulkanDevice Owner;
    FVulkanDevice Foreign;
    const bool bDevicesReady =
        InitializeFallback(Owner) && InitializeFallback(Foreign);
    IRHIDevice& OwnerRhi = Owner;

    int NativeToken = 29;
    const auto Surface = OwnerRhi.CreatePresentationSurface(
        MakeSurfaceDesc(&NativeToken, "VerificationSurface"));
    const FRHISwapchainDesc Desc = MakeSwapchainDesc();
    const auto Swapchain = OwnerRhi.CreateSwapchain(Surface.Object, Desc);
    const auto ConcreteSwapchain =
        std::dynamic_pointer_cast<FVulkanSwapchain>(Swapchain.Object);
    const bool bBackendNeutralDispatch =
        bDevicesReady && Surface.Succeeded() && Swapchain.Succeeded() &&
        ConcreteSwapchain && Surface.Object->IsValid() &&
        Surface.Object->GetDesc().Window.GetNativeHandle() == &NativeToken &&
        Surface.Object->GetDesc().DebugName.View() == "VerificationSurface";

    const auto OldImage0 = Swapchain.Object ? Swapchain.Object->GetImage(0) : nullptr;
    const auto OldImage1 = Swapchain.Object ? Swapchain.Object->GetImage(1) : nullptr;
    const bool bDescriptorAndImageContract =
        OldImage0 && OldImage1 && !Swapchain.Object->GetImage(2) &&
        OldImage0->GetDesc().Width == Desc.Width &&
        OldImage0->GetDesc().Height == Desc.Height &&
        OldImage0->GetFormat() == Desc.PreferredFormat &&
        HasRHIFlag(OldImage0->GetUsage(), ERHITextureUsage::ColorAttachment) &&
        HasRHIFlag(OldImage0->GetUsage(), ERHITextureUsage::Present);

    const uint64 OldGeneration =
        ConcreteSwapchain ? ConcreteSwapchain->GetGeneration() : 0;
    const bool bRecreated = ConcreteSwapchain &&
        ConcreteSwapchain->Recreate(3) == ERHIResult::Success;
    const auto NewImage0 = Swapchain.Object ? Swapchain.Object->GetImage(0) : nullptr;
    const bool bGenerationReplacement =
        bRecreated && ConcreteSwapchain->GetGeneration() == OldGeneration + 1 &&
        ConcreteSwapchain->GetFrameCount() == 3 && NewImage0 &&
        NewImage0 != OldImage0 && Swapchain.Object->GetImage(2) &&
        OldImage0->GetLifecycleState() ==
            ERHIResourceLifecycleState::Invalidated &&
        OldImage1->GetLifecycleState() ==
            ERHIResourceLifecycleState::Invalidated &&
        NewImage0->GetLifecycleState() == ERHIResourceLifecycleState::Valid;

    const auto AcquireSignal = Owner.CreateSemaphore();
    const auto UnsignaledWait = Owner.CreateSemaphore();
    uint32 FrameIndex = 73;
    const bool bAcquireFailureAtomic =
        AcquireSignal.Succeeded() && UnsignaledWait.Succeeded() &&
        AcquireSignal.Object->Signal() == ERHIResult::Success &&
        Swapchain.Object->AcquireNextFrame(FrameIndex, AcquireSignal.Object) ==
            ERHIResult::InvalidState &&
        FrameIndex == 73 && AcquireSignal.Object->IsSignaled() &&
        Swapchain.Object->GetState() == ERHISwapchainState::Ready;
    const bool bAcquireSucceeded =
        AcquireSignal.Object->Reset() == ERHIResult::Success &&
        Swapchain.Object->AcquireNextFrame(FrameIndex, AcquireSignal.Object) ==
            ERHIResult::Success &&
        FrameIndex == 0 && AcquireSignal.Object->IsSignaled();
    const bool bPresentFailureAtomic =
        bAcquireSucceeded &&
        Swapchain.Object->Present(FrameIndex, UnsignaledWait.Object) ==
            ERHIResult::NotReady &&
        Swapchain.Object->GetState() == ERHISwapchainState::Acquired &&
        UnsignaledWait.Object->GetState() == ERHISemaphoreState::Unsignaled &&
        AcquireSignal.Object->IsSignaled() &&
        Swapchain.Object->Present(FrameIndex, AcquireSignal.Object) ==
            ERHIResult::Success &&
        Swapchain.Object->GetState() == ERHISwapchainState::Ready &&
        AcquireSignal.Object->GetState() == ERHISemaphoreState::Consumed;
    const bool bSynchronizedFailureAtomicity =
        bAcquireFailureAtomic && bPresentFailureAtomic;

    FRHISwapchainDesc InvalidDesc = Desc;
    InvalidDesc.Width = 0;
    FRHISwapchainDesc DepthDesc = Desc;
    DepthDesc.PreferredFormat = ERHIFormat::D32_Float;
    FRHISwapchainDesc UnsupportedDesc = Desc;
    UnsupportedDesc.FramesInFlight =
        Owner.GetCapabilities().MaxInFlightFrames + 1;
    const bool bResultClassification =
        OwnerRhi.CreateSwapchain(Surface.Object, InvalidDesc).Result ==
            ERHIResult::InvalidState &&
        OwnerRhi.CreateSwapchain(Surface.Object, DepthDesc).Result ==
            ERHIResult::InvalidState &&
        OwnerRhi.CreateSwapchain(Surface.Object, UnsupportedDesc).Result ==
            ERHIResult::Unsupported &&
        Owner.CreateSwapchain(0).Result == ERHIResult::InvalidState;

    const bool bForeignRejected =
        Foreign.CreateSwapchain(Surface.Object, Desc).Result ==
        ERHIResult::InvalidState;

    FVulkanSurface LegacyOutput;
    const bool bLegacyPrepared =
        Owner.CreateSurface(FPlatformWindow(&NativeToken), LegacyOutput) ==
            ERHIResult::Success &&
        LegacyOutput.IsValid();
    const bool bFactoryFailureAtomicity =
        bLegacyPrepared &&
        Owner.CreateSurface(FPlatformWindow{}, LegacyOutput) ==
            ERHIResult::InvalidState &&
        !LegacyOutput.IsValid() && LegacyOutput.GetNativeHandle() == nullptr;

    const auto LostSurface = OwnerRhi.CreatePresentationSurface(
        MakeSurfaceDesc(&NativeToken, "SharedLossSurface"));
    const auto LostSwapchain = OwnerRhi.CreateSwapchain(LostSurface.Object, Desc);
    const auto ConcreteLostSurface =
        std::dynamic_pointer_cast<FVulkanSurface>(LostSurface.Object);
    FVulkanSurface SharedSurfaceCopy =
        ConcreteLostSurface ? *ConcreteLostSurface : FVulkanSurface{};
    uint32 PreservedIndex = 101;
    const bool bProvenanceAndSharedLoss =
        bForeignRejected && ConcreteLostSurface && LostSwapchain.Succeeded() &&
        SharedSurfaceCopy.Invalidate() == ERHIResult::Success &&
        !ConcreteLostSurface->IsValid() &&
        LostSwapchain.Object->GetState() == ERHISwapchainState::Unavailable &&
        LostSwapchain.Object->AcquireNextFrame(PreservedIndex) ==
            ERHIResult::Unavailable &&
        PreservedIndex == 101 &&
        ConcreteLostSurface->GetNativeHandle() == nullptr;

    const auto ShutdownSurface = OwnerRhi.CreatePresentationSurface(
        MakeSurfaceDesc(&NativeToken, "ShutdownSurface"));
    const auto ShutdownSwapchain =
        OwnerRhi.CreateSwapchain(ShutdownSurface.Object, Desc);
    const auto ShutdownImage =
        ShutdownSwapchain.Object ? ShutdownSwapchain.Object->GetImage(0) : nullptr;
    const bool bShutdownPrepared = ShutdownSurface.Succeeded() &&
        ShutdownSwapchain.Succeeded() && ShutdownImage;
    const bool bShutdownCascade = bShutdownPrepared &&
        Owner.Shutdown() == ERHIResult::Success &&
        !ShutdownSurface.Object->IsValid() &&
        ShutdownSwapchain.Object->AcquireNextFrame(PreservedIndex) ==
            ERHIResult::InvalidState &&
        ShutdownImage->GetLifecycleState() ==
            ERHIResourceLifecycleState::Invalidated;

    const bool bDeterministicNotNative =
        Owner.GetDiagnostics().Availability ==
            EVulkanBackendAvailability::DeterministicFallback &&
        Owner.GetDiagnostics().bUsedRuntimeFallback;

    if (Foreign.IsActive())
    {
        (void)Foreign.Shutdown();
    }

    std::cout
        << "backend_neutral_dispatch=" << bBackendNeutralDispatch << '\n'
        << "descriptor_and_image_contract=" << bDescriptorAndImageContract << '\n'
        << "generation_replacement=" << bGenerationReplacement << '\n'
        << "synchronized_failure_atomicity="
        << bSynchronizedFailureAtomicity << '\n'
        << "result_classification=" << bResultClassification << '\n'
        << "factory_failure_atomicity=" << bFactoryFailureAtomicity << '\n'
        << "provenance_and_shared_loss=" << bProvenanceAndSharedLoss << '\n'
        << "shutdown_cascade=" << bShutdownCascade << '\n'
        << "deterministic_not_native=" << bDeterministicNotNative << '\n';

    const bool bPassed = bBackendNeutralDispatch &&
        bDescriptorAndImageContract && bGenerationReplacement &&
        bSynchronizedFailureAtomicity && bResultClassification &&
        bFactoryFailureAtomicity && bProvenanceAndSharedLoss &&
        bShutdownCascade && bDeterministicNotNative;
    std::cout << "classification="
              << (bPassed ? "surface-swapchain-contracts-verified"
                          : "unexpected")
              << '\n';
    return bPassed ? 0 : 3;
}
