# B06-S18 Evidence: Forward Execution, Graph Declaration, And Diagnostics Verification

Step: `B06-S18`.

Finding verified:

- `CR001-B06-F006`: Forward graph declarations omit material resource access
  edges.

Source evidence:

- `Source/Renderer/Private/FForwardRenderGraphDeclaration.cpp:41` defines the
  material access emission helper.
- `Source/Renderer/Private/FForwardRenderGraphDeclaration.cpp:197` records
  opaque material resource reads.
- `Source/Renderer/Private/FForwardRenderGraphDeclaration.cpp:218` records
  transparent material resource reads.

Regression evidence:

- `Tests/RendererForwardPipelineTests.cpp:185` checks
  `ForwardOpaqueLighting` reads `Textures/OpaqueA`.
- `Tests/RendererForwardPipelineTests.cpp:186` checks `ForwardTransparent`
  reads `Textures/GlassFar`.

Gate evidence:

- `CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/gate-fallback-strict.json`
  records `fallback-strict` passed at `2026-07-27T07:22:11+00:00`.

Result:

- `CR001-B06-F006` is verified.
