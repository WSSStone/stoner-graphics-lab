#include "FMetalFormat.h"

#import <Metal/Metal.h>

namespace Stoner::Backend::Metal::Private
{

Core::uint64 ToMetalPixelFormat(RHI::ERHIFormat Format) noexcept
{
    using RHI::ERHIFormat;
    switch (Format)
    {
    case ERHIFormat::R8_UNorm: return MTLPixelFormatR8Unorm;
    case ERHIFormat::R8G8_UNorm: return MTLPixelFormatRG8Unorm;
    case ERHIFormat::R8G8B8A8_UNorm: return MTLPixelFormatRGBA8Unorm;
    case ERHIFormat::R8G8B8A8_sRGB: return MTLPixelFormatRGBA8Unorm_sRGB;
    case ERHIFormat::B8G8R8A8_UNorm: return MTLPixelFormatBGRA8Unorm;
    // CAMetalLayer's documented packed-10 PQ presentation pair uses BGR10A2.
    // Shader component semantics remain RGBA; this is the native byte layout.
    case ERHIFormat::R10G10B10A2_UNorm: return MTLPixelFormatBGR10A2Unorm;
    case ERHIFormat::R16G16B16A16_Float: return MTLPixelFormatRGBA16Float;
    case ERHIFormat::R32_Float: return MTLPixelFormatR32Float;
    case ERHIFormat::R32G32_Float: return MTLPixelFormatRG32Float;
    case ERHIFormat::R32G32B32A32_Float: return MTLPixelFormatRGBA32Float;
    case ERHIFormat::BC1_RGBA_UNorm: return MTLPixelFormatBC1_RGBA;
    case ERHIFormat::BC1_RGBA_sRGB: return MTLPixelFormatBC1_RGBA_sRGB;
    case ERHIFormat::BC3_RGBA_UNorm: return MTLPixelFormatBC3_RGBA;
    case ERHIFormat::BC3_RGBA_sRGB: return MTLPixelFormatBC3_RGBA_sRGB;
    case ERHIFormat::BC4_R_UNorm: return MTLPixelFormatBC4_RUnorm;
    case ERHIFormat::BC5_RG_UNorm: return MTLPixelFormatBC5_RGUnorm;
    case ERHIFormat::BC7_RGBA_UNorm: return MTLPixelFormatBC7_RGBAUnorm;
    case ERHIFormat::BC7_RGBA_sRGB: return MTLPixelFormatBC7_RGBAUnorm_sRGB;
    case ERHIFormat::EAC_R11_UNorm: return MTLPixelFormatEAC_R11Unorm;
    case ERHIFormat::EAC_RG11_UNorm: return MTLPixelFormatEAC_RG11Unorm;
    case ERHIFormat::ASTC_4x4_RGBA_UNorm: return MTLPixelFormatASTC_4x4_LDR;
    case ERHIFormat::ASTC_4x4_RGBA_sRGB: return MTLPixelFormatASTC_4x4_sRGB;
    case ERHIFormat::D24_UNorm_S8_UInt:
        return MTLPixelFormatDepth24Unorm_Stencil8;
    case ERHIFormat::D32_Float: return MTLPixelFormatDepth32Float;
    case ERHIFormat::S8_UInt: return MTLPixelFormatStencil8;
    case ERHIFormat::R32G32B32_Float:
    case ERHIFormat::ETC2_RGB8_UNorm:
    case ERHIFormat::ETC2_RGB8_sRGB:
    case ERHIFormat::ETC2_RGBA8_UNorm:
    case ERHIFormat::ETC2_RGBA8_sRGB:
    case ERHIFormat::Unknown:
    case ERHIFormat::Count:
        return MTLPixelFormatInvalid;
    }
    return MTLPixelFormatInvalid;
}

bool IsMetalFormatSupported(
    void* NativeDevice,
    RHI::ERHIFormat Format) noexcept
{
    if (NativeDevice == nullptr ||
        ToMetalPixelFormat(Format) == MTLPixelFormatInvalid)
        return false;
    id<MTLDevice> Device = (__bridge id<MTLDevice>)NativeDevice;
    if (Format == RHI::ERHIFormat::D24_UNorm_S8_UInt)
        return Device.depth24Stencil8PixelFormatSupported;
    const bool bAppleFamily = [Device supportsFamily:MTLGPUFamilyApple1];
    const bool bMacFamily = [Device supportsFamily:MTLGPUFamilyMac1];
    if (Format >= RHI::ERHIFormat::BC1_RGBA_UNorm &&
        Format <= RHI::ERHIFormat::BC7_RGBA_sRGB)
        return bMacFamily;
    if (Format == RHI::ERHIFormat::EAC_R11_UNorm ||
        Format == RHI::ERHIFormat::EAC_RG11_UNorm ||
        Format == RHI::ERHIFormat::ASTC_4x4_RGBA_UNorm ||
        Format == RHI::ERHIFormat::ASTC_4x4_RGBA_sRGB)
        return bAppleFamily;
    return true;
}

} // namespace Stoner::Backend::Metal::Private
