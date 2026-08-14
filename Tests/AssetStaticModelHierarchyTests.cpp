#include "AssetStaticModelHierarchyTests.h"

#include "StaticModelTestSupport.h"

#include <filesystem>
#include <iostream>

namespace
{

using namespace Stoner::Asset;
using namespace Stoner::Core;
using namespace StaticModelTestSupport;

void Record(
    FAssetStaticModelHierarchyTestResult& Result,
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

std::filesystem::path Invalid(const char* Name)
{
    return std::filesystem::path("Tests/Fixtures/StaticModel/Invalid/Hierarchy") /
        Name;
}

} // namespace

FAssetStaticModelHierarchyTestResult RunAssetStaticModelHierarchyTests()
{
    FAssetStaticModelHierarchyTestResult Result;

    EAssetResult ImportResult = EAssetResult::ProcessingFailure;
    const auto MultiOutputs = Import(
        MakeRequest(Valid("01-multi-scene-shared.gltf")), ImportResult);
    const auto MultiMeshes = FindPayloads<FStaticMeshAsset>(MultiOutputs);
    const auto MultiModels = FindPayloads<FStaticModelAsset>(MultiOutputs);
    bool SharedMesh = ImportResult == EAssetResult::Success &&
        MultiOutputs.size() == 4 && MultiMeshes.size() == 1 &&
        MultiModels.size() == 2;
    int DefaultScenes = 0;
    for (const auto& Model : MultiModels)
    {
        DefaultScenes += Model->GetDesc().bSourceDefaultScene ? 1 : 0;
        SharedMesh = SharedMesh && Model->GetDesc().Nodes.size() == 1 &&
            Model->GetDesc().Nodes.front().Mesh.has_value() &&
            Model->GetDesc().Nodes.front().Mesh->GetId().has_value() &&
            *Model->GetDesc().Nodes.front().Mesh->GetId() ==
                MultiMeshes.front()->GetDesc().Id;
    }
    Record(Result, SharedMesh && DefaultScenes == 1,
        "multi-scene package shares one mesh and records one default scene");

    const auto NestedOutputs = Import(
        MakeRequest(Valid("02-nested-trs-negative.gltf")), ImportResult);
    const auto NestedModels = FindPayloads<FStaticModelAsset>(NestedOutputs);
    const bool NestedValid = ImportResult == EAssetResult::Success &&
        NestedModels.size() == 1 && NestedModels.front()->GetDesc().Nodes.size() == 2 &&
        NestedModels.front()->GetDesc().RootNodeIndices == TArray<uint32>{0} &&
        NestedModels.front()->GetDesc().Nodes[0].Children == TArray<uint32>{1} &&
        NestedModels.front()->GetDesc().Nodes[0].LocalTransform.Translation.NearlyEquals(
            FVector3(3.0f, -1.0f, 2.0f)) &&
        NestedModels.front()->GetDesc().Nodes[1].bNegativeDeterminant;
    Record(Result, NestedValid,
        "nested TRS preserves root-child order and Unreal basis conversion");

    const auto MatrixOutputs = Import(
        MakeRequest(Valid("03-matrix-transform.gltf")), ImportResult);
    const auto MatrixModels = FindPayloads<FStaticModelAsset>(MatrixOutputs);
    Record(Result,
        ImportResult == EAssetResult::Success && MatrixModels.size() == 1 &&
            MatrixModels.front()->GetDesc().Nodes.size() == 1 &&
            MatrixModels.front()->GetDesc().Nodes.front().LocalTransform.Translation
                .NearlyEquals(FVector3(3.0f, -1.0f, 2.0f)),
        "matrix nodes are conjugated into the Unreal coordinate basis");

    const auto UnreferencedOutputs = Import(
        MakeRequest(Valid("06-fallback-unreferenced.gltf")), ImportResult);
    const auto UnreferencedMeshes = FindPayloads<FStaticMeshAsset>(UnreferencedOutputs);
    const auto UnreferencedModels = FindPayloads<FStaticModelAsset>(UnreferencedOutputs);
    Record(Result,
        ImportResult == EAssetResult::Success && UnreferencedMeshes.size() == 2 &&
            UnreferencedModels.size() == 1 &&
            UnreferencedModels.front()->GetDesc().Dependencies.size() == 1,
        "unreferenced meshes remain outputs without becoming model dependencies");

    FStaticModelImportProfile ShallowProfile;
    ShallowProfile.Limits.MaxHierarchyDepth = 1;
    const auto ShallowOutputs = Import(
        MakeRequest(Valid("02-nested-trs-negative.gltf"), {}, ShallowProfile),
        ImportResult);
    Record(Result,
        ImportResult == EAssetResult::CapacityExceeded && ShallowOutputs.empty(),
        "hierarchy depth limit rejects the package atomically");

    const char* InvalidFixtures[] = {
        "01-cycle.gltf",
        "02-multiple-parent.gltf",
        "03-duplicate-node-key.gltf",
        "04-invalid-mesh-key.gltf",
        "05-empty-scene.gltf"};
    bool InvalidRejected = true;
    for (const char* Fixture : InvalidFixtures)
    {
        const auto Outputs = Import(MakeRequest(Invalid(Fixture)), ImportResult);
        if (ImportResult == EAssetResult::Success || !Outputs.empty())
        {
            std::cout << "[DETAIL] unexpected invalid fixture result fixture="
                      << Fixture << " result=" << static_cast<int>(ImportResult)
                      << " outputs=" << Outputs.size() << '\n';
        }
        InvalidRejected = InvalidRejected &&
            ImportResult != EAssetResult::Success && Outputs.empty();
    }
    Record(Result, InvalidRejected,
        "cycles multiple parents invalid keys and empty scenes return no outputs");

    bool Deterministic = true;
    const FString FirstInspection = FStaticModelInspection::FormatPackage(MultiOutputs);
    for (int Iteration = 0; Iteration < 20; ++Iteration)
    {
        const auto Repeated = Import(
            MakeRequest(Valid("01-multi-scene-shared.gltf")), ImportResult);
        Deterministic = Deterministic && ImportResult == EAssetResult::Success &&
            FStaticModelInspection::FormatPackage(Repeated) == FirstInspection;
    }
    Record(Result, Deterministic,
        "twenty hierarchy imports preserve package ordering and identity");

    return Result;
}
