#pragma once

#include "Core/FPlatformTypes.h"
#include "Core/TArray.h"

#include <istream>

namespace Stoner::Core::Detail
{

[[nodiscard]] bool ReadExactBytes(
    std::istream& Stream,
    usize ByteCount,
    TArray<uint8>& OutData) noexcept;

} // namespace Stoner::Core::Detail
