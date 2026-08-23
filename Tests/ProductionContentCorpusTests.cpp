#include "ProductionContentCorpusTests.h"

#include "StaticModelTestSupport.h"

#include "Asset/FImageAsset.h"
#include "Asset/FMaterialAsset.h"
#include "Asset/FStaticMeshAsset.h"
#include "Asset/FStaticModelAsset.h"
#include "Asset/FStaticModelInspection.h"
#include "Asset/FTextureAsset.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace
{
using namespace Stoner::Asset;
using namespace Stoner::Core;
using namespace StaticModelTestSupport;

void Record(
    FProductionContentCorpusTestResult& Result,
    bool bPassed,
    const char* Name)
{
    (bPassed ? ++Result.Passed : ++Result.Failed);
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

struct FPackageSummary
{
    usize Models = 0;
    usize Meshes = 0;
    usize Materials = 0;
    usize Images = 0;
    usize Textures = 0;
    usize Primitives = 0;
    usize Nodes = 0;
    bool bHasShaderDependency = false;
};

FPackageSummary Summarize(const TArray<FAssetImportOutput>& Outputs)
{
    FPackageSummary Summary;
    for (const FAssetImportOutput& Output : Outputs)
    {
        if (const auto Model =
                std::dynamic_pointer_cast<const FStaticModelAsset>(Output.Payload))
        {
            ++Summary.Models;
            Summary.Nodes += Model->GetDesc().Nodes.size();
        }
        else if (const auto Mesh =
                     std::dynamic_pointer_cast<const FStaticMeshAsset>(Output.Payload))
        {
            ++Summary.Meshes;
            Summary.Primitives += Mesh->GetDesc().Primitives.size();
        }
        else if (std::dynamic_pointer_cast<const FMaterialAsset>(Output.Payload))
            ++Summary.Materials;
        else if (std::dynamic_pointer_cast<const FImageAsset>(Output.Payload))
            ++Summary.Images;
        else if (std::dynamic_pointer_cast<const FTextureAsset>(Output.Payload))
            ++Summary.Textures;

        for (const FAssetDependency& Dependency : Output.Metadata.Dependencies)
        {
            Summary.bHasShaderDependency = Summary.bHasShaderDependency ||
                Dependency.TargetId.GetAssetType() == FString("ShaderProgram");
        }
    }
    return Summary;
}

bool ImportDeterministically(
    const std::filesystem::path& Path,
    TArray<FAssetImportOutput>& OutOutputs,
    FPackageSummary& OutSummary,
    int Runs)
{
    EAssetResult Result = EAssetResult::ProcessingFailure;
    FAssetDiagnosticList Diagnostics;
    OutOutputs = ImportPackage(Path, Result, &Diagnostics);
    if (Result != EAssetResult::Success)
    {
        std::cout << "[DETAIL] production import result="
                  << static_cast<int>(Result) << " path="
                  << Path.generic_string() << '\n';
        for (const FAssetDiagnostic& Diagnostic : Diagnostics)
            std::cout << "[DETAIL] diagnostic=" << Diagnostic.Code.CStr()
                      << " field=" << Diagnostic.Field.CStr()
                      << " actual=" << Diagnostic.Actual.CStr()
                      << " limit=" << Diagnostic.Limit.CStr()
                      << " reason=" << Diagnostic.Reason.CStr() << '\n';
        return false;
    }

    OutSummary = Summarize(OutOutputs);
    const FString Expected = FStaticModelInspection::FormatPackage(OutOutputs);
    for (int Run = 1; Run < Runs; ++Run)
    {
        EAssetResult RepeatedResult = EAssetResult::ProcessingFailure;
        const auto Repeated = ImportPackage(Path, RepeatedResult);
        if (RepeatedResult != EAssetResult::Success ||
            FStaticModelInspection::FormatPackage(Repeated) != Expected)
            return false;
    }
    return true;
}

} // namespace

FProductionContentCorpusTestResult RunProductionContentCorpusTests()
{
    FProductionContentCorpusTestResult Result;
    const int DeterminismRuns =
        std::getenv("STONER_REQUIRE_PRODUCTION_DETERMINISM") != nullptr ? 20 : 1;
    const std::filesystem::path LanternPath =
        "Content/ProductionAcceptance/Regular/Lantern/Lantern.glb";
    TArray<FAssetImportOutput> LanternOutputs;
    FPackageSummary Lantern;
    const bool bLanternDeterministic = ImportDeterministically(
        LanternPath, LanternOutputs, Lantern, DeterminismRuns);
    std::cout << "[EVIDENCE] lantern models=" << Lantern.Models
              << " meshes=" << Lantern.Meshes
              << " materials=" << Lantern.Materials
              << " images=" << Lantern.Images
              << " textures=" << Lantern.Textures
              << " primitives=" << Lantern.Primitives
              << " nodes=" << Lantern.Nodes << '\n';
    Record(Result,
        bLanternDeterministic && Lantern.Models >= 1 && Lantern.Meshes >= 1 &&
            Lantern.Materials >= 2 && Lantern.Images >= 3 &&
            Lantern.Textures >= 3 && Lantern.Primitives >= 3 &&
            Lantern.Nodes >= 2 && Lantern.bHasShaderDependency,
        "real embedded Lantern GLB imports through the ordinary typed path");
    Record(Result, bLanternDeterministic,
        "Lantern typed identities and dependency order are stable across configured imports");

    const std::filesystem::path SponzaPath =
        "Content/ProductionAcceptance/External/Sponza/Sponza.gltf";
    const bool bRequireMedium =
        std::getenv("STONER_REQUIRE_PRODUCTION_MEDIUM") != nullptr;
    if (!std::filesystem::is_regular_file(SponzaPath))
    {
        Record(Result, !bRequireMedium,
            "external Sponza is optional unless the medium gate requires it");
        return Result;
    }

    TArray<FAssetImportOutput> SponzaOutputs;
    FPackageSummary Sponza;
    const bool bSponzaDeterministic = ImportDeterministically(
        SponzaPath, SponzaOutputs, Sponza, DeterminismRuns);
    std::cout << "[EVIDENCE] sponza models=" << Sponza.Models
              << " meshes=" << Sponza.Meshes
              << " materials=" << Sponza.Materials
              << " images=" << Sponza.Images
              << " textures=" << Sponza.Textures
              << " primitives=" << Sponza.Primitives
              << " nodes=" << Sponza.Nodes << '\n';
    Record(Result,
        bSponzaDeterministic && Sponza.Models >= 1 && Sponza.Meshes >= 1 &&
            Sponza.Materials >= 20 && Sponza.Images >= 60 &&
            Sponza.Textures >= 60 && Sponza.Primitives >= 100 &&
            Sponza.Nodes >= 1 && Sponza.bHasShaderDependency,
        "real external-dependency Sponza imports through the ordinary typed path");
    Record(Result, bSponzaDeterministic,
        "Sponza typed identities and dependency order are stable across configured imports");
    return Result;
}
