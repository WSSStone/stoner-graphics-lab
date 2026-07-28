#pragma once

#include "Asset/EAssetResult.h"
#include "Core/FString.h"

#include <string_view>

namespace Stoner::Asset
{

namespace Detail
{
inline bool IsAsciiToken(
    std::string_view Text,
    std::size_t Maximum,
    bool RequireAlphaFirst,
    bool AllowPlus)
{
    if (Text.empty() || Text.size() > Maximum)
    {
        return false;
    }
    const auto IsAlpha = [](unsigned char Character)
    {
        return (Character >= 'A' && Character <= 'Z') ||
            (Character >= 'a' && Character <= 'z');
    };
    const auto IsDigit = [](unsigned char Character)
    {
        return Character >= '0' && Character <= '9';
    };
    const unsigned char First = static_cast<unsigned char>(Text.front());
    if ((RequireAlphaFirst && !IsAlpha(First)) ||
        (!RequireAlphaFirst && !IsAlpha(First) && !IsDigit(First)))
    {
        return false;
    }
    for (const unsigned char Character : Text)
    {
        if (!IsAlpha(Character) && !IsDigit(Character) && Character != '_' &&
            Character != '.' && Character != '-' && (!AllowPlus || Character != '+'))
        {
            return false;
        }
    }
    return true;
}
} // namespace Detail

class FAssetParticipantId
{
public:
    [[nodiscard]] static EAssetResult Create(
        const Core::FString& Text,
        FAssetParticipantId& OutValue)
    {
        OutValue = {};
        if (!Detail::IsAsciiToken(Text.View(), 127, true, false))
        {
            return EAssetResult::InvalidInput;
        }
        OutValue.Value_ = Text;
        return EAssetResult::Success;
    }

    [[nodiscard]] bool IsValid() const noexcept { return !Value_.IsEmpty(); }
    [[nodiscard]] const Core::FString& ToString() const noexcept { return Value_; }
    [[nodiscard]] bool operator==(const FAssetParticipantId&) const = default;
    [[nodiscard]] bool operator<(const FAssetParticipantId& Other) const noexcept
    {
        return Value_ < Other.Value_;
    }

private:
    Core::FString Value_;
};

class FAssetProducerVersion
{
public:
    [[nodiscard]] static EAssetResult Create(
        const Core::FString& Text,
        FAssetProducerVersion& OutValue)
    {
        OutValue = {};
        if (!Detail::IsAsciiToken(Text.View(), 64, false, true))
        {
            return EAssetResult::InvalidInput;
        }
        OutValue.Value_ = Text;
        return EAssetResult::Success;
    }

    [[nodiscard]] bool IsValid() const noexcept { return !Value_.IsEmpty(); }
    [[nodiscard]] const Core::FString& ToString() const noexcept { return Value_; }
    [[nodiscard]] bool operator==(const FAssetProducerVersion&) const = default;
    [[nodiscard]] bool operator<(const FAssetProducerVersion& Other) const noexcept
    {
        return Value_ < Other.Value_;
    }

private:
    Core::FString Value_;
};

} // namespace Stoner::Asset
