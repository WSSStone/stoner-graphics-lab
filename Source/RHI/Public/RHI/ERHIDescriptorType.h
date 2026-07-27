#pragma once

namespace Stoner::RHI
{

enum class ERHIDescriptorType
{
    UniformBuffer,
    StorageBuffer,
    SampledTexture,
    StorageTexture,
    Sampler,
    CombinedTextureSampler
};

[[nodiscard]] constexpr bool IsValidRHIDescriptorType(ERHIDescriptorType Type) noexcept
{
    switch (Type)
    {
    case ERHIDescriptorType::UniformBuffer:
    case ERHIDescriptorType::StorageBuffer:
    case ERHIDescriptorType::SampledTexture:
    case ERHIDescriptorType::StorageTexture:
    case ERHIDescriptorType::Sampler:
    case ERHIDescriptorType::CombinedTextureSampler:
        return true;
    }
    return false;
}

} // namespace Stoner::RHI
