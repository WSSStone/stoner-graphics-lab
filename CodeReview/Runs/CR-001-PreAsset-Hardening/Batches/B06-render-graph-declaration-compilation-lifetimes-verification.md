# B06-S03: Render Graph Declaration, Compilation, And Lifetimes Verification

## Verification Target

`CR001-B06-F001` verifies implementation commit `7292a65`: render graph
execution must validate and resolve only resources required by compiled
scheduled passes, not resources belonging exclusively to culled branches.

## Parent Behavior

Parent revision `7292a65^` showed:

- `FRenderGraphExecutor::Execute` iterated all `Graph.Resources` at line 76.
- Missing imported resources were rejected inside that full-resource loop.
- Simulated transient resolution failure was evaluated inside that
  full-resource loop.
- Pass callbacks were invoked later from `Graph.GetCompiledGraph().ScheduledPasses`.

That ordering meant culled branch resources could fail execution before the
scheduled pass sequence ran.

## Current Behavior

Current revision derives `RequiredResources` from
`Graph.GetCompiledGraph().ScheduledPasses`, marks only scheduled-pass accesses
as required, and skips unresolved state for all other resources.

The existing required-resource behavior remains intact:

- scheduled imported resources still need valid external bindings;
- scheduled transient resources still honor transient-resolution failure;
- pass callbacks still run only for compiled scheduled passes.

## Regression Evidence

New tests are present in `Tests/RendererRenderGraphTests.cpp`:

- `Render graph culled-import fixture compiles`
- `Render graph ignores missing imports used only by culled passes`
- `Render graph culled-transient fixture compiles`
- `Render graph ignores transient resolution failures used only by culled passes`

Focused local output observed all four checks passing. A full local graphics
enabled `StonerTest` still reports the current Mac execution device's
MoltenVK/Metal unavailable boundary and the triangle deterministic lifecycle
failure; this is unrelated to the render graph fix and is avoided by the
headless CR gates below.

## Local Gates

- `fallback-strict`: passed at `2026-07-27T06:04:23+00:00`.
- `strict-release`: passed at `2026-07-27T06:04:34+00:00`.
- `sanitizers`: passed at `2026-07-27T06:05:44+00:00`.

## Finding Decision

- `CR001-B06-F001`: Verified.

B06 render graph declaration/compilation/lifetime scope has no remaining
accepted finding after this verification step.
