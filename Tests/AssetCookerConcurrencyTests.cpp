#include "AssetCookerConcurrencyTests.h"

#include "AssetCookerDerivedDataTestSupport.h"
#include "Core/FPlatformFileLease.h"
#include "FDerivedDataStore.h"

#include <chrono>
#include <filesystem>
#include <future>
#include <iostream>

FAssetCookerConcurrencyTestResult RunAssetCookerConcurrencyTests()
{
    using namespace Stoner;
    using namespace Stoner::Tests::AssetCookerDDC;
    FAssetCookerConcurrencyTestResult Result;
    const auto Root = std::filesystem::temp_directory_path() / "stoner-ddc-concurrency";
    std::filesystem::remove_all(Root);
    const auto Content = Root / "Content";
    SeedPng(Content);
    Core::TArray<std::future<FRun>> Futures;
    for (int Index = 0; Index < 8; ++Index)
    {
        auto Value = Request(Root / ("Run-" + std::to_string(Index)), Content, 1);
        Value.DerivedDataRoot = Core::FString((Root / "SharedDDC").generic_string());
        Futures.push_back(std::async(std::launch::async,
            [Value]() { return Run(Value); }));
    }
    Core::TArray<FRun> Runs;
    for (auto& Future : Futures) Runs.push_back(Future.get());
    bool Converged = !Runs.empty() && Runs.front().Result.Succeeded();
    for (const auto& Current : Runs)
        Converged = Converged && Current.Result.Succeeded() &&
            Current.Result.CanonicalManifest == Runs.front().Result.CanonicalManifest &&
            EqualArtifacts(Current.Result.Artifacts, Runs.front().Result.Artifacts);
    const auto Verify = Run([&]
    {
        auto Value = Request(Root / "Verify", Content, 4);
        Value.DerivedDataRoot = Core::FString((Root / "SharedDDC").generic_string());
        Value.CachePolicy = AssetCooker::EAssetCookCachePolicy::StrictValidate;
        return Value;
    }());
    Converged = Converged && Verify.Result.Succeeded() &&
        Verify.Report.Counts.Reused == 2;
    (Converged ? ++Result.Passed : ++Result.Failed);
    std::cout << (Converged ? "[PASS] " : "[FAIL] ")
              << "eight same-key writers converge to complete immutable entries\n";

    const auto CorruptEntry = FirstEntry(Root / "SharedDDC");
    const auto ValidPayload = Read(CorruptEntry / "Payload.sgasset");
    auto CorruptPayload = ValidPayload;
    if (!CorruptPayload.empty()) CorruptPayload.pop_back();
    Write(CorruptEntry / "Payload.sgasset", CorruptPayload);

    Asset::FAssetDerivedDataEntry Entry;
    const bool ParsedEntry = Asset::FAssetCookContractCodec::ParseDerivedDataEntry(
        Read(CorruptEntry / "Entry.json"), {}, Entry) == Asset::EAssetResult::Success;
    AssetCooker::Private::FDerivedDataLookupRequest LookupRequest;
    LookupRequest.Root = Core::FString((Root / "SharedDDC").generic_string());
    LookupRequest.DerivedKey = Entry.DerivedKey;
    LookupRequest.Evidence = Entry.Evidence;
    LookupRequest.RequiredExtensions = Entry.RequiredExtensions;
    const auto InvalidEntry = AssetCooker::Private::FDerivedDataStore::Lookup(
        LookupRequest);
    const auto StorePaths = AssetCooker::Private::FDerivedDataStore::PathsFor(
        LookupRequest.Root, LookupRequest.DerivedKey);
    Core::FPlatformFileLease WinnerLease;
    const bool HeldWinnerLease = Core::FPlatformFileLease::Acquire(
        StorePaths.LeaseFile, 1000, Core::FString("test.ddc-winner"),
        WinnerLease).IsSuccess();
    auto TimedOutQuarantine = std::async(std::launch::async,
        [LookupRequest, InvalidEntry]
        {
            return AssetCooker::Private::FDerivedDataStore::Quarantine(
                LookupRequest, InvalidEntry, std::chrono::milliseconds(30));
        });
    Write(CorruptEntry / "Payload.sgasset", ValidPayload);
    const auto WinnerResult = TimedOutQuarantine.get();
    WinnerLease.Release();
    const bool LeaseRaceResolved = ParsedEntry &&
        InvalidEntry.Status == AssetCooker::Private::EDerivedDataLookupStatus::Invalid &&
        HeldWinnerLease && WinnerResult.Result == Asset::EAssetResult::Success &&
        WinnerResult.bEntryWasReplaced && !WinnerResult.bEntryQuarantined;
    (LeaseRaceResolved ? ++Result.Passed : ++Result.Failed);
    std::cout << (LeaseRaceResolved ? "[PASS] " : "[FAIL] ")
              << "quarantine lease timeout re-queries an installed winner\n";

    Write(CorruptEntry / "Payload.sgasset", CorruptPayload);
    Futures.clear();
    for (int Index = 0; Index < 8; ++Index)
    {
        auto Value = Request(Root / ("Repair-" + std::to_string(Index)), Content, 1);
        Value.DerivedDataRoot = Core::FString((Root / "SharedDDC").generic_string());
        Futures.push_back(std::async(std::launch::async,
            [Value]() { return Run(Value); }));
    }
    Runs.clear();
    for (auto& Future : Futures) Runs.push_back(Future.get());
    bool Repaired = !Runs.empty() && Runs.front().Result.Succeeded();
    for (const auto& Current : Runs)
        Repaired = Repaired && Current.Result.Succeeded() &&
            Current.Result.CanonicalManifest == Runs.front().Result.CanonicalManifest &&
            EqualArtifacts(Current.Result.Artifacts, Runs.front().Result.Artifacts);
    if (!Repaired)
    {
        for (Core::usize Index = 0; Index < Runs.size(); ++Index)
        {
            const auto& Current = Runs[Index];
            std::cout << "  repair[" << Index << "] category="
                      << static_cast<int>(Current.Result.Category)
                      << " reason=" << Current.Result.StableReason.ToStdString()
                      << " reused=" << Current.Report.Counts.Reused
                      << " invalidated=" << Current.Report.Counts.Invalidated
                      << " quarantined=" << Current.Report.Counts.Quarantined
                      << " failed=" << Current.Report.Counts.Failed << '\n';
        }
    }
    (Repaired ? ++Result.Passed : ++Result.Failed);
    std::cout << (Repaired ? "[PASS] " : "[FAIL] ")
              << "quarantine races re-query the winner and converge cleanly\n";
    std::filesystem::remove_all(Root);
    return Result;
}
