# Quickstart: Render Graph Foundation

## Goal

Validate that the Render Graph Foundation can be declared, compiled, inspected, and executed through mock RHI behavior without requiring a physical graphics device.

## Expected Development Flow

1. Add Renderer public contracts under `Source/Renderer/Public/Renderer/`.
2. Add private implementation files under `Source/Renderer/Private/`.
3. Export the new render graph headers through `RendererMinimal.h` or direct public includes.
4. Add focused tests in `Tests/RendererRenderGraphTests.cpp` and wire them into `Tests/Main.cpp`.
5. Keep all graph behavior backend-agnostic; do not include backend-specific headers in Renderer graph code.

## Representative Graph Scenario

Create a graph with:

- Five passes: one producer graphics pass, one compute pass, one copy-like utility pass, one output pass, and one side-effect-preserving pass.
- Four virtual resources.
- One caller-supplied imported resource.
- One exported output.
- At least one unused branch for culling.
- At least one transition of each covered category: read-after-write, write-after-read, write-after-write, graphics-to-compute, compute-to-graphics.

Expected results:

- Compile succeeds.
- Schedule is deterministic across 20 repeated compilations.
- Unused branch is culled.
- Side-effect pass is preserved.
- Transition plan is inspectable.
- Execution emits transitions matching the compiled plan.
- Transient resources resolve during execution.
- Imported resource validation succeeds when provided and fails when missing.
- Eligible aliasing pairs are reported but still receive separate backing storage.

## Negative Scenarios

Cover at minimum:

- Dependency cycle.
- Transient read before write.
- Write to read-only imported resource.
- Incompatible usage/layout declaration.
- No output and no side-effect-preserving pass.
- Execution before compile.
- Missing imported resource during execution.
- Transient resource resolution failure.
- Pass callback failure stops later passes.
- Reset and invalidation reject stale schedules/resources.

## Verification Commands

```bash
conda run -n godot scons
Build/Mac/Debug/Tests/StonerTest
```

### Recorded Verification - 2026-07-01

- `conda run -n godot scons`: PASS. Build completed successfully on Mac Debug. Toolchain emitted macOS temporary-directory warnings from `confstr()`, but no project compiler warnings remained after cleanup.
- `Build/Mac/Debug/Tests/StonerTest`: PASS. Existing Core, RHI, Vulkan, and new Renderer render graph tests passed.
- Representative graph elapsed time: PASS. The test `Render graph representative declare compile inspect execute is under 60 seconds` passed in the local test run.

## Boundary Check

```bash
rg -n "Vulkan|Metal|DX12|DirectX|OpenGL|GLES|WebGL|Vk[A-Z]|ID3D|MTL" Source/Renderer/Public/Renderer Source/Renderer/Private
```

The boundary check should not find backend-specific dependencies in render graph code.

### Recorded Boundary Check - 2026-07-01

- Command returned no matches for Renderer public/private render graph code.
