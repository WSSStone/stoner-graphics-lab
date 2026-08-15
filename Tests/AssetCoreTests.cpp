#include "AssetCoreTests.h"

#include "Asset/AssetMinimal.h"

#include <array>
#include <atomic>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace
{

using namespace Stoner::Asset;
using namespace Stoner::Core;

struct FTexturePayload : FAssetPayload
{
    FString GetAssetType() const override { return FString("Texture"); }
};

struct FMeshPayload : FAssetPayload
{
    FString GetAssetType() const override { return FString("Mesh"); }
};

FAssetParticipantId MakeParticipant(const std::string& Text)
{
    FAssetParticipantId Participant;
    (void)FAssetParticipantId::Create(FString(Text), Participant);
    return Participant;
}

class FMemorySource final : public IAssetSource
{
public:
    explicit FMemorySource(TArray<uint8> Bytes)
        : Bytes_(std::move(Bytes))
    {
    }

    EAssetResult Read(uint64 Offset, usize MaximumBytes, TArray<uint8>& OutBytes) const override
    {
        ++ReadCount;
        LargestRequest = std::max(LargestRequest.load(), MaximumBytes);
        if (Offset > Bytes_.size())
        {
            OutBytes.clear();
            return EAssetResult::MalformedSource;
        }
        const usize Count = std::min<usize>(MaximumBytes, Bytes_.size() - static_cast<usize>(Offset));
        OutBytes.assign(
            Bytes_.begin() + static_cast<std::ptrdiff_t>(Offset),
            Bytes_.begin() + static_cast<std::ptrdiff_t>(Offset + Count));
        return EAssetResult::Success;
    }

    mutable std::atomic<int> ReadCount{0};
    mutable std::atomic<usize> LargestRequest{0};

private:
    TArray<uint8> Bytes_;
};

class FTestResolver final : public IAssetResolver
{
public:
    FTestResolver(std::string Name, int Priority, EAssetResult Result)
        : Name_(std::move(Name))
        , Priority_(Priority)
        , Result_(Result)
    {
    }

    FAssetExtensionCapability GetCapability() const override
    {
        FAssetExtensionCapability Capability;
        Capability.Kind = EAssetExtensionKind::Resolver;
        Capability.Participant = MakeParticipant(Name_);
        Capability.Priority = Priority_;
        Capability.Schemes = {FString("mem")};
        return Capability;
    }

    FAssetResolveResult Resolve(const FAssetResolveRequest& Request) override
    {
        ++Calls;
        FAssetResolveResult Result;
        Result.Result = Result_;
        Result.Descriptor.Location = Request.Location;
        return Result;
    }

    std::atomic<int> Calls{0};

private:
    std::string Name_;
    int Priority_;
    EAssetResult Result_;
};

class FTestImporter final : public IAssetImporter
{
public:
    FTestImporter(std::string Name, int Confidence, FAssetMetadata Output)
        : Name_(std::move(Name))
        , Confidence_(Confidence)
        , Output_(std::move(Output))
    {
    }

    ~FTestImporter() override
    {
        if (DestructionCount)
        {
            ++(*DestructionCount);
        }
    }

    FAssetExtensionCapability GetCapability() const override
    {
        FAssetExtensionCapability Capability;
        Capability.Kind = EAssetExtensionKind::Importer;
        Capability.Participant = MakeParticipant(Name_);
        FAssetProducerVersion Version;
        (void)FAssetProducerVersion::Create(FString("1.0"), Version);
        Capability.ProducerVersion = Version;
        Capability.FormatHints = {FString("synthetic")};
        Capability.ProbeByteLimit = ProbeLimit;
        return Capability;
    }

    FAssetProbeResult Probe(
        const FAssetSourceDescriptor& Descriptor,
        std::span<const uint8> Prefix) override
    {
        (void)Descriptor;
        ++ProbeCalls;
        LastProbeSize = Prefix.size();
        return {EAssetResult::Success, Confidence_, FString("synthetic")};
    }

    EAssetResult Import(
        const FAssetSourceDescriptor& Descriptor,
        const FAssetSourceLease& Source,
        TArray<FAssetImportOutput>& OutOutputs) override
    {
        (void)Descriptor;
        (void)Source;
        ++ImportCalls;
        OutOutputs = {{Output_, {}}};
        return ImportResult;
    }

    usize ProbeLimit = 64U * 1024U;
    EAssetResult ImportResult = EAssetResult::Success;
    std::atomic<int> ProbeCalls{0};
    std::atomic<int> ImportCalls{0};
    usize LastProbeSize = 0;
    std::atomic<int>* DestructionCount = nullptr;

private:
    std::string Name_;
    int Confidence_;
    FAssetMetadata Output_;
};

class FTestLoader final : public IAssetLoader
{
public:
    explicit FTestLoader(FAssetParticipantId Participant)
        : Participant_(std::move(Participant))
    {
    }

    FAssetExtensionCapability GetCapability() const override
    {
        FAssetExtensionCapability Capability;
        Capability.Kind = EAssetExtensionKind::Loader;
        Capability.Participant = Participant_;
        return Capability;
    }

    FAssetLoadResult Load(const FAssetLoadRequest& Request) override
    {
        (void)Request;
        return {
            EAssetResult::Success,
            MakeShared<FTexturePayload>(),
            {}};
    }

private:
    FAssetParticipantId Participant_;
};

class FTestCooker final : public IAssetCooker
{
public:
    explicit FTestCooker(FAssetParticipantId Participant)
        : Participant_(std::move(Participant))
    {
    }

    FAssetExtensionCapability GetCapability() const override
    {
        FAssetExtensionCapability Capability;
        Capability.Kind = EAssetExtensionKind::Cooker;
        Capability.Participant = Participant_;
        return Capability;
    }

    FAssetCookResult Cook(const FAssetCookRequest& Request) override
    {
        if (Request.TargetProfile.IsEmpty() || !Request.Payload)
        {
            return {
                EAssetResult::InvalidInput,
                {},
                {},
                {},
                {},
                {}};
        }
        const TArray<uint8> Bytes = {1, 2, 3, 4};
        return {
            EAssetResult::Success,
            Request.TargetProfile,
            Bytes,
            FAssetDigest::FromBytes(Bytes),
            {},
            {}};
    }

private:
    FAssetParticipantId Participant_;
};

} // namespace

