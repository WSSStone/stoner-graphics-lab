#include "AssetGLTFLimitTests.h"

#include "StaticModelTestSupport.h"

#include <iostream>

namespace
{
using namespace Stoner::Asset;
using namespace Stoner::Core;
using namespace StaticModelTestSupport;

void Record(FAssetGLTFLimitTestResult& Result, bool Passed, const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

EAssetResult ImportProfile(const char* Path, FStaticModelImportProfile Profile,
    FAssetDiagnosticList* Diagnostics = nullptr)
{
    FStaticModelImportRequest Request;
    Request.AssetRequest = MakeRequest(Path, {}, Profile);
    Request.Profile = MakeShared<FStaticModelImportProfile>(std::move(Profile));
    TArray<FAssetImportOutput> Outputs;
    return ImportStaticModel(Request, Outputs, Diagnostics);
}
}

FAssetGLTFLimitTestResult RunAssetGLTFLimitTests()
{
    FAssetGLTFLimitTestResult Result;
    constexpr const char* Geometry =
        "Tests/Fixtures/StaticModel/Valid/Geometry/01-basis-u16.gltf";

    FStaticModelImportProfile SourceLimit;
    SourceLimit.Limits.MaxMainSourceBytes = 1;
    Record(Result,
        ImportProfile(Geometry, SourceLimit) == EAssetResult::ImageLimitExceeded,
        "main source byte limit fails before parsing");

    FStaticModelImportProfile AllocationLimit;
    AllocationLimit.Limits.MaxParserAllocationBytes = 1;
    Record(Result,
        ImportProfile(Geometry, AllocationLimit) == EAssetResult::CapacityExceeded,
        "parser allocation budget fails without a partial document");

    FStaticModelImportProfile CountLimit;
    CountLimit.Limits.MaxMeshes = 1;
    Record(Result,
        ImportProfile(
            "Tests/Fixtures/StaticModel/Valid/Hierarchy/06-fallback-unreferenced.gltf",
            CountLimit) == EAssetResult::CapacityExceeded,
        "first mesh count above the configured limit fails closed");

    FStaticModelImportProfile DepthLimit;
    DepthLimit.Limits.MaxHierarchyDepth = 1;
    Record(Result,
        ImportProfile(
            "Tests/Fixtures/StaticModel/Valid/Hierarchy/02-nested-trs-negative.gltf",
            DepthLimit) == EAssetResult::CapacityExceeded,
        "hierarchy depth limit rejects deeper authored trees");

    FStaticModelImportProfile GeometryLimit;
    GeometryLimit.Limits.MaxDecodedGeometryBytes = 1;
    Record(Result,
        ImportProfile(Geometry, GeometryLimit) == EAssetResult::CapacityExceeded,
        "decoded geometry byte budget includes canonical streams and indices");

    FStaticModelImportProfile DiagnosticLimit;
    DiagnosticLimit.Limits.MaxDiagnostics = 1;
    FAssetDiagnosticList Diagnostics;
    const EAssetResult DiagnosticResult = ImportProfile(
        "Tests/Fixtures/StaticModel/Invalid/Materials/02-missing-uv1.gltf",
        DiagnosticLimit, &Diagnostics);
    Record(Result,
        DiagnosticResult != EAssetResult::Success && Diagnostics.size() == 1,
        "diagnostic list is capped by the import profile");

    FStaticModelImportProfile DependencyLimit;
    DependencyLimit.Limits.MaxSingleDependencyBytes = 64;
    DependencyLimit.Limits.MaxAggregateDependencyBytes = 128;
    Record(Result,
        ImportProfile(
            "Tests/Fixtures/StaticModel/Valid/Materials/01-pbr-all-embedded.gltf",
            DependencyLimit) == EAssetResult::CapacityExceeded,
        "embedded dependency bytes obey finite single and aggregate limits");
    return Result;
}
