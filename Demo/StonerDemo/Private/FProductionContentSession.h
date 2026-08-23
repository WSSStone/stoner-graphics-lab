#pragma once

#include "Asset/AssetMinimal.h"
#include "Renderer/FStaticModelRealization.h"

namespace Stoner::Demo
{

struct FProductionContentSessionConfig
{
    Core::FString PublicationRoot;
    Core::FString LeaseCoordinationRoot;
    Core::FString RootAssetIdentity;
    Asset::FAssetDigest ExpectedGeneration;
    Core::TSharedPtr<const Asset::FAssetTargetProfileEvidence> TargetEvidence;
    Core::TSharedPtr<const Asset::FAssetRuntimeExecutionContext> RuntimeContext;
    Core::uint32 WorkerCount = 4;
    Core::uint64 RequestTimeoutMilliseconds = 30000;

    [[nodiscard]] bool IsValid() const noexcept;
};

struct FProductionContentLoadedClosure
{
    Asset::FAssetDigest GenerationIdentity;
    Core::TSharedPtr<const Asset::FStaticModelAsset> Model;
    Renderer::FStaticModelRealizationDependencies Dependencies;
    Core::TArray<Core::TSharedPtr<const Asset::FShaderAsset>> RenderShaders;
    Core::TArray<Core::TSharedPtr<const Asset::FShaderPayloadAsset>>
        RenderShaderPayloads;
    Core::uint32 LoadedAssetCount = 0;
};

struct FProductionContentSessionInspection
{
    Asset::FAssetDigest GenerationIdentity;
    Core::uint32 ManifestAssetCount = 0;
    Core::uint32 LoadedAssetCount = 0;
    Core::uint32 ReleasedRequestCount = 0;
    Core::uint32 CancelledRequestCount = 0;
    Asset::FAssetManagerInspection Manager;
    Core::FString FirstFailure;
    bool bPublished = false;
    bool bShutdown = false;
    bool bStaleHandleRejected = false;
};

class FProductionContentSession
{
public:
    FProductionContentSession();
    ~FProductionContentSession();
    FProductionContentSession(const FProductionContentSession&) = delete;
    FProductionContentSession& operator=(
        const FProductionContentSession&) = delete;

    [[nodiscard]] Asset::EAssetResult Load(
        const FProductionContentSessionConfig& Config,
        FProductionContentLoadedClosure& OutClosure);
    [[nodiscard]] Asset::EAssetResult Shutdown();
    [[nodiscard]] const FProductionContentSessionInspection&
        Inspect() const noexcept;

private:
    struct FImpl;
    Core::TUniquePtr<FImpl> Impl_;
};

} // namespace Stoner::Demo
