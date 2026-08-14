#include "AssetGLTFDiagnosticTests.h"

#include "StaticModelTestSupport.h"

#include <iostream>
#include <string>

namespace
{
using namespace Stoner::Asset;
using namespace Stoner::Core;
using namespace StaticModelTestSupport;

void Record(FAssetGLTFDiagnosticTestResult& Result, bool Passed, const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

FStaticModelImportRequest MakeStaticRequest(
    TArray<uint8> Bytes, const FString& Locator)
{
    FStaticModelImportRequest Request;
    Request.AssetRequest = MakeMemoryRequest(std::move(Bytes), Locator);
    Request.Profile = MakeShared<FStaticModelImportProfile>();
    return Request;
}
}

FAssetGLTFDiagnosticTestResult RunAssetGLTFDiagnosticTests()
{
    FAssetGLTFDiagnosticTestResult Result;
    auto InvalidBytes = ReadFixture(
        "Tests/Fixtures/StaticModel/Valid/Geometry/01-basis-u16.gltf");
    InvalidBytes.resize(InvalidBytes.size() / 2);

    FAssetImportOutput Sentinel;
    (void)FAssetId::Create(FString("StaticMesh"), FString("Sentinel/Existing"),
        std::nullopt, Sentinel.Metadata.Id);
    TArray<FAssetImportOutput> Outputs{Sentinel};
    FAssetDiagnosticList Diagnostics;
    const EAssetResult ImportResult = ImportStaticModel(
        MakeStaticRequest(InvalidBytes, FString("/Users/private/model.gltf")),
        Outputs, &Diagnostics);
    Record(Result,
        ImportResult != EAssetResult::Success && Outputs.size() == 1 &&
            Outputs.front().Metadata.Id == Sentinel.Metadata.Id,
        "failed import preserves pre-existing caller outputs");

    const FString First = FAssetDiagnostics::FormatNormalized(Diagnostics);
    const FString Second = FAssetDiagnostics::FormatNormalized(Diagnostics);
    Record(Result,
        !Diagnostics.empty() && First == Second &&
            First.View().find("[redacted]") != std::string_view::npos &&
            First.View().find("/Users/private") == std::string_view::npos,
        "normalized diagnostics are deterministic and redact absolute paths");

    FAssetRegistry Registry;
    const uint64 Revision = Registry.Snapshot().Revision;
    Record(Result,
        Registry.Snapshot().Revision == Revision && Outputs.size() == 1,
        "failed package leaves registry revision unchanged");

    auto OptionalBytes = ReadFixture(
        "Tests/Fixtures/StaticModel/Valid/Geometry/01-basis-u16.gltf");
    std::string OptionalText(OptionalBytes.begin(), OptionalBytes.end());
    const std::string AssetMarker = "\"asset\":";
    const std::size_t At = OptionalText.find(AssetMarker);
    OptionalText.replace(At, AssetMarker.size(),
        "\"extensionsUsed\":[\"EXT_stoner_optional\"],"
        "\"cameras\":[{\"type\":\"perspective\",\"perspective\":{"
        "\"yfov\":1.0,\"znear\":0.1}}],\"asset\":");
    EAssetResult OptionalResult = EAssetResult::ProcessingFailure;
    const auto OptionalOutputs = Import(
        MakeMemoryRequest(TArray<uint8>(OptionalText.begin(), OptionalText.end()),
            FString("Hardening/optional.gltf")), OptionalResult);
    const FString Inspection = FStaticModelInspection::FormatPackage(OptionalOutputs);
    Record(Result,
        OptionalResult == EAssetResult::Success &&
            Inspection.View().find("model.skipped-cameras=1") !=
                std::string_view::npos &&
            Inspection.View().find("EXT_stoner_optional") !=
                std::string_view::npos,
        "inspection records skipped optional content deterministically");
    return Result;
}
