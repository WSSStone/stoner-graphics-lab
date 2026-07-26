#include "Tests/RHICoreTests.cpp"

#include <iostream>
#include <limits>

int main()
{
    FMockDevice Device;

    FRHIBufferDesc Buffer;
    Buffer.SizeInBytes = 64;
    Buffer.Usage = static_cast<ERHIBufferUsage>(1u << 31);
    const bool bUnknownBufferUsageRejected =
        !IsValidRHIBufferDesc(Buffer) &&
        Device.CreateBuffer(Buffer).Result == ERHIResult::InvalidState;

    Buffer.Usage = ERHIBufferUsage::Uniform;
    Buffer.MemoryAccess = static_cast<ERHIMemoryAccess>(255);
    const bool bUnknownMemoryAccessRejected =
        !IsValidRHIBufferDesc(Buffer) &&
        Device.CreateBuffer(Buffer).Result == ERHIResult::InvalidState;

    FRHITextureDesc Texture;
    Texture.Width = 64;
    Texture.Height = 64;
    Texture.Format = ERHIFormat::R8G8B8A8_UNorm;
    Texture.Usage = static_cast<ERHITextureUsage>(1u << 31);
    const bool bUnknownTextureUsageRejected =
        !IsValidRHITextureDesc(Texture) &&
        Device.CreateTexture(Texture).Result == ERHIResult::InvalidState;

    Texture.Usage = ERHITextureUsage::Vertex;
    const bool bVertexTextureUsageRejected =
        !IsValidRHITextureDesc(Texture) &&
        Device.CreateTexture(Texture).Result == ERHIResult::InvalidState;

    Texture.Usage = ERHITextureUsage::Sampled;
    Texture.SampleCount = static_cast<ERHISampleCount>(3);
    const bool bUnknownSampleCountRejected =
        !IsValidRHITextureDesc(Texture) &&
        Device.CreateTexture(Texture).Result == ERHIResult::InvalidState;

    FRHISamplerDesc Sampler;
    Sampler.MinFilter = static_cast<ERHISamplerFilter>(255);
    const bool bUnknownFilterRejected =
        !IsValidRHISamplerDesc(Sampler) &&
        Device.CreateSampler(Sampler).Result == ERHIResult::Unsupported;

    Sampler = {};
    Sampler.MipFilter = static_cast<ERHISamplerMipFilter>(255);
    const bool bUnknownMipFilterRejected =
        !IsValidRHISamplerDesc(Sampler) &&
        Device.CreateSampler(Sampler).Result == ERHIResult::Unsupported;

    Sampler = {};
    Sampler.AddressW = static_cast<ERHISamplerAddressMode>(255);
    const bool bUnknownAddressRejected =
        !IsValidRHISamplerDesc(Sampler) &&
        Device.CreateSampler(Sampler).Result == ERHIResult::Unsupported;

    Sampler = {};
    Sampler.CompareMode = static_cast<ERHISamplerCompareMode>(255);
    const bool bUnknownCompareRejected =
        !IsValidRHISamplerDesc(Sampler) &&
        Device.CreateSampler(Sampler).Result == ERHIResult::Unsupported;

    const bool bClosedDomains =
        bUnknownBufferUsageRejected &&
        bUnknownMemoryAccessRejected &&
        bUnknownTextureUsageRejected &&
        bVertexTextureUsageRejected &&
        bUnknownSampleCountRejected &&
        bUnknownFilterRejected &&
        bUnknownMipFilterRejected &&
        bUnknownAddressRejected &&
        bUnknownCompareRejected;

    FRHITextureDesc OnePixel = Texture;
    OnePixel.SampleCount = ERHISampleCount::One;
    OnePixel.Width = 1;
    OnePixel.Height = 1;
    OnePixel.MipLevels = 1;
    const bool bOnePixelBoundaryAccepted =
        IsValidRHITextureDesc(OnePixel) &&
        Device.CreateTexture(OnePixel).Succeeded();
    OnePixel.MipLevels = 2;
    const bool bOnePixelOverLimitRejected =
        !IsValidRHITextureDesc(OnePixel) &&
        Device.CreateTexture(OnePixel).Result == ERHIResult::InvalidState;

    FRHITextureDesc ExactChain = Texture;
    ExactChain.SampleCount = ERHISampleCount::One;
    ExactChain.MipLevels = 7;
    const bool bExactChainAccepted =
        IsValidRHITextureDesc(ExactChain) &&
        Device.CreateTexture(ExactChain).Succeeded();
    ExactChain.MipLevels = 8;
    const bool bFirstOverLimitRejected =
        !IsValidRHITextureDesc(ExactChain) &&
        Device.CreateTexture(ExactChain).Result == ERHIResult::InvalidState;
    ExactChain.MipLevels = std::numeric_limits<uint32>::max();
    const bool bUnboundedChainRejected =
        !IsValidRHITextureDesc(ExactChain) &&
        Device.CreateTexture(ExactChain).Result == ERHIResult::InvalidState;

    FRHITextureDesc Multisampled = Texture;
    Multisampled.SampleCount = ERHISampleCount::Two;
    Multisampled.MipLevels = 1;
    const bool bSingleMultisampleMipAccepted =
        IsValidRHITextureDesc(Multisampled) &&
        Device.CreateTexture(Multisampled).Succeeded();
    Multisampled.MipLevels = 2;
    const bool bMultisampleChainRejected =
        !IsValidRHITextureDesc(Multisampled) &&
        Device.CreateTexture(Multisampled).Result == ERHIResult::InvalidState;

    const bool bExactMipRules =
        bOnePixelBoundaryAccepted &&
        bOnePixelOverLimitRejected &&
        bExactChainAccepted &&
        bFirstOverLimitRejected &&
        bUnboundedChainRejected &&
        bSingleMultisampleMipAccepted &&
        bMultisampleChainRejected;

    FRHITextureDesc ColorAsDepth = Texture;
    ColorAsDepth.SampleCount = ERHISampleCount::One;
    ColorAsDepth.Usage = ERHITextureUsage::DepthStencilAttachment;
    const bool bColorAsDepthRejected =
        !IsValidRHITextureDesc(ColorAsDepth) &&
        Device.CreateTexture(ColorAsDepth).Result == ERHIResult::InvalidState;

    FRHITextureDesc DepthAsColor = Texture;
    DepthAsColor.SampleCount = ERHISampleCount::One;
    DepthAsColor.Format = ERHIFormat::D32_Float;
    DepthAsColor.Usage = ERHITextureUsage::ColorAttachment;
    const bool bDepthAsColorRejected =
        !IsValidRHITextureDesc(DepthAsColor) &&
        Device.CreateTexture(DepthAsColor).Result == ERHIResult::InvalidState;

    const bool bSharedFormatUsageRules =
        bColorAsDepthRejected &&
        bDepthAsColorRejected;
    const bool bFixed =
        bClosedDomains &&
        bExactMipRules &&
        bSharedFormatUsageRules;

    std::cout << "closed_domains=" << bClosedDomains << '\n'
              << "exact_mip_rules=" << bExactMipRules << '\n'
              << "shared_format_usage_rules=" << bSharedFormatUsageRules << '\n'
              << "classification=" << (bFixed ? "fixed" : "unexpected") << '\n';
    return bFixed ? 0 : 3;
}
