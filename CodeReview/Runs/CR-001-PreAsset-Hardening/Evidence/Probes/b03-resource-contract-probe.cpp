#include "Tests/RHICoreTests.cpp"

#include <iostream>
#include <limits>

int main()
{
    FMockDevice Device;

    FRHIBufferDesc UnknownBufferUsage;
    UnknownBufferUsage.SizeInBytes = 64;
    UnknownBufferUsage.Usage = static_cast<ERHIBufferUsage>(1u << 31);
    const bool bUnknownBufferUsageAccepted =
        IsValidRHIBufferDesc(UnknownBufferUsage) &&
        Device.CreateBuffer(UnknownBufferUsage).Succeeded();

    FRHIBufferDesc UnknownMemoryAccess;
    UnknownMemoryAccess.SizeInBytes = 64;
    UnknownMemoryAccess.Usage = ERHIBufferUsage::Uniform;
    UnknownMemoryAccess.MemoryAccess = static_cast<ERHIMemoryAccess>(255);
    const bool bUnknownMemoryAccessAccepted =
        IsValidRHIBufferDesc(UnknownMemoryAccess) &&
        Device.CreateBuffer(UnknownMemoryAccess).Succeeded();

    FRHITextureDesc BaseTexture;
    BaseTexture.Width = 64;
    BaseTexture.Height = 64;
    BaseTexture.Format = ERHIFormat::R8G8B8A8_UNorm;
    BaseTexture.Usage = ERHITextureUsage::Sampled;

    FRHITextureDesc UnknownTextureUsage = BaseTexture;
    UnknownTextureUsage.Usage = static_cast<ERHITextureUsage>(1u << 31);
    const bool bUnknownTextureUsageAccepted =
        IsValidRHITextureDesc(UnknownTextureUsage) &&
        Device.CreateTexture(UnknownTextureUsage).Succeeded();

    FRHITextureDesc TextureVertexUsage = BaseTexture;
    TextureVertexUsage.Usage = ERHITextureUsage::Vertex;
    const bool bTextureVertexUsageAccepted =
        IsValidRHITextureDesc(TextureVertexUsage) &&
        Device.CreateTexture(TextureVertexUsage).Succeeded();

    FRHITextureDesc UnknownSampleCount = BaseTexture;
    UnknownSampleCount.SampleCount = static_cast<ERHISampleCount>(3);
    const bool bUnknownSampleCountAccepted =
        IsValidRHITextureDesc(UnknownSampleCount) &&
        Device.CreateTexture(UnknownSampleCount).Succeeded();

    FRHISamplerDesc UnknownSampler;
    UnknownSampler.MinFilter = static_cast<ERHISamplerFilter>(255);
    UnknownSampler.AddressW = static_cast<ERHISamplerAddressMode>(255);
    const bool bUnknownSamplerAccepted =
        IsValidRHISamplerDesc(UnknownSampler) &&
        Device.CreateSampler(UnknownSampler).Succeeded();

    FRHITextureDesc OnePixelOverMip = BaseTexture;
    OnePixelOverMip.Width = 1;
    OnePixelOverMip.Height = 1;
    OnePixelOverMip.MipLevels = 2;
    const bool bOnePixelOverMipAccepted =
        IsValidRHITextureDesc(OnePixelOverMip) &&
        Device.CreateTexture(OnePixelOverMip).Succeeded();

    FRHITextureDesc HugeMipCount = BaseTexture;
    HugeMipCount.MipLevels = std::numeric_limits<uint32>::max();
    const bool bHugeMipCountAccepted =
        IsValidRHITextureDesc(HugeMipCount) &&
        Device.CreateTexture(HugeMipCount).Succeeded();

    FRHITextureDesc MultisampledMipChain = BaseTexture;
    MultisampledMipChain.SampleCount = ERHISampleCount::Two;
    MultisampledMipChain.MipLevels = 2;
    const bool bMultisampledMipChainAccepted =
        IsValidRHITextureDesc(MultisampledMipChain) &&
        Device.CreateTexture(MultisampledMipChain).Succeeded();

    FRHITextureDesc ColorAsDepth = BaseTexture;
    ColorAsDepth.Usage = ERHITextureUsage::DepthStencilAttachment;
    const bool bColorAsDepthValidatorMismatch =
        IsValidRHITextureDesc(ColorAsDepth) &&
        Device.CreateTexture(ColorAsDepth).Result == ERHIResult::InvalidState;

    FRHITextureDesc DepthAsColor = BaseTexture;
    DepthAsColor.Format = ERHIFormat::D32_Float;
    DepthAsColor.Usage = ERHITextureUsage::ColorAttachment;
    const bool bDepthAsColorValidatorMismatch =
        IsValidRHITextureDesc(DepthAsColor) &&
        Device.CreateTexture(DepthAsColor).Result == ERHIResult::InvalidState;

    const bool bUndefinedDomainsAccepted =
        bUnknownBufferUsageAccepted &&
        bUnknownMemoryAccessAccepted &&
        bUnknownTextureUsageAccepted &&
        bTextureVertexUsageAccepted &&
        bUnknownSampleCountAccepted &&
        bUnknownSamplerAccepted;
    const bool bInvalidMipCountsAccepted =
        bOnePixelOverMipAccepted &&
        bHugeMipCountAccepted &&
        bMultisampledMipChainAccepted;
    const bool bFormatUsageValidationContradictsFactory =
        bColorAsDepthValidatorMismatch &&
        bDepthAsColorValidatorMismatch;

    std::cout << "undefined_domains_accepted=" << bUndefinedDomainsAccepted << '\n'
              << "invalid_mip_counts_accepted=" << bInvalidMipCountsAccepted << '\n'
              << "format_usage_validator_mismatch="
              << bFormatUsageValidationContradictsFactory << '\n'
              << "classification="
              << (bUndefinedDomainsAccepted &&
                          bInvalidMipCountsAccepted &&
                          bFormatUsageValidationContradictsFactory
                      ? "resource-contract-defects"
                      : "unexpected")
              << '\n';
    return bUndefinedDomainsAccepted &&
            bInvalidMipCountsAccepted &&
            bFormatUsageValidationContradictsFactory
        ? 0
        : 3;
}
