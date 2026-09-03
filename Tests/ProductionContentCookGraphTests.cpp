#include "ProductionContentCookGraphTests.h"

#include "Asset/AssetMinimal.h"
#include "AssetCooker/FAssetCookRequest.h"
#include "AssetCookerPublicationTestSupport.h"
#include "AssetCookerTestSupport.h"
#include "Core/FPlatformFileSystem.h"
#include "FAssetCookGraph.h"
#include "FAssetSourceCatalog.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>

namespace
{
using namespace Stoner;
using namespace Stoner::Asset;
using namespace Stoner::AssetCooker;
using namespace Stoner::AssetCooker::Private;

void Record(
    FProductionContentCookGraphTestResult& Result,
    bool bPassed,
    const char* Name)
{
    (bPassed ? ++Result.Passed : ++Result.Failed);
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

FAssetId Id(const char* Type, const char* Path, const char* Subresource = nullptr)
{
    FAssetId Result;
    const std::optional<Core::FString> SubresourceValue = Subresource
        ? std::optional<Core::FString>(Core::FString(Subresource))
        : std::nullopt;
    (void)FAssetId::Create(
        Core::FString(Type), Core::FString(Path), SubresourceValue, Result);
    return Result;
}

AssetCooker::FAssetCookRequest MakeRequest(
    const std::filesystem::path& TemporaryRoot,
    bool bIncludeShaderRoot)
{
    AssetCooker::FAssetCookRequest Result;
    Result.SourceRoots = {
        Core::FString(
            "Content/ProductionAcceptance/Regular/Lantern")};
    if (bIncludeShaderRoot)
        Result.SourceRoots.push_back(
            Core::FString("Content/Shaders"));
    Result.SelectionMode = EAssetCookSelectionMode::ExplicitRoots;
    Result.ExplicitRoots = {
        Id("StaticModel", "Lantern.glb", "idx.scene.0"),
        Id("ShaderProgram", "Engine/Shaders/Deferred/Surface"),
        Id("ShaderProgram", "Engine/Shaders/Deferred/DirectionalLight"),
        Id("ShaderProgram", "Engine/Shaders/Deferred/PointLight"),
        Id("ShaderProgram", "Engine/Shaders/Deferred/SpotLight"),
        Id("ShaderProgram", "Engine/Shaders/Deferred/Composition"),
        Id("ShaderProgram", "Engine/Shaders/PostProcess/OutputTransform")};
    std::sort(Result.ExplicitRoots.begin(), Result.ExplicitRoots.end());
    Result.TargetProfilePath =
        Core::FString("Config/AssetCooker/Profiles/Mac-Vulkan.json");
    Core::TArray<Core::uint8> ProfileBytes;
    (void)Core::FPlatformFileSystem::ReadFile(
        Result.TargetProfilePath, ProfileBytes);
    (void)FAssetCookContractCodec::ParseTargetProfile(
        ProfileBytes, Result.TargetProfile);
    Result.OutputRoot = Core::FString(
        (TemporaryRoot / "Published").generic_string());
    Result.DerivedDataRoot = Core::FString(
        (TemporaryRoot / "DDC").generic_string());
    Result.ScratchRoot = Core::FString(
        (TemporaryRoot / "Scratch").generic_string());
    return Result;
}

bool Contains(const FAssetCookGraphPlan& Plan, const FAssetId& AssetId)
{
    return std::any_of(
        Plan.Nodes.begin(), Plan.Nodes.end(),
        [&AssetId](const auto& Node)
        {
            return Node.Metadata.Id == AssetId;
        });
}

Core::TArray<FAssetId> PlannedIds(const FAssetCookGraphPlan& Plan)
{
    Core::TArray<FAssetId> Result;
    Result.reserve(Plan.Nodes.size());
    for (const auto& Node : Plan.Nodes) Result.push_back(Node.Metadata.Id);
    return Result;
}

} // namespace

FProductionContentCookGraphTestResult RunProductionContentCookGraphTests()
{
    FProductionContentCookGraphTestResult Result;
    const auto Token = std::chrono::steady_clock::now()
        .time_since_epoch().count();
    const auto TemporaryRoot = std::filesystem::temp_directory_path() /
        ("sg-production-cook-graph-" + std::to_string(Token));

    const AssetCooker::FAssetCookRequest CompleteRequest =
        MakeRequest(TemporaryRoot, true);
    FAssetSourceCatalogResult Catalog;
    const EAssetResult Discovered = FAssetSourceCatalog::Discover(
        CompleteRequest, Catalog);
    Record(Result,
        CompleteRequest.Validate() == EAssetResult::Success &&
            Discovered == EAssetResult::Success &&
            CompleteRequest.SourceRoots.size() == 2,
        "production discovery accepts ordered package and shader source roots");

    FAssetCookGraphPlan Plan;
    const EAssetResult Built = FAssetCookGraph::Build(
        Catalog.Outputs,
        CompleteRequest.SelectionMode,
        CompleteRequest.ExplicitRoots,
        {}, Plan);
    const FAssetId Surface = Id(
        "ShaderProgram", "Engine/Shaders/Deferred/Surface");
    const FAssetId Model = Id(
        "StaticModel", "Lantern.glb", "idx.scene.0");
    const FAssetId SurfaceVertex = Id(
        "ShaderPayload", "Engine/Shaders/Deferred/Surface",
        "payload.vulkan.vertex");
    const FAssetId SurfaceFragment = Id(
        "ShaderPayload", "Engine/Shaders/Deferred/Surface",
        "payload.vulkan.fragment");
    Record(Result,
        Built == EAssetResult::Success &&
            Plan.Validate() == EAssetResult::Success &&
            Contains(Plan, Model) &&
            Contains(Plan, Surface) &&
            Contains(Plan, SurfaceVertex) &&
            Contains(Plan, SurfaceFragment),
        "explicit production root closes over the default deferred shader");

    const FAssetId CompositionShader = Id(
        "ShaderProgram", "Engine/Shaders/Deferred/Composition");
    const FAssetId DirectionalShader = Id(
        "ShaderProgram", "Engine/Shaders/Deferred/DirectionalLight");
    const FAssetId PointShader = Id(
        "ShaderProgram", "Engine/Shaders/Deferred/PointLight");
    const FAssetId SpotShader = Id(
        "ShaderProgram", "Engine/Shaders/Deferred/SpotLight");
    const FAssetId OutputTransformShader = Id(
        "ShaderProgram", "Engine/Shaders/PostProcess/OutputTransform");
    const FAssetId OutputTransformVertex = Id(
        "ShaderPayload", "Engine/Shaders/PostProcess/Fullscreen",
        "payload.vulkan.vertex");
    const FAssetId OutputTransformFragment = Id(
        "ShaderPayload", "Engine/Shaders/PostProcess/OutputTransform",
        "payload.vulkan.fragment");
    Record(Result,
        Built == EAssetResult::Success &&
            Contains(Plan, CompositionShader) &&
            Contains(Plan, DirectionalShader) && Contains(Plan, PointShader) &&
            Contains(Plan, SpotShader) && Contains(Plan, OutputTransformShader) &&
            Contains(Plan, OutputTransformVertex) &&
            Contains(Plan, OutputTransformFragment),
        "explicit production graph includes Deferred and formal output shader roots");

    auto OutputsWithGenericAssets = Catalog.Outputs;
    OutputsWithGenericAssets.push_back(
        Stoner::Tests::AssetCooker::Output(
            Stoner::Tests::AssetCooker::Id(
                "Image", "Cooked/UnrelatedImage")));
    OutputsWithGenericAssets.push_back(
        Stoner::Tests::AssetCooker::Output(
            Stoner::Tests::AssetCooker::Id(
                "Texture", "Cooked/UnrelatedTexture")));
    FAssetCookGraphPlan Filtered;
    const EAssetResult FilteredResult = FAssetCookGraph::Build(
        OutputsWithGenericAssets,
        EAssetCookSelectionMode::ExplicitRoots,
        CompleteRequest.ExplicitRoots,
        {}, Filtered);
    Record(Result,
        FilteredResult == EAssetResult::Success &&
            PlannedIds(Filtered) == PlannedIds(Plan),
        "independently discovered generic images and textures do not leak into the explicit closure");

    auto Reversed = Catalog.Outputs;
    std::reverse(Reversed.begin(), Reversed.end());
    FAssetCookGraphPlan Rebuilt;
    Record(Result,
        FAssetCookGraph::Build(
            Reversed,
            EAssetCookSelectionMode::ExplicitRoots,
            CompleteRequest.ExplicitRoots,
            {}, Rebuilt) == EAssetResult::Success &&
            PlannedIds(Rebuilt) == PlannedIds(Plan),
        "production closure order is independent of discovery enumeration");

    FAssetSourceCatalogResult IncompleteCatalog;
    const AssetCooker::FAssetCookRequest IncompleteRequest =
        MakeRequest(TemporaryRoot, false);
    FAssetCookGraphPlan IncompletePlan;
    const EAssetResult IncompleteDiscovery = FAssetSourceCatalog::Discover(
        IncompleteRequest, IncompleteCatalog);
    Record(Result,
        IncompleteDiscovery == EAssetResult::Success &&
            FAssetCookGraph::Build(
                IncompleteCatalog.Outputs,
                IncompleteRequest.SelectionMode,
                IncompleteRequest.ExplicitRoots,
                {}, IncompletePlan) == EAssetResult::UnresolvedDependency &&
            IncompletePlan.Nodes.empty(),
        "missing repository shader root fails without a partial cook graph");

    const auto MutationRoot = TemporaryRoot / "MutationPublication";
    std::filesystem::path SyntheticContent;
    const auto SeedRun =
        Stoner::Tests::AssetCookerPublication::Seed(
            MutationRoot, SyntheticContent);
    const auto PublishedRoot = MutationRoot / "Published";
    const auto Published =
        Stoner::AssetCooker::Private::FCookedGenerationPublisher::Publish(
            Stoner::Tests::AssetCookerPublication::Request(
                SeedRun, PublishedRoot));
    const auto CurrentBefore =
        Stoner::Tests::AssetCookerPublication::Read(
            PublishedRoot / "Current.json");
    auto ChangedRequest =
        Stoner::Tests::AssetCookerPublication::Request(
            SeedRun, PublishedRoot);
    ChangedRequest.RevalidateInputs = []
    {
        return EAssetResult::TransientFailure;
    };
    const auto Changed =
        Stoner::AssetCooker::Private::FCookedGenerationPublisher::Publish(
            ChangedRequest);
    Record(Result,
        Published.Succeeded() &&
            Changed.Category == EAssetCookResultCategory::SourceChanged &&
            !Changed.bCommitted &&
            Stoner::Tests::AssetCookerPublication::Read(
                PublishedRoot / "Current.json") == CurrentBefore &&
            Stoner::Tests::AssetCookerPublication::ValidateCurrent(
                PublishedRoot).Succeeded(),
        "source mutation before publication preserves the prior generation and current pointer");

    std::error_code Error;
    std::filesystem::remove_all(TemporaryRoot, Error);
    return Result;
}
