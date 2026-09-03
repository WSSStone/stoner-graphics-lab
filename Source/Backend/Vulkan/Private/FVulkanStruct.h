#pragma once

#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE

#include "RHI/ERHIFormat.h"
#include "RHI/ERHIPresentationColorSpace.h"

#include <vulkan/vulkan.h>

namespace Stoner::Backend::Vulkan
{

template <typename T>
T MakeVulkanStruct(VkStructureType Type) noexcept
{
    T Value{};
    Value.sType = Type;
    return Value;
}

[[nodiscard]] constexpr VkFormat ToVulkanPresentationFormat(
    Stoner::RHI::ERHIFormat Format) noexcept
{
    switch (Format)
    {
    case Stoner::RHI::ERHIFormat::B8G8R8A8_UNorm:
        return VK_FORMAT_B8G8R8A8_UNORM;
    case Stoner::RHI::ERHIFormat::R8G8B8A8_UNorm:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case Stoner::RHI::ERHIFormat::R8G8B8A8_sRGB:
        return VK_FORMAT_R8G8B8A8_SRGB;
    case Stoner::RHI::ERHIFormat::R10G10B10A2_UNorm:
        return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case Stoner::RHI::ERHIFormat::R16G16B16A16_Float:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    default:
        return VK_FORMAT_UNDEFINED;
    }
}

[[nodiscard]] constexpr Stoner::RHI::ERHIFormat
FromVulkanPresentationFormat(VkFormat Format) noexcept
{
    switch (Format)
    {
    case VK_FORMAT_B8G8R8A8_UNORM:
        return Stoner::RHI::ERHIFormat::B8G8R8A8_UNorm;
    case VK_FORMAT_R8G8B8A8_UNORM:
        return Stoner::RHI::ERHIFormat::R8G8B8A8_UNorm;
    case VK_FORMAT_R8G8B8A8_SRGB:
        return Stoner::RHI::ERHIFormat::R8G8B8A8_sRGB;
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
        return Stoner::RHI::ERHIFormat::R10G10B10A2_UNorm;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        return Stoner::RHI::ERHIFormat::R16G16B16A16_Float;
    default:
        return Stoner::RHI::ERHIFormat::Unknown;
    }
}

[[nodiscard]] constexpr VkColorSpaceKHR ToVulkanPresentationColorSpace(
    Stoner::RHI::ERHIPresentationColorSpace ColorSpace) noexcept
{
    switch (ColorSpace)
    {
    case Stoner::RHI::ERHIPresentationColorSpace::SrgbNonlinear:
    case Stoner::RHI::ERHIPresentationColorSpace::Bt709Nonlinear:
    case Stoner::RHI::ERHIPresentationColorSpace::SdrPassThrough:
        return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    case Stoner::RHI::ERHIPresentationColorSpace::Hdr10St2084:
        return VK_COLOR_SPACE_HDR10_ST2084_EXT;
    case Stoner::RHI::ERHIPresentationColorSpace::ExtendedSrgbLinear:
        return VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
    case Stoner::RHI::ERHIPresentationColorSpace::Unknown:
        return VK_COLOR_SPACE_MAX_ENUM_KHR;
    }
    return VK_COLOR_SPACE_MAX_ENUM_KHR;
}

[[nodiscard]] constexpr Stoner::RHI::ERHIPresentationColorSpace
FromVulkanPresentationColorSpace(VkColorSpaceKHR ColorSpace) noexcept
{
    switch (ColorSpace)
    {
    case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
        return Stoner::RHI::ERHIPresentationColorSpace::SrgbNonlinear;
    case VK_COLOR_SPACE_HDR10_ST2084_EXT:
        return Stoner::RHI::ERHIPresentationColorSpace::Hdr10St2084;
    case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
        return Stoner::RHI::ERHIPresentationColorSpace::ExtendedSrgbLinear;
    default:
        return Stoner::RHI::ERHIPresentationColorSpace::Unknown;
    }
}

} // namespace Stoner::Backend::Vulkan

#endif
