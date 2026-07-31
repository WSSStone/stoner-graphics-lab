#pragma once

#include "Core/CoreMinimal.h"

#include <array>

namespace Stoner::Renderer
{

// GLSL default-layout mat4 values are column-major. CPU matrices remain
// row-major, so shader-bound bytes must be emitted in column-major order.
struct alignas(16) FShaderMatrix4x4
{
    std::array<float, 16> Elements{};
};

static_assert(sizeof(FShaderMatrix4x4) == sizeof(float) * 16);
static_assert(alignof(FShaderMatrix4x4) == 16);

[[nodiscard]] FShaderMatrix4x4 PackRowMajorMatrixForShader(
    const Stoner::Core::FMatrix4x4& Matrix) noexcept;

} // namespace Stoner::Renderer
