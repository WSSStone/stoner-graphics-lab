#include "AssetStaticModelConcurrency.h"

#include "StaticModelTestSupport.h"

#include "Asset/FAssetInspection.h"
#include "Asset/FStaticMeshInspection.h"
#include "Asset/FStaticModelInspection.h"

#include <future>
#include <array>
#include <iostream>
#include <sstream>

namespace
{
using namespace Stoner::Asset;
using namespace Stoner::Core;
using namespace StaticModelTestSupport;

struct FImportCapture
{
    EAssetResult Result = EAssetResult::ProcessingFailure;
    TArray<FAssetImportOutput> Outputs;
    FAssetDiagnosticList Diagnostics;
    FString Evidence;
};

FString MakeEvidence(const FImportCapture& Capture)
{
    std::ostringstream Stream;
    Stream << static_cast<int>(Capture.Result) << '\n';
    for (const auto& Output : Capture.Outputs)
    {
        Stream << FAssetInspection::Format(Output.Metadata).CStr()
               << "|version="
               << FAssetInspection::Format(Output.Metadata.Version).CStr();
        if (const auto Mesh = std::dynamic_pointer_cast<const FStaticMeshAsset>(
                Output.Payload))
            Stream << "|geometry="
                   << FStaticMeshInspection::ComputeGeometryDigest(*Mesh)
                          .ToLowerHex().CStr();
        if (const auto Model = std::dynamic_pointer_cast<const FStaticModelAsset>(
                Output.Payload))
            Stream << "|hierarchy="
                   << FStaticModelInspection::ComputeHierarchyDigest(*Model)
                          .ToLowerHex().CStr();
        Stream << '\n';
    }
    Stream << FAssetDiagnostics::FormatNormalized(Capture.Diagnostics).CStr();
    return FString(Stream.str());
}

FImportCapture ImportRepresentative()
{
    FImportCapture Capture;
    Capture.Outputs = ImportPackage(
        "Tests/Fixtures/StaticModel/Valid/Materials/01-pbr-all-embedded.gltf",
        Capture.Result, &Capture.Diagnostics);
    Capture.Evidence = MakeEvidence(Capture);
    return Capture;
}

void Record(FAssetStaticModelConcurrencyTestResult& Result, bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}
}

FAssetStaticModelConcurrencyTestResult RunAssetStaticModelConcurrencyTests()
{
    FAssetStaticModelConcurrencyTestResult Result;
    const FImportCapture Serial = ImportRepresentative();
    std::array<std::future<FImportCapture>, 8> Futures;
    for (auto& Future : Futures)
        Future = std::async(std::launch::async, ImportRepresentative);

    bool Equivalent = Serial.Result == EAssetResult::Success &&
        !Serial.Outputs.empty();
    bool Independent = true;
    for (auto& Future : Futures)
    {
        const FImportCapture Concurrent = Future.get();
        Equivalent = Equivalent && Concurrent.Result == EAssetResult::Success &&
            Concurrent.Evidence == Serial.Evidence;
        Independent = Independent &&
            Concurrent.Outputs.size() == Serial.Outputs.size();
        for (usize Index = 0;
             Independent && Index < Concurrent.Outputs.size(); ++Index)
        {
            Independent = Concurrent.Outputs[Index].Payload.get() !=
                Serial.Outputs[Index].Payload.get();
        }
    }
    Record(Result, Equivalent,
        "eight concurrent imports equal the normalized serial result");
    Record(Result, Independent,
        "independent imports publish no shared mutable payload objects");
    return Result;
}
