#pragma once

#include "RHI/ERHIPipelineState.h"

namespace Stoner::Backend::Vulkan::Private
{

// RHI winding is expressed in the engine's canonical screen convention.
// Vulkan production command paths use a positive-height viewport, whose
// framebuffer Y convention reverses the visible winding. Adapt that parity at
// the backend boundary exactly once; transform determinant parity has already
// been resolved by the caller.
[[nodiscard]] constexpr RHI::ERHIFrontFace ResolveVulkanFrontFace(
    RHI::ERHIFrontFace Declared) noexcept
{
    return RHI::ResolveRHIFrontFaceForTransform(Declared, true);
}

} // namespace Stoner::Backend::Vulkan::Private
