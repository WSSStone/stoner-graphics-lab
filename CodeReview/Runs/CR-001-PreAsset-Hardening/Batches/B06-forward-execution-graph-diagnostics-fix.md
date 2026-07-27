# B06-S17: Forward Execution, Graph Declaration, And Diagnostics Fix

## Scope

Fixed `CR001-B06-F006`.

Code changed:

- `Source/Renderer/Private/FForwardRenderGraphDeclaration.cpp`
- `Tests/RendererForwardPipelineTests.cpp`

## Fix

- `BuildForwardRenderGraphDeclaration` now emits read access declarations for
  material resource requirements consumed by accepted opaque and transparent
  forward draws.
- Material resource access records are deduplicated per pass so repeated
  bindings do not produce noisy graph declarations.
- Empty material resource identifiers are ignored consistently with the existing
  resource declaration path.

## Regression Coverage

Added a forward pipeline regression asserting that:

- `ForwardOpaqueLighting` reads the opaque draw material texture resource.
- `ForwardTransparent` reads the transparent draw material texture resource.

## Verification

- `scons config=debug`: passed.
- `Build/Mac/Debug/Tests/StonerTest`: the new forward graph access regression
  passed; this local graphics-enabled run hit the known intermittent Deferred
  native MoltenVK failure recorded separately as `CR001-B08-F001`.
- `crctl gate fallback-strict --id CR-001`: passed at
  `2026-07-27T07:17:32+00:00`.

## Commit

- `10c3c5c`: `fix(renderer): declare forward material resource accesses`
