#include "MetalShaderCompilerTests.h"

#include "Asset/AssetMinimal.h"
#include "Core/SGPlatform.h"
#include "FMetalLibraryCompiler.h"
#include "FSpirvCrossMslDeriver.h"

#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace
{

using namespace Stoner;
using namespace Stoner::Asset;
using namespace Stoner::AssetCooker::Private;
using namespace Stoner::Core;

void Record(FMetalShaderCompilerTestResult& Result, bool Passed, const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

TArray<uint8> ReadBytes(const char* Path)
{
    std::ifstream Input(Path, std::ios::binary);
    return {std::istreambuf_iterator<char>(Input),
            std::istreambuf_iterator<char>()};
}

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

bool BuildDerivation(FString& OutMsl, FMetalShaderEvidence& OutEvidence)
{
    const TArray<uint8> Spirv =
        ReadBytes("Content/Shaders/Triangle/Triangle.vert.spv");
    FSpirvCrossMslRequest Request;
    Request.SpirvBytes = Spirv;
    Request.Stage = EShaderStage::Vertex;
    Request.EntryPoint = FString("main");
    FSpirvCrossMslResult Derived;
    if (DeriveMetalShaderSource(Request, Derived) != EAssetResult::Success)
        return false;

    FAssetId Id;
    if (FAssetId::Create(
            FString("ShaderProgram"),
            FString("Engine/Shaders/Triangle/Vertex"), {}, Id) !=
        EAssetResult::Success)
        return false;

    OutMsl = Derived.NormalizedMsl;
    OutEvidence = {};
    OutEvidence.ShaderAssetId = std::move(Id);
    OutEvidence.ShaderAssetVersion = Derived.SpirvDigest;
    OutEvidence.SpirvDigest = Derived.SpirvDigest;
    OutEvidence.Stage = EShaderStage::Vertex;
    OutEvidence.EntryPoint = FString("main");
    OutEvidence.InterfaceDigest = Derived.InterfaceDigest;
    OutEvidence.SpirvCrossOptionsDigest = Derived.OptionsDigest;
    OutEvidence.BindingEvidence = Derived.BindingEvidence;
    OutEvidence.TargetProfile = FString(
        std::string("metal-macos-12-") + HostArchitecture());
    OutEvidence.NormalizedMslDigest = Derived.NormalizedMslDigest;
    return FinalizeMetalShaderEvidence(OutEvidence) == EAssetResult::Success;
}

FProcessExecutionResult Success(const char* Output = "")
{
    FProcessExecutionResult Result;
    Result.Status = EProcessExecutionStatus::Completed;
    Result.ExitCode = 0;
    Result.StandardOutput = FString(Output);
    return Result;
}

class FFakeMetalExecutor final : public IMetalToolExecutor
{
public:
    enum class EMode
    {
        Success,
        CompileTimeout,
        CompileFailure,
        EmptyAir,
        EmptyLibrary
    };

    explicit FFakeMetalExecutor(EMode InMode = EMode::Success) : Mode(InMode) {}

    FProcessExecutionResult Execute(
        const FProcessExecutionRequest& Request) override
    {
        Requests.push_back(Request);
        const usize Index = Requests.size() - 1;
        if (Index == 0) return Success("/Applications/Xcode/metal\n");
        if (Index == 1) return Success("Metal version 32023.98\n");
        if (Index == 2) return Success("/Applications/Xcode/metallib\n");
        if (Index == 3) return Success("metallib version 32023.98\n");
        if (Index == 4) return Success("Xcode 16.4\nBuild version 16F6\n");
        if (Index == 5) return Success("15.5\n");
        if (Index == 6 && Mode == EMode::CompileTimeout)
        {
            FProcessExecutionResult Result;
            Result.Status = EProcessExecutionStatus::TimedOut;
            return Result;
        }
        if (Index == 6 && Mode == EMode::CompileFailure)
        {
            FProcessExecutionResult Result;
            Result.Status = EProcessExecutionStatus::Completed;
            Result.ExitCode = 2;
            Result.StandardError = FString("synthetic compiler failure");
            return Result;
        }
        if (Index == 6)
        {
            if (Mode != EMode::EmptyAir) WriteOutput(Request, "AIR");
            return Success();
        }
        if (Index == 7)
        {
            if (Mode != EMode::EmptyLibrary) WriteOutput(Request, "MTLB");
            return Success();
        }
        return Success();
    }

    std::vector<FProcessExecutionRequest> Requests;

private:
    static void WriteOutput(
        const FProcessExecutionRequest& Request,
        const char* Bytes)
    {
        for (usize Index = 0; Index + 1 < Request.Arguments.size(); ++Index)
        {
            if (Request.Arguments[Index] != FString("-o")) continue;
            std::ofstream Output(
                Request.Arguments[Index + 1].ToStdString(),
                std::ios::binary);
            Output << Bytes;
            return;
        }
    }

    EMode Mode;
};

bool HasArgument(const FProcessExecutionRequest& Request, const char* Expected)
{
    for (const FString& Argument : Request.Arguments)
    {
        if (Argument == FString(Expected)) return true;
    }
    return false;
}

FMetalLibraryCompileRequest MakeRequest(
    const std::filesystem::path& Root,
    const FString& Msl,
    const FMetalShaderEvidence& Evidence)
{
    FMetalLibraryCompileRequest Request;
    Request.WorkingDirectory = FString(Root.string());
    Request.Architecture = FString(HostArchitecture());
    Request.TargetProfile = Evidence.TargetProfile;
    Request.NormalizedMsl = Msl;
    Request.DerivationEvidence = Evidence;
    Request.TimeoutMilliseconds = 2000;
    return Request;
}

void TestDoctor(FMetalShaderCompilerTestResult& Result)
{
    FFakeMetalExecutor Executor;
    FMetalToolchainEvidence Evidence;
    const auto Status = InspectMetalToolchain(Executor, 2000, 4096, Evidence);
#if SG_PLATFORM_MAC
    Record(Result,
        Status == EMetalLibraryFinalizeStatus::Success &&
            Evidence.IsValid() && Executor.Requests.size() == 6 &&
            Executor.Requests[0].ExecutablePath == FString("/usr/bin/xcrun") &&
            HasArgument(Executor.Requests[0], "--find") &&
            HasArgument(Executor.Requests[1], "metal") &&
            HasArgument(Executor.Requests[2], "metallib") &&
            HasArgument(Executor.Requests[3], "--version") &&
            HasArgument(Executor.Requests[4], "xcodebuild") &&
            HasArgument(Executor.Requests[5], "--show-sdk-version"),
        "toolchain doctor uses bounded explicit xcrun argv and records evidence");
#else
    Record(Result,
        Status == EMetalLibraryFinalizeStatus::HostUnsupported &&
            Executor.Requests.empty() && !Evidence.IsValid(),
        "toolchain doctor fails closed without invoking tools off macOS");
#endif
}

void TestFinalization(FMetalShaderCompilerTestResult& Result)
{
    FString Msl;
    FMetalShaderEvidence Evidence;
    const bool Derived = BuildDerivation(Msl, Evidence);
    const std::filesystem::path Root =
        std::filesystem::temp_directory_path() /
        "stoner-metal-shader-compiler-tests";
    std::error_code Error;
    std::filesystem::remove_all(Root, Error);
    std::filesystem::create_directories(Root, Error);
    FFakeMetalExecutor Executor;
    const FMetalLibraryCompileResult Finalized = FinalizeMetalLibrary(
        MakeRequest(Root, Msl, Evidence), &Executor);
#if SG_PLATFORM_MAC
    const TArray<uint8> Expected = {'M', 'T', 'L', 'B'};
    const bool Clean =
        !std::filesystem::exists(Root / "stoner-metal-input.metal") &&
        !std::filesystem::exists(Root / "stoner-metal-output.air") &&
        !std::filesystem::exists(Root / "stoner-metal-output.metallib");
    Record(Result,
        Derived && Finalized.Succeeded() &&
            Finalized.LibraryBytes == Expected &&
            Finalized.NativeEvidence.IsValid() &&
            Finalized.NativeEvidence.NativeLibrary.has_value() &&
            Finalized.NativeEvidence.NativeLibrary->SizeBytes == 4 &&
            Executor.Requests.size() == 8 &&
            HasArgument(Executor.Requests[6], "metal") &&
            HasArgument(Executor.Requests[6], "-std=macos-metal2.4") &&
            HasArgument(Executor.Requests[6], "-mmacosx-version-min=12.0") &&
            HasArgument(Executor.Requests[7], "metallib") && Clean,
        "finalizer emits validated native evidence and cleans temporary files");
#else
    Record(Result,
        Derived &&
            Finalized.Status == EMetalLibraryFinalizeStatus::HostUnsupported &&
            Executor.Requests.empty(),
        "native finalization is HostUnsupported off macOS");
#endif
    std::filesystem::remove_all(Root, Error);
}

void TestFailures(FMetalShaderCompilerTestResult& Result)
{
#if SG_PLATFORM_MAC
    FString Msl;
    FMetalShaderEvidence Evidence;
    const bool Derived = BuildDerivation(Msl, Evidence);
    const std::filesystem::path Root =
        std::filesystem::temp_directory_path() /
        "stoner-metal-shader-compiler-failure-tests";
    const auto Run = [&](FFakeMetalExecutor::EMode Mode)
    {
        std::error_code Error;
        std::filesystem::remove_all(Root, Error);
        std::filesystem::create_directories(Root, Error);
        FFakeMetalExecutor Executor(Mode);
        return FinalizeMetalLibrary(
            MakeRequest(Root, Msl, Evidence), &Executor).Status;
    };
    const bool Timeout = Run(FFakeMetalExecutor::EMode::CompileTimeout) ==
        EMetalLibraryFinalizeStatus::TimedOut;
    const bool Failure = Run(FFakeMetalExecutor::EMode::CompileFailure) ==
        EMetalLibraryFinalizeStatus::CompilerFailed;
    const bool EmptyAir = Run(FFakeMetalExecutor::EMode::EmptyAir) ==
        EMetalLibraryFinalizeStatus::EmptyOutput;
    const bool EmptyLibrary = Run(FFakeMetalExecutor::EMode::EmptyLibrary) ==
        EMetalLibraryFinalizeStatus::EmptyOutput;
    FMetalLibraryCompileRequest Mismatch = MakeRequest(Root, Msl, Evidence);
    Mismatch.NormalizedMsl = FString("// changed\n");
    FFakeMetalExecutor Executor;
    const bool EvidenceRejected = FinalizeMetalLibrary(Mismatch, &Executor).Status ==
        EMetalLibraryFinalizeStatus::EvidenceMismatch;
    Record(Result,
        Derived && Timeout && Failure && EmptyAir && EmptyLibrary &&
            EvidenceRejected,
        "finalizer propagates timeout failure empty output and evidence mismatch");
    std::error_code Error;
    std::filesystem::remove_all(Root, Error);
#else
    Record(Result, true, "macOS compiler failure matrix is host-gated");
#endif
}

void TestNativeFinalizer(FMetalShaderCompilerTestResult& Result)
{
#if SG_PLATFORM_MAC
    const char* Enabled = std::getenv("STONER_METAL_NATIVE_COMPILER");
    if (Enabled == nullptr || std::string_view(Enabled) != "1")
    {
        std::cout << "[INFO] real Metal compiler gate not requested\n";
        return;
    }
    FString Msl;
    FMetalShaderEvidence Evidence;
    const bool Derived = BuildDerivation(Msl, Evidence);
    const std::filesystem::path Root =
        std::filesystem::temp_directory_path() /
        "stoner-metal-native-compiler-test";
    std::error_code Error;
    std::filesystem::remove_all(Root, Error);
    std::filesystem::create_directories(Root, Error);
    const FMetalLibraryCompileResult Finalized = FinalizeMetalLibrary(
        MakeRequest(Root, Msl, Evidence));
    Record(Result,
        Derived && Finalized.Succeeded() &&
            !Finalized.LibraryBytes.empty() &&
            Finalized.NativeEvidence.IsValid() &&
            Finalized.Toolchain.IsValid(),
        "real xcrun metal and metallib finalization produces native evidence");
    if (!Finalized.Succeeded())
    {
        std::cout << "[INFO] native finalizer reason: "
                  << Finalized.StableReason.CStr() << '\n'
                  << "[INFO] native finalizer stdout: "
                  << Finalized.ToolStandardOutput.CStr() << '\n'
                  << "[INFO] native finalizer stderr: "
                  << Finalized.ToolStandardError.CStr() << '\n';
    }
    std::filesystem::remove_all(Root, Error);
#else
    (void)Result;
#endif
}

} // namespace

FMetalShaderCompilerTestResult RunMetalShaderCompilerTests()
{
    FMetalShaderCompilerTestResult Result;
    TestDoctor(Result);
    TestFinalization(Result);
    TestFailures(Result);
    TestNativeFinalizer(Result);
    return Result;
}
