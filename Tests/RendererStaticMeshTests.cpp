#include "RendererStaticMeshTests.h"

#include "RendererStaticMeshTestSupport.h"
#include "Renderer/FMeshDrawCommand.h"

#include <cstring>
#include <iostream>

namespace
{

using namespace Stoner::Core;
using namespace Stoner::Renderer;
using namespace Stoner::RHI;
using namespace Stoner::Tests::StaticMesh;

void Record(FRendererStaticMeshTestResult& Result, bool Passed, const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

} // namespace

FRendererStaticMeshTestResult RunRendererStaticMeshTests()
{
    FRendererStaticMeshTestResult Result;
    const auto Asset = MakeAsset();
    FStaticMeshPackingPlan NarrowPlan;
    FString Reason;
    Record(Result,
        Asset && BuildStaticMeshPackingPlan(
            *Asset, {}, NarrowPlan, &Reason) == ERHIResult::Success &&
            NarrowPlan.IndexType == ERHIIndexType::UInt16 &&
            NarrowPlan.VertexInput.Stride == 56 &&
            NarrowPlan.VertexBytes.size() == 6 * 56 &&
            NarrowPlan.IndexBytes.size() == 6 * sizeof(uint16) &&
            NarrowPlan.ProfileDigest.IsAvailable(),
        "packing plan is deterministic interleaved uint16");

    FStaticMeshRealizationProfile WideProfile;
    WideProfile.IndexPacking = EStaticMeshIndexPackingPolicy::UInt32;
    FStaticMeshPackingPlan WidePlan;
    Record(Result,
        BuildStaticMeshPackingPlan(
            *Asset, WideProfile, WidePlan) == ERHIResult::Success &&
            WidePlan.IndexType == ERHIIndexType::UInt32 &&
            WidePlan.IndexBytes.size() == 6 * sizeof(uint32) &&
            WidePlan.ProfileDigest != NarrowPlan.ProfileDigest,
        "packing profile selects uint32 with distinct digest");

    auto Device = MakeShared<FDevice>();
    const auto Realized = FStaticMeshRealizer::Realize(
        {Device, Asset, {}});
    Record(Result,
        Realized.Succeeded() && Device->Created.size() == 2 &&
            Device->Created[0]->Bytes == NarrowPlan.VertexBytes &&
            Device->Created[1]->Bytes == NarrowPlan.IndexBytes,
        "realizer uploads planned bytes through RHI");
    Record(Result,
        Realized.Snapshot && Realized.Snapshot->Sections.size() == 2 &&
            Realized.Snapshot->Sections[0].FirstIndex == 0 &&
            Realized.Snapshot->Sections[0].VertexOffset == 0 &&
            Realized.Snapshot->Sections[1].FirstIndex == 3 &&
            Realized.Snapshot->Sections[1].VertexOffset == 3 &&
            Realized.Snapshot->Sections[0].Material.IsValid(),
        "snapshot preserves section offsets and material identities");
    Record(Result,
        Realized.Snapshot && Realized.Snapshot->Bounds.IsValid() &&
            Realized.Snapshot->SourceManifest == Asset->GetDesc().SourceManifest &&
            Realized.Snapshot->RealizationProfileDigest ==
                NarrowPlan.ProfileDigest,
        "snapshot preserves bounds source evidence and profile digest");

    const auto Draw = MakeStaticMeshSectionDrawArguments(
        Realized.Snapshot->Sections[1], 4, 7);
    Record(Result,
        Draw.IndexCount == 3 && Draw.InstanceCount == 4 &&
            Draw.FirstIndex == 3 && Draw.VertexOffset == 3 &&
            Draw.FirstInstance == 7 && IsValidRHIIndexedDrawArguments(Draw),
        "section emits complete indexed draw arguments");

    return Result;
}
