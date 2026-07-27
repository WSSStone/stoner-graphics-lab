#pragma once

#include "Core/FString.h"

namespace Stoner::Core
{

class FDynamicModuleHandle
{
public:
    FDynamicModuleHandle() noexcept = default;
    ~FDynamicModuleHandle() noexcept;

    FDynamicModuleHandle(const FDynamicModuleHandle&) = delete;
    FDynamicModuleHandle& operator=(const FDynamicModuleHandle&) = delete;

    FDynamicModuleHandle(FDynamicModuleHandle&& Other) noexcept;
    FDynamicModuleHandle& operator=(FDynamicModuleHandle&& Other) noexcept;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return Handle_ != nullptr;
    }

private:
    friend struct FPlatformProcess;

    explicit FDynamicModuleHandle(void* Handle) noexcept
        : Handle_(Handle)
    {
    }

    void Reset() noexcept;

    void* Handle_ = nullptr;
};

struct FPlatformProcess
{
    [[nodiscard]] static FDynamicModuleHandle LoadDynamicModule(const FString& ExplicitPath);
    [[nodiscard]] static void* GetSymbol(
        const FDynamicModuleHandle& Module,
        const char* SymbolName) noexcept;
    static void FreeDynamicModule(FDynamicModuleHandle& Module) noexcept;
};

} // namespace Stoner::Core
