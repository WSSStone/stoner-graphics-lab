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

} // namespace Stoner::RHI
