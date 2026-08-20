#pragma once

#include "Core/FPlatformProcess.h"
#include "FMetalShaderEvidenceCodec.h"

namespace Stoner::AssetCooker::Private
{

enum class EMetalLibraryFinalizeStatus : Core::uint8
{
    Success,
    HostUnsupported,
    InvalidRequest,
    ToolchainUnavailable,
    TimedOut,
    CompilerFailed,
    EmptyOutput,
    EvidenceMismatch,
    IoFailure
};

struct FMetalToolchainEvidence
{
    Core::FString MetalCompiler;
    Core::FString XcodeBuild;
    Core::FString Sdk;

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] bool operator==(const FMetalToolchainEvidence&) const = default;
};

struct FMetalLibraryCompileRequest
{
    Core::FString WorkingDirectory;
    Core::FString Architecture;
    Core::FString TargetProfile;
    Core::FString NormalizedMsl;
    FMetalShaderEvidence DerivationEvidence;
    Core::uint64 TimeoutMilliseconds = 60000;
    Core::usize MaxToolOutputBytes = 256U * 1024U;
    Core::usize MaxLibraryBytes = 64U * 1024U * 1024U;
};

struct FMetalLibraryCompileResult
{
    EMetalLibraryFinalizeStatus Status =
        EMetalLibraryFinalizeStatus::InvalidRequest;
    Core::TArray<Core::uint8> LibraryBytes;
    FMetalShaderEvidence NativeEvidence;
    FMetalToolchainEvidence Toolchain;
    Core::FString StableReason;
    Core::FString ToolStandardOutput;
    Core::FString ToolStandardError;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return Status == EMetalLibraryFinalizeStatus::Success;
    }
};

class IMetalToolExecutor
{
public:
    virtual ~IMetalToolExecutor() = default;
    [[nodiscard]] virtual Core::FProcessExecutionResult Execute(
        const Core::FProcessExecutionRequest& Request) = 0;
};

[[nodiscard]] EMetalLibraryFinalizeStatus InspectMetalToolchain(
    IMetalToolExecutor& Executor,
    Core::uint64 TimeoutMilliseconds,
    Core::usize MaxOutputBytes,
    FMetalToolchainEvidence& OutEvidence) noexcept;

[[nodiscard]] EMetalLibraryFinalizeStatus InspectMetalToolchain(
    Core::uint64 TimeoutMilliseconds,
    Core::usize MaxOutputBytes,
    FMetalToolchainEvidence& OutEvidence) noexcept;

[[nodiscard]] FMetalLibraryCompileResult FinalizeMetalLibrary(
    const FMetalLibraryCompileRequest& Request,
    IMetalToolExecutor* Executor = nullptr) noexcept;

} // namespace Stoner::AssetCooker::Private
