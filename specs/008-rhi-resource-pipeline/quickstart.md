# Quickstart: RHI Resource & Pipeline Interfaces

**Feature**: 008-rhi-resource-pipeline  
**Date**: 2026-06-26

This quickstart describes the expected build and verification flow after implementation tasks are generated and completed.

## Prerequisites

- Work from the repository root.
- Use the active feature directory `specs/008-rhi-resource-pipeline/`.
- Existing RHI core contracts from `specs/007-rhi-core-interfaces/` are available.
- Use the existing SCons build path and `StonerTest` executable.

## Build

```bash
conda run -n godot scons
```

Expected result:

- Build exits with code 0.
- Public RHI resource/pipeline headers compile through the existing RHI module build.
- No Backend, Renderer, Application, platform-windowing, or graphics API dependency is required by public RHI headers.

## Run Tests

```bash
Build/Mac/Debug/Tests/StonerTest
```

Expected result:

- Existing Core and RHI core tests continue to pass.
- RHI resource/pipeline mock tests pass.
- Test output includes deterministic coverage for:
  - Buffer descriptions and invalid size/usage combinations.
  - Texture descriptions and invalid dimension/mip/layer/format combinations.
  - Sampler descriptions.
  - Shader module stage, entry point, opaque payload identity, and invalid stage/use cases.
  - Multi-set pipeline layout binding declarations.
  - Descriptor set updates and incompatible binding rejection.
  - Graphics pipeline valid and invalid descriptions.
  - Compute pipeline valid and invalid descriptions.
  - Single-subpass render pass descriptions.
  - Framebuffer compatibility checks.
  - Invalidated-object negative paths.
  - Device shutdown creation rejection.

## Header Isolation Checks

Inspect public RHI resource/pipeline headers:

```bash
rg -n "Backend|Renderer|Application|Vulkan|DX12|Metal|OpenGL|GLFW|Cocoa|Win32|X11|Wayland" Source/RHI/Public/RHI
```

Expected result:

- No public RHI resource/pipeline header imports or depends on concrete backend, renderer, application, windowing, or graphics API details.
- Comments or documentation references are acceptable only if they do not introduce include dependencies or public type dependencies.

## Contract Checklist

Before closing the feature:

- `RHIMinimal.h` includes all new public resource/pipeline contracts.
- `IRHIDevice` can create or reject all new object types through explicit result/status values.
- Every public resource/pipeline object exposes lifecycle state with at least `Valid` and `Invalidated`.
- Usage flags are composable, and invalid incompatible combinations have tests.
- Pipeline layouts support set index + binding slot addressing.
- Descriptor sets are created for a specific pipeline layout and set index.
- Shader modules expose opaque payload identity, entry point, and stage without bytecode validation.
- Render pass contracts are single-subpass with attachment roles and load/store behavior.
- Mock tests include at least one success path and one negative path for every public contract introduced in this feature.
