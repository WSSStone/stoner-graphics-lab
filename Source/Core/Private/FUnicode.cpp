#include "Core/FUnicode.h"

#include "../../../ThirdParty/utf8proc/utf8proc.h"

#include <cstdlib>
#include <limits>
#include <string>

namespace Stoner::Core
{

EUnicodeResult FUnicode::NormalizeNFC(
    const FString& Input,
    FString& OutNormalized) noexcept
{
    OutNormalized.Clear();
    if (Input.Len() > static_cast<usize>(std::numeric_limits<utf8proc_ssize_t>::max()))
    {
        return EUnicodeResult::ConversionFailed;
    }

    utf8proc_uint8_t* Normalized = nullptr;
    const utf8proc_ssize_t Length = utf8proc_map(
        reinterpret_cast<const utf8proc_uint8_t*>(Input.CStr()),
        static_cast<utf8proc_ssize_t>(Input.Len()),
        &Normalized,
        static_cast<utf8proc_option_t>(UTF8PROC_STABLE | UTF8PROC_COMPOSE));
    if (Length < 0)
    {
        std::free(Normalized);
        return Length == UTF8PROC_ERROR_INVALIDUTF8
            ? EUnicodeResult::InvalidUtf8
            : EUnicodeResult::ConversionFailed;
    }

    try
    {
        OutNormalized = FString(std::string(
            reinterpret_cast<const char*>(Normalized),
            static_cast<usize>(Length)));
    }
    catch (...)
    {
        std::free(Normalized);
        OutNormalized.Clear();
        return EUnicodeResult::ConversionFailed;
    }
    std::free(Normalized);
    return EUnicodeResult::Success;
}

} // namespace Stoner::Core
