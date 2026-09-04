#include "MetalShaderDerivationTests.h"

#include "Asset/AssetMinimal.h"
#include "Core/SGPlatform.h"
#include "FMetalBindingMap.h"
#include "FMetalLibraryCompiler.h"
#include "FMetalShaderEvidenceCodec.h"
#include "FSpirvCrossMslDeriver.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>

namespace
{

using namespace Stoner;
using namespace Stoner::Asset;
using namespace Stoner::AssetCooker::Private;
using namespace Stoner::Core;

#if SG_PLATFORM_MAC
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
#endif

void Record(
    FMetalShaderDerivationTestResult& Result,
    bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

TArray<uint8> ReadBytes(const char* Path)
{
    std::ifstream Input(Path, std::ios::binary);
    return {
        std::istreambuf_iterator<char>(Input),
        std::istreambuf_iterator<char>()};
}

FAssetId Id(const char* Path)
{
    FAssetId Value;
    (void)FAssetId::Create(
        FString("ShaderProgram"), FString(Path), {}, Value);
    return Value;
}

FShaderInterfaceBinding Binding(
    uint32 Set,
    uint32 Slot,
    EShaderResourceKind Kind,
    uint32 Count,
    EShaderStage Stage)
{
    FShaderInterfaceBinding Value;
    Value.SetIndex = Set;
    Value.BindingIndex = Slot;
    Value.Kind = Kind;
    Value.ArrayCount = Count;
    Value.Visibility = {Stage};
    return Value;
}

void TestBindingPolicy(FMetalShaderDerivationTestResult& Result)
{
    const TArray<FShaderInterfaceBinding> Bindings = {
        Binding(1, 3, EShaderResourceKind::UniformBuffer, 2,
            EShaderStage::Vertex),
        Binding(0, 2, EShaderResourceKind::Sampler, 1,
            EShaderStage::Vertex),
        Binding(0, 1, EShaderResourceKind::SampledTexture, 1,
            EShaderStage::Vertex)};
    FShaderNativeBindingEvidence Evidence;
    const EAssetResult Build = BuildMetalBindingMap(
        {EShaderStage::Vertex, Bindings, {}}, Evidence);
    Record(
        Result,
        Build == EAssetResult::Success &&
            Evidence.Validate() == EAssetResult::Success &&
            Evidence.Entries.size() == 4 &&
            Evidence.Entries[0].SetIndex == 0 &&
            Evidence.Entries[0].BindingIndex == 1 &&
            Evidence.Entries[0].NativeClass ==
                EShaderNativeResourceClass::Texture &&
            Evidence.Entries[0].NativeIndex == 0 &&
            Evidence.Entries[1].NativeClass ==
                EShaderNativeResourceClass::Sampler &&
            Evidence.Entries[2].NativeIndex == 1 &&
            Evidence.Entries[3].NativeIndex == 2 &&
            Evidence.ReservedRanges.size() == 2,
        "binding policy deterministically maps stage set binding and arrays");

    TArray<FShaderInterfaceBinding> Collision = Bindings;
    Collision.push_back(Binding(
        0, 1, EShaderResourceKind::StorageTexture, 1,
        EShaderStage::Vertex));
    FShaderNativeBindingEvidence Rejected = Evidence;
    const bool CollisionRejected = BuildMetalBindingMap(
        {EShaderStage::Vertex, Collision, {}}, Rejected) ==
            EAssetResult::Conflict &&
        Rejected.Entries.empty();
    const TArray<FShaderInterfaceBinding> Buffer = {Binding(
        0, 0, EShaderResourceKind::UniformBuffer, 1,
        EShaderStage::Vertex)};
    FMetalBindingLimits Tight;
    Tight.MaxBufferBindings = 2;
    const bool LimitRejected = BuildMetalBindingMap(
        {EShaderStage::Vertex, Buffer, Tight}, Rejected) ==
            EAssetResult::CapacityExceeded &&
        Rejected.Entries.empty();
    Record(
        Result,
        CollisionRejected && LimitRejected,
        "binding policy rejects source collisions and reserved-limit overflow");
}

void TestDerivation(FMetalShaderDerivationTestResult& Result)
{
    const TArray<uint8> Spirv =
        ReadBytes("Content/Shaders/Triangle/Triangle.vert.spv");
    FSpirvCrossMslRequest Request;
    Request.SpirvBytes = Spirv;
    Request.Stage = EShaderStage::Vertex;
    Request.EntryPoint = FString("main");
    FSpirvCrossMslResult Baseline;
    const EAssetResult First = DeriveMetalShaderSource(Request, Baseline);
    bool Repeated = First == EAssetResult::Success && Baseline.IsValid();
    for (int Run = 1; Run < 20 && Repeated; ++Run)
    {
        FSpirvCrossMslResult Candidate;
        Repeated = DeriveMetalShaderSource(Request, Candidate) ==
                EAssetResult::Success &&
            Candidate.NormalizedMsl == Baseline.NormalizedMsl &&
            Candidate.NormalizedMslDigest ==
                Baseline.NormalizedMslDigest &&
            Candidate.BindingEvidence == Baseline.BindingEvidence;
    }
    Record(
        Result,
        Repeated && Baseline.NormalizedMsl.View().find("vertex") !=
                std::string_view::npos &&
            Baseline.NormalizedMsl.View().find("stoner_main") !=
                std::string_view::npos &&
            Baseline.NormalizedMsl.View().find("Invert Y-axis for Metal") !=
                std::string_view::npos &&
            Baseline.NormalizedMsl.View().back() == '\n',
        "SPIRV-Cross produces byte-stable Y-corrected MSL across twenty runs");
    if (Repeated)
        std::cout << "[EVIDENCE] metal-derivation triangle-vertex"
                  << " msl="
                  << Baseline.NormalizedMslDigest.ToLowerHex().ToStdString()
                  << " options="
                  << Baseline.OptionsDigest.ToLowerHex().ToStdString()
                  << " binding="
                  << Baseline.BindingEvidence.CanonicalDigest
                         .ToLowerHex().ToStdString()
                  << '\n';

    TArray<uint8> Malformed = Spirv;
    if (!Malformed.empty()) Malformed[0] = 0;
    Request.SpirvBytes = Malformed;
    FSpirvCrossMslResult Unchanged = Baseline;
    Record(
        Result,
        DeriveMetalShaderSource(Request, Unchanged) !=
                EAssetResult::Success &&
            !Unchanged.IsValid(),
        "malformed SPIR-V fails without partial MSL evidence");

    FString Normalized;
    Record(
        Result,
        NormalizeMetalShaderSource(
            "line  \r\n// stoner-volatile: /tmp/build.metal\rnext\t\n",
            Normalized) == EAssetResult::Success &&
            Normalized == FString("line\nnext\n"),
        "source normalization removes volatile lines whitespace and CRLF");
}

void TestReflectedBindingAndEvidence(
    FMetalShaderDerivationTestResult& Result)
{
    const TArray<uint8> Spirv =
        ReadBytes("Content/Shaders/Deferred/Surface.frag.spv");
    const TArray<FShaderInterfaceBinding> Interface = {
        Binding(1, 0, EShaderResourceKind::UniformBuffer, 1,
            EShaderStage::Fragment),
        Binding(1, 1, EShaderResourceKind::CombinedTextureSampler, 1,
            EShaderStage::Fragment),
        Binding(1, 2, EShaderResourceKind::CombinedTextureSampler, 1,
            EShaderStage::Fragment),
        Binding(1, 3, EShaderResourceKind::CombinedTextureSampler, 1,
            EShaderStage::Fragment),
        Binding(1, 4, EShaderResourceKind::CombinedTextureSampler, 1,
            EShaderStage::Fragment),
        Binding(1, 5, EShaderResourceKind::CombinedTextureSampler, 1,
            EShaderStage::Fragment)};
    FSpirvCrossMslRequest Request;
    Request.SpirvBytes = Spirv;
    Request.Stage = EShaderStage::Fragment;
    Request.EntryPoint = FString("main");
    Request.InterfaceBindings = Interface;
    FSpirvCrossMslResult Derived;
    const EAssetResult Derive = DeriveMetalShaderSource(Request, Derived);
    FMetalShaderEvidence Evidence;
    Evidence.ShaderAssetId = Id("Engine/Shaders/Deferred/Surface");
    Evidence.ShaderAssetVersion = Derived.SpirvDigest;
    Evidence.SpirvDigest = Derived.SpirvDigest;
    Evidence.Stage = EShaderStage::Fragment;
    Evidence.EntryPoint = FString("main");
    Evidence.InterfaceDigest = Derived.InterfaceDigest;
    Evidence.SpirvCrossOptionsDigest = Derived.OptionsDigest;
    Evidence.BindingEvidence = Derived.BindingEvidence;
    Evidence.TargetProfile = FString("metal-macos-12-arm64");
    Evidence.NormalizedMslDigest = Derived.NormalizedMslDigest;
    const EAssetResult Finalize = FinalizeMetalShaderEvidence(Evidence);
    FString Canonical;
    const EAssetResult Write = WriteMetalShaderEvidence(Evidence, Canonical);
    Record(
        Result,
        Derive == EAssetResult::Success && Derived.IsValid() &&
            Derived.BindingEvidence.Entries.size() == 11 &&
            Derived.BindingEvidence.Entries.front().SetIndex == 1 &&
            Derived.BindingEvidence.Entries.front().BindingIndex == 0 &&
            Finalize == EAssetResult::Success &&
            Write == EAssetResult::Success &&
            Canonical.View().find("metal-direct-binding-v1") !=
                std::string_view::npos &&
            Canonical.View().find(
                "a0fba56c34a6700f1724bf9b751da5b488a3775c") !=
                std::string_view::npos,
        "reflection validates declared interface and writes canonical evidence");
    if (Write == EAssetResult::Success)
        std::cout << "[EVIDENCE] metal-derivation surface-fragment"
                  << " msl="
                  << Derived.NormalizedMslDigest.ToLowerHex().ToStdString()
                  << " evidence="
                  << Evidence.EvidenceDigest.ToLowerHex().ToStdString()
                  << " binding="
                  << Derived.BindingEvidence.CanonicalDigest
                         .ToLowerHex().ToStdString()
                  << '\n';

    Request.InterfaceBindings = {};
    FSpirvCrossMslResult MissingInterface = Derived;
    Record(
        Result,
        DeriveMetalShaderSource(Request, MissingInterface) ==
                EAssetResult::DependencyMismatch &&
            !MissingInterface.IsValid(),
        "reflection rejects undeclared SPIR-V resource bindings");

    FMetalShaderEvidence Tampered = Evidence;
    Tampered.TargetProfile = FString("metal-other");
    Canonical = FString("unchanged");
    Record(
        Result,
        WriteMetalShaderEvidence(Tampered, Canonical) !=
                EAssetResult::Success &&
            Canonical.IsEmpty(),
        "evidence digest rejects identity-bearing field mutation");
}

void TestDeferredCombinedTextureSamplers(
    FMetalShaderDerivationTestResult& Result)
{
    const TArray<uint8> Spirv = ReadBytes(
        "Content/Shaders/Deferred/DirectionalLight.frag.spv");
    const TArray<FShaderInterfaceBinding> Interface = {
        Binding(2, 0, EShaderResourceKind::CombinedTextureSampler, 1,
            EShaderStage::Fragment),
        Binding(2, 1, EShaderResourceKind::CombinedTextureSampler, 1,
            EShaderStage::Fragment),
        Binding(2, 2, EShaderResourceKind::CombinedTextureSampler, 1,
            EShaderStage::Fragment),
        Binding(2, 3, EShaderResourceKind::CombinedTextureSampler, 1,
            EShaderStage::Fragment),
        Binding(3, 0, EShaderResourceKind::StorageBuffer, 1,
            EShaderStage::Fragment)};
    FSpirvCrossMslRequest Request;
    Request.SpirvBytes = Spirv;
    Request.Stage = EShaderStage::Fragment;
    Request.EntryPoint = FString("main");
    Request.InterfaceBindings = Interface;
    FSpirvCrossMslResult Derived;
    const EAssetResult Derive = DeriveMetalShaderSource(Request, Derived);

    bool bCompletePairs = Derived.BindingEvidence.Entries.size() == 9;
    for (uint32 Slot = 0; Slot < 4 && bCompletePairs; ++Slot)
    {
        bool bTexture = false;
        bool bSampler = false;
        for (const auto& Entry : Derived.BindingEvidence.Entries)
        {
            if (Entry.SetIndex != 2 || Entry.BindingIndex != Slot ||
                Entry.DescriptorType !=
                    EShaderResourceKind::CombinedTextureSampler)
                continue;
            bTexture = bTexture || Entry.NativeClass ==
                EShaderNativeResourceClass::Texture;
            bSampler = bSampler || Entry.NativeClass ==
                EShaderNativeResourceClass::Sampler;
        }
        bCompletePairs = bCompletePairs && bTexture && bSampler;
    }
    Record(Result,
        Derive == EAssetResult::Success && Derived.IsValid() &&
            bCompletePairs &&
            Derived.NormalizedMsl.View().find("texture2d") !=
                std::string_view::npos &&
            Derived.NormalizedMsl.View().find("sampler") !=
                std::string_view::npos,
        "deferred combined samplers derive paired Metal texture and sampler bindings");
}

void TestOutputTransformDerivation(
    FMetalShaderDerivationTestResult& Result)
{
    const TArray<uint8> Spirv = ReadBytes(
        "Content/Shaders/PostProcess/OutputTransform.frag.spv");
    const TArray<FShaderInterfaceBinding> Interface = {
        Binding(0, 0, EShaderResourceKind::CombinedTextureSampler, 1,
            EShaderStage::Fragment),
        Binding(0, 1, EShaderResourceKind::UniformBuffer, 1,
            EShaderStage::Fragment)};
    FSpirvCrossMslRequest Request;
    Request.SpirvBytes = Spirv;
    Request.Stage = EShaderStage::Fragment;
    Request.EntryPoint = FString("main");
    Request.InterfaceBindings = Interface;
    FSpirvCrossMslResult Baseline;
    bool bStable = DeriveMetalShaderSource(Request, Baseline) ==
            EAssetResult::Success && Baseline.IsValid() &&
        Baseline.BindingEvidence.Entries.size() == 3;
    for (int Run = 1; Run < 20 && bStable; ++Run)
    {
        FSpirvCrossMslResult Candidate;
        bStable = DeriveMetalShaderSource(Request, Candidate) ==
                EAssetResult::Success && Candidate.IsValid() &&
            Candidate.SpirvDigest == Baseline.SpirvDigest &&
            Candidate.InterfaceDigest == Baseline.InterfaceDigest &&
            Candidate.OptionsDigest == Baseline.OptionsDigest &&
            Candidate.NormalizedMslDigest == Baseline.NormalizedMslDigest &&
            Candidate.NormalizedMsl == Baseline.NormalizedMsl &&
            Candidate.BindingEvidence == Baseline.BindingEvidence;
    }
    Record(Result, bStable &&
            Baseline.NormalizedMsl.View().find("fragment") !=
                std::string_view::npos &&
            Baseline.NormalizedMsl.View().find("stoner_main") !=
                std::string_view::npos &&
            Baseline.NormalizedMsl.View().find("texture2d") !=
                std::string_view::npos &&
            Baseline.NormalizedMsl.View().find("sampler") !=
                std::string_view::npos,
        "output transform SPIR-V derives stable bound MSL across twenty runs");
    if (bStable)
    {
        std::cout << "[EVIDENCE] metal-derivation output-transform-fragment"
                  << " spirv="
                  << Baseline.SpirvDigest.ToLowerHex().ToStdString()
                  << " msl="
                  << Baseline.NormalizedMslDigest.ToLowerHex().ToStdString()
                  << " options="
                  << Baseline.OptionsDigest.ToLowerHex().ToStdString()
                  << " binding="
                  << Baseline.BindingEvidence.CanonicalDigest
                         .ToLowerHex().ToStdString()
                  << '\n';
    }

#if SG_PLATFORM_MAC
    FAssetId ShaderId;
    const FString TargetProfile(
        std::string("metal-macos-12-") + HostArchitecture());
    FMetalShaderEvidence Evidence;
    const bool bEvidenceReady = bStable &&
        FAssetId::Create(FString("ShaderProgram"),
            FString("Engine/Shaders/PostProcess/OutputTransform"), {},
            ShaderId) == EAssetResult::Success;
    if (bEvidenceReady)
    {
        Evidence.ShaderAssetId = std::move(ShaderId);
        Evidence.ShaderAssetVersion = Baseline.SpirvDigest;
        Evidence.SpirvDigest = Baseline.SpirvDigest;
        Evidence.Stage = EShaderStage::Fragment;
        Evidence.EntryPoint = FString("main");
        Evidence.InterfaceDigest = Baseline.InterfaceDigest;
        Evidence.SpirvCrossOptionsDigest = Baseline.OptionsDigest;
        Evidence.BindingEvidence = Baseline.BindingEvidence;
        Evidence.TargetProfile = TargetProfile;
        Evidence.NormalizedMslDigest = Baseline.NormalizedMslDigest;
    }
    const std::filesystem::path Scratch =
        std::filesystem::temp_directory_path() /
        "stoner-output-transform-metallib-test";
    std::error_code Error;
    std::filesystem::remove_all(Scratch, Error);
    std::filesystem::create_directories(Scratch, Error);
    FMetalLibraryCompileRequest Compile;
    Compile.WorkingDirectory = FString(Scratch.string());
    Compile.Architecture = FString(HostArchitecture());
    Compile.TargetProfile = TargetProfile;
    Compile.NormalizedMsl = Baseline.NormalizedMsl;
    const bool bEvidenceFinalized = bEvidenceReady && Error.value() == 0 &&
        FinalizeMetalShaderEvidence(Evidence) == EAssetResult::Success;
    if (bEvidenceFinalized)
        Compile.DerivationEvidence = Evidence;
    const FMetalLibraryCompileResult Library =
        bEvidenceFinalized
        ? FinalizeMetalLibrary(Compile)
        : FMetalLibraryCompileResult{};
    Record(Result,
        Library.Succeeded() && !Library.LibraryBytes.empty() &&
            Library.NativeEvidence.IsValid(),
        "output transform MSL finalizes through offline metal and metallib");
    if (Library.Succeeded())
    {
        std::cout << "[EVIDENCE] metal-finalization output-transform-fragment"
                  << " metallib="
                  << Library.NativeEvidence.NativeLibrary->LibraryDigest
                         .ToLowerHex().ToStdString()
                  << " size="
                  << Library.NativeEvidence.NativeLibrary->SizeBytes
                  << " evidence="
                  << Library.NativeEvidence.EvidenceDigest
                         .ToLowerHex().ToStdString()
                  << '\n';
    }
    else
    {
        std::cout << "[EVIDENCE] metal-finalization-failure"
                  << " reason=" << Library.StableReason.ToStdString()
                  << " stderr=" << Library.ToolStandardError.ToStdString()
                  << '\n';
    }
    std::filesystem::remove_all(Scratch, Error);
#else
    Record(Result, bStable,
        "output transform metallib finalization is macOS-only");
#endif
}

} // namespace

FMetalShaderDerivationTestResult RunMetalShaderDerivationTests()
{
    FMetalShaderDerivationTestResult Result;
    TestBindingPolicy(Result);
    TestDerivation(Result);
    TestReflectedBindingAndEvidence(Result);
    TestDeferredCombinedTextureSamplers(Result);
    TestOutputTransformDerivation(Result);
    return Result;
}
