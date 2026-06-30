#pragma once

#include <memory>
#include <utility>

namespace Stoner::Core
{

template <typename T>
using TSharedPtr = std::shared_ptr<T>;

template <typename T>
using TWeakPtr = std::weak_ptr<T>;

template <typename T, typename... Args>
[[nodiscard]] TSharedPtr<T> MakeShared(Args&&... Arguments)
{
    return std::make_shared<T>(std::forward<Args>(Arguments)...);
}

} // namespace Stoner::Core
