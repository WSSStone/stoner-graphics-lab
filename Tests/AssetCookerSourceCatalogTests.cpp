#include "AssetCookerSourceCatalogTests.h"

#include "Asset/AssetMinimal.h"
#include "AssetCooker/FAssetCookRequest.h"
#include "FAssetSourceCatalog.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace
{
using namespace Stoner;
using namespace Stoner::Asset;
using namespace Stoner::AssetCooker;
using namespace Stoner::AssetCooker::Private;

void Record(
    FAssetCookerSourceCatalogTestResult& Result,
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
    Output.write(reinterpret_cast<const char*>(Bytes.data()), Bytes.size());
}

AssetCooker::FAssetCookRequest Request(
    const std::filesystem::path& Base,
    Core::TArray<Core::FString> SourceRoots)
{
    AssetCooker::FAssetCookRequest Value;
    Value.SourceRoots = std::move(SourceRoots);
    Value.SelectionMode = EAssetCookSelectionMode::CookAll;
    Value.TargetProfilePath = Core::FString(
        "Tests/Fixtures/AssetCooker/Contracts/Profiles/mac-vulkan.json");
    const auto ProfileBytes = Read(Value.TargetProfilePath.ToStdString());
    (void)FAssetCookContractCodec::ParseTargetProfile(
        ProfileBytes, Value.TargetProfile);
    Value.OutputRoot = Core::FString((Base / "Output").string());
    Value.DerivedDataRoot = Core::FString((Base / "DDC").string());
    Value.ScratchRoot = Core::FString((Base / "Scratch").string());
    return Value;
}

void TestStableDiscovery(FAssetCookerSourceCatalogTestResult& Result)
{
    const auto Base = std::filesystem::temp_directory_path() /
        "stoner-asset-catalog-stable";
    std::filesystem::remove_all(Base);
    const auto Root = Base / "Content";
    const auto Png = Read("Tests/Fixtures/Images/Valid/png-rgb-3x5.png");
    Write(Root / "Z.png", Png);
    Write(Root / "Nested" / "A.png", Png);
    Write(Root / "Ignored.txt", {1, 2, 3});
    FAssetSourceCatalogResult Catalog;
    const EAssetResult Discovered = FAssetSourceCatalog::Discover(
        Request(Base, {Core::FString(Root.string())}), Catalog);
    bool Ordered = Discovered == EAssetResult::Success &&
        Catalog.Sources.size() == 2 && Catalog.Outputs.size() == 4 &&
        Catalog.Sources[0].NormalizedRelativePath <
            Catalog.Sources[1].NormalizedRelativePath;
    Record(Result, Ordered,
        "bounded source discovery emits supported outputs in normalized order");
    Record(Result,
        Catalog.SnapshotSources.size() == Catalog.Sources.size(),
        "every discovered authoritative source is available for snapshot pinning");
    std::filesystem::remove_all(Base);
}

void TestEmptyAndCollision(FAssetCookerSourceCatalogTestResult& Result)
{
    const auto Base = std::filesystem::temp_directory_path() /
        "stoner-asset-catalog-edge";
    std::filesystem::remove_all(Base);
    const auto Empty = Base / "Empty";
    std::filesystem::create_directories(Empty);
    FAssetSourceCatalogResult Catalog;
    Record(Result,
        FAssetSourceCatalog::Discover(
            Request(Base, {Core::FString(Empty.string())}), Catalog) ==
            EAssetResult::NotFound && Catalog.Outputs.empty(),
        "empty cook-all discovery fails without a partial catalog");

    const auto First = Base / "First";
    const auto Second = Base / "Second";
    const auto Png = Read("Tests/Fixtures/Images/Valid/png-rgb-3x5.png");
    Write(First / "Alias.png", Png);
    Write(Second / "alias.PNG", Png);
    Record(Result,
        FAssetSourceCatalog::Discover(
            Request(Base, {
                Core::FString(First.string()),
                Core::FString(Second.string())}), Catalog) ==
            EAssetResult::Conflict && Catalog.Outputs.empty(),
        "case-folded locator aliases fail before catalog publication");
    std::filesystem::remove_all(Base);
}

void TestDependencySnapshot(FAssetCookerSourceCatalogTestResult& Result)
{
    const auto Base = std::filesystem::temp_directory_path() /
        "stoner-asset-catalog-dependencies";
    std::filesystem::remove_all(Base);
    const auto Root = Base / "Content";
    std::filesystem::create_directories(Root);
    for (const char* Name : {
             "Surface.shader.json", "Surface.vert", "Surface.frag",
             "Surface.vert.spv", "Surface.frag.spv"})
        std::filesystem::copy_file(
            std::filesystem::path("Tests/Fixtures/AssetCooker/Representative") /
                Name,
            Root / Name,
            std::filesystem::copy_options::overwrite_existing);
    FAssetSourceCatalogResult Catalog;
    const EAssetResult Discovered = FAssetSourceCatalog::Discover(
        Request(Base, {Core::FString(Root.string())}), Catalog);
    Record(Result,
        Discovered == EAssetResult::Success && Catalog.Sources.size() == 1 &&
            Catalog.Outputs.size() == 5 && Catalog.SnapshotSources.size() == 5,
        "shader definition and four consumed dependencies are snapshot-pinned");
    Record(Result,
        Discovered == EAssetResult::Success && Catalog.Revalidate &&
            std::all_of(
                Catalog.SnapshotSources.begin(), Catalog.SnapshotSources.end(),
                [&Catalog](const auto& Source)
                {
                    return Catalog.Revalidate(Source.Descriptor.Location).Result ==
                        EAssetResult::Success;
                }),
        "every pinned dependency can be independently re-resolved");
    std::filesystem::remove_all(Base);
}
} // namespace

FAssetCookerSourceCatalogTestResult RunAssetCookerSourceCatalogTests()
{
    FAssetCookerSourceCatalogTestResult Result;
    TestStableDiscovery(Result);
    TestEmptyAndCollision(Result);
    TestDependencySnapshot(Result);
    return Result;
}
