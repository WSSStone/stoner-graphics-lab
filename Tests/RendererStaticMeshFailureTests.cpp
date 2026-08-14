#include "RendererStaticMeshFailureTests.h"

#include "RendererStaticMeshTestSupport.h"

#include <iostream>

namespace
{

using namespace Stoner::Core;
using namespace Stoner::Renderer;
using namespace Stoner::RHI;
using namespace Stoner::Tests::StaticMesh;

void Record(
    FRendererStaticMeshFailureTestResult& Result,
    bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

} // namespace

FRendererStaticMeshFailureTestResult RunRendererStaticMeshFailureTests()
{
    FRendererStaticMeshFailureTestResult Result;
    const auto Asset = MakeAsset();

    const auto Missing = FStaticMeshRealizer::Realize({});
    Record(Result,
        !Missing.Succeeded() && !Missing.Snapshot &&
            Missing.Diagnostic.Stage ==
                EStaticMeshRealizationStage::ValidateAsset &&
            Missing.Diagnostic.Code ==
                FString("StaticMeshRealization.DeviceInactive"),
        "validate failure is normalized and publishes nothing");

    auto PlanDevice = MakeShared<FDevice>();
    FStaticMeshRealizationProfile InvalidProfile;
    InvalidProfile.Version = 99;
    const auto PlanFailure = FStaticMeshRealizer::Realize(
        {PlanDevice, Asset, InvalidProfile});
    Record(Result,
        !PlanFailure.Snapshot && PlanDevice->Created.empty() &&
            PlanFailure.Diagnostic.Stage ==
                EStaticMeshRealizationStage::Plan,
        "plan failure allocates no resources");

    for (int CreateFailure = 1; CreateFailure <= 2; ++CreateFailure)
    {
        auto Device = MakeShared<FDevice>();
        Device->FailCreateCall = CreateFailure;
        const auto Failure = FStaticMeshRealizer::Realize({Device, Asset, {}});
        Record(Result,
            !Failure.Snapshot && Failure.Diagnostic.Stage ==
                EStaticMeshRealizationStage::Allocate &&
                (Device->Created.empty() || AllCreatedInvalid(*Device)),
            CreateFailure == 1
                ? "vertex allocation failure publishes nothing"
                : "index allocation failure rolls back vertex buffer");
    }

    for (int UploadFailure = 1; UploadFailure <= 2; ++UploadFailure)
    {
        auto Device = MakeShared<FDevice>();
        Device->FailUploadCall = UploadFailure;
        const auto Failure = FStaticMeshRealizer::Realize({Device, Asset, {}});
        Record(Result,
            !Failure.Snapshot && Failure.Diagnostic.Stage ==
                EStaticMeshRealizationStage::Upload &&
                AllCreatedInvalid(*Device),
            UploadFailure == 1
                ? "vertex upload failure rolls back all buffers"
                : "index upload failure rolls back all buffers");
    }

    auto FinalizeDevice = MakeShared<FDevice>();
    FinalizeDevice->bInvalidateAfterUpload = true;
    const auto FinalizeFailure = FStaticMeshRealizer::Realize(
        {FinalizeDevice, Asset, {}});
    Record(Result,
        !FinalizeFailure.Snapshot &&
            FinalizeFailure.Diagnostic.Stage ==
                EStaticMeshRealizationStage::Finalize &&
            AllCreatedInvalid(*FinalizeDevice),
        "finalize failure rolls back uploaded resources");

    auto FirstDevice = MakeShared<FDevice>();
    const auto First = FStaticMeshRealizer::Realize({FirstDevice, Asset, {}});
    auto ReplacementDevice = MakeShared<FDevice>();
    const auto Replacement = FStaticMeshRealizer::Realize(
        {ReplacementDevice, Asset, {}});
    Record(Result,
        First.Succeeded() && Replacement.Succeeded() &&
            First.Snapshot != Replacement.Snapshot &&
            First.Snapshot->VertexBuffer->GetLifecycleState() ==
                ERHIResourceLifecycleState::Valid,
        "explicit reconversion does not mutate prior snapshot");

    return Result;
}
