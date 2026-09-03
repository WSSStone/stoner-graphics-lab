#include "FMetalLibraryCompiler.h"

#include "Asset/FAssetDigest.h"
#include "Core/SGPlatform.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <new>
#include <span>
#include <string>

namespace Stoner::AssetCooker::Private
{
namespace
{

class FPlatformToolExecutor final : public IMetalToolExecutor
{
public:
    Core::FProcessExecutionResult Execute(
        const Core::FProcessExecutionRequest& Request) override
    {
        return Core::FPlatformProcess::Execute(Request);
    }
};

#if SG_PLATFORM_MAC
std::filesystem::path NativePath(const Core::FString& Value)
{
    const std::string Utf8 = Value.ToStdString();
    std::u8string Native;
    Native.reserve(Utf8.size());
    for (const unsigned char Byte : Utf8)
        Native.push_back(static_cast<char8_t>(Byte));
    return std::filesystem::path(Native);
}

Core::FString Utf8Path(const std::filesystem::path& Value)
{
    const std::u8string Native = Value.u8string();
    return Core::FString(std::string(
        reinterpret_cast<const char*>(Native.data()), Native.size()));
}

Core::FString Trimmed(const Core::FString& Value)
{
    std::string Text = Value.ToStdString();
    while (!Text.empty() &&
        (Text.back() == '\r' || Text.back() == '\n' ||
         Text.back() == ' ' || Text.back() == '\t'))
        Text.pop_back();
    const auto Begin = Text.find_first_not_of(" \t\r\n");
    return Begin == std::string::npos
        ? Core::FString{}
        : Core::FString(Text.substr(Begin));
}

Core::FProcessExecutionRequest ToolRequest(
    Core::TArray<Core::FString> Arguments,
    Core::uint64 Timeout,
    Core::usize Maximum)
{
    Core::FProcessExecutionRequest Request;
    Request.ExecutablePath = Core::FString("/usr/bin/xcrun");
    Request.Arguments = std::move(Arguments);
    Request.Limits.TimeoutMilliseconds = Timeout;
    Request.Limits.MaxStdoutBytes = Maximum;
    Request.Limits.MaxStderrBytes = Maximum;
    return Request;
}

Core::FProcessExecutionRequest ExecutableRequest(
    const Core::FString& ExecutablePath,
    Core::TArray<Core::FString> Arguments,
    Core::uint64 Timeout,
    Core::usize Maximum)
{
    Core::FProcessExecutionRequest Request;
    Request.ExecutablePath = ExecutablePath;
    Request.Arguments = std::move(Arguments);
    Request.Limits.TimeoutMilliseconds = Timeout;
    Request.Limits.MaxStdoutBytes = Maximum;
    Request.Limits.MaxStderrBytes = Maximum;
    return Request;
}

EMetalLibraryFinalizeStatus ExecuteTool(
    IMetalToolExecutor& Executor,
    const Core::FProcessExecutionRequest& Request,
    Core::FProcessExecutionResult& Out)
{
    Out = Executor.Execute(Request);
    if (Out.Status == Core::EProcessExecutionStatus::TimedOut)
        return EMetalLibraryFinalizeStatus::TimedOut;
    if (!Out.Succeeded() || Out.bStdoutTruncated || Out.bStderrTruncated)
        return EMetalLibraryFinalizeStatus::CompilerFailed;
    return EMetalLibraryFinalizeStatus::Success;
}

Asset::FAssetDigest ArgumentDigest(
    const Core::FProcessExecutionRequest& First,
    const Core::FProcessExecutionRequest& Second,
    const std::filesystem::path& WorkingDirectory)
{
    std::string Canonical;
    const std::string PhysicalRoot = WorkingDirectory.generic_string();
    const auto Normalize = [&PhysicalRoot](std::string Value)
    {
        std::string::size_type Position = 0;
        while (!PhysicalRoot.empty() &&
            (Position = Value.find(PhysicalRoot, Position)) !=
                std::string::npos)
        {
            Value.replace(Position, PhysicalRoot.size(), "<WORK>");
            Position += 6;
        }
        return Value;
    };
    const auto Append = [&Canonical, &Normalize](
        const Core::FProcessExecutionRequest& Request)
    {
        Canonical += Request.ExecutablePath.ToStdString();
        Canonical.push_back('\0');
        for (const auto& Argument : Request.Arguments)
        {
            Canonical += Normalize(Argument.ToStdString());
            Canonical.push_back('\0');
        }
    };
    Append(First);
    Append(Second);
    return Asset::FAssetDigest::FromBytes(std::span<const Core::uint8>(
        reinterpret_cast<const Core::uint8*>(Canonical.data()),
        Canonical.size()));
}

struct FTemporaryOutputs
{
    std::filesystem::path Source;
    std::filesystem::path Air;
    std::filesystem::path Library;
    std::filesystem::path ModuleCache;

