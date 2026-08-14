#include "AssetCookerWorkflowTests.h"

#include "AssetCookerDerivedDataTestSupport.h"
#include "FAssetCookCli.h"

#include <filesystem>
#include <iostream>

namespace
{

using namespace Stoner;
using namespace Stoner::AssetCooker;
using namespace Stoner::AssetCooker::Private;
using namespace Stoner::Tests::AssetCookerDDC;

void Record(
    FAssetCookerWorkflowTestResult& Result, bool Passed, const char* Label)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Label << '\n';
}

FAssetCookCliResult Run(std::initializer_list<std::string> Values)
{
    Core::TArray<Core::FString> Arguments;
    for (const auto& Value : Values) Arguments.emplace_back(Value);
    FAssetCookCliInvocation Invocation;
    Core::FString Reason;
    if (FAssetCookCli::Parse(Arguments, Invocation, Reason) !=
        EAssetCookResultCategory::Success)
    {
        FAssetCookCliResult Failure;
        Failure.Category = EAssetCookResultCategory::InvalidArguments;
        Failure.StableReason = Reason;
        return Failure;
    }
    return FAssetCookCli::Execute(Invocation);
}

} // namespace

FAssetCookerWorkflowTestResult RunAssetCookerWorkflowTests()
{
    FAssetCookerWorkflowTestResult Result;
    const auto Root = std::filesystem::temp_directory_path() /
        "stoner-asset-cooker-us5-workflow";
    std::filesystem::remove_all(Root);
    const auto Content = Root / "Content";
    const auto Output = Root / "Cooked";
    const auto Ddc = Root / "DDC";
    const auto Reports = Root / "Reports";
    SeedPng(Content);
    const std::string Profile =
        "Config/AssetCooker/Profiles/Mac-Vulkan.json";

    const auto SourceBefore = Read(Content / "Representative.png");
    const auto Plan = Run({"plan", "--source-root", Content.string(),
        "--cook-all", "--target-profile", Profile, "--output",
        Output.string(), "--ddc", Ddc.string(), "--workers", "4",
        "--normalized-report", "--report", (Reports / "plan.json").string()});
    Record(Result, Plan.Succeeded() &&
        !std::filesystem::exists(Output) && !std::filesystem::exists(Ddc) &&
        !std::filesystem::exists(Output.string() + ".scratch") &&
        Read(Content / "Representative.png") == SourceBefore &&
        std::filesystem::exists(Reports / "plan.json") &&
        Plan.CanonicalReport.View().find("\"action\": \"miss\"") !=
            std::string_view::npos,
        "plan explains prospective misses while mutating only its report output");

    const auto Clean = Run({"cook", "--source-root", Content.string(),
        "--cook-all", "--target-profile", Profile, "--output",
        Output.string(), "--ddc", Ddc.string(), "--clean", "--workers", "4",
        "--lease-timeout-ms", "30000", "--normalized-report", "--report",
        (Reports / "clean.json").string()});
    const auto Incremental = Run({"cook", "--source-root", Content.string(),
        "--cook-all", "--target-profile", Profile, "--output",
        Output.string(), "--ddc", Ddc.string(), "--workers", "8",
        "--lease-timeout-ms", "30000", "--normalized-report", "--report",
        (Reports / "incremental.json").string()});
    Record(Result, Clean.Succeeded() && Incremental.Succeeded() &&
        Incremental.CanonicalReport.View().find("\"action\": \"reuse\"") !=
            std::string_view::npos,
        "clean publication followed by unchanged incremental cook reuses DDC");

    const auto CacheValidation = Run({"validate-cache", "--ddc", Ddc.string(),
        "--normalized-report", "--report",
        (Reports / "cache.json").string()});
    const auto EntryDirectory = FirstEntry(Ddc);
    const auto CacheInspection = Run({"inspect", "--ddc", Ddc.string(),
        "--key", EntryDirectory.filename().string(), "--normalized-report",
        "--report", (Reports / "inspect-cache.json").string()});
    const auto ProfileInspection = Run({"inspect", "--target-profile", Profile,
        "--normalized-report", "--report",
        (Reports / "profile.json").string()});
    const auto PublishedInspection = Run({"inspect", "--output", Output.string(),
        "--normalized-report", "--report",
        (Reports / "inspect-published.json").string()});
    Record(Result, CacheValidation.Succeeded() && CacheInspection.Succeeded() &&
        ProfileInspection.Succeeded() && PublishedInspection.Succeeded() &&
        ProfileInspection.CanonicalReport.View().find(
            "\"targetProfileDigest\"") != std::string_view::npos &&
        CacheInspection.CanonicalReport.View().find(
            "\"sourceEvidence\"") != std::string_view::npos &&
        PublishedInspection.CanonicalReport.View().find(
            "\"dependencyEvidence\"") != std::string_view::npos,
        "cache profile and publication inspection emit complete canonical evidence");

    std::filesystem::remove_all(Content);
    std::filesystem::remove_all(Ddc);
    const auto Published = Run({"validate", "--output", Output.string(),
        "--strict-files", "--normalized-report", "--report",
        (Reports / "published.json").string()});
    Record(Result, Published.Succeeded() &&
        Published.CanonicalReport.View().find("\"action\": \"validate\"") !=
            std::string_view::npos,
        "published generation validates after source and DDC removal");

    std::filesystem::remove_all(Root);
    return Result;
}
