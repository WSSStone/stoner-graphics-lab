#pragma once

#include "Core/FPlatformTypes.h"
#include "Core/FString.h"

namespace Stoner::Core
{

enum class EUnicodeResult : uint8
{
    Success,
    InvalidUtf8,
    ConversionFailed
};

struct FUnicode
{
    [[nodiscard]] static EUnicodeResult NormalizeNFC(
        const FString& Input,
        FString& OutNormalized) noexcept;
};

} // namespace Stoner::Core
