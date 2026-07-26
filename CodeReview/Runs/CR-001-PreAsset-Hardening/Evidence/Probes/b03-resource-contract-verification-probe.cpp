#include "Tests/RHICoreTests.cpp"

#include <array>
#include <cstring>
#include <iostream>
#include <limits>

namespace
{

FRHITextureDesc MakeProbeTexture()
{
    FRHITextureDesc Desc;
    Desc.Width = 64;
    Desc.Height = 64;
    Desc.Format = ERHIFormat::R8G8B8A8_UNorm;
    Desc.Usage = ERHITextureUsage::Sampled;
    return Desc;
}

bool BufferRejected(FMockDevice& Device, const FRHIBufferDesc& Desc)
{
    return !IsValidRHIBufferDesc(Desc) &&
        Device.CreateBuffer(Desc).Result == ERHIResult::InvalidState;
}

bool TextureRejected(FMockDevice& Device, const FRHITextureDesc& Desc)
{
    return !IsValidRHITextureDesc(Desc) &&
        Device.CreateTexture(Desc).Result == ERHIResult::InvalidState;
}

bool TextureAccepted(FMockDevice& Device, const FRHITextureDesc& Desc)
{
    return IsValidRHITextureDesc(Desc) && Device.CreateTexture(Desc).Succeeded();
}

bool SamplerRejected(FMockDevice& Device, const FRHISamplerDesc& Desc)
{
    return !IsValidRHISamplerDesc(Desc) &&
        Device.CreateSampler(Desc).Result == ERHIResult::Unsupported;
}

} // namespace

int main(int ArgCount, char** Args)
{
    if (ArgCount != 2 ||
        (std::strcmp(Args[1], "parent") != 0 &&
            std::strcmp(Args[1], "current") != 0))
    {
        return 2;
    }

    FMockDevice Device;
    int ClosedDomainRejections = 0;

    FRHIBufferDesc Buffer;
    Buffer.SizeInBytes = 64;
    Buffer.Usage = static_cast<ERHIBufferUsage>(1u << 31);
    ClosedDomainRejections += BufferRejected(Device, Buffer);
    Buffer.Usage = ERHIBufferUsage::Uniform;
    Buffer.MemoryAccess = static_cast<ERHIMemoryAccess>(255);
    ClosedDomainRejections += BufferRejected(Device, Buffer);

    FRHITextureDesc Texture = MakeProbeTexture();
    Texture.Usage = static_cast<ERHITextureUsage>(1u << 31);
    ClosedDomainRejections += TextureRejected(Device, Texture);
    Texture.Usage = ERHITextureUsage::Vertex;
    ClosedDomainRejections += TextureRejected(Device, Texture);
    Texture = MakeProbeTexture();
    Texture.SampleCount = static_cast<ERHISampleCount>(3);
    ClosedDomainRejections += TextureRejected(Device, Texture);

    std::array<FRHISamplerDesc, 7> InvalidSamplers{};
    InvalidSamplers[0].MinFilter = static_cast<ERHISamplerFilter>(255);
    InvalidSamplers[1].MagFilter = static_cast<ERHISamplerFilter>(255);
    InvalidSamplers[2].MipFilter = static_cast<ERHISamplerMipFilter>(255);
    InvalidSamplers[3].AddressU = static_cast<ERHISamplerAddressMode>(255);
    InvalidSamplers[4].AddressV = static_cast<ERHISamplerAddressMode>(255);
    InvalidSamplers[5].AddressW = static_cast<ERHISamplerAddressMode>(255);
    InvalidSamplers[6].CompareMode = static_cast<ERHISamplerCompareMode>(255);
    for (const FRHISamplerDesc& Sampler : InvalidSamplers)
    {
        ClosedDomainRejections += SamplerRejected(Device, Sampler);
    }

    FRHITextureDesc OnePixel = MakeProbeTexture();
    OnePixel.Width = 1;
    OnePixel.Height = 1;
    int ValidMipBoundaries = TextureAccepted(Device, OnePixel);
    OnePixel.MipLevels = 2;
    int InvalidMipRejections = TextureRejected(Device, OnePixel);

    FRHITextureDesc ExactChain = MakeProbeTexture();
    ExactChain.MipLevels = 7;
    ValidMipBoundaries += TextureAccepted(Device, ExactChain);
    ExactChain.MipLevels = 8;
    InvalidMipRejections += TextureRejected(Device, ExactChain);
    ExactChain.MipLevels = std::numeric_limits<uint32>::max();
    InvalidMipRejections += TextureRejected(Device, ExactChain);

    FRHITextureDesc Multisampled = MakeProbeTexture();
    Multisampled.SampleCount = ERHISampleCount::Two;
    ValidMipBoundaries += TextureAccepted(Device, Multisampled);
    Multisampled.MipLevels = 2;
    InvalidMipRejections += TextureRejected(Device, Multisampled);

    FRHITextureDesc ColorAsDepth = MakeProbeTexture();
    ColorAsDepth.Usage = ERHITextureUsage::DepthStencilAttachment;
    int FormatUsageRejections = TextureRejected(Device, ColorAsDepth);
    FRHITextureDesc DepthAsColor = MakeProbeTexture();
    DepthAsColor.Format = ERHIFormat::D32_Float;
    DepthAsColor.Usage = ERHITextureUsage::ColorAttachment;
    FormatUsageRejections += TextureRejected(Device, DepthAsColor);

    const bool bExpectParent = std::strcmp(Args[1], "parent") == 0;
    const bool bMatchesExpected =
        ValidMipBoundaries == 3 &&
        (bExpectParent
                ? ClosedDomainRejections == 0 &&
                    InvalidMipRejections == 0 &&
                    FormatUsageRejections == 0
                : ClosedDomainRejections == 12 &&
                    InvalidMipRejections == 4 &&
                    FormatUsageRejections == 2);

    std::cout << "closed_domain_rejections=" << ClosedDomainRejections << "/12\n"
              << "valid_mip_boundaries=" << ValidMipBoundaries << "/3\n"
              << "invalid_mip_rejections=" << InvalidMipRejections << "/4\n"
              << "format_usage_rejections=" << FormatUsageRejections << "/2\n"
              << "classification="
              << (bMatchesExpected
                      ? (bExpectParent ? "parent-defects" : "current-fixed")
                      : "unexpected")
              << '\n';
    return bMatchesExpected ? 0 : 3;
}
