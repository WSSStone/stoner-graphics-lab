#include "Renderer/FShaderMatrixPacking.h"

namespace Stoner::Renderer
{

FShaderMatrix4x4 PackRowMajorMatrixForShader(
    const Stoner::Core::FMatrix4x4& Matrix) noexcept
{
    FShaderMatrix4x4 Packed;
    for (int Column = 0; Column < 4; ++Column)
    {
        for (int Row = 0; Row < 4; ++Row)
        {
            Packed.Elements[static_cast<std::size_t>(Column * 4 + Row)] =
                Matrix.M[Row][Column];
        }
    }
    return Packed;
}

} // namespace Stoner::Renderer
