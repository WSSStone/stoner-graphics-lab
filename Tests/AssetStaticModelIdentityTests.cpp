#include "AssetStaticModelIdentityTests.h"

#include "StaticModelTestSupport.h"

#include <algorithm>
#include <filesystem>
#include <iostream>

namespace
{

using namespace Stoner::Asset;
using namespace Stoner::Core;
using namespace StaticModelTestSupport;

void Record(
    FAssetStaticModelIdentityTestResult& Result,
    bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

std::filesystem::path Valid(const char* Name)
{
    return std::filesystem::path("Tests/Fixtures/StaticModel/Valid/Hierarchy") /
        Name;
}

bool HasSubresource(
    const TArray<FAssetImportOutput>& Outputs,
    const char* Type,
    const char* Subresource)
{
    return std::any_of(Outputs.begin(), Outputs.end(),
        [Type, Subresource](const FAssetImportOutput& Output)
        {
            return Output.Metadata.Id.GetAssetType() == FString(Type) &&
                Output.Metadata.Id.GetSubresource().has_value() &&
                *Output.Metadata.Id.GetSubresource() == FString(Subresource);
        });
}

} // namespace

FAssetStaticModelIdentityTestResult RunAssetStaticModelIdentityTests()
{
    FAssetStaticModelIdentityTestResult Result;
    const FString SharedPath("Tests/StaticModel/Identity/Reordered");
    EAssetResult FirstResult = EAssetResult::ProcessingFailure;
    EAssetResult SecondResult = EAssetResult::ProcessingFailure;
    const auto First = Import(
        MakeRequest(Valid("04-explicit-keys-a.gltf"), SharedPath), FirstResult);
    const auto Second = Import(
        MakeRequest(Valid("05-explicit-keys-reordered.gltf"), SharedPath), SecondResult);
    Record(Result,
        FirstResult == EAssetResult::Success &&
            SecondResult == EAssetResult::Success &&
            SortedIds(First) == SortedIds(Second),
        "explicit keys preserve typed identities across reorder and renaming");
    Record(Result,
        HasSubresource(First, "StaticMesh", "key.hero") &&
            HasSubresource(First, "StaticMesh", "key.prop") &&
            HasSubresource(First, "StaticModel", "key.showroom"),
        "explicit mesh and scene keys become typed package subresources");

    EAssetResult FallbackResult = EAssetResult::ProcessingFailure;
    const auto Fallback = Import(
        MakeRequest(Valid("06-fallback-unreferenced.gltf")), FallbackResult);
    Record(Result,
        FallbackResult == EAssetResult::Success &&
            HasSubresource(Fallback, "StaticMesh", "idx.mesh.0") &&
            HasSubresource(Fallback, "StaticMesh", "idx.mesh.1") &&
            HasSubresource(Fallback, "StaticModel", "idx.scene.0"),
        "missing explicit keys use deterministic structural fallback identities");

    FAssetRegistry Registry;
    FAssetMutationBatch Batch;
    for (const FAssetImportOutput& Output : First) Batch.Register(Output.Metadata);
    const EAssetResult PublishResult = Registry.Apply(Batch);
    Record(Result,
        PublishResult == EAssetResult::Success &&
            Registry.Snapshot().Revision == 1 &&
            Registry.Snapshot().Records.size() == First.size(),
        "complete package metadata publishes in one registry revision");

    EAssetResult InvalidResult = EAssetResult::Success;
    const auto Invalid = Import(MakeRequest(
        "Tests/Fixtures/StaticModel/Invalid/Hierarchy/03-duplicate-node-key.gltf"),
        InvalidResult);
    Record(Result,
        InvalidResult == EAssetResult::Conflict && Invalid.empty() &&
            Registry.Snapshot().Revision == 1,
        "identity conflicts return no package and cannot partially mutate registry");

    return Result;
}
