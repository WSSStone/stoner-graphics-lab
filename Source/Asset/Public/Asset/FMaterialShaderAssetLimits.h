#pragma once

#include "Asset/EAssetResult.h"
#include "Core/FPlatformTypes.h"

#include <cstddef>

namespace Stoner::Asset
{

struct FMaterialShaderAssetLimits
{
    Core::uint64 MaxDefinitionBytes = 1ULL * 1024ULL * 1024ULL;
    Core::uint64 MaxShaderSourceBytes = 4ULL * 1024ULL * 1024ULL;
    Core::uint64 MaxShaderPayloadBytes = 16ULL * 1024ULL * 1024ULL;
    Core::uint64 MaxProgramDependencyBytes = 64ULL * 1024ULL * 1024ULL;
    Core::usize MaxJsonDepth = 32;
    Core::usize MaxJsonValues = 32768;
    Core::usize MaxObjectMembers = 256;
    Core::usize MaxArrayElements = 4096;
    Core::usize MaxTextBytes = 4096;
    Core::usize MaxTokenBytes = 128;
    Core::usize MaxLocatorBytes = 1024;
    Core::usize MaxNumberTokenBytes = 64;
    Core::usize MaxExtensions = 64;
    Core::usize MaxStages = 16;
    Core::usize MaxPermutationFlags = 64;
    Core::usize MaxSourceRecords = 64;
    Core::usize MaxVariants = 1024;
    Core::usize MaxPayloadRecords = 2048;
    Core::usize MaxParameters = 256;
    Core::usize MaxInterfaceBindingsPerStage = 256;
    Core::usize MaxDependencies = 1024;
    Core::usize MaxInstanceDepth = 64;

    [[nodiscard]] EAssetResult Validate() const noexcept;
};

[[nodiscard]] bool CheckedMaterialShaderAdd(
    Core::uint64 Left,
    Core::uint64 Right,
    Core::uint64& OutValue) noexcept;
[[nodiscard]] bool CheckedMaterialShaderMultiply(
    Core::uint64 Left,
    Core::uint64 Right,
    Core::uint64& OutValue) noexcept;

} // namespace Stoner::Asset
