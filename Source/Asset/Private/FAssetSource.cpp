#include "Asset/FAssetSource.h"

#include "Core/FUnicode.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace Stoner::Asset
{

EAssetResult FAssetSourceLocator::Create(
    const Core::FString& Scheme,
    const Core::FString& Locator,
    FAssetSourceLocator& OutLocator)
{
    OutLocator = {};
    if (Scheme.IsEmpty() || Scheme.Len() > 63)
    {
        return EAssetResult::MalformedSource;
    }
    std::string CanonicalScheme = Scheme.ToStdString();
    std::transform(
        CanonicalScheme.begin(),
        CanonicalScheme.end(),
        CanonicalScheme.begin(),
        [](unsigned char Character) { return static_cast<char>(std::tolower(Character)); });
    const auto IsAlpha = [](unsigned char Character)
    {
        return Character >= 'a' && Character <= 'z';
    };
    const auto IsDigit = [](unsigned char Character)
    {
        return Character >= '0' && Character <= '9';
    };
    if (!IsAlpha(static_cast<unsigned char>(CanonicalScheme.front())))
    {
        return EAssetResult::MalformedSource;
    }
    for (const unsigned char Character : CanonicalScheme)
    {
        if (!IsAlpha(Character) && !IsDigit(Character) &&
            Character != '+' && Character != '.' && Character != '-')
        {
            return EAssetResult::MalformedSource;
        }
    }

    Core::FString CanonicalLocator;
    const Core::EUnicodeResult UnicodeResult =
        Core::FUnicode::NormalizeNFC(Locator, CanonicalLocator);
    if (UnicodeResult == Core::EUnicodeResult::InvalidUtf8)
    {
        return EAssetResult::InvalidUtf8;
    }
    if (UnicodeResult != Core::EUnicodeResult::Success)
    {
        return EAssetResult::ProcessingFailure;
    }
    if (CanonicalLocator.IsEmpty() || CanonicalLocator.Len() > 1024)
    {
        return EAssetResult::MalformedSource;
    }
    for (const unsigned char Character : CanonicalLocator.View())
    {
        if (Character == 0 || Character < 0x20U || Character == 0x7fU)
        {
            return EAssetResult::MalformedSource;
        }
    }

    OutLocator.Scheme_ = Core::FString(std::move(CanonicalScheme));
    OutLocator.Locator_ = std::move(CanonicalLocator);
    return EAssetResult::Success;
}

bool FAssetSourceLocator::IsValid() const noexcept
{
    return !Scheme_.IsEmpty() && !Locator_.IsEmpty();
}

const Core::FString& FAssetSourceLocator::GetScheme() const noexcept
{
    return Scheme_;
}

const Core::FString& FAssetSourceLocator::GetLocator() const noexcept
{
    return Locator_;
}

Core::FString FAssetSourceLocator::ToString() const
{
    return Core::FString(Scheme_.ToStdString() + ":" + Locator_.ToStdString());
}

bool FAssetSourceLocator::operator<(const FAssetSourceLocator& Other) const noexcept
{
    return Scheme_ != Other.Scheme_ ? Scheme_ < Other.Scheme_ : Locator_ < Other.Locator_;
}

} // namespace Stoner::Asset
