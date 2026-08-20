#pragma once

#include "Core/FString.h"

namespace Stoner::Core
{

struct FProcessExecutionRequest;

namespace Detail
{

[[nodiscard]] bool IsExplicitDynamicModulePath(const FString& Path);
[[nodiscard]] bool IsValidProcessRequest(
    const FProcessExecutionRequest& Request);

} // namespace Detail
} // namespace Stoner::Core
