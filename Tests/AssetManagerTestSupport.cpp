#include "AssetManagerTestSupport.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <thread>

namespace
{
using namespace Stoner;
using namespace Stoner::Asset;
using namespace Stoner::Core;

FAssetParticipantId Participant(const char* Value)
{
    FAssetParticipantId Result;
    (void)FAssetParticipantId::Create(FString(Value), Result);
    return Result;
}

FAssetProducerVersion Version()
{
    FAssetProducerVersion Result;
    (void)FAssetProducerVersion::Create(FString("1.0.0"), Result);
    return Result;
}

class FMutableMemorySource final : public IAssetSource
{
public:
    explicit FMutableMemorySource(TArray<uint8> Bytes)
        : Bytes_(std::move(Bytes))
    {
    }

    EAssetResult Read(
        uint64 Offset,
        usize MaximumBytes,
        TArray<uint8>& OutBytes) const override
    {
        std::lock_guard Lock(Mutex_);
        if (Offset > Bytes_.size()) return EAssetResult::MalformedSource;
        const usize Begin = static_cast<usize>(Offset);
        const usize Count = std::min(MaximumBytes, Bytes_.size() - Begin);
        OutBytes.assign(Bytes_.begin() + static_cast<std::ptrdiff_t>(Begin),
            Bytes_.begin() + static_cast<std::ptrdiff_t>(Begin + Count));
        return EAssetResult::Success;
    }

    [[nodiscard]] uint64 Size() const
    {
        std::lock_guard Lock(Mutex_);
        return static_cast<uint64>(Bytes_.size());
    }

    void Mutate()
    {
        std::lock_guard Lock(Mutex_);
        Bytes_.push_back(0x26);
    }

private:
    mutable std::mutex Mutex_;
    TArray<uint8> Bytes_;
};

class FRuntimeTestResolver final : public IAssetResolver
{
public:
    FRuntimeTestResolver(
        TSharedPtr<FMutableMemorySource> Source,
        TSharedPtr<std::atomic<int>> Calls)
        : Source_(std::move(Source)), Calls_(std::move(Calls))
    {
    }

    FAssetExtensionCapability GetCapability() const override
    {
        FAssetExtensionCapability Result;
        Result.Kind = EAssetExtensionKind::Resolver;
        Result.Participant = Participant("resolver.runtime-test");
        Result.ProducerVersion = Version();
        Result.Priority = 100;
        Result.Schemes = {FString("asset")};
        Result.bRuntimeCompatible = true;
        return Result;
    }

    FAssetResolveResult Resolve(const FAssetResolveRequest& Request) override
    {
        ++(*Calls_);
        if (Request.RuntimeContext && Request.RuntimeContext->ShouldStop())
            return {EAssetResult::Cancelled, {}, {}};
        FAssetSourceDescriptor Descriptor;
        Descriptor.Location = Request.Location;
        Descriptor.Size = Source_->Size();
        Descriptor.FormatHint = FString("runtime-test");
        return {EAssetResult::Success, Descriptor, FAssetSourceLease(Source_)};
    }

private:
    TSharedPtr<FMutableMemorySource> Source_;
    TSharedPtr<std::atomic<int>> Calls_;
};

class FRuntimeTestImporter final : public IAssetImporter
{
public:
    FRuntimeTestImporter(
        FAssetId Id,
        FString Value,
        TSharedPtr<FMutableMemorySource> Source,
        TSharedPtr<std::atomic<int>> Calls,
        TSharedPtr<std::atomic<bool>> MutateAfterImport)
        : Id_(std::move(Id)), Value_(std::move(Value)),
          Source_(std::move(Source)), Calls_(std::move(Calls)),
          MutateAfterImport_(std::move(MutateAfterImport))
    {
    }

    FAssetExtensionCapability GetCapability() const override
    {
        FAssetExtensionCapability Result;
        Result.Kind = EAssetExtensionKind::Importer;
        Result.Participant = Participant("importer.runtime-test");
        Result.ProducerVersion = Version();
        Result.Priority = 100;
        Result.FormatHints = {FString("runtime-test")};
        Result.ProbeByteLimit = 64;
        Result.bRuntimeCompatible = true;
        return Result;
    }

    FAssetProbeResult Probe(
        const FAssetSourceDescriptor& Descriptor,
        std::span<const uint8> Prefix) override
    {
        (void)Descriptor;
        return Prefix.empty()
            ? FAssetProbeResult{EAssetResult::MalformedSource, 0, {}}
            : FAssetProbeResult{EAssetResult::Success, 100, FString("test")};
    }

    EAssetResult Import(
        const FAssetImportRequest& Request,
        TArray<FAssetImportOutput>& OutOutputs) override
    {
        ++(*Calls_);
        if (!Request.RuntimeContext || Request.RuntimeContext->ShouldStop())
            return EAssetResult::Cancelled;

        TArray<uint8> Bytes;
        if (Request.Source.ReadBounded(1024, Request.Descriptor.Size, Bytes) !=
            EAssetResult::Success)
            return EAssetResult::MalformedSource;

        FAssetMetadata Metadata;
        Metadata.Id = Id_;
        Metadata.Source = Request.Descriptor.Location;
        Metadata.Producer = Participant("importer.runtime-test");
        Metadata.ProducerVersion = Version();
        Metadata.Version.SourceDigest = FAssetDigest::FromBytes(Bytes);
        Metadata.Version.ContentDigest = FAssetDigest::FromBytes(
            std::span<const uint8>(
                reinterpret_cast<const uint8*>(Value_.View().data()),
                Value_.View().size()));
        Metadata.Version.Producer = Metadata.Producer;
        Metadata.Version.ProducerVersion = Metadata.ProducerVersion;
        OutOutputs = {{std::move(Metadata),
            MakeShared<FRuntimeTestPayload>(Value_)}};
        if (MutateAfterImport_->exchange(false)) Source_->Mutate();
        return EAssetResult::Success;
    }

