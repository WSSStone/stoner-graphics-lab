#pragma once

#include <unordered_map>

namespace Stoner::Core
{

template <typename K, typename V>
using TMap = std::unordered_map<K, V>;

} // namespace Stoner::Core
