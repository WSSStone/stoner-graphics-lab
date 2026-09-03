#include "ProductionContentStrictRuntimeTests.h"

#include "Asset/AssetMinimal.h"
#include "Core/FPlatformFileSystem.h"
#include "FProductionContentSession.h"
#include "ProductionAssetClosureTestSupport.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>

namespace
{
using namespace Stoner;
using namespace Stoner::Asset;

void Record(
    FProductionContentStrictRuntimeTestResult& Result,
    bool bPassed,
    const char* Name)
{
    (bPassed ? ++Result.Passed : ++Result.Failed);
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

const char* Environment(const char* Name)
{
    const char* Value = std::getenv(Name);
    return Value && *Value != '\0' ? Value : nullptr;
}

std::filesystem::path NativeFilesystemPath(
    const std::filesystem::path& Path)
{
#if SG_PLATFORM_WINDOWS
    const std::filesystem::path Absolute = std::filesystem::absolute(Path);
    const std::wstring Native = Absolute.native();
    if (Native.rfind(LR"(\\?\)", 0) == 0)
        return Absolute;
    if (Native.rfind(LR"(\\)", 0) == 0)
        return std::filesystem::path(
            std::wstring(LR"(\\?\UNC\)") + Native.substr(2));
    return std::filesystem::path(std::wstring(LR"(\\?\)") + Native);
#else
    return Path;
#endif
}

Core::TArray<Core::uint8> Read(const std::filesystem::path& Path)
{
    std::ifstream Input(NativeFilesystemPath(Path), std::ios::binary);
    return {
        std::istreambuf_iterator<char>(Input),
        std::istreambuf_iterator<char>()};
}

bool Write(
    const std::filesystem::path& Path,
    const Core::TArray<Core::uint8>& Bytes)
{
    std::ofstream Output(
        NativeFilesystemPath(Path), std::ios::binary | std::ios::trunc);
    Output.write(
        reinterpret_cast<const char*>(Bytes.data()),
        static_cast<std::streamsize>(Bytes.size()));
    return Output.good();
}

FPublishedGenerationValidationResult Validate(
    const std::filesystem::path& Publication)
{
    FPublishedGenerationValidationRequest Request;
    Request.SubjectRoot = Core::FString(Publication.generic_string());
    return FPublishedGenerationValidator::Validate(Request);
}

bool WaitTerminal(
    const FAssetManager& Manager,
    FAssetRequestHandle Request,
    FAssetRequestSnapshot& Out)
{
    const auto Deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(30);
    while (std::chrono::steady_clock::now() < Deadline)
    {
        if (Manager.Query(Request, Out) != EAssetResult::Success)
            return false;
        if (Out.State == EAssetRequestState::Ready ||
            Out.State == EAssetRequestState::Failed ||
            Out.State == EAssetRequestState::Cancelled)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}

EAssetResult RequestRecord(
    FAssetManager& Manager,
    const FAssetCookManifestRecord& Record,
    FAssetRequestHandle& Out)
{
    if (Record.AssetType == Core::FString("Image"))
        return Manager.Request<FImageAsset>(Record.AssetId, Out);
    if (Record.AssetType == Core::FString("Texture"))
        return Manager.Request<FKTX2TextureArtifact>(Record.AssetId, Out);
    if (Record.AssetType == Core::FString("ShaderSource"))
        return Manager.Request<FShaderSourceAsset>(Record.AssetId, Out);
    if (Record.AssetType == Core::FString("ShaderPayload"))
        return Manager.Request<FShaderPayloadAsset>(Record.AssetId, Out);
    if (Record.AssetType == Core::FString("ShaderProgram"))
        return Manager.Request<FShaderAsset>(Record.AssetId, Out);
    if (Record.AssetType == Core::FString("Material"))
        return Manager.Request<FMaterialAsset>(Record.AssetId, Out);
    if (Record.AssetType == Core::FString("MaterialInstance"))
        return Manager.Request<FMaterialInstanceAsset>(Record.AssetId, Out);
    if (Record.AssetType == Core::FString("StaticMesh"))
        return Manager.Request<FStaticMeshAsset>(Record.AssetId, Out);
    if (Record.AssetType == Core::FString("StaticModel"))
        return Manager.Request<FStaticModelAsset>(Record.AssetId, Out);
    return EAssetResult::Unsupported;
}

FAssetManagerConfig StrictConfig(
    const std::filesystem::path& Publication,
    const std::filesystem::path& Coordination,
    const Core::TSharedPtr<FAssetExtensionRegistry>& Registry,
    Core::TSharedPtr<const FAssetTargetProfileEvidence> Target)
{
    FAssetManagerConfig Config;
    Config.Mode = EAssetManagerMode::StrictCooked;
    Config.ExtensionRegistry = Registry;
    Config.PublicationRoot = Core::FString(Publication.generic_string());
    Config.LeaseCoordinationRoot =
        Core::FString(Coordination.generic_string());
    Config.TargetEvidence = std::move(Target);
    Config.WorkerCount = 1;
    Config.Limits.MaxPayloadBytes = 1024ULL * 1024ULL * 1024ULL;
    Config.Limits.MaxAggregatePayloadBytes = 8ULL * 1024ULL * 1024ULL * 1024ULL;
    return Config;
}

bool CopyPublication(
    const std::filesystem::path& Source,
    const std::filesystem::path& Destination)
{
    std::error_code Error;
    std::filesystem::remove_all(NativeFilesystemPath(Destination), Error);
    Error.clear();
    std::filesystem::create_directories(
        NativeFilesystemPath(Destination.parent_path()), Error);
    if (Error) return false;
    Error.clear();
    std::filesystem::copy(
        NativeFilesystemPath(Source), NativeFilesystemPath(Destination),
        std::filesystem::copy_options::recursive, Error);
    return !Error;
}

bool Remove(const std::filesystem::path& Path)
{
    std::error_code Error;
    const bool Removed = std::filesystem::remove(
        NativeFilesystemPath(Path), Error);
    return Removed && !Error;
}

bool RemoveTree(const std::filesystem::path& Path)
{
    std::error_code Error;
    const auto Removed = std::filesystem::remove_all(
        NativeFilesystemPath(Path), Error);
    return Removed > 0 && !Error;
}

} // namespace

FProductionContentStrictRuntimeTestResult
RunProductionContentStrictRuntimeTests()
{
    FProductionContentStrictRuntimeTestResult Result;
    const char* PublicationText = Environment(
        "STONER_PRODUCTION_PUBLICATION_ROOT");
    const char* TargetProfileText = Environment(
        "STONER_PRODUCTION_TARGET_PROFILE");
    if (!PublicationText || !TargetProfileText)
    {
        Record(Result, false,
            "strict runtime tests require explicit publication and target profile roots");
        return Result;
    }

    const std::filesystem::path Publication(PublicationText);
    const FPublishedGenerationValidationResult Seed = Validate(Publication);
    Record(Result, Seed.Succeeded(),
        "strict runtime seed generation passes full standalone validation");
    if (!Seed.Succeeded()) return Result;

    Core::TArray<Core::uint8> ProfileBytes;
    FAssetTargetProfileEvidence Profile;
    const bool ProfileValid = Core::FPlatformFileSystem::ReadFile(
            Core::FString(TargetProfileText), ProfileBytes) &&
        FAssetCookContractCodec::ParseTargetProfile(
            ProfileBytes, Profile) == EAssetResult::Success &&
        Profile == Seed.Manifest.TargetProfile;
    Record(Result, ProfileValid,
        "strict runtime target profile exactly matches the generation");
    if (!ProfileValid) return Result;

    const auto Token = std::chrono::steady_clock::now()
        .time_since_epoch().count();
    const std::filesystem::path TemporaryRoot =
        std::filesystem::temp_directory_path() /
        ("sg-production-strict-runtime-" + std::to_string(Token));
    const auto MissingSources = TemporaryRoot / "UnavailableSources";
    FProductionAssetExtensionSet Extensions;
    const bool ExtensionsReady = CreateProductionAssetExtensionSet(
        MissingSources / "Package", MissingSources / "Shaders", Extensions);
    Record(Result, ExtensionsReady,
        "strict runtime registry is complete while source roots are unavailable");
    if (!ExtensionsReady) return Result;

    auto SharedProfile =
        Core::MakeShared<const FAssetTargetProfileEvidence>(Profile);
    std::filesystem::create_directories(TemporaryRoot / "LeaseValid");
    FAssetManagerConfig Config = StrictConfig(
        Publication, TemporaryRoot / "LeaseValid",
        Extensions.Registry, SharedProfile);
    Core::TSharedPtr<FAssetManager> Manager;
    FAssetDiagnosticList Diagnostics;
    const bool ManagerCreated = FAssetManager::Create(
        Config, Manager, Diagnostics) == EAssetResult::Success;
    FProductionAssetClosure Closure;
    Core::FString Failure;
    const bool Loaded = ManagerCreated && LoadProductionAssetClosure(
        *Manager, Seed.Manifest, true, Closure, Failure);
    const FAssetManagerInspection Inspection = ManagerCreated
        ? Manager->Inspect() : FAssetManagerInspection{};
    Record(Result,
        Loaded && Closure.Entries.size() == Seed.Manifest.Records.size(),
        "strict runtime publishes the complete typed closure atomically");
    Record(Result,
        Loaded && Inspection.ResolverExecutions == 0 &&
            Inspection.ImporterExecutions == 0 &&
            Inspection.AuthoringDecoderExecutions == 0 &&
            Inspection.SourceFallbackExecutions == 0 &&
            Inspection.StrictLoaderExecutions >= Seed.Manifest.Records.size(),
        "strict runtime uses loaders only and never participates in source fallback");
    Closure = {};
    if (Manager) (void)Manager->Shutdown();

    std::filesystem::create_directories(TemporaryRoot / "LeaseDemoSession");
    Demo::FProductionContentSessionConfig SessionConfig;
    SessionConfig.PublicationRoot = Core::FString(Publication.generic_string());
    SessionConfig.LeaseCoordinationRoot = Core::FString(
        (TemporaryRoot / "LeaseDemoSession").generic_string());
    const auto ModelRoot = std::find_if(
        Seed.Manifest.Selection.Roots.begin(),
        Seed.Manifest.Selection.Roots.end(),
        [](const auto& Candidate)
        { return Candidate.GetAssetType() == Core::FString("StaticModel"); });
    SessionConfig.RootAssetIdentity = ModelRoot ==
            Seed.Manifest.Selection.Roots.end()
        ? Core::FString{}
        : ModelRoot->ToString();
    SessionConfig.ExpectedGeneration = Seed.Manifest.GenerationId;
    SessionConfig.TargetEvidence = SharedProfile;
    auto InvalidWorkerSessionConfig = SessionConfig;
    InvalidWorkerSessionConfig.WorkerCount = 0;
    Record(Result, !InvalidWorkerSessionConfig.IsValid(),
        "Demo strict session rejects an invalid worker count");
    Demo::FProductionContentSession Session;
    Demo::FProductionContentLoadedClosure SessionClosure;
    const auto SessionResult = Session.Load(SessionConfig, SessionClosure);
    const auto SessionInspection = Session.Inspect();
    Record(Result,
        SessionResult == EAssetResult::Success && SessionClosure.Model &&
            SessionClosure.LoadedAssetCount == Seed.Manifest.Records.size() &&
            SessionClosure.Dependencies.Meshes.size() > 0 &&
            SessionClosure.Dependencies.Materials.size() > 0 &&
            SessionClosure.Dependencies.Shaders.size() > 0 &&
            SessionClosure.Dependencies.ShaderPayloads.size() > 0 &&
            SessionClosure.Dependencies.Textures.size() > 0 &&
            SessionClosure.RenderShaders.size() == 6 &&
            SessionClosure.RenderShaderPayloads.size() == 11 &&
            SessionInspection.bPublished &&
            SessionInspection.Manager.ResolverExecutions == 0 &&
            SessionInspection.Manager.ImporterExecutions == 0 &&
            SessionInspection.Manager.AuthoringDecoderExecutions == 0 &&
            SessionInspection.Manager.SourceFallbackExecutions == 0,
        "Demo strict session publishes one complete renderable closure without source participants");
    SessionClosure = {};
    const auto SessionShutdown = Session.Shutdown();
    Record(Result,
        SessionShutdown == EAssetResult::Success &&
            Session.Inspect().bShutdown &&
            Session.Inspect().Manager.ActiveOperations == 0 &&
            Session.Inspect().Manager.RequestRetentions == 0 &&
            Session.Inspect().Manager.ExternalHandleRetentions == 0,
        "Demo strict session releases request and generation ownership on shutdown");

    Record(Result,
        Demo::ShouldLoadProductionManifestDependencyFirst(
            "production-content-sponza-v2", 1000) &&
            !Demo::ShouldLoadProductionManifestDependencyFirst(
                "production-content-lantern-v2", 1000) &&
            !Demo::ShouldLoadProductionManifestDependencyFirst(
                "production-content-sponza-v2", 20),
        "dependency-first loading is isolated to the Sponza v2 1000-cycle profile");
    Record(Result,
        Demo::ShouldReuseProductionCookedEnvelopeAuthentication(
            "production-content-sponza-v2", 1000) &&
            Demo::ShouldReuseProductionCookedEnvelopeAuthentication(
                "production-content-lantern-v2", 1000) &&
            !Demo::ShouldReuseProductionCookedEnvelopeAuthentication(
                "production-content-sponza-v2", 20) &&
            !Demo::ShouldReuseProductionCookedEnvelopeAuthentication(
                "production-content-lantern-v2", 20) &&
            !Demo::ShouldReuseProductionCookedEnvelopeAuthentication(
                "production-content-unknown-v2", 1000),
        "envelope authentication reuse is isolated to accepted v2 1000-cycle profiles");
    Record(Result,
        Demo::SelectProductionContentWorkerCount(
            "production-content-lantern-v2", 1000,
            EAssetGraphicsBackend::Metal,
            EAssetTargetCpuArchitecture::Arm64) == 8 &&
            Demo::SelectProductionContentWorkerCount(
                "production-content-sponza-v2", 1000,
                EAssetGraphicsBackend::Metal,
                EAssetTargetCpuArchitecture::Arm64) == 16 &&
            Demo::SelectProductionContentWorkerCount(
                "production-content-lantern-v2", 20,
                EAssetGraphicsBackend::Metal,
                EAssetTargetCpuArchitecture::Arm64) == 1 &&
            Demo::SelectProductionContentWorkerCount(
                "production-content-lantern-v2", 20,
                EAssetGraphicsBackend::Vulkan,
                EAssetTargetCpuArchitecture::X86_64) == 4,
        "worker selection isolates Sponza throughput from regular and Lantern profiles");

    std::filesystem::create_directories(
        TemporaryRoot / "LeaseDemoDependencyFirst");
    auto DependencyFirstConfig = SessionConfig;
    DependencyFirstConfig.LeaseCoordinationRoot = Core::FString(
        (TemporaryRoot / "LeaseDemoDependencyFirst").generic_string());
    DependencyFirstConfig.bLoadManifestDependencyFirst = true;
    DependencyFirstConfig.bReuseCookedEnvelopeAuthentication = true;
    Demo::FProductionContentSession DependencyFirstSession;
    Demo::FProductionContentLoadedClosure DependencyFirstClosure;
    const auto DependencyFirstResult = DependencyFirstSession.Load(
        DependencyFirstConfig, DependencyFirstClosure);
    const auto DependencyFirstInspection = DependencyFirstSession.Inspect();
    Record(Result,
        DependencyFirstResult == EAssetResult::Success &&
            DependencyFirstClosure.Model &&
            DependencyFirstClosure.LoadedAssetCount ==
                Seed.Manifest.Records.size() &&
            DependencyFirstInspection.Manager.StrictLoaderExecutions ==
                Seed.Manifest.Records.size(),
        "Demo medium session loads each manifest record exactly once in dependency-first batches");
    DependencyFirstClosure = {};
    Record(Result,
        DependencyFirstSession.Shutdown() == EAssetResult::Success &&
            DependencyFirstSession.Inspect().Manager.ActiveOperations == 0 &&
            DependencyFirstSession.Inspect().Manager.RequestRetentions == 0 &&
            DependencyFirstSession.Inspect().Manager.ExternalHandleRetentions == 0,
        "Demo medium dependency-first session releases every manager retention");
    Demo::FProductionContentLoadedClosure ReusedClosure;
    const auto ReusedResult = DependencyFirstSession.Load(
        DependencyFirstConfig, ReusedClosure);
    const auto ReusedInspection = DependencyFirstSession.Inspect();
    Record(Result,
        ReusedResult == EAssetResult::Success && ReusedClosure.Model &&
            ReusedInspection.bGenerationValidationReused &&
            ReusedInspection.CookedEnvelopeAuthentication.GenerationId ==
                Seed.Manifest.GenerationId &&
            ReusedInspection.CookedEnvelopeAuthentication.
                AuthenticatedEnvelopeCount == Seed.Manifest.Records.size() &&
            ReusedInspection.CookedEnvelopeAuthentication.ReuseHits >=
                Seed.Manifest.Records.size(),
        "Demo medium session reuses bounded validated metadata and generation authentication while reloading the full closure");
    ReusedClosure = {};
    Record(Result,
        DependencyFirstSession.Shutdown() == EAssetResult::Success &&
            DependencyFirstSession.Inspect().Manager.ActiveOperations == 0 &&
            DependencyFirstSession.Inspect().Manager.RequestRetentions == 0 &&
            DependencyFirstSession.Inspect().Manager.ExternalHandleRetentions == 0,
        "Demo reused-authentication session still releases every manager retention");

    Core::TArray<Core::uint8> WrongProfileBytes;
    FAssetTargetProfileEvidence WrongEvidence;
    const bool WrongWritten = Core::FPlatformFileSystem::ReadFile(
            Core::FString("Config/AssetCooker/Profiles/Mac-Metal-Arm64.json"),
            WrongProfileBytes) &&
        FAssetCookContractCodec::ParseTargetProfile(
            WrongProfileBytes, WrongEvidence) == EAssetResult::Success;
    std::filesystem::create_directories(TemporaryRoot / "LeaseWrong");
    FAssetManagerConfig WrongConfig = StrictConfig(
        Publication, TemporaryRoot / "LeaseWrong", Extensions.Registry,
        Core::MakeShared<const FAssetTargetProfileEvidence>(WrongEvidence));
    Core::TSharedPtr<FAssetManager> WrongManager;
    Diagnostics.clear();
    Record(Result,
        WrongWritten && FAssetManager::Create(
            WrongConfig, WrongManager, Diagnostics) == EAssetResult::Conflict &&
            !WrongManager,
        "strict generation bind rejects mismatched target evidence without partial manager state");

    struct FMutationCase
    {
        const char* Name;
        const char* Reason;
        EPublishedCorruptionCategory Category;
        int Mutation;
    };
    const FMutationCase Cases[] = {
        {"missing current pointer", "published.pointer.missing",
         EPublishedCorruptionCategory::PointerMissing, 1},
        {"malformed current pointer", "published.pointer.invalid",
         EPublishedCorruptionCategory::PointerInvalid, 2},
        {"missing generation", "published.generation.missing",
         EPublishedCorruptionCategory::GenerationMissing, 3},
        {"missing manifest", "published.manifest.missing",
         EPublishedCorruptionCategory::ManifestInvalid, 4},
        {"missing payload", "published.payload.missing",
         EPublishedCorruptionCategory::PayloadMissing, 5},
        {"same-size corrupt payload", "published.payload.invalid",
         EPublishedCorruptionCategory::PayloadInvalid, 6},
        {"substituted payload", "published.payload.invalid",
         EPublishedCorruptionCategory::PayloadInvalid, 7},
        {"unexpected generation file", "published.generation.unexpected-file",
         EPublishedCorruptionCategory::UnexpectedFile, 8},
    };
    for (Core::usize Index = 0; Index < std::size(Cases); ++Index)
    {
        const auto Mutated = TemporaryRoot / "Cases" /
            std::to_string(Index);
        bool Prepared = CopyPublication(Publication, Mutated);
        const auto Generation = Mutated / "Generations" /
            Seed.Manifest.GenerationId.ToLowerHex().ToStdString();
        const auto Payload = Generation /
            Seed.Manifest.Records.front().PayloadLocator.ToStdString();
        if (Prepared && Cases[Index].Mutation == 1)
            Prepared = Remove(Mutated / "Current.json");
        else if (Prepared && Cases[Index].Mutation == 2)
            Prepared = Write(Mutated / "Current.json", {'{', '}'});
        else if (Prepared && Cases[Index].Mutation == 3)
            Prepared = RemoveTree(Generation);
        else if (Prepared && Cases[Index].Mutation == 4)
            Prepared = Remove(Generation / "Manifest.json");
        else if (Prepared && Cases[Index].Mutation == 5)
            Prepared = Remove(Payload);
        else if (Prepared && Cases[Index].Mutation == 6)
        {
            auto Bytes = Read(Payload);
            if (!Bytes.empty()) Bytes.back() ^= 0x01U;
            Prepared = !Bytes.empty() && Write(Payload, Bytes);
        }
        else if (Prepared && Cases[Index].Mutation == 7)
        {
            const auto Other = Generation /
                Seed.Manifest.Records.back().PayloadLocator.ToStdString();
            auto Bytes = Read(Other);
            auto Original = Read(Payload);
            if (Bytes.size() != Original.size()) Bytes.resize(Original.size());
            Prepared = !Bytes.empty() && Write(Payload, Bytes);
        }
        else if (Prepared && Cases[Index].Mutation == 8)
            Prepared = Write(Generation / "unexpected.bin", {1, 2, 3});
        const auto Detected = Prepared ? Validate(Mutated)
            : FPublishedGenerationValidationResult{};
        if (!Prepared || Detected.Category != Cases[Index].Category ||
            Detected.StableReason != Core::FString(Cases[Index].Reason))
            std::cout << "[DETAIL] mutation=" << Cases[Index].Name
                      << " prepared=" << Prepared
                      << " category="
                      << static_cast<unsigned int>(Detected.Category)
                      << " reason=" << Detected.StableReason.CStr() << '\n';
        Record(Result,
            Prepared && Detected.Category == Cases[Index].Category &&
                Detected.StableReason == Core::FString(Cases[Index].Reason),
            Cases[Index].Name);
    }

    const auto MissingPayloadPublication = TemporaryRoot / "NoPartial";
    bool MissingPrepared = CopyPublication(
        Publication, MissingPayloadPublication);
    const auto MissingGeneration = MissingPayloadPublication / "Generations" /
        Seed.Manifest.GenerationId.ToLowerHex().ToStdString();
    const auto MissingPayload = MissingGeneration /
        Seed.Manifest.Records.front().PayloadLocator.ToStdString();
    MissingPrepared = MissingPrepared && Remove(MissingPayload);
    std::filesystem::create_directories(TemporaryRoot / "LeaseNoPartial");
    FAssetManagerConfig MissingConfig = StrictConfig(
        MissingPayloadPublication, TemporaryRoot / "LeaseNoPartial",
        Extensions.Registry, SharedProfile);
    Core::TSharedPtr<FAssetManager> MissingManager;
    Diagnostics.clear();
    FProductionAssetClosure EmptyClosure;
    Record(Result,
        MissingPrepared && FAssetManager::Create(
            MissingConfig, MissingManager, Diagnostics) != EAssetResult::Success &&
            !MissingManager && EmptyClosure.Entries.empty(),
        "missing dependency payload rejects startup with no partial root closure");

    std::filesystem::create_directories(TemporaryRoot / "LeaseCancel");
    FAssetManagerConfig CancelConfig = StrictConfig(
        Publication, TemporaryRoot / "LeaseCancel",
        Extensions.Registry, SharedProfile);
    Core::TSharedPtr<FAssetManager> CancelManager;
    Diagnostics.clear();
    const bool CancelCreated = FAssetManager::Create(
        CancelConfig, CancelManager, Diagnostics) == EAssetResult::Success;
    Core::TArray<FAssetRequestHandle> Requests;
    if (CancelCreated)
    {
        for (const auto& RecordValue : Seed.Manifest.Records)
        {
            FAssetRequestHandle Request;
            if (RequestRecord(
                    *CancelManager, RecordValue, Request) ==
                EAssetResult::Success)
                Requests.push_back(Request);
        }
    }
    bool Cancelled = false;
    if (!Requests.empty())
    {
        const FAssetRequestHandle Request = Requests.back();
        Cancelled = CancelManager->Cancel(Request) == EAssetResult::Success;
        FAssetRequestSnapshot Snapshot;
        Cancelled = Cancelled && WaitTerminal(
            *CancelManager, Request, Snapshot) &&
            Snapshot.State == EAssetRequestState::Cancelled &&
            Snapshot.Result == EAssetResult::Cancelled;
    }
    Record(Result, Cancelled,
        "strict runtime cancellation reaches a terminal cancelled state");
    if (CancelManager) (void)CancelManager->Shutdown();

    const auto FixtureBytes = Read(
        "Tests/Fixtures/ProductionContent/Failures/cook-runtime-cases.json");
    const std::string Fixture(
        FixtureBytes.begin(), FixtureBytes.end());
    Core::uint32 CaseCount = 0;
    for (Core::usize Offset = 0;
         (Offset = Fixture.find("\"id\":", Offset)) != std::string::npos;
         Offset += 5)
        ++CaseCount;
    Record(Result,
        CaseCount >= 10 &&
            Fixture.find("published.payload.invalid") != std::string::npos &&
            Fixture.find("empty-closure") != std::string::npos,
        "cook/runtime failure catalog records at least ten exact first failures");

    std::error_code Error;
    std::filesystem::remove_all(TemporaryRoot, Error);
    return Result;
}
