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

    FName(const FName&) = default;
    FName& operator=(const FName&) = default;

    FName(FName&& Other) noexcept
        : Text_(std::move(Other.Text_))
        , Hash_(HashString(Text_.View()))
    {
        Other.Hash_ = HashString(Other.Text_.View());
    }

    FName& operator=(FName&& Other) noexcept
    {
        if (this != &Other)
        {
            Text_ = std::move(Other.Text_);
            Hash_ = HashString(Text_.View());
            Other.Hash_ = HashString(Other.Text_.View());
        }
        return *this;
    }

    [[nodiscard]] static bool CompareWithForcedCommonHashForTesting(
        const FString& LeftText,
        const FString& RightText,
        uint64 CommonHash) noexcept
    {
        return AreEqual(LeftText, CommonHash, RightText, CommonHash);
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
        return AreEqual(Left.Text_, Left.Hash_, Right.Text_, Right.Hash_);
    }

    friend bool operator!=(const FName& Left, const FName& Right) noexcept
    {
        return !(Left == Right);
    }

private:
    [[nodiscard]] static bool AreEqual(
        const FString& LeftText,
        uint64 LeftHash,
        const FString& RightText,
        uint64 RightHash) noexcept
    {
        return LeftHash == RightHash && LeftText == RightText;
    }

    [[nodiscard]] static uint64 HashString(std::string_view Text) noexcept
    {
        return static_cast<uint64>(std::hash<std::string_view>{}(Text));
    }

    FString Text_;
    uint64 Hash_ = HashString({});
};

} // namespace Stoner::Core
