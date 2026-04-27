#pragma once

#include <cstddef>
#include <cstdint>

namespace Stoner::Core
{

struct FPlatformTypes
{
    using int8 = std::int8_t;
    using int16 = std::int16_t;
    using int32 = std::int32_t;
    using int64 = std::int64_t;

    using uint8 = std::uint8_t;
    using uint16 = std::uint16_t;
    using uint32 = std::uint32_t;
    using uint64 = std::uint64_t;

    using size_t = std::size_t;
    using ssize_t = std::ptrdiff_t;
    using uintptr_t = std::uintptr_t;
    using intptr_t = std::intptr_t;

    using ansichar = char;
    using widechar = wchar_t;
    using utf8char = char8_t;
    using bool_t = bool;
};

using int8 = FPlatformTypes::int8;
using int16 = FPlatformTypes::int16;
using int32 = FPlatformTypes::int32;
using int64 = FPlatformTypes::int64;

using uint8 = FPlatformTypes::uint8;
using uint16 = FPlatformTypes::uint16;
using uint32 = FPlatformTypes::uint32;
using uint64 = FPlatformTypes::uint64;

using usize = FPlatformTypes::size_t;
using ssize = FPlatformTypes::ssize_t;
using uintptr = FPlatformTypes::uintptr_t;
using intptr = FPlatformTypes::intptr_t;

using ANSICHAR = FPlatformTypes::ansichar;
using WIDECHAR = FPlatformTypes::widechar;
using UTF8CHAR = FPlatformTypes::utf8char;

} // namespace Stoner::Core