namespace Stoner::Asset
{

template <>
struct TAssetTypeTraits<FTexturePayload>
{
    static Core::FString GetAssetType() { return Core::FString("Texture"); }
};

template <>
struct TAssetTypeTraits<FMeshPayload>
{
    static Core::FString GetAssetType() { return Core::FString("Mesh"); }
};

} // namespace Stoner::Asset

namespace
{

void Record(FAssetCoreTestResult& Result, bool Passed, const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

FAssetId MakeId(const char* Type, const char* Path, const char* Subresource = nullptr)
{
    FAssetId Id;
    const std::optional<FString> Sub =
        Subresource != nullptr ? std::optional<FString>(FString(Subresource)) : std::nullopt;
    const EAssetResult Result = FAssetId::Create(FString(Type), FString(Path), Sub, Id);
    return Result == EAssetResult::Success ? Id : FAssetId{};
}

FAssetSourceLocator MakeSource(const char* Scheme, const char* Locator)
{
    FAssetSourceLocator Source;
    const EAssetResult Result =
        FAssetSourceLocator::Create(FString(Scheme), FString(Locator), Source);
    return Result == EAssetResult::Success ? Source : FAssetSourceLocator{};
}

FAssetMetadata MakeMetadata(
    const FAssetId& Id,
    const char* SourcePath,
    TArray<FAssetDependency> Dependencies = {})
{
    FAssetParticipantId Producer;
    FAssetProducerVersion ProducerVersion;
    (void)FAssetParticipantId::Create(FString("test.importer"), Producer);
    (void)FAssetProducerVersion::Create(FString("1.0.0"), ProducerVersion);

    FAssetMetadata Metadata;
    Metadata.Id = Id;
    Metadata.Source = MakeSource("MEM", SourcePath);
    Metadata.Producer = Producer;
    Metadata.ProducerVersion = ProducerVersion;
    Metadata.Dependencies = std::move(Dependencies);
    return Metadata;
}

void TestIdentity(FAssetCoreTestResult& Result)
{
    FAssetId Id;
    Record(
        Result,
        FAssetId::Create(FString("Texture"), FString("UI\\Icons//./Play"), std::nullopt, Id) ==
                EAssetResult::Success &&
            Id.ToString() == FString("Texture:UI/Icons/Play"),
        "asset identity canonicalizes portable separators");

    FAssetId Composed;
    FAssetId Decomposed;
    const FString ComposedPath("Caf\xc3\xa9/Icon");
    const FString DecomposedPath("Cafe\xcc\x81/Icon");
    bool NfcStable = true;
    for (int Iteration = 0; Iteration < 20; ++Iteration)
    {
        NfcStable = NfcStable &&
            FAssetId::Create(FString("Texture"), ComposedPath, std::nullopt, Composed) ==
                EAssetResult::Success &&
            FAssetId::Create(FString("Texture"), DecomposedPath, std::nullopt, Decomposed) ==
                EAssetResult::Success &&
            Composed == Decomposed;
    }
    Record(Result, NfcStable, "asset identity NFC equivalence is deterministic across 20 runs");

    FAssetId Invalid;
    Record(
        Result,
        FAssetId::Create(FString("9Texture"), FString("A"), std::nullopt, Invalid) ==
                EAssetResult::InvalidIdentity &&
            FAssetId::Create(FString("Texture"), FString("../A"), std::nullopt, Invalid) ==
                EAssetResult::InvalidIdentity &&
            FAssetId::Create(FString("Texture"), FString("/A"), std::nullopt, Invalid) ==
                EAssetResult::InvalidIdentity,
        "asset identity rejects invalid type and unsafe paths");

    const FAssetId Upper = MakeId("Texture", "UI/Icon");
    const FAssetId Lower = MakeId("Texture", "ui/Icon");
    Record(
        Result,
        Upper != Lower && FAssetId::CompareWithForcedCommonHashForTesting(Upper, Upper, 1) &&
            !FAssetId::CompareWithForcedCommonHashForTesting(Upper, Lower, 1),
        "asset identity is case-sensitive and collision-safe");
}

void TestDigest(FAssetCoreTestResult& Result)
{
    const std::array<uint8, 0> Empty{};
    const FAssetDigest EmptyDigest = FAssetDigest::FromBytes(Empty);
    const std::string Abc = "abc";
    const FAssetDigest AbcDigest = FAssetDigest::FromBytes(std::span<const uint8>(
        reinterpret_cast<const uint8*>(Abc.data()),
        Abc.size()));
    Record(
        Result,
        EmptyDigest.ToLowerHex() ==
                FString("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") &&
            AbcDigest.ToLowerHex() ==
                FString("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"),
        "SHA-256 matches NIST vectors");

    FAssetDigest Parsed;
    Record(
        Result,
        FAssetDigest::ParseLowerHex(AbcDigest.ToLowerHex(), Parsed) == EAssetResult::Success &&
            Parsed == AbcDigest && !FAssetDigest{}.IsAvailable(),
        "digest lowercase round-trip preserves availability and bytes");
}

void TestSoftReferences(FAssetCoreTestResult& Result)
{
    TSoftAssetRef<FTexturePayload> Empty;
    TSoftAssetRef<FTexturePayload> TextureReference;
    TSoftAssetRef<FMeshPayload> MeshReference;
    const FAssetId Texture = MakeId("Texture", "UI/Icon");
    Record(
        Result,
        Empty.IsEmpty() &&
            TSoftAssetRef<FTexturePayload>::Create(Texture, TextureReference) ==
                EAssetResult::Success &&
            !TextureReference.IsEmpty() &&
            TSoftAssetRef<FMeshPayload>::Create(Texture, MeshReference) ==
                EAssetResult::TypeMismatch &&
            MeshReference.IsEmpty(),
        "typed soft reference accepts matching type and rejects mismatch");
}

void TestSourceAndMetadata(FAssetCoreTestResult& Result)
{
    const FAssetSourceLocator Source = MakeSource("MEM", "Caf\xc3\xa9");
    const FAssetSourceLocator Equivalent = MakeSource("mem", "Cafe\xcc\x81");
    Record(
        Result,
        Source.IsValid() && Source == Equivalent &&
            Source.ToString() == FString("mem:Caf\xc3\xa9"),
        "source locator lowercases scheme and normalizes locator NFC");

    const FAssetId Id = MakeId("Texture", "Registry/A");
    FAssetMetadata Left = MakeMetadata(Id, "source/a");
    Left.Attributes = {{FString("z"), FString("2")}, {FString("a"), FString("1")}};
    FAssetMetadata Right = MakeMetadata(Id, "source/a");
    Right.Attributes = {{FString("a"), FString("1")}, {FString("z"), FString("2")}};
    Record(
        Result,
        Left.Validate() == EAssetResult::Success &&
            Right.Validate() == EAssetResult::Success &&
            Left.IsCanonicallyEquivalent(Right),
        "metadata equivalence ignores attribute insertion order");
}

void TestRegistry(FAssetCoreTestResult& Result)
{
    FAssetRegistry Registry;
    const FAssetId A = MakeId("Texture", "Registry/A");
    const FAssetId B = MakeId("Texture", "Registry/B");
    const FAssetId C = MakeId("Mesh", "Registry/C");
    FAssetDependency DependsOnB;
    DependsOnB.TargetId = B;
    FAssetMutationBatch RegisterA;
    RegisterA.Register(MakeMetadata(A, "group/a", {DependsOnB}));
    Record(
        Result,
        Registry.Apply(RegisterA) == EAssetResult::Success &&
            Registry.Find(A).has_value() && Registry.Snapshot().Revision == 1,
        "registry atomically registers metadata");

    TArray<FAssetDependency> Unresolved;
    Record(
        Result,
        Registry.ValidateCompleteness(Unresolved) == EAssetResult::IncompleteRegistry &&
            Unresolved.size() == 1 &&
            Registry.GetDependencies(A)[0].Resolution == EAssetDependencyResolution::Unresolved,
        "registry retains unresolved required dependency");

    FAssetMutationBatch RegisterB;
    RegisterB.Register(MakeMetadata(B, "group/b"));
    Record(
        Result,
        Registry.Apply(RegisterB) == EAssetResult::Success &&
            Registry.ValidateCompleteness(Unresolved) == EAssetResult::Success &&
            Registry.GetDependents(B) == TArray<FAssetId>{A},
        "registry resolves dependencies and reverse lookup");

    FAssetDependency DependsOnA;
    DependsOnA.TargetId = A;
    FAssetMutationBatch Cycle;
    Cycle.Replace(MakeMetadata(B, "group/b", {DependsOnA}));
    const uint64 BeforeCycle = Registry.Snapshot().Revision;
    Record(
        Result,
        Registry.Apply(Cycle) == EAssetResult::DependencyCycle &&
            Registry.Snapshot().Revision == BeforeCycle &&
            Registry.GetDependencies(B).empty(),
        "required cycle rejects whole replacement");

    FAssetMutationBatch Conflict;
    Conflict.Register(MakeMetadata(C, "group/c"));
    FAssetMetadata ConflictingA = MakeMetadata(A, "other/a");
    Conflict.Register(std::move(ConflictingA));
    Record(
        Result,
        Registry.Apply(Conflict) == EAssetResult::AlreadyExists &&
            !Registry.Find(C).has_value(),
        "conflicting batch rolls back every staged record");

    FAssetMutationBatch RemoveB;
    RemoveB.Remove(B);
    Record(
        Result,
        Registry.Apply(RemoveB) == EAssetResult::Success &&
            Registry.ValidateCompleteness(Unresolved) == EAssetResult::IncompleteRegistry,
        "target removal returns dependency to unresolved");
}

void TestRegistryConcurrency(FAssetCoreTestResult& Result)
{
    FAssetRegistry Registry;
    const FAssetId Id = MakeId("Texture", "Stress/Single");
    FAssetMutationBatch Initial;
    Initial.Register(MakeMetadata(Id, "stress/source"));
    const bool InitialApplied = Registry.Apply(Initial) == EAssetResult::Success;

    std::atomic<bool> Running{true};
    std::atomic<int> InvalidSnapshots{0};
    std::vector<std::thread> Readers;
    for (int Reader = 0; Reader < 8; ++Reader)
    {
        Readers.emplace_back([&]
        {
            while (Running.load())
            {
                const FAssetRegistrySnapshot Snapshot = Registry.Snapshot();
                if (Snapshot.Records.size() != 1 || Snapshot.Records[0].Id != Id)
                {
                    ++InvalidSnapshots;
                }
            }
        });
    }

    bool WritesSucceeded = true;
    for (int BatchIndex = 0; BatchIndex < 100; ++BatchIndex)
    {
        FAssetMetadata Replacement = MakeMetadata(Id, "stress/source");
        Replacement.Attributes.push_back({
            FString("revision"),
            FString(std::to_string(BatchIndex))});
        FAssetMutationBatch Batch;
        Batch.Replace(std::move(Replacement));
        WritesSucceeded = WritesSucceeded &&
            Registry.Apply(Batch) == EAssetResult::Success;
    }
    Running.store(false);
    for (std::thread& Reader : Readers)
    {
        Reader.join();
    }
    Record(
        Result,
        InitialApplied && WritesSucceeded && InvalidSnapshots.load() == 0 &&
            Registry.Snapshot().Revision == 101,
        "eight readers observe only complete snapshots across 100 writes");
}

void TestResolverDispatch(FAssetCoreTestResult& Result)
{
    FAssetExtensionRegistry Registry;
    auto Low = MakeShared<FTestResolver>("resolver.low", 1, EAssetResult::NotFound);
    auto High = MakeShared<FTestResolver>("resolver.high", 2, EAssetResult::AccessDenied);
    FAssetRegistrationToken LowToken;
    FAssetRegistrationToken HighToken;
    const bool Registered =
        Registry.Register(Low, LowToken) == EAssetResult::Success &&
        Registry.Register(High, HighToken) == EAssetResult::Success;
    FAssetResolveRequest Request{MakeSource("mem", "missing"), {}};
    const FAssetResolveResult Winner = FAssetDispatch::Resolve(Registry, Request);
    Record(
        Result,
        Registered && Winner.Result == EAssetResult::AccessDenied &&
            Low->Calls.load() == 0 && High->Calls.load() == 1,
        "resolver unique highest priority wins and preserves failure category");

    auto Tie = MakeShared<FTestResolver>("resolver.tie", 2, EAssetResult::Success);
    FAssetRegistrationToken TieToken;
    (void)Registry.Register(Tie, TieToken);
    Record(
        Result,
        FAssetDispatch::Resolve(Registry, Request).Result ==
                EAssetResult::AmbiguousResolver &&
            Tie->Calls.load() == 0,
        "equal resolver priority fails without callback");
}

void TestImporterDispatch(FAssetCoreTestResult& Result)
{
    const FAssetMetadata Output = MakeMetadata(
        MakeId("Texture", "Imported/Main"),
        "source/import");
    FAssetSourceDescriptor Descriptor;
    Descriptor.Location = MakeSource("mem", "payload");
    Descriptor.FormatHint = FString("synthetic");
    TArray<uint8> Bytes(100U * 1024U, 7U);
    auto SourceObject = MakeShared<FMemorySource>(Bytes);
    FAssetSourceLease Source(SourceObject);

    FAssetExtensionRegistry Registry;
    auto Weak = MakeShared<FTestImporter>("importer.weak", 40, Output);
    auto Strong = MakeShared<FTestImporter>("importer.strong", 90, Output);
    Strong->ProbeLimit = 100U * 1024U;
    FAssetRegistrationToken WeakToken;
    FAssetRegistrationToken StrongToken;
    const bool Registered =
        Registry.Register(Weak, WeakToken) == EAssetResult::Success &&
        Registry.Register(Strong, StrongToken) == EAssetResult::Success;
    TArray<FAssetImportOutput> Outputs;
    Record(
        Result,
        Registered &&
            FAssetDispatch::Import(Registry, Descriptor, Source, Outputs) ==
                EAssetResult::Success &&
            Outputs.size() == 1 && Outputs[0].Metadata.Id == Output.Id &&
            Weak->ImportCalls.load() == 0 && Strong->ImportCalls.load() == 1 &&
            Strong->LastProbeSize == 64U * 1024U &&
            SourceObject->LargestRequest.load() == 64U * 1024U,
        "importer unique confidence wins with 64 KiB probe cap");

    auto Tie = MakeShared<FTestImporter>("importer.tie", 90, Output);
    FAssetRegistrationToken TieToken;
    (void)Registry.Register(Tie, TieToken);
    Record(
        Result,
        FAssetDispatch::Import(Registry, Descriptor, Source, Outputs) ==
                EAssetResult::AmbiguousImporter &&
            Tie->ImportCalls.load() == 0,
        "equal importer confidence is ambiguous");

    FAssetExtensionRegistry CapacityRegistry;
    std::vector<FAssetRegistrationToken> Tokens;
    Tokens.reserve(65);
    for (int Index = 0; Index < 65; ++Index)
    {
        auto Importer = MakeShared<FTestImporter>(
            "capacity." + std::to_string(Index),
            1,
            Output);
        FAssetRegistrationToken Token;
        (void)CapacityRegistry.Register(Importer, Token);
        Tokens.push_back(std::move(Token));
    }
    const int ReadsBefore = SourceObject->ReadCount.load();
    Record(
        Result,
        FAssetDispatch::Import(CapacityRegistry, Descriptor, Source, Outputs) ==
                EAssetResult::CapacityExceeded &&
            SourceObject->ReadCount.load() == ReadsBefore,
        "65 eligible importers fail before source read or probe");
}

void TestExtensionLifetime(FAssetCoreTestResult& Result)
{
    Record(
        Result,
        !std::is_copy_constructible_v<FAssetRegistrationToken> &&
            std::is_move_constructible_v<FAssetRegistrationToken>,
        "registration token is move-only");

    std::atomic<int> Destructions{0};
    FAssetExecutionLease Lease;
    {
        FAssetExtensionRegistry Registry;
        auto Importer = MakeShared<FTestImporter>(
            "lifetime.importer",
            100,
            MakeMetadata(MakeId("Texture", "Lifetime/Main"), "lifetime/source"));
        Importer->DestructionCount = &Destructions;
        FAssetRegistrationToken Token;
        const bool Registered =
            Registry.Register(Importer, Token) == EAssetResult::Success;
        Importer.reset();
        Lease = Registry.Acquire(
            EAssetExtensionKind::Importer,
            MakeParticipant("lifetime.importer"));
        Token.Reset();
        Record(
            Result,
            Registered && Lease.IsValid() &&
                Registry.Snapshot(EAssetExtensionKind::Importer).empty() &&
                Destructions.load() == 0,
            "unregister blocks future selection while lease retains instance");
    }
    Lease = {};
    Record(
        Result,
        Destructions.load() == 1,
        "extension destroys exactly once after final lease");

    bool RacesPassed = true;
    for (int Iteration = 0; Iteration < 100; ++Iteration)
    {
        FAssetExtensionRegistry Registry;
        auto Importer = MakeShared<FTestImporter>(
            "race.importer",
            100,
            MakeMetadata(MakeId("Texture", "Race/Main"), "race/source"));
        FAssetRegistrationToken Token;
        RacesPassed = RacesPassed &&
            Registry.Register(Importer, Token) == EAssetResult::Success;
        FAssetExecutionLease InFlight = Registry.Acquire(
            EAssetExtensionKind::Importer,
            MakeParticipant("race.importer"));
        Token.Reset();
        RacesPassed = RacesPassed && InFlight.IsValid() &&
            !Registry.Acquire(
                EAssetExtensionKind::Importer,
                MakeParticipant("race.importer")).IsValid();
    }
    Record(Result, RacesPassed, "100 unregister races preserve acquired leases");
}

void TestLoaderAndCooker(FAssetCoreTestResult& Result)
{
    FAssetExtensionRegistry Registry;
    const FAssetParticipantId LoaderId = MakeParticipant("loader.synthetic");
    const FAssetParticipantId CookerId = MakeParticipant("cooker.synthetic");
    FAssetRegistrationToken LoaderToken;
    FAssetRegistrationToken CookerToken;
    const bool Registered =
        Registry.Register(MakeShared<FTestLoader>(LoaderId), LoaderToken) ==
            EAssetResult::Success &&
        Registry.Register(MakeShared<FTestCooker>(CookerId), CookerToken) ==
            EAssetResult::Success;

    const FAssetMetadata Metadata = MakeMetadata(
        MakeId("Texture", "Load/Main"),
        "load/source");
    FAssetLoadRequest LoadRequest;
    LoadRequest.Metadata = Metadata;
    const FAssetLoadResult Loaded =
        FAssetDispatch::Load(Registry, LoaderId, LoadRequest);
    FAssetCookRequest CookRequest;
    CookRequest.Metadata = Metadata;
    CookRequest.Payload = Loaded.Payload;
    CookRequest.TargetProfile = FString("mac-arm64");
    const FAssetCookResult Cooked =
        FAssetDispatch::Cook(Registry, CookerId, CookRequest);
    Record(
        Result,
        Registered &&
            !LoadRequest.Parameters &&
            !CookRequest.Parameters &&
            Loaded.Result == EAssetResult::Success &&
            Loaded.Payload &&
            Loaded.Diagnostics.empty() &&
            Cooked.Result == EAssetResult::Success &&
            Cooked.TargetProfile == FString("mac-arm64") &&
            !Cooked.Artifact.empty() &&
            Cooked.CookDigest.IsAvailable() &&
            !Cooked.Payload &&
            Cooked.Diagnostics.empty(),
        "legacy loader and cooker preserve null-parameter behavior");
}

void TestDiagnosticsAndInspection(FAssetCoreTestResult& Result)
{
    FAssetDiagnostic Error;
    Error.Stage = EAssetStage::Import;
    Error.Result = EAssetResult::ProcessingFailure;
    Error.Severity = EAssetDiagnosticSeverity::Error;
    Error.Code = FString("asset.import.failed");
    Error.Subject = FString("Texture:Imported/Main");
    Error.Participant = FString("importer.synthetic");
    Error.Reason = FString("invalid payload");

    FAssetDiagnostic Warning;
    Warning.Stage = EAssetStage::Registry;
    Warning.Result = EAssetResult::IncompleteRegistry;
    Warning.Severity = EAssetDiagnosticSeverity::Warning;
    Warning.Code = FString("asset.registry.incomplete");
    const FString First = FAssetDiagnostics::FormatNormalized({Error, Warning});
    const FString Second = FAssetDiagnostics::FormatNormalized({Warning, Error});
    Record(
        Result,
        First == Second &&
            FAssetDiagnostics::FirstActionable({Warning, Error}) == Error &&
            First.View().find("0x") == std::string_view::npos &&
            First.View().find("thread") == std::string_view::npos,
        "diagnostics normalize ordering and expose first actionable error");

    FAssetRegistry EmptyRegistry;
    const FString EmptyDump = FAssetInspection::Format(EmptyRegistry.Snapshot());
    FAssetExtensionCapability Zeta;
    Zeta.Participant = MakeParticipant("zeta");
    FAssetExtensionCapability Alpha;
    Alpha.Participant = MakeParticipant("alpha");
    Record(
        Result,
        EmptyDump == FString("revision=0") &&
            FAssetInspection::FormatCapabilities({Zeta, Alpha}) ==
                FString("alpha\nzeta") &&
            FAssetInspection::FormatAmbiguity(
                EAssetExtensionKind::Importer,
                {Zeta, Alpha}) == FString("ambiguous-importer\nalpha\nzeta"),
        "inspection formats empty registry and capabilities canonically");
}

} // namespace

FAssetCoreTestResult RunAssetCoreTests()
{
    FAssetCoreTestResult Result;
    TestIdentity(Result);
    TestDigest(Result);
    TestSoftReferences(Result);
    TestSourceAndMetadata(Result);
    TestRegistry(Result);
    TestRegistryConcurrency(Result);
    TestResolverDispatch(Result);
    TestImporterDispatch(Result);
    TestExtensionLifetime(Result);
    TestLoaderAndCooker(Result);
    TestDiagnosticsAndInspection(Result);
    return Result;
}
