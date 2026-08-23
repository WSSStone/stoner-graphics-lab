#include "AssetCookerDeterminismTests.h"

#include "Asset/AssetMinimal.h"
#include "AssetCooker/FAssetCookRunner.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>

namespace
{
using namespace Stoner;

void Record(
    FAssetCookerDeterminismTestResult& Result,
    bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

Core::TArray<Core::uint8> Read(const std::filesystem::path& Path)
{
    std::ifstream Input(Path, std::ios::binary);
    return {
        std::istreambuf_iterator<char>(Input),
        std::istreambuf_iterator<char>()};
}

void Write(const std::filesystem::path& Path, const Core::TArray<Core::uint8>& Bytes)
{
    std::filesystem::create_directories(Path.parent_path());
    std::ofstream Output(Path, std::ios::binary);
    Output.write(reinterpret_cast<const char*>(Bytes.data()),
        static_cast<std::streamsize>(Bytes.size()));
}

AssetCooker::FAssetCookRequest Request(
    const std::filesystem::path& Base,
    const std::filesystem::path& Content,
    Core::uint32 Workers)
{
    AssetCooker::FAssetCookRequest Value;
    Value.SourceRoots = {Core::FString(Content.generic_string())};
    Value.SelectionMode = Asset::EAssetCookSelectionMode::CookAll;
    Value.TargetProfilePath = Core::FString(
        "Tests/Fixtures/AssetCooker/Contracts/Profiles/mac-vulkan.json");
    const auto ProfileBytes = Read(Value.TargetProfilePath.ToStdString());
    (void)Asset::FAssetCookContractCodec::ParseTargetProfile(
        ProfileBytes, Value.TargetProfile);
    Value.OutputRoot = Core::FString((Base / "Output").generic_string());
    Value.DerivedDataRoot = Core::FString((Base / "DDC").generic_string());
    Value.ScratchRoot = Core::FString((Base / "Scratch").generic_string());
    Value.WorkerCount = Workers;
    return Value;
}

struct FRun
{
    AssetCooker::FAssetCookResult Result;
    AssetCooker::FAssetCookReport Report;
};

FRun Cook(
    const std::filesystem::path& Base,
    const std::filesystem::path& Content,
    Core::uint32 Workers)
{
    FRun Run;
    Run.Result = AssetCooker::FAssetCookRunner::Run(
        Request(Base, Content, Workers), Run.Report);
    return Run;
}

bool EqualArtifacts(
    const Core::TArray<AssetCooker::FAssetCookArtifact>& Left,
    const Core::TArray<AssetCooker::FAssetCookArtifact>& Right)
{
    if (Left.size() != Right.size()) return false;
    for (Core::usize Index = 0; Index < Left.size(); ++Index)
        if (Left[Index].AssetId != Right[Index].AssetId ||
            Left[Index].RelativeLocator != Right[Index].RelativeLocator ||
            Left[Index].EnvelopeDigest != Right[Index].EnvelopeDigest ||
            Left[Index].Bytes != Right[Index].Bytes)
            return false;
    return true;
}

void TestCleanCook(FAssetCookerDeterminismTestResult& TestResult)
{
    const auto Root = std::filesystem::temp_directory_path() /
        "stoner-asset-cooker-clean";
    std::filesystem::remove_all(Root);
    const auto Content = Root / "Content";
    Write(Content / "Representative.png",
        Read("Tests/Fixtures/Images/Valid/png-rgb-3x5.png"));

    const FRun Serial = Cook(Root / "Serial", Content, 1);
    const FRun Parallel = Cook(Root / "Parallel", Content, 8);
    const bool Equal = Serial.Result.Succeeded() && Parallel.Result.Succeeded() &&
        Serial.Result.CanonicalManifest == Parallel.Result.CanonicalManifest &&
        Serial.Result.Manifest.GenerationId ==
            Parallel.Result.Manifest.GenerationId &&
        EqualArtifacts(Serial.Result.Artifacts, Parallel.Result.Artifacts);
    if (!Equal)
    {
        std::cout << "[DETAIL] serial-category="
                  << static_cast<int>(Serial.Result.Category)
                  << " serial-reason=" << Serial.Result.StableReason.ToStdString()
                  << " parallel-category="
                  << static_cast<int>(Parallel.Result.Category)
                  << " parallel-reason="
                  << Parallel.Result.StableReason.ToStdString()
                  << " serial-artifacts=" << Serial.Result.Artifacts.size()
                  << " parallel-artifacts=" << Parallel.Result.Artifacts.size()
                  << '\n';
    }
    Record(TestResult, Equal,
        "clean cook is byte-identical with one and eight workers");
    Record(TestResult,
        Equal && Serial.Report.Counts.DiscoveredSources == 1 &&
            Serial.Report.Counts.ReachableAssets == 2 &&
            Serial.Report.Counts.Cooked == 2 &&
            Serial.Report.Validate() == Asset::EAssetResult::Success,
        "clean report accounts for discovered, reachable, and cooked assets");

    bool Reloaded = Equal;
    for (const auto& Artifact : Serial.Result.Artifacts)
    {
        Core::TSharedPtr<const Asset::FAssetPayload> Payload;
        Asset::FAssetCookedPayloadEnvelope Envelope;
        Reloaded = Reloaded &&
            Asset::FAssetCookContractCodec::LoadTypedPayload(
                Artifact.Bytes, {}, Payload, &Envelope) ==
                Asset::EAssetResult::Success && Payload &&
            Payload->GetAssetType() == Artifact.AssetId.GetAssetType() &&
            Envelope.Header.AssetId == Artifact.AssetId;
        const auto Physical =
            std::filesystem::path(Serial.Result.GenerationImageRoot.ToStdString()) /
            Artifact.RelativeLocator.ToStdString();
        Reloaded = Reloaded && Read(Physical) == Artifact.Bytes;
    }
    Record(TestResult, Reloaded,
        "generation image payloads typed-load and match returned artifacts");

    bool Repeated = Equal;
    for (int Index = 0; Index < 20 && Repeated; ++Index)
    {
        const FRun Current = Cook(
            Root / ("Repeat-" + std::to_string(Index)), Content,
            Index % 2 == 0 ? 1U : 8U);
        Repeated = Current.Result.Succeeded() &&
            Current.Result.CanonicalManifest == Serial.Result.CanonicalManifest &&
            EqualArtifacts(Current.Result.Artifacts, Serial.Result.Artifacts);
    }
    Record(TestResult, Repeated,
        "twenty clean cooks preserve normalized bytes and generation identity");
    std::filesystem::remove_all(Root);
}

void TestPlanOnly(FAssetCookerDeterminismTestResult& TestResult)
{
    const auto Root = std::filesystem::temp_directory_path() /
        "stoner-asset-cooker-plan";
    std::filesystem::remove_all(Root);
    const auto Content = Root / "Content";
    Write(Content / "Representative.png",
        Read("Tests/Fixtures/Images/Valid/png-rgb-3x5.png"));
    auto Value = Request(Root / "Plan", Content, 4);
    Value.Mode = AssetCooker::EAssetCookRunMode::PlanOnly;
    Value.LeaseTimeout = std::chrono::milliseconds(0);
    AssetCooker::FAssetCookReport Report;
    const auto Planned = AssetCooker::FAssetCookRunner::Run(Value, Report);
    Record(TestResult,
        Planned.Succeeded() && Planned.Artifacts.empty() &&
            Planned.CanonicalManifest.IsEmpty() &&
            Report.Counts.ReachableAssets == 2 && Report.Counts.Cooked == 0,
        "plan-only computes closure without writing cooked artifacts");
    std::filesystem::remove_all(Root);
}

void TestRepresentativeCorpus(FAssetCookerDeterminismTestResult& TestResult)
{
    const auto Root = std::filesystem::temp_directory_path() /
        "stoner-asset-cooker-representative";
    std::filesystem::remove_all(Root);
    const std::filesystem::path Content =
        "Tests/Fixtures/AssetCooker/Representative";
    const Core::TArray<Core::TArray<std::string>> Groups = {
        {"representative.png"},
        {"representative.ktx2"},
        {"representative.gltf", "Surface.shader.json", "Surface.vert",
         "Surface.frag", "Surface.vert.spv", "Surface.frag.spv"},
        {"Surface.shader.json", "Surface.vert", "Surface.frag",
         "Surface.vert.spv", "Surface.frag.spv"}};
    bool Adapters = true;
    for (Core::usize Index = 0; Index < Groups.size(); ++Index)
    {
        const auto GroupRoot = Root / ("Group-" + std::to_string(Index));
        const auto GroupContent = GroupRoot / "Content";
        std::filesystem::create_directories(GroupContent);
        for (const auto& Name : Groups[Index])
            std::filesystem::copy_file(
                Content / Name, GroupContent / Name,
                std::filesystem::copy_options::overwrite_existing);
        const FRun Group = Cook(GroupRoot / "Run", GroupContent, 4);
        if (!Group.Result.Succeeded())
            std::cout << "[DETAIL] representative-group=" << Index
                      << " category=" << static_cast<int>(Group.Result.Category)
                      << " reason=" << Group.Result.StableReason.ToStdString()
                      << '\n';
        Adapters = Adapters && Group.Result.Succeeded();
    }
    Record(TestResult, Adapters,
        "every representative source adapter completes an isolated clean cook");
    const FRun Run = Cook(Root, Content, 8);
    std::set<std::string> Codecs;
    bool Reloaded = Run.Result.Succeeded();
    for (const auto& Artifact : Run.Result.Artifacts)
    {
        Core::TSharedPtr<const Asset::FAssetPayload> Payload;
        Asset::FAssetCookedPayloadEnvelope Envelope;
        Reloaded = Reloaded &&
            Asset::FAssetCookContractCodec::LoadTypedPayload(
                Artifact.Bytes, {}, Payload, &Envelope) ==
                Asset::EAssetResult::Success;
        if (Reloaded) Codecs.insert(Envelope.Header.CodecId.ToStdString());
    }
    const bool Families = Codecs.contains("stoner.image") &&
        Codecs.contains("stoner.ktx2") &&
        Codecs.contains("stoner.shader-program") &&
        Codecs.contains("stoner.shader-source") &&
        Codecs.contains("stoner.shader-payload") &&
        Codecs.contains("stoner.static-mesh") &&
        Codecs.contains("stoner.static-model");
    if (Run.Result.Succeeded())
        std::cout << "[EVIDENCE] representative-sources="
                  << Run.Report.Counts.DiscoveredSources
                  << " reachable=" << Run.Report.Counts.ReachableAssets
                  << " cooked=" << Run.Report.Counts.Cooked
                  << " codecs=" << Codecs.size()
                  << " generation="
                  << Run.Result.Manifest.GenerationId.ToLowerHex().ToStdString()
                  << '\n';
    if (!Reloaded || !Families)
        std::cout << "[DETAIL] representative-category="
                  << static_cast<int>(Run.Result.Category)
                  << " reason=" << Run.Result.StableReason.ToStdString()
                  << " artifacts=" << Run.Result.Artifacts.size()
                  << " codecs=" << Codecs.size() << '\n';
    Record(TestResult, Reloaded && Families,
        "representative Feature 021-024 corpus cooks and typed-loads");
    std::filesystem::remove_all(Root);
}

} // namespace

FAssetCookerDeterminismTestResult RunAssetCookerDeterminismTests()
{
    FAssetCookerDeterminismTestResult Result;
    TestCleanCook(Result);
    TestPlanOnly(Result);
    TestRepresentativeCorpus(Result);
    return Result;
}
