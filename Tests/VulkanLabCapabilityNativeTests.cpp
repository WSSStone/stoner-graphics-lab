#include "FVulkanLabCapabilityQuery.h"
#include "FVulkanLabDeviceStartup.h"

#include "Core/SGPlatform.h"

#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
#include "FVulkanStruct.h"

#include <vulkan/vulkan.h>
#endif

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string_view>
#include <vector>

namespace
{

using namespace Stoner::Backend::Vulkan::Private;

struct FTestState
{
    int Failed = 0;
};

void Record(FTestState& State, bool bPassed, const char* Name)
{
    if (!bPassed)
    {
        ++State.Failed;
    }
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

bool NativeQueryRequired() noexcept
{
    const char* Value = std::getenv("STONER_REQUIRE_VULKAN_CAPABILITY_QUERY");
    return Value != nullptr && std::string_view(Value) == "1";
}

void TestQueryMechanismSelection(FTestState& State)
{
    FVulkanLabCapabilityQueryFacts Core11;
    Core11.NegotiatedApiVersion =
        FVulkanLabCapabilityQuery::MakeApiVersion(1, 1);
    Record(State,
        FVulkanLabCapabilityQuery::SelectQueryMechanism(Core11) ==
            EVulkanLabCapabilityQueryMechanism::Core11,
        "negotiated Vulkan 1.1 selects the core feature query");

    FVulkanLabCapabilityQueryFacts KHR;
    KHR.NegotiatedApiVersion =
        FVulkanLabCapabilityQuery::MakeApiVersion(1, 0);
    KHR.bKHRGetPhysicalDeviceProperties2Enabled = true;
    Record(State,
        FVulkanLabCapabilityQuery::SelectQueryMechanism(KHR) ==
            EVulkanLabCapabilityQueryMechanism::KHRProperties2,
        "negotiated Vulkan 1.0 with enabled KHR properties2 selects its query");

    Record(State,
        FVulkanLabCapabilityQuery::SelectQueryMechanism({}) ==
            EVulkanLabCapabilityQueryMechanism::Unavailable,
        "Vulkan 1.0 without enabled properties2 reports query unavailability");
}

#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE

constexpr const char* KHRProperties2Extension =
    "VK_KHR_get_physical_device_properties2";
constexpr const char* KHRSurfaceExtension = "VK_KHR_surface";
constexpr const char* KHRSurfaceCapabilities2Extension =
    "VK_KHR_get_surface_capabilities2";
constexpr const char* EXTSurfaceMaintenance1Extension =
    "VK_EXT_surface_maintenance1";
constexpr const char* KHRSwapchainExtension = "VK_KHR_swapchain";
constexpr const char* KHRPortabilitySubsetExtension =
    "VK_KHR_portability_subset";
constexpr const char* PortabilityEnumerationExtension =
    "VK_KHR_portability_enumeration";

bool HasInstanceExtension(
    const std::vector<VkExtensionProperties>& Extensions,
    const char* Name)
{
    for (const VkExtensionProperties& Extension : Extensions)
    {
        if (std::strcmp(Extension.extensionName, Name) == 0)
        {
            return true;
        }
    }
    return false;
}

bool CreateMinimalInstance(
    VkInstance& OutInstance,
    bool& OutKHRProperties2Enabled,
    bool& OutSurfaceMaintenance1Enabled,
    bool& OutSurfaceEnabled,
    bool bAllowOptionalInstanceExtensions = true)
{
    OutInstance = VK_NULL_HANDLE;
    OutKHRProperties2Enabled = false;
    OutSurfaceMaintenance1Enabled = false;
    OutSurfaceEnabled = false;

    Stoner::Core::uint32 ExtensionCount = 0;
    if (vkEnumerateInstanceExtensionProperties(
            nullptr, &ExtensionCount, nullptr) != VK_SUCCESS)
    {
        return false;
    }
    std::vector<VkExtensionProperties> Extensions(ExtensionCount);
    if (vkEnumerateInstanceExtensionProperties(
            nullptr, &ExtensionCount, Extensions.data()) != VK_SUCCESS)
    {
        return false;
    }

    const bool bHasKHRProperties2 =
        HasInstanceExtension(Extensions, KHRProperties2Extension);
    const bool bHasSurface =
        HasInstanceExtension(Extensions, KHRSurfaceExtension);
    const bool bEnableSurfaceMaintenance1 =
        bAllowOptionalInstanceExtensions && bHasSurface &&
        HasInstanceExtension(Extensions, KHRSurfaceCapabilities2Extension) &&
        HasInstanceExtension(Extensions, EXTSurfaceMaintenance1Extension);
    const bool bHasPortabilityEnumeration = HasInstanceExtension(
        Extensions, PortabilityEnumerationExtension);
    std::vector<const char*> EnabledExtensions;
    if (bHasKHRProperties2)
    {
        EnabledExtensions.push_back(KHRProperties2Extension);
    }
    if (bHasSurface)
    {
        // Ordinary KHR_swapchain needs KHR_surface even when every optional
        // surface-maintenance dependency is unavailable or disabled.
        EnabledExtensions.push_back(KHRSurfaceExtension);
    }
    if (bEnableSurfaceMaintenance1)
    {
        // VK_EXT_surface_maintenance1 has transitive KHR_surface and
        // KHR_get_surface_capabilities2 instance dependencies.
        EnabledExtensions.push_back(KHRSurfaceCapabilities2Extension);
        EnabledExtensions.push_back(EXTSurfaceMaintenance1Extension);
    }
#if defined(VK_KHR_portability_enumeration)
    const bool bEnablePortabilityEnumeration = bHasPortabilityEnumeration;
#else
    const bool bEnablePortabilityEnumeration = false;
#endif
    if (bEnablePortabilityEnumeration)
    {
        EnabledExtensions.push_back(PortabilityEnumerationExtension);
    }

    VkApplicationInfo ApplicationInfo =
        Stoner::Backend::Vulkan::MakeVulkanStruct<VkApplicationInfo>(
            VK_STRUCTURE_TYPE_APPLICATION_INFO);
    ApplicationInfo.pApplicationName = "Stoner Vulkan Capability Test";
    ApplicationInfo.applicationVersion = 1;
    ApplicationInfo.pEngineName = "Stoner";
    ApplicationInfo.engineVersion = 1;
    ApplicationInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo CreateInfo =
        Stoner::Backend::Vulkan::MakeVulkanStruct<VkInstanceCreateInfo>(
            VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);
#if defined(VK_KHR_portability_enumeration)
    if (bEnablePortabilityEnumeration)
    {
        CreateInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
#endif
    CreateInfo.pApplicationInfo = &ApplicationInfo;
    CreateInfo.enabledExtensionCount = static_cast<uint32_t>(
        EnabledExtensions.size());
    CreateInfo.ppEnabledExtensionNames =
        EnabledExtensions.empty() ? nullptr : EnabledExtensions.data();
    if (vkCreateInstance(&CreateInfo, nullptr, &OutInstance) != VK_SUCCESS)
    {
        return false;
    }
    OutKHRProperties2Enabled = bHasKHRProperties2;
    OutSurfaceMaintenance1Enabled = bEnableSurfaceMaintenance1;
    OutSurfaceEnabled = bHasSurface;
    return true;
}

bool HasDeviceExtension(VkPhysicalDevice PhysicalDevice, const char* Name)
{
    uint32_t Count = 0;
    if (vkEnumerateDeviceExtensionProperties(
            PhysicalDevice, nullptr, &Count, nullptr) != VK_SUCCESS)
    {
        return false;
    }
    std::vector<VkExtensionProperties> Extensions(Count);
    if (Count == 0)
    {
        return false;
    }
    if (vkEnumerateDeviceExtensionProperties(
            PhysicalDevice, nullptr, &Count, Extensions.data()) != VK_SUCCESS)
    {
        return false;
    }
    return HasInstanceExtension(Extensions, Name);
}

bool MakeBaseDeviceCreateInfo(
    VkPhysicalDevice PhysicalDevice,
    std::vector<const char*>& OutExtensions,
    VkDeviceQueueCreateInfo& OutQueueInfo,
    VkDeviceCreateInfo& OutCreateInfo)
{
    uint32_t QueueCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        PhysicalDevice, &QueueCount, nullptr);
    if (QueueCount == 0)
    {
        return false;
    }
    std::vector<VkQueueFamilyProperties> QueueProperties(QueueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(
        PhysicalDevice, &QueueCount, QueueProperties.data());
    uint32_t QueueFamily = QueueCount;
    for (uint32_t Index = 0; Index < QueueCount; ++Index)
    {
        if (QueueProperties[Index].queueCount != 0)
        {
            QueueFamily = Index;
            break;
        }
    }
    if (QueueFamily == QueueCount)
    {
        return false;
    }

    OutExtensions.clear();
    if (!HasDeviceExtension(PhysicalDevice, KHRSwapchainExtension))
    {
        return false;
    }
    OutExtensions.push_back(KHRSwapchainExtension);
    if (HasDeviceExtension(PhysicalDevice, KHRPortabilitySubsetExtension))
    {
        OutExtensions.push_back(KHRPortabilitySubsetExtension);
    }

    static constexpr float Priority = 1.0f;
    OutQueueInfo = Stoner::Backend::Vulkan::MakeVulkanStruct<
        VkDeviceQueueCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
    OutQueueInfo.queueFamilyIndex = QueueFamily;
    OutQueueInfo.queueCount = 1;
    OutQueueInfo.pQueuePriorities = &Priority;
    OutCreateInfo = Stoner::Backend::Vulkan::MakeVulkanStruct<
        VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
    OutCreateInfo.queueCreateInfoCount = 1;
    OutCreateInfo.pQueueCreateInfos = &OutQueueInfo;
    OutCreateInfo.enabledExtensionCount = static_cast<uint32_t>(
        OutExtensions.size());
    OutCreateInfo.ppEnabledExtensionNames = OutExtensions.data();
    return true;
}

void TestLogicalDeviceStartup(
    FTestState& State,
    VkPhysicalDevice PhysicalDevice,
    const FVulkanLabPresentationCapabilityObservation& Observation)
{
    std::vector<const char*> BaseExtensions;
    VkDeviceQueueCreateInfo QueueInfo{};
    VkDeviceCreateInfo BaseCreateInfo{};
    if (!MakeBaseDeviceCreateInfo(
            PhysicalDevice, BaseExtensions, QueueInfo, BaseCreateInfo))
    {
        Record(State, !NativeQueryRequired(),
            "selected Vulkan device has no usable swapchain startup fixture");
        return;
    }

    const FVulkanLabPresentationPolicyCandidate Candidate =
        FVulkanLabPresentationPolicy::SelectCandidate({}, Observation);
    const auto FailedObservation = [&]()
    {
        auto Value = Observation;
        Value.QueryState = EVulkanLabCapabilityQueryState::Failed;
        Value.NativeQueryResult = static_cast<Stoner::Core::int32>(
            VK_ERROR_INITIALIZATION_FAILED);
        Value.QueryFailure = "synthetic query failure for startup rejection";
        return Value;
    }();
    const FVulkanLabDeviceStartupResult QueryFailedStartup =
        FVulkanLabDeviceStartup::Create(
            PhysicalDevice, BaseCreateInfo, {}, FailedObservation);
    Record(State,
        QueryFailedStartup.Device == VK_NULL_HANDLE &&
            !QueryFailedStartup.bBaselineCreationAttempted &&
            !QueryFailedStartup.bOptionalCreationAttempted &&
            !QueryFailedStartup.bFallbackCreationAttempted &&
            QueryFailedStartup.Selection.Reason ==
                Stoner::RHI::ERHIPresentationRetirementReason::QueryFailed &&
            QueryFailedStartup.Selection.NativeQueryResult ==
                FailedObservation.NativeQueryResult,
        "failed capability observation prevents logical-device creation");

    const FVulkanLabDeviceStartupResult Startup =
        FVulkanLabDeviceStartup::Create(
            PhysicalDevice, BaseCreateInfo, {}, Observation);
    std::cout << "[INFO] Vulkan logical startup candidate="
        << static_cast<int>(Candidate.Candidate)
        << " optionalAttempted="
        << (Startup.bOptionalCreationAttempted ? "true" : "false")
        << " fallbackAttempted="
        << (Startup.bFallbackCreationAttempted ? "true" : "false")
        << " succeeded=" << (Startup.bSucceeded ? "true" : "false")
        << "\n";
    Record(State,
        Startup.Device != VK_NULL_HANDLE && Startup.bSucceeded &&
            Startup.Selection.IsUsable() &&
            Startup.bUsedPresentEntryPointResolved,
        "usable swapchain device finalizes startup only after vkQueuePresentKHR resolves");
    if (Startup.Device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(Startup.Device, nullptr);
    }

    if (Candidate.Candidate ==
        EVulkanLabPresentationCandidate::PresentationFence)
    {
        Record(State,
            Startup.bOptionalCreationAttempted ||
                Startup.Selection.Reason ==
                    Stoner::RHI::ERHIPresentationRetirementReason::OptionalEnablementFailed,
            "advertised maintenance1 candidate attempts optional enablement before fallback");
        if (Startup.bOptionalCreationAttempted &&
            Startup.OptionalCreationResult == VK_ERROR_DEVICE_LOST)
        {
            Record(State,
                !Startup.bFallbackCreationAttempted && !Startup.bSucceeded,
                "optional device loss never becomes a fallback success");
        }
    }
    else
    {
        Record(State,
            !Startup.bOptionalCreationAttempted &&
                !Startup.bFallbackCreationAttempted &&
                Startup.bBaselineCreationAttempted,
            "unsupported or forced-off optional path uses one ordinary device create");
    }

    std::vector<const char*> InvalidExtensions = BaseExtensions;
    InvalidExtensions.push_back("VK_EXT_swapchain_maintenance1");
    VkDeviceCreateInfo InvalidBase = BaseCreateInfo;
    InvalidBase.enabledExtensionCount = static_cast<uint32_t>(
        InvalidExtensions.size());
    InvalidBase.ppEnabledExtensionNames = InvalidExtensions.data();
    const FVulkanLabDeviceStartupResult Rejected =
        FVulkanLabDeviceStartup::Create(
            PhysicalDevice, InvalidBase, {}, Observation);
    Record(State,
        Rejected.bInputRejected && Rejected.Device == VK_NULL_HANDLE &&
            !Rejected.bBaselineCreationAttempted &&
            !Rejected.bOptionalCreationAttempted &&
            !Rejected.bFallbackCreationAttempted,
        "base create info containing maintenance1 is rejected before creation");

    FVulkanLabPresentationPolicyRequest ForceOff;
    ForceOff.bForceAcquireHistory = true;
    const FVulkanLabDeviceStartupResult Forced =
        FVulkanLabDeviceStartup::Create(
            PhysicalDevice, BaseCreateInfo, ForceOff, Observation);
    Record(State,
        Forced.Device != VK_NULL_HANDLE && Forced.bSucceeded &&
            Forced.Selection.Mode ==
                Stoner::RHI::ERHIPresentationRetirementMode::AcquireHistory &&
            Forced.bUsedPresentEntryPointResolved,
        "force-off startup uses a usable ordinary swapchain device");
    if (Forced.Device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(Forced.Device, nullptr);
    }
}

void TestNativeSelectedDevice(
    FTestState& State, bool bAllowOptionalInstanceExtensions = true)
{
    FVulkanLabCapabilityQueryFacts InvalidFacts;
    const FVulkanLabPresentationCapabilityObservation InvalidObservation =
        FVulkanLabCapabilityQuery::Query(
            VK_NULL_HANDLE, VK_NULL_HANDLE, InvalidFacts);
    Record(State,
        InvalidObservation.QueryState == EVulkanLabCapabilityQueryState::Failed &&
            InvalidObservation.NativeQueryResult == 0 &&
            !InvalidObservation.QueryFailure.IsEmpty(),
        "invalid capability-query handles report local failure without inventing VkResult");

    VkInstance Instance = VK_NULL_HANDLE;
    bool bKHRProperties2Enabled = false;
    bool bSurfaceMaintenance1Enabled = false;
    bool bSurfaceEnabled = false;
    if (!CreateMinimalInstance(
            Instance, bKHRProperties2Enabled, bSurfaceMaintenance1Enabled,
            bSurfaceEnabled, bAllowOptionalInstanceExtensions))
    {
        Record(State, !NativeQueryRequired(),
            "Vulkan capability fixture is explicitly unavailable");
        return;
    }

    Stoner::Core::uint32 DeviceCount = 0;
    const VkResult DeviceCountResult = vkEnumeratePhysicalDevices(
        Instance, &DeviceCount, nullptr);
    if (DeviceCountResult != VK_SUCCESS || DeviceCount == 0)
    {
        vkDestroyInstance(Instance, nullptr);
        Record(State, !NativeQueryRequired(),
            "Vulkan capability fixture has no selected physical device");
        return;
    }
    std::vector<VkPhysicalDevice> Devices(DeviceCount);
    const VkResult DeviceResult = vkEnumeratePhysicalDevices(
        Instance, &DeviceCount, Devices.data());
    if (DeviceResult != VK_SUCCESS || Devices.empty())
    {
        vkDestroyInstance(Instance, nullptr);
        Record(State, !NativeQueryRequired(),
            "Vulkan capability fixture device enumeration is unavailable");
        return;
    }

    FVulkanLabCapabilityQueryFacts Facts;
    Facts.NegotiatedApiVersion = VK_API_VERSION_1_0;
    Facts.bKHRGetPhysicalDeviceProperties2Enabled =
        bKHRProperties2Enabled;
    const bool bSwapchainAdvertised = HasDeviceExtension(
        Devices.front(), KHRSwapchainExtension);
    Facts.bSurfaceMaintenance1Enabled = bSurfaceMaintenance1Enabled;
    // The logical-device fixture requests KHR_swapchain only when the
    // selected device advertises it; Query therefore checks the real
    // selected-device advertisement and the actual enabled instance facts.
    Facts.bKHRSwapchainRequested = bSwapchainAdvertised && bSurfaceEnabled;
    const FVulkanLabPresentationCapabilityObservation Observation =
        FVulkanLabCapabilityQuery::Query(Instance, Devices.front(), Facts);

    VkPhysicalDeviceProperties Properties{};
    vkGetPhysicalDeviceProperties(Devices.front(), &Properties);
    const char* MechanismName = "Unavailable";
    switch (FVulkanLabCapabilityQuery::SelectQueryMechanism(Facts))
    {
    case EVulkanLabCapabilityQueryMechanism::Core11:
        MechanismName = "Core11";
        break;
    case EVulkanLabCapabilityQueryMechanism::KHRProperties2:
        MechanismName = "KHRProperties2";
        break;
    case EVulkanLabCapabilityQueryMechanism::Unavailable:
        break;
    }
    std::cout << "[INFO] Vulkan capability device=\"" << Properties.deviceName
        << "\" mechanism=" << MechanismName
        << " optionalInstanceDependenciesAllowed="
        << (bAllowOptionalInstanceExtensions ? "true" : "false")
        << " extensionAdvertised="
        << (Observation.bExtensionAdvertised ? "true" : "false")
        << " featureAdvertised="
        << (Observation.bFeatureAdvertised ? "true" : "false")
        << " queryState=" << static_cast<int>(Observation.QueryState)
        << '\n';

    Record(State,
        Observation.QueryState != EVulkanLabCapabilityQueryState::Failed,
        "selected Vulkan device capability query preserves native failures");
    if (Observation.QueryState == EVulkanLabCapabilityQueryState::Failed)
    {
        Record(State,
            Observation.NativeQueryResult != 0 &&
                !Observation.QueryFailure.IsEmpty(),
            "failed Vulkan capability query retains VkResult and diagnostic");
    }
    else if (Observation.QueryState == EVulkanLabCapabilityQueryState::Succeeded)
    {
        Record(State,
            Observation.bInstanceDependenciesAvailable ==
                (Facts.bKHRSwapchainRequested && bSurfaceMaintenance1Enabled &&
                    FVulkanLabCapabilityQuery::SelectQueryMechanism(Facts) !=
                        EVulkanLabCapabilityQueryMechanism::Unavailable),
            "native fixture reports actual presentation dependency negotiation");
        const FVulkanLabPresentationPolicyCandidate Candidate =
            FVulkanLabPresentationPolicy::SelectCandidate({}, Observation);
        Record(State,
            Candidate.IsValid() && !Candidate.bEntryPointsResolved &&
                Candidate.bRequiresOptionalEnablement ==
                    (Observation.bExtensionAdvertised &&
                        Observation.bFeatureAdvertised &&
                        Observation.bInstanceDependenciesAvailable),
            "native capability candidate preserves negotiation facts before device enablement");
    }
    else
    {
        Record(State,
            Observation.QueryState ==
                EVulkanLabCapabilityQueryState::Unavailable &&
                !Observation.bFeatureAdvertised &&
                !Observation.bEntryPointsResolved,
            "optional Vulkan feature query mechanism is explicit and unavailable");
    }
    if (Observation.QueryState != EVulkanLabCapabilityQueryState::Failed &&
        Facts.bKHRSwapchainRequested)
    {
        TestLogicalDeviceStartup(State, Devices.front(), Observation);
    }
    else
    {
        Record(State, true,
            "logical-device startup fixture is skipped without KHR_swapchain");
    }
    vkDestroyInstance(Instance, nullptr);
}

#else

void TestNativeSelectedDevice(FTestState& State)
{
    const FVulkanLabPresentationCapabilityObservation Observation =
        FVulkanLabCapabilityQuery::Query({});
    Record(State,
        Observation.QueryState == EVulkanLabCapabilityQueryState::Unavailable &&
            !Observation.bExtensionAdvertised &&
            !Observation.bFeatureAdvertised &&
            !NativeQueryRequired(),
        "Vulkan-disabled builds report the optional query as unavailable");
}

#endif

} // namespace

int RunVulkanLabCapabilityNativeTests()
{
    FTestState State;
    TestQueryMechanismSelection(State);
    TestNativeSelectedDevice(State);
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
    // A real ordinary device must remain usable when the optional instance
    // dependencies are deliberately omitted, even on an EXT-capable host.
    TestNativeSelectedDevice(State, false);
#endif
    return State.Failed;
}
