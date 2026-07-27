# B08-S07 Evidence: Deferred Frame Plan, Surfaces, And Graph Inspection

Step: `B08-S07`.

Files inspected:

- `Source/Renderer/Public/Renderer/FDeferredSurfaceData.h`
- `Source/Renderer/Private/FDeferredSurfaceData.cpp`
- `Source/Renderer/Public/Renderer/FDeferredFramePlan.h`
- `Source/Renderer/Private/FDeferredFramePlan.cpp`
- `Source/Renderer/Public/Renderer/FDeferredRenderer.h`
- `Source/Renderer/Private/FDeferredRenderer.cpp`
- `Source/Renderer/Public/Renderer/FDeferredRenderGraphDeclaration.h`
- `Source/Renderer/Private/FDeferredRenderGraphDeclaration.cpp`
- `Source/Renderer/Public/Renderer/FDeferredFrameExecutor.h`
- `Source/Renderer/Private/FDeferredFrameExecutor.cpp`
- `Tests/DeferredRenderingTests.cpp`
- `specs/019-deferred-rendering-pipeline/spec.md`
- `specs/019-deferred-rendering-pipeline/data-model.md`
- `specs/019-deferred-rendering-pipeline/contracts/deferred-renderer-contract.md`

Requirement evidence:

- `specs/019-deferred-rendering-pipeline/spec.md:123` through
  `specs/019-deferred-rendering-pipeline/spec.md:135` require semantic surface
  data, one composition output, and render graph passes with explicit resource
  reads/writes and lifetimes.
- `specs/019-deferred-rendering-pipeline/data-model.md:239` through
  `specs/019-deferred-rendering-pipeline/data-model.md:249` state that
  lighting reads surface semantics and writes accumulation, while composition
  reads accumulation plus required surface data and writes final output.
- `specs/019-deferred-rendering-pipeline/contracts/deferred-renderer-contract.md:81`
  through `specs/019-deferred-rendering-pipeline/contracts/deferred-renderer-contract.md:100`
  define canonical pass order and prohibit reads before prior writes/imports.

Finding evidence:

- `Source/Renderer/Private/FDeferredRenderer.cpp:130` through
  `Source/Renderer/Private/FDeferredRenderer.cpp:132` adds `LightingAccumulation`
  to the `SurfaceData` pass write list.
- `Source/Renderer/Private/FDeferredRenderer.cpp:141` through
  `Source/Renderer/Private/FDeferredRenderer.cpp:157` uses
  `LightingAccumulation` as the lighting-stage write target and composition
  read input.
- `Source/Renderer/Private/FDeferredFrameExecutor.cpp:199` through
  `Source/Renderer/Private/FDeferredFrameExecutor.cpp:231` records the surface
  stage using only the surface stage render pass/framebuffer and surface
  geometry commands.
- `Tests/DeferredRenderingTests.cpp:159` through
  `Tests/DeferredRenderingTests.cpp:175` build the surface framebuffer from
  `BaseColorAO`, `NormalRoughness`, `EmissiveMetallic`, and `Depth` only.
- `Tests/DeferredRenderingTests.cpp:255` through
  `Tests/DeferredRenderingTests.cpp:259` assert graph validity/resource
  count/final output, but not the exact per-pass write set.

Positive evidence:

- `Source/Renderer/Private/FDeferredSurfaceData.cpp:111` through
  `Source/Renderer/Private/FDeferredSurfaceData.cpp:135` declares the initial
  three-color-plus-depth layout and semantic mapping.
- `Source/Renderer/Private/FDeferredSurfaceData.cpp:153` through
  `Source/Renderer/Private/FDeferredSurfaceData.cpp:190` builds the
  inverse-transpose world-normal matrix and rejects singular transforms.
- `Source/Renderer/Private/FDeferredRenderer.cpp:67` through
  `Source/Renderer/Private/FDeferredRenderer.cpp:78` rejects invalid
  view/output/layout before setting a valid plan.
- `Source/Renderer/Private/FDeferredRenderer.cpp:156` through
  `Source/Renderer/Private/FDeferredRenderer.cpp:168` places composition before
  optional forward transparency and validation readback.
- `Source/Renderer/Private/FDeferredRenderGraphDeclaration.cpp:48` through
  `Source/Renderer/Private/FDeferredRenderGraphDeclaration.cpp:57` rejects
  invalid frame plans.
- `Source/Renderer/Private/FDeferredRenderGraphDeclaration.cpp:81` through
  `Source/Renderer/Private/FDeferredRenderGraphDeclaration.cpp:110` rejects
  undeclared reads/writes.
- `Tests/DeferredRenderingTests.cpp:211` through
  `Tests/DeferredRenderingTests.cpp:288` cover surface semantics, canonical pass
  order, graph validity, transparent handoff, and 20-run stable dumps.

Finding recorded:

- `CR001-B08-F004`: Deferred surface pass declares lighting accumulation writes.
- Severity: S2.
- Disposition: Accepted.

Next step:

- Fix `CR001-B08-F004` by removing `LightingAccumulation` from the surface-data
  pass write set and adding focused graph/write-set regression coverage for
  lit and no-light frames.
