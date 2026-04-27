#pragma once

#include <memory>
#include <utility>

namespace Stoner::Core
{

template <typename T>
using TUniquePtr = std::unique_ptr<T>;

template <typename T, typename... Args>
[[nodiscard]] TUniquePtr<T> MakeUnique(Args&&... Arguments)
{
    return std::make_unique<T>(std::forward<Args>(Arguments)...);
}

} // namespace Stoner::Core
