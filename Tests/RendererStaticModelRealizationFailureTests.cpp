#include "RendererStaticModelRealizationFailureTests.h"

#include "RendererStaticModelRealizationTestSupport.h"

#include <chrono>
#include <iostream>

namespace
{

using namespace Stoner;
using namespace Stoner::Tests::StaticModelRealization;

void Record(
    FRendererStaticModelRealizationFailureTestResult& Result,
    bool bPassed,
    const char* Name)
{
    (bPassed ? ++Result.Passed : ++Result.Failed);
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

void CheckFailure(
    FRendererStaticModelRealizationFailureTestResult& Result,
    EFailurePoint Point,
    const char* Name)
{
    auto Fixture = MakeFixture();
    auto Device = std::dynamic_pointer_cast<FDevice>(Fixture.Request.Device);
    Device->Ledger()->Failure = Point;
    Renderer::FStaticModelRealizationInspection Inspection;
    Core::TSharedPtr<const Renderer::FStaticModelRenderSnapshot> Snapshot;
    const auto Realized = Renderer::FStaticModelRealizer::Realize(
        Fixture.Request, Snapshot, Inspection);
    Record(Result,
        Realized != RHI::ERHIResult::Success && !Snapshot &&
            !Inspection.bCommitted &&
            (Device->Ledger()->Created.empty() ||
                Device->Ledger()->EveryCreatedReleasedOnce()),
        Name);
}

} // namespace

FRendererStaticModelRealizationFailureTestResult
RunRendererStaticModelRealizationFailureTests()
{
    FRendererStaticModelRealizationFailureTestResult Result;

    auto Missing = MakeFixture();
    Missing.Request.Dependencies.Shaders.clear();
    auto MissingDevice = std::dynamic_pointer_cast<FDevice>(
        Missing.Request.Device);
    Renderer::FStaticModelRealizationInspection MissingInspection;
    Core::TSharedPtr<const Renderer::FStaticModelRenderSnapshot> MissingSnapshot;
    const auto MissingResult = Renderer::FStaticModelRealizer::Realize(
        Missing.Request, MissingSnapshot, MissingInspection);
    Record(Result,
        MissingResult == RHI::ERHIResult::InvalidState && !MissingSnapshot &&
            MissingDevice->Ledger()->Created.empty() &&
            MissingInspection.FirstFailure.Code ==
                Core::FString("renderer.static-model.version-set"),
        "incomplete typed closure fails before any GPU allocation");

    auto Duplicate = MakeFixture();
    Duplicate.Request.Dependencies.Meshes.push_back(
        Duplicate.Request.Dependencies.Meshes.front());
    auto DuplicateDevice = std::dynamic_pointer_cast<FDevice>(
        Duplicate.Request.Device);
    Renderer::FStaticModelRealizationInspection DuplicateInspection;
    Core::TSharedPtr<const Renderer::FStaticModelRenderSnapshot> DuplicateSnapshot;
    const auto DuplicateResult = Renderer::FStaticModelRealizer::Realize(
        Duplicate.Request, DuplicateSnapshot, DuplicateInspection);
    Record(Result,
        DuplicateResult == RHI::ERHIResult::InvalidState &&
            !DuplicateSnapshot && DuplicateDevice->Ledger()->Created.empty() &&
            DuplicateInspection.FirstFailure.Code ==
                Core::FString("renderer.static-model.dependency-duplicate"),
        "duplicate dependency identity is rejected before realization");

    auto Stale = MakeFixture();
    Stale.Request.Dependencies.Versions.front().Version =
        Version("stale-mesh-version");
    auto StaleDevice = std::dynamic_pointer_cast<FDevice>(Stale.Request.Device);
    Renderer::FStaticModelRealizationInspection StaleInspection;
    Core::TSharedPtr<const Renderer::FStaticModelRenderSnapshot> StaleSnapshot;
    const auto StaleResult = Renderer::FStaticModelRealizer::Realize(
        Stale.Request, StaleSnapshot, StaleInspection);
    Record(Result,
        StaleResult == RHI::ERHIResult::InvalidState && !StaleSnapshot &&
            StaleDevice->Ledger()->Created.empty() &&
            StaleInspection.FirstFailure.Code ==
                Core::FString("renderer.static-model.version-mismatch"),
        "stale dependency version is rejected before GPU realization");

    auto UnsupportedTarget = MakeFixture();
    auto UnsupportedDevice = std::dynamic_pointer_cast<FDevice>(
        UnsupportedTarget.Request.Device);
    UnsupportedDevice->RemoveFormat(RHI::ERHIFormat::D32_Float);
    Renderer::FStaticModelRealizationInspection UnsupportedInspection;
    Core::TSharedPtr<const Renderer::FStaticModelRenderSnapshot>
        UnsupportedSnapshot;
    const auto UnsupportedResult = Renderer::FStaticModelRealizer::Realize(
        UnsupportedTarget.Request, UnsupportedSnapshot,
        UnsupportedInspection);
    Record(Result,
        UnsupportedResult == RHI::ERHIResult::InvalidState &&
            !UnsupportedSnapshot && UnsupportedDevice->Ledger()->Created.empty() &&
            UnsupportedInspection.FirstFailure.Code ==
                Core::FString("renderer.static-model.request-invalid"),
        "unsupported render-target capability is rejected before allocation");

    auto PipelineLimit = MakeFixture();
    PipelineLimit.Request.Limits.MaxPipelines = 1;
    auto PipelineLimitDevice = std::dynamic_pointer_cast<FDevice>(
        PipelineLimit.Request.Device);
    Renderer::FStaticModelRealizationInspection PipelineLimitInspection;
    Core::TSharedPtr<const Renderer::FStaticModelRenderSnapshot>
        PipelineLimitSnapshot;
    const auto PipelineLimitResult = Renderer::FStaticModelRealizer::Realize(
        PipelineLimit.Request, PipelineLimitSnapshot,
        PipelineLimitInspection);
    Record(Result,
        PipelineLimitResult == RHI::ERHIResult::Unavailable &&
            !PipelineLimitSnapshot && !PipelineLimitInspection.bRolledBack &&
            PipelineLimitDevice->Ledger()->Created.empty() &&
            PipelineLimitInspection.FirstFailure.Code ==
                Core::FString("renderer.static-model.pipeline-limit"),
        "pipeline limit rejects before GPU work or partial publication");

    for (const auto& Case : {
             std::pair{EFailurePoint::BufferCreate,
                 "buffer allocation failure rolls back atomically"},
             std::pair{EFailurePoint::BufferUpload,
                 "buffer upload failure rolls back atomically"},
             std::pair{EFailurePoint::TextureCreate,
                 "texture allocation failure rolls back prior mesh resources"},
             std::pair{EFailurePoint::TextureUpload,
                 "texture upload failure rolls back prior resources"},
             std::pair{EFailurePoint::ShaderCreate,
                 "shader failure rolls back geometry and texture resources"},
             std::pair{EFailurePoint::LayoutCreate,
                 "layout failure rolls back shader and prior resources"},
             std::pair{EFailurePoint::DescriptorCreate,
                 "descriptor allocation failure rolls back atomically"},
             std::pair{EFailurePoint::DescriptorUpdate,
                 "descriptor update failure rolls back atomically"},
             std::pair{EFailurePoint::SamplerCreate,
                 "sampler allocation failure rolls back atomically"},
             std::pair{EFailurePoint::PipelineCreate,
                 "pipeline allocation failure rolls back atomically"}})
        CheckFailure(Result, Case.first, Case.second);

    auto Cancelled = MakeFixture();
    auto Context = Core::MakeShared<Asset::FAssetRuntimeExecutionContext>();
    Context->Deadline = std::chrono::steady_clock::now();
    Cancelled.Request.RuntimeContext = Context;
    auto CancelDevice = std::dynamic_pointer_cast<FDevice>(
        Cancelled.Request.Device);
    Renderer::FStaticModelRealizationInspection CancelInspection;
    Core::TSharedPtr<const Renderer::FStaticModelRenderSnapshot> CancelSnapshot;
    const auto CancelResult = Renderer::FStaticModelRealizer::Realize(
        Cancelled.Request, CancelSnapshot, CancelInspection);
    Record(Result,
        CancelResult == RHI::ERHIResult::Failed && !CancelSnapshot &&
            CancelDevice->Ledger()->Created.empty() &&
            CancelInspection.FirstFailure.Code ==
                Core::FString("renderer.static-model.cancelled"),
        "pre-cancelled realization publishes nothing and performs no GPU work");

    auto Lost = MakeFixture();
    auto LostDevice = std::dynamic_pointer_cast<FDevice>(Lost.Request.Device);
    LostDevice->Ledger()->bLoseDeviceAfterFirstCreate = true;
    Renderer::FStaticModelRealizationInspection LostInspection;
    Core::TSharedPtr<const Renderer::FStaticModelRenderSnapshot> LostSnapshot;
    const auto LostResult = Renderer::FStaticModelRealizer::Realize(
        Lost.Request, LostSnapshot, LostInspection);
    Record(Result,
        LostResult != RHI::ERHIResult::Success && !LostSnapshot &&
            LostDevice->Ledger()->EveryCreatedReleasedOnce(),
        "device loss during realization releases the partial transaction");

    auto First = MakeFixture();
    Core::TSharedPtr<const Renderer::FStaticModelRenderSnapshot> FirstSnapshot;
    Renderer::FStaticModelRealizationInspection FirstInspection;
    const bool FirstOk = Renderer::FStaticModelRealizer::Realize(
        First.Request, FirstSnapshot, FirstInspection) == RHI::ERHIResult::Success;
    auto Second = MakeFixture();
    Core::TSharedPtr<const Renderer::FStaticModelRenderSnapshot> SecondSnapshot;
    Renderer::FStaticModelRealizationInspection SecondInspection;
    const bool SecondOk = Renderer::FStaticModelRealizer::Realize(
        Second.Request, SecondSnapshot, SecondInspection) == RHI::ERHIResult::Success;
    Record(Result,
        FirstOk && SecondOk && FirstSnapshot && SecondSnapshot &&
            FirstSnapshot->GetSnapshotGeneration() <
                SecondSnapshot->GetSnapshotGeneration() &&
            FirstSnapshot->GetDraws().size() == 4,
        "destroy and recreate generations are monotonic without mutating stale snapshots");
    FirstSnapshot.reset();
    SecondSnapshot.reset();

    return Result;
}
