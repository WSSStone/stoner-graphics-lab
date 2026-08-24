#pragma once

#include "RHI/ERHIPipelineState.h"

namespace Stoner::Backend::Metal::Private
{

// RHI winding is expressed in the engine's canonical screen convention.
// Authoritative MSL vertex payloads use SPIRV-Cross flip_vert_y, so the native
// Metal rasterizer observes the opposite winding. Adapt that parity at the
// backend boundary exactly once; transform determinant parity has already been
// resolved by the caller.
[[nodiscard]] constexpr RHI::ERHIFrontFace ResolveMetalFrontFace(
    RHI::ERHIFrontFace Declared) noexcept
{
    return RHI::ResolveRHIFrontFaceForTransform(Declared, true);
}

} // namespace Stoner::Backend::Metal::Private
