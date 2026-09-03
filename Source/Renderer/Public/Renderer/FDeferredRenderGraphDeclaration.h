#pragma once

#include "Renderer/FDeferredFramePlan.h"
#include "Renderer/FRenderGraphPass.h"
#include "Renderer/FRenderGraphResource.h"

namespace Stoner::Renderer
{

struct FDeferredGraphResource
{
    Stoner::Core::FString Name;
    ERenderGraphResourceKind Kind = ERenderGraphResourceKind::Texture;
    ERenderGraphResourceOwnership Ownership = ERenderGraphResourceOwnership::Transient;
    Stoner::RHI::ERHIFormat Format = Stoner::RHI::ERHIFormat::Unknown;
    FDeferredExtent2D Extent;
};

struct FDeferredGraphAccess
{
    Stoner::Core::FString PassName;
    Stoner::Core::FString ResourceName;
    ERenderGraphAccessType Access = ERenderGraphAccessType::Read;
    ERenderGraphResourceState RequiredState = ERenderGraphResourceState::Read;
};

struct FDeferredGraphPass
{
    Stoner::Core::FString Name;
    EDeferredPassStage Stage = EDeferredPassStage::SurfaceData;
    ERenderGraphPassType Type = ERenderGraphPassType::Graphics;
    bool bCulled = false;
};

struct FDeferredRenderGraphDeclaration
{
    Stoner::Core::TArray<FDeferredGraphResource> Resources;
    Stoner::Core::TArray<FDeferredGraphPass> Passes;
    Stoner::Core::TArray<FDeferredGraphAccess> Accesses;
    Stoner::Core::FString SceneColorOutput;
    // Retained empty while downstream Feature 028 evidence migrates to the
    // shared Feature 029 formal-output path.
    Stoner::Core::FString FinalOutput;
    bool bValid = false;

    [[nodiscard]] const FDeferredGraphResource* FindResource(
        const Stoner::Core::FString& Name) const noexcept;
    [[nodiscard]] Stoner::Core::FString Dump() const;
};

[[nodiscard]] FDeferredRenderGraphDeclaration BuildDeferredRenderGraphDeclaration(
    const FDeferredFramePlan& Plan, FDeferredDiagnosticLog* Diagnostics = nullptr);

} // namespace Stoner::Renderer
