#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FForwardDiagnostics.h"
#include "Renderer/FForwardFramePlan.h"
#include "Renderer/FForwardFrameExecutor.h"
#include "Renderer/FForwardLightData.h"
#include "Renderer/FForwardRenderGraphDeclaration.h"
#include "Renderer/FForwardRenderer.h"
#include "Renderer/FForwardViewData.h"
#include "Renderer/FMaterial.h"
#include "Renderer/FMaterialDiagnostics.h"
#include "Renderer/FMaterialInstance.h"
#include "Renderer/FMaterialParameterSet.h"
#include "Renderer/FMaterialResourceRequirement.h"
#include "Renderer/FMaterialShaderBinding.h"
#include "Renderer/FMeshDrawCommand.h"
#include "Renderer/FRenderGraph.h"
#include "Renderer/FRenderGraphBuilder.h"
#include "Renderer/FRenderGraphCompiler.h"
#include "Renderer/FRenderGraphDiagnostics.h"
#include "Renderer/FRenderGraphExecutor.h"
#include "Renderer/FRenderGraphPass.h"
#include "Renderer/FRenderGraphResource.h"
#include "Renderer/FShaderLibrary.h"
#include "Renderer/FShaderPermutation.h"

// Renderer layer minimal header — high-level rendering
// Note: RHI dependency is used internally (Private/) but not exposed in public API.
// This enforces the adjacent-only dependency contract at the include path level.
namespace Stoner::Renderer
{
} // namespace Stoner::Renderer
