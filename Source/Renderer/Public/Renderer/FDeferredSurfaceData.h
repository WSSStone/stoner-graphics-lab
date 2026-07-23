#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FDeferredDiagnostics.h"
#include "RHI/ERHIFormat.h"
#include "RHI/ERHIPipelineState.h"

namespace Stoner::Renderer
{

enum class EDeferredSurfaceSemantic
{
    BaseColor,
    WorldNormal,
    Metallic,
    Roughness,
    Emissive,
    AmbientOcclusion,
    Depth
};

enum class EDeferredDepthConvention
{
    StandardZ,
    ReversedZ
};

struct FDeferredExtent2D
{
    Stoner::Core::uint32 Width = 0;
    Stoner::Core::uint32 Height = 0;

    [[nodiscard]] bool IsPositive() const noexcept { return Width > 0 && Height > 0; }
};

struct FDeferredDepthPolicy
{
    EDeferredDepthConvention Convention = EDeferredDepthConvention::StandardZ;
    float NearPlane = 0.1f;
    float FarPlane = 1000.0f;
    float FarClearValue = 1.0f;
    Stoner::RHI::ERHICompareOp CompareOp = Stoner::RHI::ERHICompareOp::LessEqual;

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] Stoner::Core::FString GetIdentity() const;
};

struct FDeferredSurfaceAttachment
{
    Stoner::Core::FString Name;
    Stoner::RHI::ERHIFormat Format = Stoner::RHI::ERHIFormat::Unknown;
    Stoner::Core::TArray<EDeferredSurfaceSemantic> Semantics;
    Stoner::Core::FVector4 ClearValue = Stoner::Core::FVector4::Zero();
    bool bDepth = false;
};

struct FDeferredSurfaceLayout
{
    Stoner::Core::FString LayoutId;
    FDeferredExtent2D Extent;
    Stoner::RHI::ERHISampleCount SampleCount = Stoner::RHI::ERHISampleCount::One;
    FDeferredDepthPolicy DepthPolicy;
    Stoner::Core::TArray<FDeferredSurfaceAttachment> Attachments;

    [[nodiscard]] bool IsValid(FDeferredDiagnosticLog* Diagnostics = nullptr) const;
    [[nodiscard]] const FDeferredSurfaceAttachment* FindAttachment(EDeferredSurfaceSemantic Semantic) const noexcept;
};

[[nodiscard]] FDeferredDepthPolicy MakeDeferredDepthPolicy(EDeferredDepthConvention Convention,
    float NearPlane = 0.1f, float FarPlane = 1000.0f) noexcept;
[[nodiscard]] FDeferredSurfaceLayout MakeDefaultDeferredSurfaceLayout(FDeferredExtent2D Extent,
    EDeferredDepthConvention Convention = EDeferredDepthConvention::StandardZ,
    float NearPlane = 0.1f, float FarPlane = 1000.0f);
[[nodiscard]] bool TryBuildWorldNormalFromModel(const Stoner::Core::FMatrix4x4& Model,
    Stoner::Core::FMatrix4x4& OutWorldNormalFromModel) noexcept;
[[nodiscard]] bool IsDeferredFinite(const Stoner::Core::FMatrix4x4& Value) noexcept;
[[nodiscard]] const char* ToString(EDeferredSurfaceSemantic Semantic) noexcept;
[[nodiscard]] const char* ToString(EDeferredDepthConvention Convention) noexcept;

} // namespace Stoner::Renderer
