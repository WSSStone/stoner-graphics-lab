"""Extract specification requirements and seed traceability records."""

from __future__ import annotations

import re
from pathlib import Path
from typing import Any

from reviewlib import write_csv


FEATURE_RANGE = range(3, 20)
REQUIREMENT_PATTERN = re.compile(
    r"^\s*[-*]\s+\*\*(FR|SC)-(\d{3}[a-z]?)\*\*:\s*(.+?)\s*$"
)
TRACE_FIELDS = [
    "trace_id",
    "feature",
    "kind",
    "requirement_id",
    "requirement",
    "spec_path",
    "api",
    "implementation",
    "tests",
    "ci_evidence",
    "classification",
    "notes",
]

FEATURE_EVIDENCE: dict[str, dict[str, str]] = {
    "003": {
        "api": "Source/Core/Public/Core/{FPlatformTypes,FString,FName,FMemory,TArray,TMap,TSharedPtr,TUniquePtr}.h",
        "implementation": "Source/Core/Private/{CoreModule,FMemory}.cpp",
        "tests": "Tests/CoreFoundationTests.cpp; Tests/Main.cpp",
        "ci_evidence": ".github/workflows/ci.yml; Build/*/Tests/StonerTest",
        "notes": "Feature-level evidence seed for Core types, memory, containers, and ownership vocabulary.",
    },
    "004": {
        "api": "Source/Core/Public/Core/{FMath,FVector2,FVector3,FVector4,FMatrix4x4,FQuat,FTransform,FColor,FBox,FSphere,FPlane}.h",
        "implementation": "Header-only Core math implementation",
        "tests": "Tests/CoreMathTests.cpp; Tests/Main.cpp",
        "ci_evidence": ".github/workflows/ci.yml; Build/*/Tests/StonerTest",
        "notes": "Feature-level evidence seed for Core math contracts and boundary tests.",
    },
    "005": {
        "api": "Source/Core/Public/Core/{FLog,FLogCategory,FLogConsoleSink,SGAssert,SGLog,SGPlatformBreak}.h",
        "implementation": "Source/Core/Private/{FLog,FLogCategory,FLogConsoleSink}.cpp",
        "tests": "Tests/LoggingAssertionTests.cpp; Tests/Main.cpp",
        "ci_evidence": ".github/workflows/ci.yml; Build/*/Tests/StonerTest",
        "notes": "Feature-level evidence seed for logging, assertion, filtering, and thread-safety contracts.",
    },
    "006": {
        "api": "Source/Core/Public/Core/{FPlatformFileSystem,FPlatformMemory,FPlatformMisc,FPlatformProcess,FPlatformTime,FPlatformWindow,SGPlatform}.h",
        "implementation": "Source/Core/Private/FPlatform*.cpp",
        "tests": "Tests/CorePlatformTests.cpp; Tests/CorePlatformOwnershipTests.cpp; Tests/verify_platform_identity.py",
        "ci_evidence": ".github/workflows/ci.yml; Build/*/Tests/StonerTest",
        "notes": "Feature-level evidence seed for platform abstraction and edge-case coverage.",
    },
    "007": {
        "api": "Source/RHI/Public/RHI/IRHI*.h; Source/RHI/Public/RHI/ERHI*.h; Source/RHI/Public/RHI/FRHI*Desc.h",
        "implementation": "Source/RHI/Private/RHIModule.cpp; mock implementations in Tests/RHICoreTests.cpp",
        "tests": "Tests/RHICoreTests.cpp; Tests/Main.cpp",
        "ci_evidence": ".github/workflows/ci.yml; Build/*/Tests/StonerTest",
        "notes": "Feature-level evidence seed for RHI core interfaces, lifecycles, and mock contracts.",
    },
    "008": {
        "api": "Source/RHI/Public/RHI/{IRHIBuffer,IRHITexture,IRHISampler,IRHIShaderModule,IRHIDescriptorSet,IRHIGraphicsPipeline,IRHIComputePipeline,IRHIRenderPass,IRHIFramebuffer}.h",
        "implementation": "Source/RHI/Public/RHI/FRHI*Desc.h validators; Tests/RHICoreTests.cpp mocks",
        "tests": "Tests/RHICoreTests.cpp; Tests/VulkanBackendTests.cpp; Tests/Main.cpp",
        "ci_evidence": ".github/workflows/ci.yml; Build/*/Tests/StonerTest",
        "notes": "Feature-level evidence seed for resource, descriptor, pipeline, render-pass, and framebuffer contracts.",
    },
    "009": {
        "api": "Source/Backend/Vulkan/Public/VulkanRHI/{FVulkanInstance,FVulkanDevice,FVulkanSurface,FVulkanSwapchain,FVulkanQueue,FVulkanFence,FVulkanSemaphore,FVulkanRuntimeSnapshot}.h",
        "implementation": "Source/Backend/Vulkan/Private/{FVulkanInstance,FVulkanDevice,FVulkanSurface,FVulkanSwapchain,FVulkanQueue,FVulkanFence,FVulkanSemaphore}.cpp",
        "tests": "Tests/VulkanBackendTests.cpp; Tests/VulkanNativeIntegrationTests.cpp",
        "ci_evidence": ".github/workflows/ci.yml; Build/*/Tests/StonerTest",
        "notes": "Feature-level evidence seed for Vulkan instance, device, queue, sync, surface, and swapchain behavior.",
    },
    "010": {
        "api": "Source/Backend/Vulkan/Public/VulkanRHI/{FVulkanBuffer,FVulkanTexture,FVulkanSampler,FVulkanDescriptorPool,FVulkanDescriptorSet,FVulkanMemoryAllocator,FVulkanResourceAllocation,FVulkanUploadStaging}.h",
        "implementation": "Source/Backend/Vulkan/Private/FVulkan{Buffer,Texture,Sampler,DescriptorPool,DescriptorSet,MemoryAllocator,ResourceAllocation,UploadStaging}.cpp",
        "tests": "Tests/VulkanBackendTests.cpp",
        "ci_evidence": ".github/workflows/ci.yml; Build/*/Tests/StonerTest",
        "notes": "Feature-level evidence seed for Vulkan resources, descriptors, uploads, allocation, and lifecycle invalidation.",
    },
    "011": {
        "api": "Source/Backend/Vulkan/Public/VulkanRHI/{FVulkanCommandBuffer,FVulkanCommandPool,FVulkanCommandSubmission,FVulkanQueue,FVulkanRenderPass,FVulkanFramebuffer}.h",
        "implementation": "Source/Backend/Vulkan/Private/FVulkan{CommandBuffer,CommandPool,Queue,RenderPass,Framebuffer}.cpp",
        "tests": "Tests/VulkanBackendTests.cpp; Tests/RHICoreTests.cpp",
        "ci_evidence": ".github/workflows/ci.yml; Build/*/Tests/StonerTest",
        "notes": "Feature-level evidence seed for command recording, queue submission, barriers, render pass, framebuffer, and uploads.",
    },
    "012": {
        "api": "Source/Backend/Vulkan/Public/VulkanRHI/{FVulkanShaderModule,FVulkanPipelineLayout,FVulkanGraphicsPipeline,FVulkanComputePipeline,FVulkanPipelineCache,FVulkanNativeContext}.h",
        "implementation": "Source/Backend/Vulkan/Private/FVulkan{ShaderModule,PipelineLayout,GraphicsPipeline,ComputePipeline,PipelineCache,NativeContext}.cpp",
        "tests": "Tests/VulkanBackendTests.cpp; Tests/VulkanNativeIntegrationTests.cpp; Tests/RHICoreTests.cpp",
        "ci_evidence": ".github/workflows/ci.yml; Build/*/Tests/StonerTest",
        "notes": "Feature-level evidence seed for Vulkan shader validation, pipeline creation, pipeline cache, and native ownership.",
    },
    "013": {
        "api": "Source/Renderer/Public/Renderer/FRenderGraph*.h",
        "implementation": "Source/Renderer/Private/FRenderGraph*.cpp",
        "tests": "Tests/RendererRenderGraphTests.cpp; Tests/Main.cpp",
        "ci_evidence": ".github/workflows/ci.yml; Build/*/Tests/StonerTest",
        "notes": "Feature-level evidence seed for render graph declaration, compilation, diagnostics, execution, and dumps.",
    },
    "014": {
        "api": "Source/Renderer/Public/Renderer/{FMaterial,FMaterialInstance,FMaterialParameterSet,FMaterialShaderBinding,FMaterialResourceRequirement,FShaderLibrary,FShaderPermutation,FMaterialDiagnostics}.h",
        "implementation": "Source/Renderer/Private/FMaterial*.cpp; Source/Renderer/Private/FShader*.cpp",
        "tests": "Tests/RendererMaterialShaderTests.cpp; Tests/Main.cpp",
        "ci_evidence": ".github/workflows/ci.yml; Build/*/Tests/StonerTest",
        "notes": "Feature-level evidence seed for material, shader record, permutation, binding, diagnostics, and resource requirement behavior.",
    },
    "015": {
        "api": "Source/Renderer/Public/Renderer/FForward*.h; Source/Renderer/Public/Renderer/FMeshDrawCommand.h",
        "implementation": "Source/Renderer/Private/FForward*.cpp; Source/Renderer/Private/FMeshDrawCommand.cpp",
        "tests": "Tests/RendererForwardPipelineTests.cpp; Tests/Main.cpp",
        "ci_evidence": ".github/workflows/ci.yml; Build/*/Tests/StonerTest",
        "notes": "Feature-level evidence seed for forward planning, light selection, draw sorting, diagnostics, and render-graph declaration.",
    },
    "016": {
        "api": "Source/Application/Public/Application/{FWindow,FWindowDesc,FWindowEvent,FInputEvent,FInputManager,FInputState,EKey,EMouseButton,FApplicationLoop,FApplicationDiagnostics}.h",
        "implementation": "Source/Application/Private/{FWindow,FGlfwWindowDriver,FHeadlessWindowDriver,FInputEvent,FInputManager,FInputState,FApplicationLoop,FApplicationDiagnostics}.cpp",
        "tests": "Tests/ApplicationWindowInputTests.cpp; Tests/Main.cpp",
        "ci_evidence": ".github/workflows/ci.yml; Build/*/Tests/StonerTest",
        "notes": "Feature-level evidence seed for window lifecycle, input events/states, headless driver, GLFW mapping, and loop semantics.",
    },
    "017": {
        "api": "Source/Application/Public/Application/{FWorld,FEntity,FEntityHierarchy,FTransformComponent,FMeshComponent,FLightComponent,FCameraComponent,FRenderSystem,FSceneDiagnostics,FSceneRenderSummary}.h",
        "implementation": "Source/Application/Private/{FWorld,FEntity,FEntityHierarchy,FTransformComponent,FMeshComponent,FLightComponent,FCameraComponent,FRenderSystem,FSceneDiagnostics,FSceneRenderSummary}.cpp",
        "tests": "Tests/ApplicationSceneEcsTests.cpp; Tests/Main.cpp",
        "ci_evidence": ".github/workflows/ci.yml; Build/*/Tests/StonerTest",
        "notes": "Feature-level evidence seed for ECS handles, components, hierarchy, transforms, render collection, and scene diagnostics.",
    },
    "018": {
        "api": "Demo/StonerDemo/Private/*.h; Source/Backend/Vulkan/Public/VulkanRHI/FVulkanNativeContext.h",
        "implementation": "Demo/StonerDemo/Private/*.cpp; Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp; .github/scripts/run_triangle_demo_validation.py",
        "tests": "Tests/TriangleDemoIntegrationTests.cpp; Tests/VulkanNativeIntegrationTests.cpp; Tests/Main.cpp",
        "ci_evidence": "Validation/018/completion.md; Validation/018/{Windows,macOS,Linux}; .github/workflows/ci.yml",
        "notes": "Feature-level evidence seed for demo integration, deterministic/native modes, validation monitoring, and visible evidence.",
    },
    "019": {
        "api": "Source/Renderer/Public/Renderer/FDeferred*.h; Source/Renderer/Public/Renderer/FRendererComparisonReport.h; Source/Backend/Vulkan/Public/VulkanRHI/FVulkanNativeContext.h",
        "implementation": "Source/Renderer/Private/FDeferred*.cpp; Source/Renderer/Private/FRendererComparisonReport.cpp; Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp; .github/scripts/run_deferred_validation.py",
        "tests": "Tests/DeferredRenderingTests.cpp; Tests/DeferredNativeIntegrationTests.cpp; Tests/RendererComparisonTests.cpp; Tests/Main.cpp",
        "ci_evidence": "Validation/019/completion.md; Validation/019/Linux/deferred-readback-report.txt; Validation/019/Linux/renderer-comparison-report.txt; .github/workflows/ci.yml",
        "notes": "Feature-level evidence seed for deferred planning, execution, native readback, comparison artifacts, and validation wrappers.",
    },
}


