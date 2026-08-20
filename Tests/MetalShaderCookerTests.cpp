#include "MetalShaderCookerTests.h"

#include "Asset/AssetMinimal.h"
#include "Core/SGPlatform.h"
#include "FMetalShaderCooker.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string_view>

namespace
{

using namespace Stoner;
using namespace Stoner::Asset;
using namespace Stoner::AssetCooker::Private;
using namespace Stoner::Core;

void Record(FMetalShaderCookerTestResult& Result, bool Passed, const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

TArray<uint8> Read(const char* Path)
{
    std::ifstream Input(Path, std::ios::binary);
    return {std::istreambuf_iterator<char>(Input),
            std::istreambuf_iterator<char>()};
}

FAssetId Id(const char* Type, const char* Path, const char* Subresource = nullptr)
{
    FAssetId Value;
    std::optional<FString> Sub;
    if (Subresource) Sub = FString(Subresource);
    (void)FAssetId::Create(FString(Type), FString(Path), Sub, Value);
    return Value;
}

FAssetParticipantId Participant(const char* Text)
{
    FAssetParticipantId Value;
    (void)FAssetParticipantId::Create(FString(Text), Value);
    return Value;
}

FAssetProducerVersion Producer(const char* Text)
{
    FAssetProducerVersion Value;
    (void)FAssetProducerVersion::Create(FString(Text), Value);
    return Value;
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

bool ParseProfile(FAssetTargetProfileEvidence& Out)
{
    const char* Profile = nullptr;
    if (std::string_view(HostArchitecture()) == "arm64")
        Profile = "Config/AssetCooker/Profiles/Mac-Metal-Arm64.json";
    else if (std::string_view(HostArchitecture()) == "x86_64")
        Profile = "Config/AssetCooker/Profiles/Mac-Metal-X86_64.json";
    if (!Profile) return false;
    const TArray<uint8> Bytes = Read(Profile);
    return FAssetCookContractCodec::ParseTargetProfile(Bytes, Out) ==
        EAssetResult::Success;
}

bool MakePayload(FShaderPayloadAsset& Out)
{
    TArray<uint8> Bytes =
        Read("Content/Shaders/Triangle/Triangle.vert.spv");
    FAssetVersion Version;
    Version.SourceDigest = FAssetDigest::FromBytes(Bytes);
    Version.ContentDigest = Version.SourceDigest;
    return FShaderPayloadAsset::Create(
        Id("ShaderPayload", "Engine/Shaders/Triangle",
            "payload.vulkan.vertex"),
        Version, EShaderBackendFamily::Vulkan, FString("vulkan-1.3"),
        EShaderPayloadFormat::SPIRV, EShaderStage::Vertex,
        FString("main"), {}, std::move(Bytes), Out) == EAssetResult::Success;
}

class FFakeExecutor final : public IMetalToolExecutor
{
public:
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
        if (Index == 6)
        {
            WriteOutput(Request, "AIR");
            return Success();
        }
        if (Index == 7)
        {
            WriteOutput(Request, "MTLB");
            return Success();
        }
        return Success();
    }

    TArray<FProcessExecutionRequest> Requests;

private:
    static FProcessExecutionResult Success(const char* Output = "")
    {
        FProcessExecutionResult Result;
        Result.Status = EProcessExecutionStatus::Completed;
        Result.ExitCode = 0;
        Result.StandardOutput = FString(Output);
        return Result;
    }

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
};

FMetalShaderCookParameters Parameters(
    const std::filesystem::path& Root,
    IMetalToolExecutor* Executor)
{
    FMetalShaderCookParameters Value;
    Value.ShaderAssetId = Id(
        "ShaderProgram", "Engine/Shaders/Triangle");
    Value.ShaderAssetVersion = FAssetDigest::FromBytes(
        std::span<const uint8>(
            reinterpret_cast<const uint8*>("program-v1"), 10));
    Value.GlslDigest = FAssetDigest::FromBytes(
        std::span<const uint8>(
            reinterpret_cast<const uint8*>("glsl-v1"), 7));
    Value.WorkingDirectory = FString(Root.generic_string());
    Value.Architecture = FString(HostArchitecture());
    Value.ToolchainEvidence = {
        FString(
            "/Applications/Xcode/metal\n"
            "Metal version 32023.98\n"
            "/Applications/Xcode/metallib\n"
            "metallib version 32023.98"),
        FString("Xcode 16.4\nBuild version 16F6"),
        FString("15.5")};
    Value.ToolExecutor = Executor;
    return Value;
}

void TestRegistrationAndProjection(FMetalShaderCookerTestResult& Result)
{
    FAssetTargetProfileEvidence Profile;
    FMetalShaderCooker Cooker;
    FAssetProfileProjectionEvidence Projection;
    Record(
        Result,
        ParseProfile(Profile) &&
            Cooker.GetCapability().Participant ==
                FMetalShaderCooker::ParticipantId() &&
            Cooker.GetCapability().ProducerVersion ==
                FMetalShaderCooker::ProducerVersion() &&
            Cooker.GetRelevantProfileEvidence(Profile, Projection) ==
                EAssetResult::Success &&
            Projection.Validate() == EAssetResult::Success,
        "Metal shader cooker exposes deterministic registration and profile projection");
}

void TestDeterminismAndKeyEvidence(FMetalShaderCookerTestResult& Result)
{
    FAssetTargetProfileEvidence Profile;
    FShaderPayloadAsset Payload;
    const auto Root = std::filesystem::temp_directory_path() /
        "stoner-metal-shader-cooker-determinism";
    const auto Params = Parameters(Root, nullptr);
    FSpirvCrossMslResult Baseline;
    TArray<FAssetDerivedNamedEvidence> Evidence;
    bool Stable = ParseProfile(Profile) && MakePayload(Payload) &&
        BuildMetalShaderDerivedEvidence(
            Payload, Params, Profile, Baseline, Evidence) ==
            EAssetResult::Success && Evidence.size() == 7;
    for (int Run = 1; Run < 20 && Stable; ++Run)
    {
        FSpirvCrossMslResult Candidate;
        TArray<FAssetDerivedNamedEvidence> CandidateEvidence;
        Stable = BuildMetalShaderDerivedEvidence(
                Payload, Params, Profile, Candidate,
                CandidateEvidence) == EAssetResult::Success &&
            Candidate.NormalizedMsl == Baseline.NormalizedMsl &&
            Candidate.NormalizedMslDigest == Baseline.NormalizedMslDigest &&
            CandidateEvidence == Evidence;
    }
    auto ChangedSource = Params;
    ChangedSource.GlslDigest = FAssetDigest::FromBytes(
        std::span<const uint8>(
            reinterpret_cast<const uint8*>("glsl-v2"), 7));
    FSpirvCrossMslResult ChangedSourceDerivation;
    TArray<FAssetDerivedNamedEvidence> ChangedSourceEvidence;
    const bool SourceInvalidates = BuildMetalShaderDerivedEvidence(
            Payload, ChangedSource, Profile, ChangedSourceDerivation,
            ChangedSourceEvidence) == EAssetResult::Success &&
        ChangedSourceDerivation.NormalizedMsl == Baseline.NormalizedMsl &&
        ChangedSourceEvidence != Evidence;
    auto ChangedHost = Params;
    ChangedHost.ToolchainEvidence.Sdk = FString("15.6");
    FSpirvCrossMslResult ChangedHostDerivation;
    TArray<FAssetDerivedNamedEvidence> ChangedHostEvidence;
    const bool HostTupleOnlyInvalidatesFinalKey =
        BuildMetalShaderDerivedEvidence(
            Payload, ChangedHost, Profile, ChangedHostDerivation,
            ChangedHostEvidence) == EAssetResult::Success &&
        ChangedHostDerivation.NormalizedMsl == Baseline.NormalizedMsl &&
        ChangedHostDerivation.NormalizedMslDigest ==
            Baseline.NormalizedMslDigest &&
        ChangedHostEvidence != Evidence;
    FAssetDerivedKeyEvidence KeyEvidence;
    KeyEvidence.KeyFormatVersion =
        FAssetDerivedKeyEvidence::CurrentKeyFormatVersion;
    KeyEvidence.AssetId = Payload.GetId();
    KeyEvidence.SourceVersion = Payload.GetVersion().SourceDigest;
    FAssetSourceLocator Source;
    (void)FAssetSourceLocator::Create(
        FString("content"), FString("Triangle.vert.spv"), Source);
    KeyEvidence.SourceManifest = {{Source, KeyEvidence.SourceVersion}};
    KeyEvidence.ImporterId = Participant("stoner.material-shader.dependency");
    KeyEvidence.ImporterVersion = Producer("023-v1");
    KeyEvidence.CookerId = FMetalShaderCooker::ParticipantId();
    KeyEvidence.CookerVersion = FMetalShaderCooker::ProducerVersion();
    KeyEvidence.CodecId = Participant("stoner.shader-payload");
    KeyEvidence.CodecVersion = Producer("2");
    KeyEvidence.PayloadSchemaVersion = 2;
    KeyEvidence.EffectiveSettingsDigest = Profile.EffectiveProfileDigest;
    KeyEvidence.RelevantProfileDigest = Profile.EffectiveProfileDigest;
    KeyEvidence.AdditionalEvidence = Evidence;
    FAssetDerivedKey First;
    const bool KeyBuilt = FAssetCookContractCodec::BuildDerivedKey(
        KeyEvidence, First) == EAssetResult::Success;
    Record(
        Result, Stable && SourceInvalidates && HostTupleOnlyInvalidatesFinalKey &&
            KeyBuilt && First.IsValid(),
        "Metal derivation and complete v2 key evidence are stable across twenty runs");
}

void TestCook(FMetalShaderCookerTestResult& Result)
{
    FAssetTargetProfileEvidence Profile;
    FShaderPayloadAsset Payload;
    const auto Root = std::filesystem::temp_directory_path() /
        "stoner-metal-shader-cooker";
    std::error_code Error;
    std::filesystem::remove_all(Root, Error);
    std::filesystem::create_directories(Root, Error);
    FFakeExecutor Executor;
    auto Params = Core::MakeShared<FMetalShaderCookParameters>(
        Parameters(Root, &Executor));
    FAssetCookRequest Request;
    const bool Inputs = ParseProfile(Profile) && MakePayload(Payload);
    if (Inputs)
    {
        Request.Metadata.Id = Payload.GetId();
        Request.Metadata.Version = Payload.GetVersion();
        (void)FAssetSourceLocator::Create(
            FString("content"), FString("Triangle.vert.spv"),
            Request.Metadata.Source);
        Request.Metadata.Producer = Participant(
            "stoner.material-shader.dependency");
        Request.Metadata.ProducerVersion = Producer("023-v1");
        Request.Payload = Core::MakeShared<const FShaderPayloadAsset>(Payload);
        Request.TargetProfile = Profile.Profile.DisplayName;
        Request.TargetProfileEvidence =
            Core::MakeShared<const FAssetTargetProfileEvidence>(Profile);
        Request.Parameters = Params;
    }
    FMetalShaderCooker Cooker;
    const FAssetCookResult Cooked = Cooker.Cook(Request);
#if SG_PLATFORM_MAC
    Core::TSharedPtr<const FAssetPayload> Loaded;
    FAssetCookedPayloadEnvelope Envelope;
    const bool RoundTrip = Cooked.Result == EAssetResult::Success &&
        FAssetCookContractCodec::LoadTypedPayload(
            Cooked.Artifact, {}, Loaded, &Envelope) == EAssetResult::Success;
    const auto Metal =
        std::dynamic_pointer_cast<const FShaderPayloadAsset>(Loaded);
    const bool Passed = Inputs && RoundTrip && Metal &&
        Metal->GetBackend() == EShaderBackendFamily::Metal &&
        Metal->GetFormat() == EShaderPayloadFormat::MetalLibrary &&
            Metal->GetBytes() == TArray<uint8>({'M', 'T', 'L', 'B'}) &&
            Metal->GetNativeBindingEvidence() != nullptr &&
            Metal->GetNativeLibraryEvidence() != nullptr &&
            Metal->GetNativeLibraryEvidence()->LibraryDigest ==
                FAssetDigest::FromBytes(Metal->GetBytes()) &&
            Executor.Requests.size() == 8;
    if (!Passed)
    {
        std::cout << "  cook-result=" << static_cast<int>(Cooked.Result)
                  << " artifact-bytes=" << Cooked.Artifact.size()
                  << " round-trip=" << RoundTrip
                  << " loaded=" << static_cast<bool>(Loaded)
                  << " metal=" << static_cast<bool>(Metal)
                  << " requests=" << Executor.Requests.size() << '\n';
    }
    Record(
        Result, Passed,
        "macOS cooker finalizes and round-trips a strict MetalLibrary envelope");
#else
    Record(
        Result,
        Inputs && Cooked.Result == EAssetResult::TargetUnavailable &&
            Cooked.Artifact.empty() && Executor.Requests.empty(),
        "non-macOS cooker rejects finalization without publishing an artifact");
#endif
    std::filesystem::remove_all(Root, Error);
}

} // namespace

FMetalShaderCookerTestResult RunMetalShaderCookerTests()
{
    FMetalShaderCookerTestResult Result;
    TestRegistrationAndProjection(Result);
    TestDeterminismAndKeyEvidence(Result);
    TestCook(Result);
    return Result;
}
