#include "Asset/AssetMinimal.h"

#include <chrono>
#include <iostream>
#include <optional>
#include <string>

namespace
{

using namespace Stoner::Asset;
using namespace Stoner::Core;

FAssetId MakeId(int Index)
{
    FAssetId Id;
    (void)FAssetId::Create(
        FString("Texture"),
        FString("Benchmark/Asset" + std::to_string(Index)),
        std::nullopt,
        Id);
    return Id;
}

FAssetMetadata MakeMetadata(int Index)
{
    FAssetMetadata Metadata;
    Metadata.Id = MakeId(Index);
    (void)FAssetSourceLocator::Create(
        FString("mem"),
        FString("benchmark/source" + std::to_string(Index / 100)),
        Metadata.Source);
    (void)FAssetParticipantId::Create(FString("benchmark.importer"), Metadata.Producer);
    (void)FAssetProducerVersion::Create(FString("1.0"), Metadata.ProducerVersion);
    for (int Edge = 1; Edge <= 5 && Edge <= Index; ++Edge)
    {
        FAssetDependency Dependency;
        Dependency.TargetId = MakeId(Index - Edge);
        Metadata.Dependencies.push_back(std::move(Dependency));
    }
    if (Index >= 6 && Index <= 20)
    {
        FAssetDependency ExtraDependency;
        ExtraDependency.TargetId = MakeId(0);
        Metadata.Dependencies.push_back(std::move(ExtraDependency));
    }
    return Metadata;
}

template <typename TCallable>
long long MeasureMilliseconds(TCallable&& Callable)
{
    const auto Start = std::chrono::steady_clock::now();
    Callable();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - Start).count();
}

} // namespace

int main()
{
    constexpr int RecordCount = 10000;
    FAssetRegistry Registry;
    FAssetMutationBatch Batch;
    for (int Index = 0; Index < RecordCount; ++Index)
    {
        Batch.Register(MakeMetadata(Index));
    }

    EAssetResult MutationResult = EAssetResult::ProcessingFailure;
    const long long MutationMs = MeasureMilliseconds([&]
    {
        MutationResult = Registry.Apply(Batch);
    });
    if (MutationResult != EAssetResult::Success)
    {
        std::cerr << "benchmark registry setup failed\n";
        return 1;
    }

    std::size_t QueryCount = 0;
    const long long QueryMs = MeasureMilliseconds([&]
    {
        for (int Index = 0; Index < RecordCount; ++Index)
        {
            QueryCount += Registry.GetDependencies(MakeId(Index)).size();
        }
    });

    FString Dump;
    const long long DumpMs = MeasureMilliseconds([&]
    {
        Dump = FAssetInspection::Format(Registry.Snapshot());
    });
    std::cout << "records=" << Registry.Snapshot().Records.size()
              << " edges=" << QueryCount
              << " mutation_ms=" << MutationMs
              << " query_ms=" << QueryMs
              << " dump_ms=" << DumpMs
              << " dump_bytes=" << Dump.Len() << '\n';
    return QueryCount == 50000 ? 0 : 1;
}
