#include "VulkanRHI/FVulkanDevice.h"
#include "VulkanRHI/FVulkanPhysicalDevice.h"

#include <cstring>
#include <iostream>
#include <string>

using namespace Stoner::Backend::Vulkan;
using namespace Stoner::RHI;

namespace
{

FVulkanAdapterCandidate MakeCandidate(
    const char* Name,
    bool bDepthSupported = true)
{
    return {
        Name,
        EVulkanPhysicalDeviceType::Discrete,
        true,
        {true, true, true, false},
        false,
        {true, bDepthSupported},
        0,
        "",
    };
}

int RunSummary()
{
    FVulkanDevice DefaultDevice;
    const ERHIResult DefaultResult = DefaultDevice.Initialize();
    const FVulkanDiagnostics& DefaultDiagnostics =
        DefaultDevice.GetDiagnostics();
    const bool bFallbackMasqueradesAsAvailable =
        DefaultResult == ERHIResult::Success &&
        DefaultDevice.IsActive() &&
        DefaultDiagnostics.bUsedRuntimeFallback &&
        DefaultDiagnostics.Availability ==
            EVulkanBackendAvailability::Available &&
        std::strcmp(
            DefaultDevice.GetSelectedAdapter().Name,
            "Stoner Vulkan Compatible Adapter") == 0;

    std::string MutableName = "Owned-By-Caller";
    FVulkanAdapterSelection Selection =
        SelectBestAdapter({MakeCandidate(MutableName.c_str())});
    MutableName[0] = 'X';
    const bool bSelectedIdentityAliasesCallerStorage =
        Selection.bSucceeded &&
        Selection.Selected.Name == MutableName.c_str() &&
        Selection.Selected.Name[0] == 'X';

    FVulkanInstanceDesc LimitedDesc;
    LimitedDesc.SyntheticCandidates = {
        MakeCandidate("No-Depth-Adapter", false)};
    FVulkanDevice LimitedDevice;
    const ERHIResult LimitedResult = LimitedDevice.Initialize(LimitedDesc);

    FRHITextureDesc DepthTexture;
    DepthTexture.Width = 4;
    DepthTexture.Height = 4;
    DepthTexture.Format = ERHIFormat::D32_Float;
    DepthTexture.Usage = ERHITextureUsage::DepthStencilAttachment;
    const bool bDepthCapabilityOverclaimed =
        LimitedResult == ERHIResult::Success &&
        !LimitedDevice.GetSelectedAdapter().Formats.bDepth &&
        LimitedDevice.GetCapabilities().SupportsFormat(
            ERHIFormat::D32_Float) &&
        LimitedDevice.CreateTexture(DepthTexture).Succeeded();

    FVulkanDevice UninitializedDevice;
    const bool bUninitializedShutdownReportsSuccess =
        UninitializedDevice.Shutdown() == ERHIResult::Success &&
        UninitializedDevice.GetState() == ERHIDeviceState::Shutdown;

    std::cout
        << "fallback_masquerades_as_available="
        << bFallbackMasqueradesAsAvailable << '\n'
        << "selected_identity_aliases_caller_storage="
        << bSelectedIdentityAliasesCallerStorage << '\n'
        << "depth_capability_overclaimed="
        << bDepthCapabilityOverclaimed << '\n'
        << "uninitialized_shutdown_reports_success="
        << bUninitializedShutdownReportsSuccess << '\n'
        << "classification="
        << (bFallbackMasqueradesAsAvailable &&
                    bSelectedIdentityAliasesCallerStorage &&
                    bDepthCapabilityOverclaimed &&
                    bUninitializedShutdownReportsSuccess
                ? "instance-device-contract-defects"
                : "unexpected")
        << '\n';

    if (DefaultDevice.IsActive())
    {
        (void)DefaultDevice.Shutdown();
    }
    if (LimitedDevice.IsActive())
    {
        (void)LimitedDevice.Shutdown();
    }
    return bFallbackMasqueradesAsAvailable &&
            bSelectedIdentityAliasesCallerStorage &&
            bDepthCapabilityOverclaimed &&
            bUninitializedShutdownReportsSuccess
        ? 0
        : 3;
}

int RunNullName()
{
    FVulkanAdapterCandidate NullName = MakeCandidate(nullptr);
    FVulkanAdapterCandidate Named = MakeCandidate("Named");
    const FVulkanAdapterSelection Selection =
        SelectBestAdapter({NullName, Named});
    return Selection.bSucceeded ? 0 : 3;
}

} // namespace

int main(int ArgCount, char** Args)
{
    if (ArgCount != 2)
    {
        return 2;
    }
    if (std::strcmp(Args[1], "summary") == 0)
    {
        return RunSummary();
    }
    if (std::strcmp(Args[1], "null-name") == 0)
    {
        return RunNullName();
    }
    return 2;
}