def enrich_records(records: list[dict[str, Any]]) -> list[dict[str, Any]]:
    enriched: list[dict[str, Any]] = []
    for record in records:
        item = dict(record)
        evidence = FEATURE_EVIDENCE.get(item.get("feature", ""), {})
        for field in ("api", "implementation", "tests", "ci_evidence", "notes"):
            if not item.get(field):
                item[field] = evidence.get(field, "")
        if item.get("api") and item.get("implementation") and item.get("tests"):
            item["classification"] = "FeatureMapped"
        enriched.append(item)
    return enriched


def feature_specs(repo: Path) -> list[Path]:
    specs = []
    for feature in FEATURE_RANGE:
        matches = sorted((repo / "specs").glob(f"{feature:03d}-*/spec.md"))
        specs.extend(matches)
    return specs


def extract(repo: Path) -> list[dict[str, Any]]:
    records: list[dict[str, Any]] = []
    for path in feature_specs(repo):
        feature = path.parent.name[:3]
        for line_number, line in enumerate(
            path.read_text(encoding="utf-8").splitlines(), start=1
        ):
            match = REQUIREMENT_PATTERN.match(line)
            if not match:
                continue
            kind, number, requirement = match.groups()
            requirement_id = f"{kind}-{number}"
            records.append(
                {
                    "trace_id": f"{feature}-{requirement_id}",
                    "feature": feature,
                    "kind": kind,
                    "requirement_id": requirement_id,
                    "requirement": requirement,
                    "spec_path": f"{path.relative_to(repo)}:{line_number}",
                    "api": "",
                    "implementation": "",
                    "tests": "",
                    "ci_evidence": "",
                    "classification": "Unclassified",
                    "notes": "",
                }
            )
    return records


def write_seed(repo: Path, output: Path) -> int:
    records = enrich_records(extract(repo))
    write_csv(output, TRACE_FIELDS, records)
    return len(records)
