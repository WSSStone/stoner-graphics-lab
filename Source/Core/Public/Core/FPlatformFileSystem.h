#pragma once

#include "Core/FPlatformTypes.h"
#include "Core/FString.h"
#include "Core/TArray.h"

namespace Stoner::Core
{

struct FPlatformFileSystem
{
    [[nodiscard]] static bool Exists(const FString& Path);
    [[nodiscard]] static bool CreateDirectory(const FString& Path);
    [[nodiscard]] static bool ReadFile(const FString& Path, TArray<uint8>& OutData);
    [[nodiscard]] static bool WriteFile(const FString& Path, const TArray<uint8>& Data);
};

} // namespace Stoner::Core
