# B08-S08 Evidence: Deferred Frame Plan, Surfaces, And Graph Fix

Step: `B08-S08`.

Finding fixed:

- `CR001-B08-F004`: Deferred surface pass declares lighting accumulation writes.

Code evidence:

- `Source/Renderer/Private/FDeferredRenderer.cpp:130` through
  `Source/Renderer/Private/FDeferredRenderer.cpp:132` now declare the
  `SurfaceData` write set as `BaseColorAO`, `NormalRoughness`,
  `EmissiveMetallic`, and `Depth`.
- `Source/Renderer/Private/FDeferredRenderer.cpp:141` through
  `Source/Renderer/Private/FDeferredRenderer.cpp:157` retain
  `LightingAccumulation` as the lighting-stage write target and composition
  read input.

Test evidence:

- `Tests/DeferredRenderingTests.cpp:36` through
  `Tests/DeferredRenderingTests.cpp:39` add a deterministic resource-name
  containment helper for exact pass read/write assertions.
- `Tests/DeferredRenderingTests.cpp:260` through
  `Tests/DeferredRenderingTests.cpp:263` assert lit-frame graph ownership keeps
  `LightingAccumulation` out of surface writes and in lighting/composition
  access sets.
- `Tests/DeferredRenderingTests.cpp:277` through
  `Tests/DeferredRenderingTests.cpp:280` assert empty-frame planning also keeps
  `LightingAccumulation` out of surface writes.

Gate evidence:

- Command: `conda run -n stoner-cr python CodeReview/Tools/crctl.py gate fallback-strict --id CR-001`
- Result: passed.
- Recorded at: `2026-07-27T09:23:16+00:00`.
- Build profile: `scons config=debug strict=1 graphics=disabled`.
- Test executable: `Build/Mac/Debug/Tests/StonerTest`.

Focused PASS extraction:

```text
[PASS] Deferred graph ownership keeps lighting accumulation out of surface writes
[PASS] Deferred empty frame graph keeps accumulation as composition input only
```

Commit:

- `d42714b fix(renderer): align deferred surface graph writes`

Next step:

- Verify `CR001-B08-F004` in B08-S09 using the recorded fallback-strict gate
  and focused regression PASS lines.
