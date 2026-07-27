# B06-S02 Evidence: Render Graph Culled Resource Fix

Finding: `CR001-B06-F001`.

Fix commit: `7292a65`.

Code evidence:

- `Source/Renderer/Private/FRenderGraphExecutor.cpp` derives
  `RequiredResources` from `Compiled.ScheduledPasses`.
- The executor skips resource validation/resolution for handles not accessed by
  scheduled passes.
- Required scheduled resources retain existing imported binding and transient
  resolution checks.

Test evidence:

- `Tests/RendererRenderGraphTests.cpp` now covers missing imported resources
  used only by culled passes.
- `Tests/RendererRenderGraphTests.cpp` now covers transient resolution failures
  used only by culled passes.
- Focused test output showed all four new culled-resource checks passing.

Gate evidence:

- `gate-fallback-strict.json`: passed at `2026-07-27T06:04:23+00:00`.
- `gate-strict-release.json`: passed at `2026-07-27T06:04:34+00:00`.
- `gate-sanitizers.json`: passed at `2026-07-27T06:05:44+00:00`.

Decision: `CR001-B06-F001` is Fixed and ready for a later B06 verification step.