    ~FTemporaryOutputs()
    {
        std::error_code Error;
        std::filesystem::remove(Source, Error);
        std::filesystem::remove(Air, Error);
        std::filesystem::remove(Library, Error);
        std::filesystem::remove_all(ModuleCache, Error);
    }
};

const char* HostArchitecture()
{
#if defined(__aarch64__) || defined(__arm64__)
    return "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#else
    return "unsupported";
#endif
}

struct FResolvedMetalToolchain
{
    FMetalToolchainEvidence Evidence;
    Core::FString MetallibExecutable;
};

bool IsAbsoluteToolPath(const Core::FString& Value)
{
    return !Value.IsEmpty() && NativePath(Value).is_absolute();
}

EMetalLibraryFinalizeStatus InspectMetalToolchainResolved(
    IMetalToolExecutor& Executor,
    Core::uint64 TimeoutMilliseconds,
    Core::usize MaxOutputBytes,
    FResolvedMetalToolchain& OutToolchain)
{
    OutToolchain = {};
    Core::FProcessExecutionResult MetalPath;
    Core::FProcessExecutionResult MetalVersion;
    Core::FProcessExecutionResult MetallibPath;
    Core::FProcessExecutionResult MetallibVersion;
    Core::FProcessExecutionResult Xcode;
    Core::FProcessExecutionResult Sdk;
    auto Status = ExecuteTool(
        Executor,
        ToolRequest(
            {Core::FString("--find"), Core::FString("metal")},
            TimeoutMilliseconds, MaxOutputBytes),
        MetalPath);
    if (Status != EMetalLibraryFinalizeStatus::Success)
        return Status == EMetalLibraryFinalizeStatus::TimedOut
            ? Status : EMetalLibraryFinalizeStatus::ToolchainUnavailable;
    Status = ExecuteTool(
        Executor,
        ToolRequest(
            {Core::FString("metal"), Core::FString("--version")},
            TimeoutMilliseconds, MaxOutputBytes),
        MetalVersion);
    if (Status != EMetalLibraryFinalizeStatus::Success)
        return Status == EMetalLibraryFinalizeStatus::TimedOut
            ? Status : EMetalLibraryFinalizeStatus::ToolchainUnavailable;

    Status = ExecuteTool(
        Executor,
        ToolRequest(
            {Core::FString("--find"), Core::FString("metallib")},
            TimeoutMilliseconds, MaxOutputBytes),
        MetallibPath);
    Core::FString ResolvedMetallib = Trimmed(MetallibPath.StandardOutput);
    if (Status == EMetalLibraryFinalizeStatus::TimedOut)
        return Status;
    if (Status != EMetalLibraryFinalizeStatus::Success ||
        !IsAbsoluteToolPath(ResolvedMetallib))
    {
        MetallibPath = {};
        Status = ExecuteTool(
            Executor,
            ToolRequest(
                {Core::FString("metal"),
                 Core::FString("-print-prog-name=metallib")},
                TimeoutMilliseconds, MaxOutputBytes),
            MetallibPath);
        if (Status != EMetalLibraryFinalizeStatus::Success)
            return Status == EMetalLibraryFinalizeStatus::TimedOut
                ? Status : EMetalLibraryFinalizeStatus::ToolchainUnavailable;
        ResolvedMetallib = Trimmed(MetallibPath.StandardOutput);
        if (!IsAbsoluteToolPath(ResolvedMetallib))
            return EMetalLibraryFinalizeStatus::ToolchainUnavailable;
    }
    Status = ExecuteTool(
        Executor,
        ExecutableRequest(
            ResolvedMetallib,
            {Core::FString("--version")},
            TimeoutMilliseconds, MaxOutputBytes),
        MetallibVersion);
    if (Status != EMetalLibraryFinalizeStatus::Success)
        return Status == EMetalLibraryFinalizeStatus::TimedOut
            ? Status : EMetalLibraryFinalizeStatus::ToolchainUnavailable;
    Status = ExecuteTool(
        Executor,
        ToolRequest(
            {Core::FString("xcodebuild"), Core::FString("-version")},
            TimeoutMilliseconds, MaxOutputBytes),
        Xcode);
    if (Status != EMetalLibraryFinalizeStatus::Success) return Status;
    Status = ExecuteTool(
        Executor,
        ToolRequest(
            {Core::FString("--sdk"), Core::FString("macosx"),
             Core::FString("--show-sdk-version")},
            TimeoutMilliseconds, MaxOutputBytes),
        Sdk);
    if (Status != EMetalLibraryFinalizeStatus::Success) return Status;

    OutToolchain.Evidence.MetalCompiler = Core::FString(
        Trimmed(MetalPath.StandardOutput).ToStdString() + "\n" +
        Trimmed(MetalVersion.StandardOutput).ToStdString() + "\n" +
        ResolvedMetallib.ToStdString() + "\n" +
        Trimmed(MetallibVersion.StandardOutput).ToStdString());
    OutToolchain.Evidence.XcodeBuild = Trimmed(Xcode.StandardOutput);
    OutToolchain.Evidence.Sdk = Trimmed(Sdk.StandardOutput);
    OutToolchain.MetallibExecutable = std::move(ResolvedMetallib);
    if (!OutToolchain.Evidence.IsValid() ||
        !IsAbsoluteToolPath(OutToolchain.MetallibExecutable))
    {
        OutToolchain = {};
        return EMetalLibraryFinalizeStatus::ToolchainUnavailable;
    }
    return EMetalLibraryFinalizeStatus::Success;
}
#endif

FMetalLibraryCompileResult Failure(
    EMetalLibraryFinalizeStatus Status,
    const char* Reason)
{
    FMetalLibraryCompileResult Result;
    Result.Status = Status;
    Result.StableReason = Core::FString(Reason);
    return Result;
}

#if SG_PLATFORM_MAC
FMetalLibraryCompileResult ProcessFailure(
    EMetalLibraryFinalizeStatus Status,
    const char* Reason,
    const Core::FProcessExecutionResult& Process)
{
    FMetalLibraryCompileResult Result = Failure(Status, Reason);
    Result.ToolStandardOutput = Process.StandardOutput;
    Result.ToolStandardError = Process.StandardError;
    return Result;
}
#endif

} // namespace

bool FMetalToolchainEvidence::IsValid() const noexcept
{
    return !MetalCompiler.IsEmpty() && !XcodeBuild.IsEmpty() &&
        !Sdk.IsEmpty();
}

EMetalLibraryFinalizeStatus InspectMetalToolchain(
    IMetalToolExecutor& Executor,
    Core::uint64 TimeoutMilliseconds,
    Core::usize MaxOutputBytes,
    FMetalToolchainEvidence& OutEvidence) noexcept
{
    OutEvidence = {};
#if !SG_PLATFORM_MAC
    (void)Executor;
    (void)TimeoutMilliseconds;
    (void)MaxOutputBytes;
    return EMetalLibraryFinalizeStatus::HostUnsupported;
#else
    if (TimeoutMilliseconds == 0 || MaxOutputBytes == 0)
        return EMetalLibraryFinalizeStatus::InvalidRequest;
    try
    {
        FResolvedMetalToolchain Resolved;
        const EMetalLibraryFinalizeStatus Status =
            InspectMetalToolchainResolved(
                Executor, TimeoutMilliseconds, MaxOutputBytes, Resolved);
        OutEvidence = std::move(Resolved.Evidence);
        return Status;
    }
    catch (const std::bad_alloc&)
    {
        OutEvidence = {};
        return EMetalLibraryFinalizeStatus::IoFailure;
    }
#endif
}

EMetalLibraryFinalizeStatus InspectMetalToolchain(
    Core::uint64 TimeoutMilliseconds,
    Core::usize MaxOutputBytes,
    FMetalToolchainEvidence& OutEvidence) noexcept
{
    FPlatformToolExecutor Executor;
    return InspectMetalToolchain(
        Executor, TimeoutMilliseconds, MaxOutputBytes, OutEvidence);
}

FMetalLibraryCompileResult FinalizeMetalLibrary(
    const FMetalLibraryCompileRequest& Request,
    IMetalToolExecutor* Executor) noexcept
{
#if !SG_PLATFORM_MAC
    (void)Request;
    (void)Executor;
    return Failure(
        EMetalLibraryFinalizeStatus::HostUnsupported,
        "metal-finalize-host-unsupported");
#else
    if (Request.WorkingDirectory.IsEmpty() ||
        Request.Architecture != Core::FString(HostArchitecture()) ||
        Request.TargetProfile.IsEmpty() || Request.NormalizedMsl.IsEmpty() ||
        Request.TimeoutMilliseconds == 0 || Request.MaxToolOutputBytes == 0 ||
        Request.MaxLibraryBytes == 0 ||
        !Request.DerivationEvidence.IsValid() ||
        Request.DerivationEvidence.Kind !=
            EMetalShaderEvidenceKind::Derivation ||
        Request.DerivationEvidence.TargetProfile != Request.TargetProfile)
        return Failure(
            EMetalLibraryFinalizeStatus::InvalidRequest,
            "metal-finalize-invalid-request");
    try
    {
        const auto* MslBegin = reinterpret_cast<const Core::uint8*>(
            Request.NormalizedMsl.View().data());
        const Asset::FAssetDigest MslDigest = Asset::FAssetDigest::FromBytes(
            std::span<const Core::uint8>(
                MslBegin, Request.NormalizedMsl.Len()));
        if (MslDigest != Request.DerivationEvidence.NormalizedMslDigest)
            return Failure(
                EMetalLibraryFinalizeStatus::EvidenceMismatch,
                "metal-finalize-msl-evidence-mismatch");

        const std::filesystem::path Root = NativePath(Request.WorkingDirectory);
        if (!Root.is_absolute() || !std::filesystem::is_directory(Root))
            return Failure(
                EMetalLibraryFinalizeStatus::InvalidRequest,
                "metal-finalize-work-directory");
        FTemporaryOutputs Files{
            Root / "stoner-metal-input.metal",
            Root / "stoner-metal-output.air",
            Root / "stoner-metal-output.metallib",
            Root / "metal-module-cache"};
        if (std::filesystem::exists(Files.Source) ||
            std::filesystem::exists(Files.Air) ||
            std::filesystem::exists(Files.Library) ||
            std::filesystem::exists(Files.ModuleCache))
            return Failure(
                EMetalLibraryFinalizeStatus::InvalidRequest,
                "metal-finalize-work-collision");
        std::error_code ModuleCacheError;
        if (!std::filesystem::create_directory(
                Files.ModuleCache, ModuleCacheError) || ModuleCacheError)
            return Failure(
                EMetalLibraryFinalizeStatus::IoFailure,
                "metal-finalize-module-cache-create");
        {
            std::ofstream Output(Files.Source, std::ios::binary);
            Output.write(
                Request.NormalizedMsl.View().data(),
                static_cast<std::streamsize>(Request.NormalizedMsl.Len()));
            if (!Output.good())
                return Failure(
                    EMetalLibraryFinalizeStatus::IoFailure,
                    "metal-finalize-source-write");
        }

        FPlatformToolExecutor PlatformExecutor;
        IMetalToolExecutor& Tool = Executor ? *Executor : PlatformExecutor;
        FResolvedMetalToolchain ResolvedToolchain;
        const EMetalLibraryFinalizeStatus Doctor =
            InspectMetalToolchainResolved(
                Tool, Request.TimeoutMilliseconds,
                Request.MaxToolOutputBytes, ResolvedToolchain);
        if (Doctor != EMetalLibraryFinalizeStatus::Success)
            return Failure(Doctor, "metal-finalize-toolchain");
        FMetalToolchainEvidence& Toolchain = ResolvedToolchain.Evidence;

        auto Metal = ToolRequest(
            {Core::FString("-sdk"), Core::FString("macosx"),
             Core::FString("metal"), Core::FString("-std=macos-metal2.4"),
             Core::FString("-mmacosx-version-min=12.0"),
             Core::FString("-greproducible"),
             Core::FString("-frecord-sources=no"),
             Core::FString("-fdebug-compilation-dir=/stoner-metal-work"),
             Core::FString(
                 "-ffile-prefix-map=" +
                 Root.generic_string() + "=/stoner-metal-work"),
             Core::FString(
                 "-fdebug-prefix-map=" +
                 Root.generic_string() + "=/stoner-metal-work"),
             Core::FString(
                 "-fmacro-prefix-map=" +
                 Root.generic_string() + "=/stoner-metal-work"),
             Core::FString(
                 "-fmodules-cache-path=" +
                 Utf8Path(Files.ModuleCache).ToStdString()),
             Core::FString("-c"), Utf8Path(Files.Source),
             Core::FString("-o"), Utf8Path(Files.Air)},
            Request.TimeoutMilliseconds, Request.MaxToolOutputBytes);
        auto Metallib = ExecutableRequest(
            ResolvedToolchain.MetallibExecutable,
            {Utf8Path(Files.Air),
             Core::FString("-o"), Utf8Path(Files.Library)},
            Request.TimeoutMilliseconds, Request.MaxToolOutputBytes);
        Core::FProcessExecutionResult Process;
        EMetalLibraryFinalizeStatus Status =
            ExecuteTool(Tool, Metal, Process);
        if (Status != EMetalLibraryFinalizeStatus::Success)
            return ProcessFailure(
                Status, "metal-finalize-metal-compile", Process);
        if (!std::filesystem::is_regular_file(Files.Air) ||
            std::filesystem::file_size(Files.Air) == 0)
            return Failure(
                EMetalLibraryFinalizeStatus::EmptyOutput,
                "metal-finalize-empty-air");
        Status = ExecuteTool(Tool, Metallib, Process);
        if (Status != EMetalLibraryFinalizeStatus::Success)
            return ProcessFailure(
                Status, "metal-finalize-metallib-link", Process);
        if (!std::filesystem::is_regular_file(Files.Library))
            return Failure(
                EMetalLibraryFinalizeStatus::EmptyOutput,
                "metal-finalize-missing-library");
        const Core::uint64 Size = std::filesystem::file_size(Files.Library);
        if (Size == 0 || Size > Request.MaxLibraryBytes ||
            Size > std::numeric_limits<Core::usize>::max())
            return Failure(
                Size == 0
                    ? EMetalLibraryFinalizeStatus::EmptyOutput
                    : EMetalLibraryFinalizeStatus::IoFailure,
                "metal-finalize-library-size");
        std::ifstream Input(Files.Library, std::ios::binary);
        Core::TArray<Core::uint8> Bytes{
            std::istreambuf_iterator<char>(Input),
            std::istreambuf_iterator<char>()};
        if (!Input.good() && !Input.eof())
            return Failure(
                EMetalLibraryFinalizeStatus::IoFailure,
                "metal-finalize-library-read");
        if (Bytes.size() != Size)
            return Failure(
                EMetalLibraryFinalizeStatus::IoFailure,
                "metal-finalize-library-short-read");

        FMetalShaderEvidence Native = Request.DerivationEvidence;
        Native.Kind = EMetalShaderEvidenceKind::NativeLibrary;
        Native.NativeLibrary = FMetalNativeLibraryEvidence{
            Request.Architecture,
            Toolchain.MetalCompiler,
            Toolchain.XcodeBuild,
            Toolchain.Sdk,
            ArgumentDigest(Metal, Metallib, Root),
            Asset::FAssetDigest::FromBytes(Bytes),
            Size};
        if (FinalizeMetalShaderEvidence(Native) !=
            Asset::EAssetResult::Success)
            return Failure(
                EMetalLibraryFinalizeStatus::EvidenceMismatch,
                "metal-finalize-native-evidence");

        FMetalLibraryCompileResult Result;
        Result.Status = EMetalLibraryFinalizeStatus::Success;
        Result.LibraryBytes = std::move(Bytes);
        Result.NativeEvidence = std::move(Native);
        Result.Toolchain = std::move(Toolchain);
        Result.StableReason = Core::FString("metal-finalize-success");
        return Result;
    }
    catch (const std::bad_alloc&)
    {
        return Failure(
            EMetalLibraryFinalizeStatus::IoFailure,
            "metal-finalize-capacity");
    }
    catch (const std::filesystem::filesystem_error&)
    {
        return Failure(
            EMetalLibraryFinalizeStatus::IoFailure,
            "metal-finalize-filesystem");
    }
#endif
}

} // namespace Stoner::AssetCooker::Private
