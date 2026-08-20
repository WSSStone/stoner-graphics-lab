#pragma once

#include "Core/FString.h"
#include "Core/FPlatformTypes.h"
#include "Core/TArray.h"

namespace Stoner::Core
{

enum class EProcessExecutionStatus : uint8
{
    Completed,
    InvalidRequest,
    LaunchFailed,
    TimedOut,
    Terminated
};

struct FProcessExecutionLimits
{
    uint64 TimeoutMilliseconds = 30000;
    usize MaxStdoutBytes = 1024U * 1024U;
    usize MaxStderrBytes = 1024U * 1024U;
};

struct FProcessExecutionRequest
{
    FString ExecutablePath;
    TArray<FString> Arguments;
    FProcessExecutionLimits Limits;
};

struct FProcessExecutionResult
{
    EProcessExecutionStatus Status = EProcessExecutionStatus::InvalidRequest;
    int32 ExitCode = -1;
    FString StandardOutput;
    FString StandardError;
    bool bStdoutTruncated = false;
    bool bStderrTruncated = false;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return Status == EProcessExecutionStatus::Completed && ExitCode == 0;
    }
};

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
    [[nodiscard]] static FProcessExecutionResult Execute(
        const FProcessExecutionRequest& Request);
    [[nodiscard]] static FDynamicModuleHandle LoadDynamicModule(const FString& ExplicitPath);
    [[nodiscard]] static void* GetSymbol(
        const FDynamicModuleHandle& Module,
        const char* SymbolName) noexcept;
    static void FreeDynamicModule(FDynamicModuleHandle& Module) noexcept;
};

} // namespace Stoner::Core
