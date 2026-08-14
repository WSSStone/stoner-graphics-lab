#include "AssetStaticModelDeterminism.h"

#include "AssetTests.h"
#include "StaticModelTestSupport.h"

#include "Asset/FAssetInspection.h"
#include "Asset/FImageAsset.h"
#include "Asset/FMaterialShaderInspection.h"
#include "Asset/FStaticMeshInspection.h"
#include "Asset/FStaticModelInspection.h"
#include "Asset/FTextureAsset.h"

#include <iostream>
#include <sstream>

namespace
{
using namespace Stoner::Asset;
using namespace Stoner::Core;
using namespace StaticModelTestSupport;

void Record(FAssetStaticModelDeterminismTestResult& Result, bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

FString PackageEvidence(
    const TArray<FAssetImportOutput>& Outputs,
    const FAssetDiagnosticList& Diagnostics)
{
    std::ostringstream Stream;
    for (const FAssetImportOutput& Output : Outputs)
    {
        Stream << FAssetInspection::Format(Output.Metadata).CStr()
               << "|version="
               << FAssetInspection::Format(Output.Metadata.Version).CStr()
               << "|type=" << Output.Payload->GetAssetType().CStr();
        if (const auto Mesh = std::dynamic_pointer_cast<const FStaticMeshAsset>(
                Output.Payload))
            Stream << '|' << FStaticMeshInspection::Format(*Mesh).CStr();
        else if (const auto Model =
                     std::dynamic_pointer_cast<const FStaticModelAsset>(Output.Payload))
            Stream << '|' << FStaticModelInspection::Format(*Model).CStr();
        else if (const auto Material =
                     std::dynamic_pointer_cast<const FMaterialAsset>(Output.Payload))
            Stream << '|' << InspectMaterialAsset(*Material).CStr();
        else if (const auto Image =
                     std::dynamic_pointer_cast<const FImageAsset>(Output.Payload))
            Stream << "|source=" << Image->GetSourceDigest().ToLowerHex().CStr()
                   << "|content=" << Image->GetContentDigest().ToLowerHex().CStr();
        else if (const auto Texture =
                     std::dynamic_pointer_cast<const FTextureAsset>(Output.Payload))
            Stream << "|content=" << Texture->GetContentDigest().ToLowerHex().CStr();
        Stream << '\n';
    }
    Stream << FAssetDiagnostics::FormatNormalized(Diagnostics).CStr();
    return FString(Stream.str());
}
}

FAssetStaticModelDeterminismTestResult
RunAssetStaticModelDeterminismTests(const FAssetStaticModelTestOptions& Options)
{
    FAssetStaticModelDeterminismTestResult Result;
    const auto Fixtures = ValidFixturePaths();
    bool Complete = Fixtures.size() >= 20;
    bool Deterministic = true;
    for (const auto& Path : Fixtures)
    {
        EAssetResult BaselineResult = EAssetResult::ProcessingFailure;
        FAssetDiagnosticList BaselineDiagnostics;
        const auto Baseline = ImportPackage(
            Path, BaselineResult, &BaselineDiagnostics);
        Complete = Complete && BaselineResult == EAssetResult::Success &&
            !Baseline.empty();
        if (BaselineResult != EAssetResult::Success) continue;
        const FString Evidence = PackageEvidence(Baseline, BaselineDiagnostics);
        for (int Run = 1; Run < Options.DeterminismRuns; ++Run)
        {
            EAssetResult RepeatedResult = EAssetResult::ProcessingFailure;
            FAssetDiagnosticList RepeatedDiagnostics;
            const auto Repeated = ImportPackage(
                Path, RepeatedResult, &RepeatedDiagnostics);
            Deterministic = Deterministic &&
                RepeatedResult == EAssetResult::Success &&
                PackageEvidence(Repeated, RepeatedDiagnostics) == Evidence;
        }
    }
    Record(Result, Complete,
        "every valid static-model fixture imports through the acceptance path");
    Record(Result, Deterministic,
        "configured repeats preserve complete package and diagnostic evidence");
    return Result;
}
