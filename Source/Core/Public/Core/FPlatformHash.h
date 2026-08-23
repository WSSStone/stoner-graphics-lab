#pragma once

#include "Core/FPlatformTypes.h"

#include <array>
#include <span>

namespace Stoner::Core
{

struct FPlatformHash
{
    [[nodiscard]] static bool TrySha256(
        std::span<const uint8> Bytes,
        std::array<uint8, 32>& OutDigest) noexcept;
};

} // namespace Stoner::Core