    EAssetResult Import(
        const FAssetSourceDescriptor& Descriptor,
        const FAssetSourceLease& Source,
        TArray<FAssetImportOutput>& OutOutputs) override
    {
        return Import({Descriptor, Source, {}, {}, {}}, OutOutputs);
    }

private:
    FAssetId Id_;
    FString Value_;
    TSharedPtr<FMutableMemorySource> Source_;
    TSharedPtr<std::atomic<int>> Calls_;
    TSharedPtr<std::atomic<bool>> MutateAfterImport_;
};
} // namespace

Stoner::Asset::FAssetId MakeRuntimeTestId(const char* LogicalPath)
{
    Stoner::Asset::FAssetId Result;
    (void)Stoner::Asset::FAssetId::Create(
        Stoner::Core::FString("RuntimeTest"),
        Stoner::Core::FString(LogicalPath), {}, Result);
    return Result;
}

Stoner::Core::TSharedPtr<const Stoner::Asset::FAssetTargetProfileEvidence>
LoadRuntimeTestTargetEvidence()
{
    using namespace Stoner;
    using namespace Stoner::Asset;
    std::ifstream Input(
        "Tests/Fixtures/AssetCooker/Contracts/Profiles/mac-vulkan.json",
        std::ios::binary);
    const std::string Text{
        std::istreambuf_iterator<char>(Input),
        std::istreambuf_iterator<char>()};
    FAssetTargetProfileEvidence Evidence;
    if (FAssetCookContractCodec::ParseTargetProfile(
            std::span<const Core::uint8>(
                reinterpret_cast<const Core::uint8*>(Text.data()), Text.size()),
            Evidence) != EAssetResult::Success)
        return {};
    return Core::MakeShared<const FAssetTargetProfileEvidence>(
        std::move(Evidence));
}

FAssetManagerTestExtensions MakeRuntimeTestExtensions(
    const Stoner::Asset::FAssetId& Id,
    const char* Value)
{
    using namespace Stoner;
    using namespace Stoner::Asset;
    FAssetManagerTestExtensions Result;
    Result.Registry = Core::MakeShared<FAssetExtensionRegistry>();
    Result.ResolveCalls = Core::MakeShared<std::atomic<int>>(0);
    Result.ImportCalls = Core::MakeShared<std::atomic<int>>(0);
    Result.MutateAfterImport = Core::MakeShared<std::atomic<bool>>(false);
    auto Source = Core::MakeShared<FMutableMemorySource>(
        Core::TArray<Core::uint8>{'S', 'G', '2', '6'});
    (void)Result.Registry->Register(
        Core::MakeShared<FRuntimeTestResolver>(Source, Result.ResolveCalls),
        Result.ResolverToken);
    (void)Result.Registry->Register(
        Core::MakeShared<FRuntimeTestImporter>(Id, Core::FString(Value), Source,
            Result.ImportCalls, Result.MutateAfterImport),
        Result.ImporterToken);
    return Result;
}

FAssetManagerTestExtensions MakeRuntimeImageExtensions(
    Stoner::Core::TArray<Stoner::Core::uint8> Bytes)
{
    using namespace Stoner;
    using namespace Stoner::Asset;
    FAssetManagerTestExtensions Result;
    Result.Registry = Core::MakeShared<FAssetExtensionRegistry>();
    Result.ResolveCalls = Core::MakeShared<std::atomic<int>>(0);
    Result.ImportCalls = Core::MakeShared<std::atomic<int>>(0);
    Result.MutateAfterImport = Core::MakeShared<std::atomic<bool>>(false);
    auto Source = Core::MakeShared<FMutableMemorySource>(std::move(Bytes));
    (void)Result.Registry->Register(
        Core::MakeShared<FRuntimeTestResolver>(Source, Result.ResolveCalls),
        Result.ResolverToken);
    (void)RegisterImageAssetImporter(*Result.Registry, Result.ImporterToken);
    return Result;
}

Stoner::Asset::FAssetManagerConfig MakeDevelopmentManagerConfig(
    const FAssetManagerTestExtensions& Extensions)
{
    Stoner::Asset::FAssetManagerConfig Config;
    Config.Mode = Stoner::Asset::EAssetManagerMode::DevelopmentSource;
    Config.ExtensionRegistry = Extensions.Registry;
    Config.SourceRoot = Stoner::Core::FString("Content");
    Config.TargetEvidence = LoadRuntimeTestTargetEvidence();
    Config.WorkerCount = 2;
    Config.Limits.MaxRequests = 32;
    Config.Limits.MaxQueuedWork = 32;
    Config.Limits.MaxCompletions = 32;
    Config.Limits.MaxPayloadBytes = 1024;
    Config.Limits.MaxAggregatePayloadBytes = 4096;
    return Config;
}

bool WaitForRequestTerminal(
    const Stoner::Asset::FAssetManager& Manager,
    Stoner::Asset::FAssetRequestHandle Request,
    Stoner::Asset::FAssetRequestSnapshot& OutSnapshot)
{
    using namespace std::chrono_literals;
    for (int Attempt = 0; Attempt < 200; ++Attempt)
    {
        if (Manager.Query(Request, OutSnapshot) !=
            Stoner::Asset::EAssetResult::Success)
            return false;
        if (OutSnapshot.State == Stoner::Asset::EAssetRequestState::Ready ||
            OutSnapshot.State == Stoner::Asset::EAssetRequestState::Failed ||
            OutSnapshot.State == Stoner::Asset::EAssetRequestState::Cancelled)
            return true;
        std::this_thread::sleep_for(5ms);
    }
    return false;
}
