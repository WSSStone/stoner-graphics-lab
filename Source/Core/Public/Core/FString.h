#pragma once

#include <string>
#include <string_view>
#include <utility>

namespace Stoner::Core
{

class FString
{
public:
    FString() = default;

    FString(const char* Text)
        : Text_(Text != nullptr ? Text : "")
    {
    }

    FString(std::string Text)
        : Text_(std::move(Text))
    {
    }

    FString(std::string_view Text)
        : Text_(Text)
    {
    }

    [[nodiscard]] const char* CStr() const noexcept
    {
        return Text_.c_str();
    }

    [[nodiscard]] std::string_view View() const noexcept
    {
        return Text_;
    }

    [[nodiscard]] const std::string& ToStdString() const noexcept
    {
        return Text_;
    }

    [[nodiscard]] std::size_t Len() const noexcept
    {
        return Text_.size();
    }

    [[nodiscard]] bool IsEmpty() const noexcept
    {
        return Text_.empty();
    }

    void Clear() noexcept
    {
        Text_.clear();
    }

    friend bool operator==(const FString& Left, const FString& Right) noexcept
    {
        return Left.Text_ == Right.Text_;
    }

    friend bool operator!=(const FString& Left, const FString& Right) noexcept
    {
        return !(Left == Right);
    }

    friend bool operator<(const FString& Left, const FString& Right) noexcept
    {
        return Left.Text_ < Right.Text_;
    }

private:
    std::string Text_;
};

} // namespace Stoner::Core
