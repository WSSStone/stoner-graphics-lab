#pragma once

#include "Core/FString.h"

namespace Stoner::Core::Detail
{

[[nodiscard]] bool IsExplicitDynamicModulePath(const FString& Path);

} // namespace Stoner::Core::Detail
