# Quickstart: Scene Graph & ECS Foundation

## Goal

Validate that Application scene graph and ECS contracts can organize entities, components, hierarchy transforms, and render-facing summaries without requiring a visible window, GPU presentation, local assets, or backend graphics API runtime.

## Expected Workflow

1. Create a world.
2. Create entities and verify live handles.
3. Add transform, mesh, light, and camera components.
4. Attempt duplicate component add and verify rejection.
5. Destroy a parent entity and verify descendants plus stale handles become invalid.
6. Reuse destroyed slots and verify generation/version validation protects stale handles.
7. Build a hierarchy and propagate world transforms in topological order.
8. Reparent with default world-transform preservation and explicit local-transform preservation.
9. Collect render summaries and verify per-category entity-identity ordering.
10. Inspect diagnostics and debug dumps for stable output.

## Local Build

```sh
conda run -n godot scons
```

Expected result:

- Build succeeds on the local development platform.
- Application scene/ECS test sources compile into the existing `StonerTest` executable.

## Local Test

macOS:

```sh
Build/Mac/Debug/Tests/StonerTest
```

Linux:

```sh
Build/Linux/Debug/Tests/StonerTest
```

Windows:

```sh
Build/Win64/Debug/Tests/StonerTest.exe
```

Expected result:

- Core, RHI, Renderer, Vulkan, Application window/input, and Application scene/ECS tests pass.
- Scene/ECS tests report zero failures.

## Required Test Coverage

Scene/ECS validation must cover:

- Empty world reset and query behavior.
- Entity creation, destruction, slot reuse, and stale-handle rejection.
- Recursive destruction of descendants.
- Duplicate component add rejection.
- Explicit component update, replace, read, and removal.
- Invalid component data diagnostics.
- Parent, unparent, and reparent operations.
- Default world-transform-preserving reparent.
- Explicit local-transform-preserving reparent.
- Cycle and self-parent rejection.
- Topological hierarchy operation order: parents before children, roots in creation order, siblings in insertion order.
- Render collection accepted/rejected records.
- Render collection category ordering by entity identity, with optional sort key tie-break behavior.
- Deterministic diagnostics and byte-stable inspection dumps across at least 20 repeated runs.

## Cross-Platform Validation

The existing GitHub Actions or equivalent CI matrix must continue to cover:

- `ubuntu-latest`
- `macos-latest`
- `windows-latest`

Each platform should run the SCons build and deterministic headless `StonerTest` executable. This feature must not require display access, a GPU-backed window, local scene assets, external services, or platform-specific APIs.

## Boundary Checks

Public Application scene headers must not include or expose:

- Vulkan, Metal, DX, OpenGL, GLES, or WebGL types.
- RHI resource handles.
- Renderer frame plan objects.
- Backend allocation or command objects.
- Native OS handles.
- Asset loader or serialization formats.

Suggested source inspection:

```sh
rg -n "\bVk[A-Za-z_]*\b|Vulkan|Metal|D3D|ID3D|OpenGL|GLFW|NSWindow|HWND|xcb_|wl_|IRHI|FVulkan|FForwardFramePlan" Source/Application/Public/Application
```

Expected result:

- No matches for backend, platform-window, RHI resource, or Renderer execution leakage in public Application scene contracts.

## Out-of-Scope Confirmation

This feature is not complete if it silently adds any of the following:

- Physics components.
- Animation components.
- Scripting or behavior components.
- Scene serialization or asset loading.
- Editor UI or gizmos.
- Multi-world scheduling.
- Archetype-based query optimization.
- Octree, BVH, grid, or other spatial acceleration structures as authoritative world storage.
- Live GPU/RHI/backend resource ownership in Application scene components.
