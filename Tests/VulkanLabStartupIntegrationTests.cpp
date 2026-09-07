#include "Application/FWindow.h"
#include "Application/FWindowDesc.h"
#include "Core/SGPlatform.h"
#include "RHI/RHIMinimal.h"
#include "VulkanRHI/FVulkanDevice.h"
#include "VulkanRHI/FVulkanNativeContext.h"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace
{

using namespace Stoner;
using namespace Stoner::Application;
using namespace Stoner::Backend::Vulkan;
using namespace Stoner::RHI;

void Record(int& Failed, bool bPassed, const char* Name)
{
    if (!bPassed)
    {
        ++Failed;
    }
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

[[nodiscard]] bool IsRequired() noexcept
{
    const char* Value = std::getenv("STONER_REQUIRE_VULKAN_LAB_STARTUP");
    return Value != nullptr && std::string_view(Value) == "1";
}

[[nodiscard]] FVulkanInstanceDesc MakeDeviceDescription()
{
    FVulkanInstanceDesc Description;
    // FVulkanDevice owns deterministic adapter selection.  The explicit lab
    // opt-in below is what replaces the context with a real visible device.
    Description.RuntimeMode = EVulkanInstanceRuntimeMode::DeterministicFallback;
    Description.bRequestValidation = false;
    return Description;
}

[[nodiscard]] FWindowDesc MakeWindowDescription(const char* Title)
{
    FWindowDesc Description;
    Description.Title = Title;
    Description.ClientWidth = 160;
    Description.ClientHeight = 96;
    Description.bVisible = true;
    Description.bHighDensityFramebuffer = true;
    return Description;
}

bool QueryLabCapabilities(
    FVulkanDevice& Device,
    FRHIPresentationCapabilities& OutCapabilities)
{
    const auto Context = Device.GetNativePresentationContext();
    return Context && Context->IsAvailable() &&
        Context->QueryVisiblePresentationCapabilities(OutCapabilities) ==
            ERHIResult::Success;
}

[[maybe_unused]] bool RunLabStartupCase(
    int& Failed, bool bForceAcquireHistory, const char* Name)
{
    FWindow Window;
    const bool bWindowCreated =
        Window.CreateRealWindow(MakeWindowDescription(Name)) ==
        EApplicationResult::Success;

    FVulkanDevice Device;
    const bool bDeviceInitialized = bWindowCreated &&
        Device.Initialize(MakeDeviceDescription()) == ERHIResult::Success;
    const ERHIResult StartupResult = bDeviceInitialized
        ? Device.EnableNativeLabPresentationRuntime(
            Window.GetPlatformWindow(), bForceAcquireHistory)
        : ERHIResult::InvalidState;

    FRHIPresentationCapabilities Capabilities;
    const bool bCapabilities = StartupResult == ERHIResult::Success &&
        QueryLabCapabilities(Device, Capabilities);
    const bool bModeDeferredUntilBorrowedPath = bCapabilities &&
        Capabilities.PresentationRetirementMode ==
            ERHIPresentationRetirementMode::Unknown;
    const bool bAdvertisementAndEnablementAreIndependent = bCapabilities &&
        (!Capabilities.bOptionalPresentationFenceEnabled ||
            Capabilities.bOptionalPresentationFenceAdvertised);
    const bool bForceOffHonored = !bForceAcquireHistory ||
        (bCapabilities && !Capabilities.bOptionalPresentationFenceEnabled);
    const bool bNoSwapchainsBeforeShutdown =
        Device.GetRuntimeSnapshot().LiveSwapchains == 0;

    if (bCapabilities)
    {
        std::cout << "[INFO] Vulkan lab startup forceAcquireHistory="
            << (bForceAcquireHistory ? "true" : "false")
            << " optionalAdvertised="
            << (Capabilities.bOptionalPresentationFenceAdvertised ? "true" : "false")
            << " optionalEnabled="
            << (Capabilities.bOptionalPresentationFenceEnabled ? "true" : "false")
            << " retirementMode=Unknown\n";
    }

    Record(Failed,
        bWindowCreated && bDeviceInitialized &&
            StartupResult == ERHIResult::Success,
        Name);
    Record(Failed,
        bCapabilities && bModeDeferredUntilBorrowedPath &&
            bAdvertisementAndEnablementAreIndependent && bForceOffHonored,
        bForceAcquireHistory
            ? "forced acquire-history startup exposes facts without publishing a retirement mode"
            : "automatic lab startup exposes facts without publishing a retirement mode");

    const ERHIResult ShutdownResult = Device.Shutdown();
    const bool bNoNativeObjects =
        Device.GetRuntimeSnapshot().GetTotalLiveObjectCount() == 0;
    Record(Failed,
        bNoSwapchainsBeforeShutdown && ShutdownResult == ERHIResult::Success &&
            bNoNativeObjects,
        bForceAcquireHistory
            ? "forced acquire-history startup shuts down without native leaks"
            : "automatic lab startup shuts down without native leaks");
    (void)Window.Destroy();
    return bCapabilities && bModeDeferredUntilBorrowedPath &&
        bAdvertisementAndEnablementAreIndependent && bForceOffHonored &&
        bNoSwapchainsBeforeShutdown && ShutdownResult == ERHIResult::Success &&
        bNoNativeObjects;
}

[[maybe_unused]] void TestFormalStartupDoesNotEnableOptionalPath(int& Failed)
{
    FWindow Window;
    const bool bWindowCreated =
        Window.CreateRealWindow(MakeWindowDescription(
            "Stoner Vulkan Formal Startup Probe")) ==
        EApplicationResult::Success;
    FVulkanDevice Device;
    const bool bDeviceInitialized = bWindowCreated &&
        Device.Initialize(MakeDeviceDescription()) == ERHIResult::Success;
    const ERHIResult StartupResult = bDeviceInitialized
        ? Device.EnableNativePresentationRuntime(Window.GetPlatformWindow())
        : ERHIResult::InvalidState;

    FRHIPresentationCapabilities Capabilities;
    const bool bCapabilities = StartupResult == ERHIResult::Success &&
        QueryLabCapabilities(Device, Capabilities);
    const bool bNoSwapchainsBeforeShutdown =
        Device.GetRuntimeSnapshot().LiveSwapchains == 0;
    Record(Failed,
        bCapabilities &&
            !Capabilities.bOptionalPresentationFenceEnabled &&
            Capabilities.PresentationRetirementMode ==
                ERHIPresentationRetirementMode::Unknown,
        "formal native startup does not claim optional presentation enablement");

    const ERHIResult ShutdownResult = Device.Shutdown();
    Record(Failed,
        ShutdownResult == ERHIResult::Success &&
            bNoSwapchainsBeforeShutdown &&
            Device.GetRuntimeSnapshot().GetTotalLiveObjectCount() == 0,
        "formal native startup shuts down without native leaks");
    (void)Window.Destroy();
}

[[maybe_unused]] void TestStaleStartupRequestDoesNotMutateDevice(int& Failed)
{
    FWindow Window;
    const bool bWindowCreated =
        Window.CreateRealWindow(MakeWindowDescription(
            "Stoner Vulkan Stale Startup Probe")) ==
        EApplicationResult::Success;
    FVulkanDevice Device;
    const bool bDeviceInitialized = bWindowCreated &&
        Device.Initialize(MakeDeviceDescription()) == ERHIResult::Success;

    FRHIPresentationSurfaceDesc SurfaceDescription;
    if (bWindowCreated)
    {
        SurfaceDescription.Window = Window.GetPlatformWindow();
    }
    const auto Surface = bDeviceInitialized
        ? Device.CreatePresentationSurface(SurfaceDescription)
        : TRHIObjectResult<IRHIPresentationSurface>{};
    const bool bBefore = bDeviceInitialized && Surface.Succeeded() &&
        Surface.Object && Surface.Object->IsValid() &&
        !Device.HasNativePresentationRuntime() &&
        !Device.GetNativePresentationContext();
    const ERHIRuntimeMode RuntimeBefore = Device.GetRuntimeMode();

    const ERHIResult StartupResult = bBefore
        ? Device.EnableNativeLabPresentationRuntime(
            Window.GetPlatformWindow(), false)
        : ERHIResult::InvalidState;
    Record(Failed,
        bBefore && StartupResult == ERHIResult::InvalidState &&
            Surface.Object->IsValid() &&
            !Device.HasNativePresentationRuntime() &&
            !Device.GetNativePresentationContext() &&
            Device.GetRuntimeMode() == RuntimeBefore,
        "stale lab startup request is rejected without mutating admitted surface state");

    (void)Device.Shutdown();
    (void)Window.Destroy();
}

} // namespace

int RunVulkanLabStartupIntegrationTests()
{
    int Failed = 0;
    if (!IsRequired())
    {
        Record(Failed, true,
            "Vulkan lab startup is explicit opt-in via STONER_REQUIRE_VULKAN_LAB_STARTUP=1");
        return Failed;
    }

#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE && \
    defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
    TestFormalStartupDoesNotEnableOptionalPath(Failed);
    TestStaleStartupRequestDoesNotMutateDevice(Failed);
    (void)RunLabStartupCase(Failed, false, "automatic Vulkan lab startup creates a visible native device");
    (void)RunLabStartupCase(Failed, true, "forced acquire-history lab startup creates a visible native device");
#else
    Record(Failed, false,
        "required Vulkan lab startup needs native Vulkan and GLFW support");
#endif
    return Failed;
}
