# B06-S18: Forward Execution, Graph Declaration, And Diagnostics Verification

## Scope

Verified `CR001-B06-F006`.

Files checked:

- `Source/Renderer/Private/FForwardRenderGraphDeclaration.cpp`
- `Tests/RendererForwardPipelineTests.cpp`

## Verification

- `BuildForwardRenderGraphDeclaration` calls `AddMaterialResourceAccesses` for
  accepted opaque draws in `ForwardOpaqueLighting`.
- `BuildForwardRenderGraphDeclaration` calls `AddMaterialResourceAccesses` for
  accepted transparent draws in `ForwardTransparent`.
- `RendererForwardPipelineTests.cpp` asserts read access declarations for
  `Textures/OpaqueA` and `Textures/GlassFar`.

## Gate Evidence

- `crctl gate fallback-strict --id CR-001`: passed at
  `2026-07-27T07:22:11+00:00`.

## Result

`CR001-B06-F006` is verified.
