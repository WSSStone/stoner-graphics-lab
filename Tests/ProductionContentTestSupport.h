#pragma once

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <string>
#include <string_view>

namespace ProductionContentTestSupport
{

inline std::string ReadText(const std::filesystem::path& Path)
{
    std::ifstream Input(Path, std::ios::binary);
    return std::string(
        std::istreambuf_iterator<char>(Input),
        std::istreambuf_iterator<char>());
}

inline bool ContainsAll(
    std::string_view Text,
    std::initializer_list<std::string_view> Tokens)
{
    for (const std::string_view Token : Tokens)
        if (Text.find(Token) == std::string_view::npos) return false;
    return true;
}

} // namespace ProductionContentTestSupport
