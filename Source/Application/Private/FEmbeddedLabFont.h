#pragma once
#include <span>
namespace Stoner::Application
{
[[nodiscard]] std::span<const unsigned char> GetEmbeddedLabFont() noexcept;
}
