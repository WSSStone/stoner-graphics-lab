#include "VulkanRHI/FVulkanDevice.h"
#include "VulkanRHI/FVulkanPhysicalDevice.h"

#include <iostream>
#include <string>

using namespace Stoner::Backend::Vulkan;
using namespace Stoner::Core;
using namespace Stoner::RHI;

namespace
{

FVulkanAdapterCandidate MakeCandidate(
    const char* Name,
    FVulkanFormatSupport Formats = {true, true})
{
    return {
        Name,
        EVulkanPhysicalDeviceType::Discrete,
        true,
        {true, true, true, false},
        false,
        std::move(Formats),
        0,
        "",
    };
}

FVulkanAdapterSelection SelectFromTemporaryStorage()
{
    std::string TemporaryName = "TemporaryOwnedIdentity";
    return SelectBestAdapter({MakeCandidate(TemporaryName.c_str())});
}

FRHITextureDesc MakeTextureDesc(ERHIFormat Format, ERHITextureUsage Usage)
{
    FRHITextureDesc Desc;
    Desc.Width = 4;
    Desc.Height = 4;
    Desc.Format = Format;
    Desc.Usage = Usage;
    return Desc;
}

} // namespace

int main()
{
    const auto RealFactory = CreateVulkanDevice();
    const bool bDefaultFactoryTruthful =
        RealFactory.Result == ERHIResult::Unsupported && !RealFactory.Object;

    FVulkanInstanceDesc FallbackDesc;
    FallbackDesc.RuntimeMode = EVulkanInstanceRuntimeMode::DeterministicFallback;
    const auto FallbackFactory = CreateVulkanDevice(FallbackDesc);
    const auto FallbackDevice =
        std::dynamic_pointer_cast<FVulkanDevice>(FallbackFactory.Object);
    const bool bFallbackFactoryExplicit =
        FallbackFactory.Succeeded() && FallbackDevice &&
        FallbackDevice->GetDiagnostics().Availability ==
            EVulkanBackendAvailability::DeterministicFallback &&
        FallbackDevice->GetDiagnostics().bUsedRuntimeFallback &&
        FallbackDevice->GetDiagnostics().RuntimeModeReason[0] != '\0';

    const FVulkanAdapterSelection OwnedSelection = SelectFromTemporaryStorage();
    const bool bIdentityLifetimeStable = OwnedSelection.bSucceeded &&
        OwnedSelection.Selected.Name.View() == "TemporaryOwnedIdentity";

    const FVulkanAdapterSelection EmptySelection = SelectBestAdapter({
        MakeCandidate(nullptr),
        MakeCandidate("ValidIdentity"),
    });
    const bool bEmptyIdentitySafe = EmptySelection.bSucceeded &&
        EmptySelection.Selected.Name.View() == "ValidIdentity" &&
        EmptySelection.Candidates[0].Score < 0 &&
        EmptySelection.Candidates[0].RejectionReason.View() ==
            "missing adapter identity";

    FVulkanFormatSupport ExactFormats(TArray<ERHIFormat>{
        ERHIFormat::Unknown,
        ERHIFormat::R8G8B8A8_UNorm,
        ERHIFormat::R8G8B8A8_UNorm,
    });
    const bool bFormatSetNormalized =
        ExactFormats.GetSupportedFormats().size() == 1 &&
        ExactFormats.SupportsFormat(ERHIFormat::R8G8B8A8_UNorm) &&
        !ExactFormats.SupportsFormat(ERHIFormat::Unknown) &&
        !ExactFormats.SupportsFormat(ERHIFormat::D32_Float);

    FVulkanInstanceDesc ExactDesc = FallbackDesc;
    ExactDesc.SyntheticCandidates = {
        MakeCandidate("ExactFormats", std::move(ExactFormats)),
    };
    FVulkanDevice ExactDevice;
    const bool bExactInitialized =
        ExactDevice.Initialize(ExactDesc) == ERHIResult::Success;
    const auto AllowedTexture = ExactDevice.CreateTexture(MakeTextureDesc(
        ERHIFormat::R8G8B8A8_UNorm,
        ERHITextureUsage::ColorAttachment));
    const auto OtherColorTexture = ExactDevice.CreateTexture(MakeTextureDesc(
        ERHIFormat::B8G8R8A8_UNorm,
        ERHITextureUsage::ColorAttachment));
    const auto DepthTexture = ExactDevice.CreateTexture(MakeTextureDesc(
        ERHIFormat::D32_Float,
        ERHITextureUsage::DepthStencilAttachment));
    const bool bFormatFactoryExact = bExactInitialized &&
        ExactDevice.GetCapabilities().SupportedFormats.size() == 1 &&
        AllowedTexture.Succeeded() &&
        OtherColorTexture.Result == ERHIResult::Unsupported &&
        DepthTexture.Result == ERHIResult::Unsupported;

    bool bTieBreakStable = true;
    for (int Index = 0; Index < 20; ++Index)
    {
        const FVulkanAdapterSelection TieSelection = SelectBestAdapter({
            MakeCandidate("Zulu"),
            MakeCandidate("Alpha"),
        });
        bTieBreakStable = bTieBreakStable && TieSelection.bSucceeded &&
            TieSelection.Selected.Name.View() == "Alpha";
    }

    std::cout
        << "default_factory_truthful=" << bDefaultFactoryTruthful << '\n'
        << "fallback_factory_explicit=" << bFallbackFactoryExplicit << '\n'
        << "identity_lifetime_stable=" << bIdentityLifetimeStable << '\n'
        << "empty_identity_safe=" << bEmptyIdentitySafe << '\n'
        << "format_set_normalized=" << bFormatSetNormalized << '\n'
        << "format_factory_exact=" << bFormatFactoryExact << '\n'
        << "tie_break_stable=" << bTieBreakStable << '\n';

    if (FallbackDevice && FallbackDevice->IsActive())
    {
        (void)FallbackDevice->Shutdown();
    }
    if (ExactDevice.IsActive())
    {
        (void)ExactDevice.Shutdown();
    }

    const bool bPassed = bDefaultFactoryTruthful && bFallbackFactoryExplicit &&
        bIdentityLifetimeStable && bEmptyIdentitySafe &&
        bFormatSetNormalized && bFormatFactoryExact && bTieBreakStable;
    std::cout << "classification="
              << (bPassed ? "instance-device-contracts-verified" : "unexpected")
              << '\n';
    return bPassed ? 0 : 3;
}
