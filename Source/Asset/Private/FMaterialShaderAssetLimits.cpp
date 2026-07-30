#include "Asset/FMaterialShaderAssetLimits.h"

#include <limits>

namespace Stoner::Asset
{

EAssetResult FMaterialShaderAssetLimits::Validate() const noexcept
{
    const bool bAllPositive =
        MaxDefinitionBytes > 0 &&
        MaxShaderSourceBytes > 0 &&
        MaxShaderPayloadBytes > 0 &&
        MaxProgramDependencyBytes > 0 &&
        MaxJsonDepth > 0 &&
        MaxJsonValues > 0 &&
        MaxObjectMembers > 0 &&
        MaxArrayElements > 0 &&
        MaxTextBytes > 0 &&
        MaxTokenBytes > 0 &&
        MaxLocatorBytes > 0 &&
        MaxNumberTokenBytes > 0 &&
        MaxExtensions > 0 &&
        MaxStages > 0 &&
        MaxPermutationFlags > 0 &&
        MaxSourceRecords > 0 &&
        MaxVariants > 0 &&
        MaxPayloadRecords > 0 &&
        MaxParameters > 0 &&
        MaxInterfaceBindingsPerStage > 0 &&
        MaxDependencies > 0 &&
        MaxInstanceDepth > 0;
    if (!bAllPositive ||
        MaxProgramDependencyBytes < MaxShaderSourceBytes ||
        MaxProgramDependencyBytes < MaxShaderPayloadBytes ||
        MaxJsonValues < MaxObjectMembers ||
        MaxJsonValues < MaxArrayElements)
    {
        return EAssetResult::InvalidInput;
    }
    return EAssetResult::Success;
}

bool CheckedMaterialShaderAdd(
    Core::uint64 Left,
    Core::uint64 Right,
    Core::uint64& OutValue) noexcept
{
    if (Right > std::numeric_limits<Core::uint64>::max() - Left)
    {
        OutValue = 0;
        return false;
    }
    OutValue = Left + Right;
    return true;
}

bool CheckedMaterialShaderMultiply(
    Core::uint64 Left,
    Core::uint64 Right,
    Core::uint64& OutValue) noexcept
{
    if (Left != 0 &&
        Right > std::numeric_limits<Core::uint64>::max() / Left)
    {
        OutValue = 0;
        return false;
    }
    OutValue = Left * Right;
    return true;
}

} // namespace Stoner::Asset
