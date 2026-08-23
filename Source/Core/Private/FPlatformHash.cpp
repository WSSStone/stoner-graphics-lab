#include "Core/FPlatformHash.h"

#include "Core/SGPlatform.h"

#include <limits>

#if SG_PLATFORM_MAC
#include <CommonCrypto/CommonDigest.h>
#endif

namespace Stoner::Core
{

bool FPlatformHash::TrySha256(
    std::span<const uint8> Bytes,
    std::array<uint8, 32>& OutDigest) noexcept
{
#if SG_PLATFORM_MAC
    if (Bytes.size() > std::numeric_limits<CC_LONG>::max()) return false;
    return CC_SHA256(
        Bytes.data(), static_cast<CC_LONG>(Bytes.size()),
        OutDigest.data()) != nullptr;
#else
    (void)Bytes;
    (void)OutDigest;
    return false;
#endif
}

} // namespace Stoner::Core
