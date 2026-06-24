#pragma once

#include "Core/FString.h"

namespace Stoner::Core
{

struct FDynamicModuleHandle
{
    void* Handle = nullptr;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return Handle != nullptr;
    }
};

struct FPlatformProcess
{
    [[nodiscard]] static FDynamicModuleHandle LoadDynamicModule(const FString& ExplicitPath);
    [[nodiscard]] static void* GetSymbol(FDynamicModuleHandle Module, const char* SymbolName) noexcept;
    static void FreeDynamicModule(FDynamicModuleHandle& Module) noexcept;
};

} // namespace Stoner::Core
