#include "RendererStaticModelRealizationTests.h"

#include "RendererStaticModelRealizationTestSupport.h"

#include <algorithm>
#include <iostream>
#include <set>

namespace
{

using namespace Stoner;
using namespace Stoner::Tests::StaticModelRealization;

void Record(
    FRendererStaticModelRealizationTestResult& Result,
    bool bPassed,
    const char* Name)
{
    (bPassed ? ++Result.Passed : ++Result.Failed);
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

} // namespace

FRendererStaticModelRealizationTestResult
RunRendererStaticModelRealizationTests()
{
    FRendererStaticModelRealizationTestResult Result;
    auto Fixture = MakeFixture();
    auto Device = std::dynamic_pointer_cast<FDevice>(Fixture.Request.Device);
    Core::TSharedPtr<const Renderer::FStaticModelRenderSnapshot> Snapshot;
    Renderer::FStaticModelRealizationInspection Inspection;
    const auto Realized = Renderer::FStaticModelRealizer::Realize(
        Fixture.Request, Snapshot, Inspection);
    if (Realized != RHI::ERHIResult::Success)
        std::cout << "[DETAIL] static-model realization stage="
                  << Renderer::ToString(Inspection.FirstFailure.Stage)
                  << " code=" << Inspection.FirstFailure.Code.CStr()
                  << " reason=" << Inspection.FirstFailure.Reason.CStr()
                  << '\n';

    Record(Result,
        Realized == RHI::ERHIResult::Success && Snapshot &&
            Inspection.bCommitted && !Inspection.bRolledBack &&
            Inspection.Stage ==
                Renderer::EStaticModelRealizationStage::Published,
        "aggregate realization publishes only a complete committed snapshot");
    Record(Result,
        Snapshot && Snapshot->GetNodes().size() == 2 &&
            Snapshot->GetNodes()[0].SourceNodeIndex == 0 &&
            Snapshot->GetNodes()[1].SourceNodeIndex == 1 &&
            Snapshot->GetDraws().size() == 4 &&
            std::is_sorted(
                Snapshot->GetDraws().begin(), Snapshot->GetDraws().end(),
                [](const auto& Left, const auto& Right)
                { return Left.StableKey < Right.StableKey; }),
        "planning is parent-first with deterministic multi-node draw order");
    Record(Result,
        Snapshot && Inspection.UniqueMeshCount == 1 &&
            Inspection.UniqueMaterialCount == 2 &&
            Inspection.UniqueShaderCount == 1 &&
            Inspection.UniqueTextureCount == 1 &&
            Snapshot->GetMeshes().size() == 1 &&
            Snapshot->GetMaterials().size() == 2 &&
            Snapshot->GetShaders().size() == 1,
        "shared mesh shader and texture dependencies realize once by identity");
    Record(Result,
        Snapshot && Snapshot->GetTextures().size() == 1 &&
            Snapshot->GetTextures().front().AssetId.IsValid() &&
            Snapshot->GetTextures().front().Version.Validate() ==
                Asset::EAssetResult::Success &&
            Snapshot->GetTextures().front().Texture &&
            Snapshot->GetMaterialResources().size() == 2 &&
            std::all_of(
                Snapshot->GetMaterialResources().begin(),
                Snapshot->GetMaterialResources().end(),
                [](const auto& Resources)
                {
                    return Resources.PipelineLayout && Resources.Pipeline &&
                        Resources.DescriptorSets.size() == 2;
                }),
        "snapshot exposes complete immutable draw resource views");
    std::set<const RHI::IRHIDescriptorSet*> DrawDescriptorSets;
    if (Snapshot)
    {
        for (const auto& Resources : Snapshot->GetDrawResources())
        {
            const auto Found = std::find_if(
                Resources.DescriptorSets.begin(),
                Resources.DescriptorSets.end(),
                [](const auto& Descriptor)
                { return Descriptor && Descriptor->GetSetIndex() == 1; });
            if (Found != Resources.DescriptorSets.end())
                DrawDescriptorSets.insert(Found->get());
        }
    }
    Record(Result,
        Snapshot && Snapshot->GetDrawResources().size() ==
                Snapshot->GetDraws().size() &&
            DrawDescriptorSets.size() == Snapshot->GetDraws().size() &&
            std::all_of(
                Snapshot->GetDrawResources().begin(),
                Snapshot->GetDrawResources().end(),
                [](const auto& Resources)
                {
                    return Resources.DescriptorSets.size() == 2 &&
                        Resources.BufferBindings.size() == 2;
                }),
        "each aggregate draw owns an independent draw descriptor and uniform buffer");
    Record(Result,
        Snapshot && std::all_of(
            Snapshot->GetDrawResources().begin(),
            Snapshot->GetDrawResources().end(),
            [](const auto& Resources)
            {
                const auto Found = std::find_if(
                    Resources.DescriptorSets.begin(),
                    Resources.DescriptorSets.end(),
                    [](const auto& Descriptor)
                    { return Descriptor && Descriptor->GetSetIndex() == 1; });
                if (Found == Resources.DescriptorSets.end()) return false;
                for (Core::uint32 Slot = 1; Slot <= 5; ++Slot)
                    if ((*Found)->GetBoundResourceKind(Slot, 0) !=
                        RHI::ERHIDescriptorResourceKind::CombinedTextureSampler)
                        return false;
                return true;
            }) && Snapshot->GetTextures().size() == 1,
        "optional material textures bind semantic fallbacks without publishing fake assets");

    const auto Generation = Snapshot
        ? Snapshot->GetSnapshotGeneration() : 0;
    const auto DrawCount = Snapshot ? Snapshot->GetDraws().size() : 0;
    Fixture.Request.Model.reset();
    Fixture.Request.Dependencies = {};
    Record(Result,
        Snapshot && Snapshot->GetSnapshotGeneration() == Generation &&
            Snapshot->GetDraws().size() == DrawCount &&
            Snapshot->GetRootAssetId().IsValid(),
        "published snapshot owns immutable CPU state after source handles release");

    const auto Ledger = Device->Ledger();
    const auto Created = Ledger->Created;
    Snapshot.reset();
    Record(Result,
        !Created.empty() && Ledger->EveryCreatedReleasedOnce() &&
            Ledger->Released.front() == Created.back() &&
            Ledger->Released.back() == Created.front(),
        "final snapshot release invalidates all RHI owners once in reverse order");

    auto UnusedFixture = MakeFixture(true);
    Core::TSharedPtr<const Renderer::FStaticModelRenderSnapshot> UnusedSnapshot;
    Renderer::FStaticModelRealizationInspection UnusedInspection;
    const auto UnusedResult = Renderer::FStaticModelRealizer::Realize(
        UnusedFixture.Request, UnusedSnapshot, UnusedInspection);
    Record(Result,
        UnusedResult == RHI::ERHIResult::Success && UnusedSnapshot &&
            UnusedSnapshot->GetMaterials().size() == 1 &&
            UnusedInspection.UniqueMaterialCount == 1 &&
            std::all_of(
                UnusedSnapshot->GetDraws().begin(),
                UnusedSnapshot->GetDraws().end(),
                [](const auto& Draw) { return Draw.MaterialIndex == 0; }),
        "declared but unused material slots do not require GPU realization");

    auto MultiTextureFixture = MakeFixture(false, true);
    Core::TSharedPtr<const Renderer::FStaticModelRenderSnapshot>
        MultiTextureSnapshot;
    Renderer::FStaticModelRealizationInspection MultiTextureInspection;
    const auto MultiTextureResult = Renderer::FStaticModelRealizer::Realize(
        MultiTextureFixture.Request, MultiTextureSnapshot,
        MultiTextureInspection);
    if (MultiTextureResult != RHI::ERHIResult::Success)
        std::cout << "[DETAIL] multi-texture realization stage="
                  << Renderer::ToString(
                         MultiTextureInspection.FirstFailure.Stage)
                  << " code="
                  << MultiTextureInspection.FirstFailure.Code.CStr()
                  << " subject="
                  << MultiTextureInspection.FirstFailure.Subject.CStr()
                  << " reason="
                  << MultiTextureInspection.FirstFailure.Reason.CStr()
                  << '\n';
    Record(Result,
        MultiTextureResult == RHI::ERHIResult::Success &&
            MultiTextureSnapshot &&
            MultiTextureSnapshot->GetTextures().size() == 2 &&
            MultiTextureInspection.UniqueTextureCount == 2,
        "aggregate realization selects target profiles per texture identity");
    return Result;
}
