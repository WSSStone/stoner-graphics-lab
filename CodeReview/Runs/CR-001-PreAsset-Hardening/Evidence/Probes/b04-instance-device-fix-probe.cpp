#include "VulkanRHI/FVulkanDevice.h"
#include "VulkanRHI/FVulkanPhysicalDevice.h"

#include <iostream>
#include <string>

using namespace Stoner::Backend::Vulkan;
using namespace Stoner::RHI;

namespace
{

FVulkanAdapterCandidate MakeCandidate(const char* Name, bool bDepthSupported = true)
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

} // namespace

int main()
{
    FVulkanDevice RealDevice;
    const bool bRealDefaultRejected =
        RealDevice.Initialize() == ERHIResult::Unsupported &&
        !RealDevice.IsActive() &&
        !RealDevice.GetDiagnostics().bUsedRuntimeFallback &&
        RealDevice.GetDiagnostics().Availability ==
            EVulkanBackendAvailability::UnsupportedRuntime;

    FVulkanInstanceDesc FallbackDesc;
    FallbackDesc.RuntimeMode = EVulkanInstanceRuntimeMode::DeterministicFallback;
    FVulkanDevice FallbackDevice;
    const bool bFallbackExplicit =
        FallbackDevice.Initialize(FallbackDesc) == ERHIResult::Success &&
        FallbackDevice.IsActive() &&
        FallbackDevice.GetDiagnostics().bUsedRuntimeFallback &&
        FallbackDevice.GetDiagnostics().Availability ==
            EVulkanBackendAvailability::DeterministicFallback;

    std::string MutableName = "CallerOwned";
    const FVulkanAdapterSelection OwnedSelection =
        SelectBestAdapter({MakeCandidate(MutableName.c_str())});
    MutableName[0] = 'X';
    const bool bIdentityOwned = OwnedSelection.bSucceeded &&
        OwnedSelection.Selected.Name.View() == "CallerOwned";

    const FVulkanAdapterSelection NullSelection =
        SelectBestAdapter({MakeCandidate(nullptr), MakeCandidate("Named")});
    const bool bNullIdentityRejected = NullSelection.bSucceeded &&
        NullSelection.Selected.Name.View() == "Named" &&
        NullSelection.Candidates[0].Score < 0 &&
        !NullSelection.Candidates[0].RejectionReason.IsEmpty();

    FVulkanInstanceDesc ColorOnlyDesc = FallbackDesc;
    ColorOnlyDesc.SyntheticCandidates = {MakeCandidate("ColorOnly", false)};
    FVulkanDevice ColorOnlyDevice;
    FRHITextureDesc DepthTexture;
    DepthTexture.Width = 4;
    DepthTexture.Height = 4;
    DepthTexture.Format = ERHIFormat::D32_Float;
    DepthTexture.Usage = ERHITextureUsage::DepthStencilAttachment;
    const bool bFormatSetPreserved =
        ColorOnlyDevice.Initialize(ColorOnlyDesc) == ERHIResult::Success &&
        !ColorOnlyDevice.GetCapabilities().SupportsFormat(ERHIFormat::D32_Float) &&
        ColorOnlyDevice.CreateTexture(DepthTexture).Result == ERHIResult::Unsupported;

    std::cout
        << "real_default_rejected=" << bRealDefaultRejected << '\n'
        << "fallback_explicit=" << bFallbackExplicit << '\n'
        << "identity_owned=" << bIdentityOwned << '\n'
        << "null_identity_rejected=" << bNullIdentityRejected << '\n'
        << "format_set_preserved=" << bFormatSetPreserved << '\n';

    if (FallbackDevice.IsActive())
    {
        (void)FallbackDevice.Shutdown();
    }
    if (ColorOnlyDevice.IsActive())
    {
        (void)ColorOnlyDevice.Shutdown();
    }

    const bool bPassed = bRealDefaultRejected && bFallbackExplicit &&
        bIdentityOwned && bNullIdentityRejected && bFormatSetPreserved;
    std::cout << "classification="
              << (bPassed ? "instance-device-contracts-fixed" : "unexpected")
              << '\n';
    return bPassed ? 0 : 3;
}
