# Quickstart: Material & Shader System

## Goal

Validate that the Material & Shader System can define materials, resolve material instance chains, select precompiled shader variants, report resource requirements, and produce deterministic diagnostics/text dumps without requiring a physical graphics device or visible presentation.

## Expected Development Flow

1. Add Renderer public contracts under `Source/Renderer/Public/Renderer/`.
2. Add private implementation files under `Source/Renderer/Private/`.
3. Export material/shader foundation headers through `RendererMinimal.h` or direct public includes.
4. Add focused tests in `Tests/RendererMaterialShaderTests.cpp` and wire them into `Tests/Main.cpp`.
5. Keep all material/shader behavior backend-agnostic; do not include backend-specific headers in Renderer material code.
6. Keep shader records explicitly registered in memory; do not implement runtime shader compilation or shader file loading.

## Representative Material Scenario

Create a material library with:

- At least 5 root materials.
- At least 10 material instances.
- At least one instance chain that inherits from another instance.
- At least one deliberate inheritance cycle negative case.
- Four parameter categories: scalar, vector, color-like, and abstract resource reference.
- At least 3 shader records with declared allowed permutation flags.
- At least 3 matching precompiled variants.
- At least one material with no resource requirements.
- At least one material instance that overrides a resource reference.

Expected results:

- Valid materials and instances validate successfully.
- Effective parameters follow nearest-override precedence through parent chains.
- Inheritance cycles fail before parameter resolution.
- Unknown root parameters and type-mismatched overrides fail with diagnostics.
- Equivalent permutation flag sets resolve to the same canonical key across 20 repeated resolutions.
- Unknown permutation flags fail before missing-variant checks.
- Missing shader records and missing variants fail with deterministic diagnostics.
- Resource requirements use abstract Renderer-level references and can be consumed by a render graph pass declaration flow.
- Text dumps are byte-identical across 20 repeated inspections of unchanged data.

## Negative Scenarios

Cover at minimum:

- Duplicate material parameter names.
- Unsupported domain/blend combinations.
- Missing shader record.
- Unknown permutation flag.
- Missing shader variant.
- Missing required shader parameter.
- Override for a parameter not defined by the root material.
- Override type mismatch.
- Material instance inheritance cycle.
- Use after invalidation.
- Attempt to treat live RHI resources or graph-local handles as material parameter values.
- Empty shader library lookup.

## Verification Commands

```bash
conda run -n godot scons
Build/Mac/Debug/Tests/StonerTest
```

## Boundary Check

```bash
rg -n "Vulkan|Metal|DX12|DirectX|OpenGL|GLES|WebGL|Vk[A-Z]|ID3D|MTL" Source/Renderer/Public/Renderer Source/Renderer/Private
```

The boundary check should not find backend-specific dependencies in Renderer material/shader code.

## Implementation Verification Results

Recorded on 2026-07-01 from branch `014-material-shader-system`.

| Check | Result | Notes |
|-------|--------|-------|
| `conda run -n godot scons` | PASS | Built Core, RHI, Renderer, Application, Vulkan backend, and `Build/Mac/Debug/Tests/StonerTest`; Renderer auto-discovered 16 private source files. |
| `Build/Mac/Debug/Tests/StonerTest` | PASS | Full test executable returned 0; material/shader tests added 23 passing assertions covering all four user stories and negative scenarios. |
| Representative elapsed target | PASS | `Representative material validation inspection and resource summary is under 60 seconds` passed in `Tests/RendererMaterialShaderTests.cpp`. |
| Boundary check | PASS | `rg -n "Vulkan|Metal|DX12|DirectX|OpenGL|GLES|WebGL|Vk[A-Z]|ID3D|MTL" Source/Renderer/Public/Renderer Source/Renderer/Private` returned no matches. |
