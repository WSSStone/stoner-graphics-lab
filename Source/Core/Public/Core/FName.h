#pragma once

#include "Core/FString.h"
#include "Core/FPlatformTypes.h"

#include <functional>
#include <string_view>
#include <utility>

namespace Stoner::Core
{

class FName
{
public:
    FName() = default;

    FName(const char* Text)
        : FName(FString(Text))
    {
    }

    FName(const FString& Text)
        : Text_(Text)
        , Hash_(HashString(Text_.View()))
    {
    }

    FName(std::string_view Text)
        : FName(FString(Text))
    {
    }

    [[nodiscard]] static FName FromTextAndHashForTesting(FString Text, uint64 Hash)
    {
        return FName(std::move(Text), Hash);
    }

    [[nodiscard]] const FString& ToString() const noexcept
    {
        return Text_;
    }

    [[nodiscard]] std::string_view View() const noexcept
    {
        return Text_.View();
    }

    [[nodiscard]] uint64 GetHash() const noexcept
    {
        return Hash_;
    }

    [[nodiscard]] bool IsEmpty() const noexcept
    {
        return Text_.IsEmpty();
    }

    friend bool operator==(const FName& Left, const FName& Right) noexcept
    {
        return Left.Hash_ == Right.Hash_ && Left.Text_ == Right.Text_;
    }

    friend bool operator!=(const FName& Left, const FName& Right) noexcept
    {
        return !(Left == Right);
    }

private:
    FName(FString Text, uint64 Hash)
        : Text_(std::move(Text))
        , Hash_(Hash)
    {
    }

    [[nodiscard]] static uint64 HashString(std::string_view Text) noexcept
    {
        return static_cast<uint64>(std::hash<std::string_view>{}(Text));
    }

    FString Text_;
    uint64 Hash_ = HashString({});
};

} // namespace Stoner::Core
