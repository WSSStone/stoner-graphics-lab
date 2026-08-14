#include "AssetCookerPublicationTests.h"

#include "AssetCookerPublicationTestSupport.h"

#include <filesystem>
#include <iostream>

FAssetCookerPublicationTestResult RunAssetCookerPublicationTests()
{
    using namespace Stoner;
    using namespace Stoner::Tests::AssetCookerPublication;
    namespace Private = AssetCooker::Private;
    FAssetCookerPublicationTestResult Result;
    const auto Root = std::filesystem::temp_directory_path() /
        "stoner-cooker-publication";
    std::filesystem::remove_all(Root);
    std::filesystem::path Content;
    const FRun SeedRun = Seed(Root, Content);
    const auto Output = Root / "Published";
    const auto First = Private::FCookedGenerationPublisher::Publish(
        Request(SeedRun, Output));
    const auto Before = Read(Output / "Current.json");
    Record(Result.Passed, Result.Failed,
        SeedRun.Result.Succeeded() && First.Succeeded() && First.bCommitted &&
            ValidateCurrent(Output).Result == Asset::EAssetResult::Success,
        "a complete generation becomes current through one atomic pointer");

    const auto Repeated = Private::FCookedGenerationPublisher::Publish(
        Request(SeedRun, Output));
    Record(Result.Passed, Result.Failed,
        Repeated.Succeeded() && Repeated.bCommitted &&
            Repeated.GenerationDirectory == First.GenerationDirectory &&
            Read(Output / "Current.json") == Before &&
            ValidateCurrent(Output).Result == Asset::EAssetResult::Success,
        "an equivalent installed generation is published idempotently");

    const Core::TArray<Private::EPublicationBoundary> PreCommit{
        Private::EPublicationBoundary::RequestImageValidation,
        Private::EPublicationBoundary::LeaseAcquired,
        Private::EPublicationBoundary::OutputStageCreate,
        Private::EPublicationBoundary::PayloadCopy,
        Private::EPublicationBoundary::ManifestCopy,
        Private::EPublicationBoundary::StagedValidation,
        Private::EPublicationBoundary::InputRevalidation,
        Private::EPublicationBoundary::GenerationInstall,
        Private::EPublicationBoundary::PointerWrite,
        Private::EPublicationBoundary::PointerReplace};
    bool Preserved = !Before.empty();
    for (const auto Boundary : PreCommit)
    {
        Private::FPublicationTestHooks Hooks;
        Hooks.ShouldFail = [Boundary](Private::EPublicationBoundary Current)
        { return Current == Boundary; };
        const auto Attempt = Private::FCookedGenerationPublisher::Publish(
            Request(SeedRun, Output, &Hooks));
        Preserved = Preserved && !Attempt.Succeeded() && !Attempt.bCommitted &&
            Read(Output / "Current.json") == Before &&
            ValidateCurrent(Output).Result == Asset::EAssetResult::Success;
    }
    Record(Result.Passed, Result.Failed, Preserved,
        "every injected pre-commit failure preserves the previous current generation");

    Private::FPublicationTestHooks AuditHooks;
    AuditHooks.ShouldFail = [](Private::EPublicationBoundary Boundary)
    { return Boundary == Private::EPublicationBoundary::PostCommitAudit; };
    const auto Audit = Private::FCookedGenerationPublisher::Publish(
        Request(SeedRun, Output, &AuditHooks));
    Record(Result.Passed, Result.Failed,
        Audit.Succeeded() && Audit.bCommitted && Audit.bPostCommitAuditWarning &&
            ValidateCurrent(Output).Result == Asset::EAssetResult::Success,
        "post-commit audit failure remains committed success with a warning");

    Private::FPublicationTestHooks CleanupHooks;
    CleanupHooks.ShouldFail = [](Private::EPublicationBoundary Boundary)
    { return Boundary == Private::EPublicationBoundary::Cleanup; };
    const auto Cleanup = Private::FCookedGenerationPublisher::Publish(
        Request(SeedRun, Output, &CleanupHooks));
    Record(Result.Passed, Result.Failed,
        Cleanup.Succeeded() &&
            ValidateCurrent(Output).Result == Asset::EAssetResult::Success,
        "best-effort staging cleanup cannot invalidate committed content");
    std::filesystem::remove_all(Root);
    return Result;
}
