# Research: Deferred Rendering Pipeline

## Decision: Keep forward and deferred as sibling Renderer strategies

**Decision**: Add `FDeferredRenderer` as a sibling strategy to `FForwardRenderer`. Both consume shared view, material, draw, light identity, and output semantics, but own separate frame plans, diagnostics, graph declarations, and executors. Scene/ECS collection selects and feeds a renderer; neither renderer traverses or owns the world.

**Rationale**: Feature 019 is an alternative path, not a replacement. A sibling Strategy preserves the existing default, prevents deferred-only resources from leaking into forward frames, and leaves a stable selection boundary for later application settings.

**Alternatives considered**:

- Add deferred branches throughout `FForwardRenderer`: rejected because one class would accumulate incompatible pass, resource, and light policies.
- Make the scene/ECS world build the G-Buffer directly: rejected because scene ownership and rendering execution are separate layers.
- Replace forward opaque rendering: rejected because FR-001 and existing regression behavior require forward to remain unchanged.

## Decision: Use three color surface targets plus one depth target

**Decision**: Define the initial single-sample surface layout as:

| Target | Format | Semantic channels | Clear value |
|--------|--------|-------------------|-------------|
| `SurfaceBaseColorAO` | `R8G8B8A8_UNorm` | RGB linear base color, A ambient occlusion | `(0, 0, 0, 1)` |
| `SurfaceNormalRoughness` | `R16G16B16A16_Float` | XYZ normalized world-space normal, W roughness | `(0, 0, 1, 1)` |
| `SurfaceEmissiveMetallic` | `R16G16B16A16_Float` | RGB linear emissive, A metallic | `(0, 0, 0, 0)` |
| `SurfaceDepth` | `D32_Float` | normalized device depth used with inverse-view-projection world-space reconstruction | far depth: `1.0` for standard-Z, `0.0` for reversed-Z |

All targets share extent, sample count one, and one layout compatibility identity. Alpha from a masked material controls coverage and is not stored as a deferred lighting input.

**Rationale**: This layout carries every clarified material semantic with formats already present in the RHI. Direct XYZ normals avoid octahedral encode/decode ambiguity during the first native pixel-validation milestone. Three color targets are a practical correctness-first baseline; channel compression can be measured later.

**Alternatives considered**:

- Store only roadmap-minimum base color, normal, metallic/roughness, and depth: rejected because Q4 requires emissive and ambient occlusion.
- Octahedrally encode normals into two channels: deferred because it adds precision policy before a baseline exists.
- Store world position: rejected because depth reconstruction avoids a large additional target and is compatible with later screen-space techniques.
- Use sRGB attachment formats: rejected for the first contract because the RHI does not yet expose sRGB formats and validation is clearer in explicitly linear values.

## Decision: Use world-space lighting inputs and depth reconstruction

**Decision**: Store normalized world-space normals and reconstruct world-space position from sampled depth, pixel coordinates, inverse view-projection, and the active depth convention. Directional, point, and spot positions/directions remain in world space for lighting. The frame plan rejects an incompatible view, matrix, or depth target before execution.

**Rationale**: World-space normals remain stable under camera rotation, align with scene and light records, and provide a direct boundary for later ray-tracing/GI consumers. Reconstructing position avoids a separate world-position target. Screen-space consumers can explicitly transform the reconstructed world-space inputs into view space, while one declared convention prevents silent matrix/depth mismatches.

Standard-Z clears depth to `1.0` and uses `ERHICompareOp::LessEqual`; reversed-Z clears depth to `0.0` and uses `ERHICompareOp::GreaterEqual`. Projection, inverse view-projection, clear value, comparison operation, and probe interpretation must all carry the same convention identity.

**Alternatives considered**:

- View-space normals and reconstructed view-space position: viable and convenient for screen-space effects, but rejected because camera rotation changes the stored normal and all scene light records would require per-view conversion.
- Store world-space position directly: rejected due to bandwidth and memory cost.
- Let each shader choose a convention: rejected because surface and lighting stages would no longer have a testable compatibility contract.

## Decision: Use fullscreen directional lighting and bounded local-light volumes

**Decision**: Directional lights issue one fullscreen-triangle additive lighting draw each. Point lights use reusable sphere volume geometry and spot lights use reusable cone volume geometry. CPU-side view intersection removes lights that cannot affect the active view. Remaining lights are ordered by directional, point, then spot type and by ascending stable entity identity within each type; there is no additional influence-order key because every accepted light executes. Camera-outside volumes use back-face culling; camera-inside or near-plane-intersecting volumes use the complementary cull/depth variant so affected pixels are not lost. Spot inner/outer cone angles use radians with `0 <= inner <= outer < pi/2`.

**Rationale**: This directly satisfies the roadmap's light-volume deliverable while keeping geometry work independent of light count. Shared sphere/cone meshes avoid per-light geometry allocation, and explicit camera-inside handling covers the failure-prone local-volume boundary.

**Alternatives considered**:

- Fullscreen draw for every local light: rejected because it ignores the requested bounded light-volume strategy.
- Tiled or clustered light assignment: explicitly deferred by scope.
- Stencil pre-pass optimization for every volume: deferred until the baseline comparison demonstrates need.

## Decision: Extend RHI with reusable binding and readback commands

**Decision**: Add the minimum backend-neutral command capabilities needed by deferred execution:

- bind descriptor sets to graphics/compute pipelines;
- bind an index buffer with index type and offset;
- copy a texture region into a host-visible buffer;
- pass explicit clear values when beginning a render pass;
- expose stable command records for those operations.

Existing descriptor set updates, texture usages, multiple render-pass attachments, additive blend factors, indexed draws, barriers, and texture sampling remain the foundation. No Vulkan handles or image-layout enums enter Renderer contracts.

**Rationale**: The abstract model already represents most deferred resources, but native execution cannot currently bind sampled surface data, bind volume indices, or return pixels. These commands are generally useful for meshlets, post-processing, GI, and backend validation.

**Alternatives considered**:

- Add Vulkan calls inside `FDeferredFrameExecutor`: rejected by the constitution.
- Add a deferred-specific RHI interface: rejected because binding, indexed geometry, and readback are general graphics capabilities.
- Put all light data in push constants: rejected because the RHI has no push-constant contract and uniform/storage descriptors scale better for later batching.

## Decision: Fix one canonical deferred shader interface before shader work

**Decision**: Extend `ERHIFormat` with `R32G32_Float` and `R32G32B32_Float`, then use these canonical interfaces:

- Set 0 binding 0: frame/view uniform buffer, visible to vertex and fragment stages.
- Set 1 binding 0: per-draw/material uniform buffer, visible to vertex and fragment stages.
- Set 2 bindings 0..4: combined texture samplers for base-color/AO, normal/roughness, emissive/metallic, depth, and lighting accumulation, visible to fragment stages.
- Set 3 binding 0: ordered light storage buffer, visible to vertex and fragment stages.
- Mirrored records use column-major `float32` matrices and 16-byte `vec4` slots. Frame/view fields are `View`/`Projection`/`InverseViewProjection`/`ViewProjection` at offsets 0/64/128/192, followed by world-space camera position and extent/depth slots at 256/272/288, for total size 304 bytes. Per-draw/material uses `WorldNormalFromModel = transpose(inverse(mat3(Model)))` embedded as an affine `mat4` with zero translation and bottom-right `1`; non-finite or non-invertible model transforms are rejected before recording, and the shader normalizes the transformed normal. Its offsets are 0/64/128/144/160 with total size 176 bytes. Light positions/directions are world-space and use offsets 0/16/32/48 with stride 64 bytes. Integer-like flags/type/mode values use exactly representable non-negative `float32` codes and reserved components are zero.
- Surface vertices: position `R32G32B32_Float` at location 0 and normal `R32G32B32_Float` at location 1, stride 24 bytes.
- Fullscreen vertices: position `R32G32_Float` at location 0, stride 8 bytes.
- Sphere/cone volume vertices: position `R32G32B32_Float` at location 0, stride 12 bytes, with `UInt16` indices.

Surface pipelines bind sets 0/1; directional, point, and spot pipelines bind sets 0/2/3; composition binds sets 0/2. Mirrored C++/GLSL uniform and storage records use ordered 16-byte slots specified in the execution contract.

**Rationale**: Renderer, shader assets, deterministic mocks, and native Vulkan must agree before they can be implemented independently. Explicit formats and binding slots remove the current triangle path's ambiguous scalar-format shorthand and make interface validation testable.

**Alternatives considered**:

- Let each backend assign bindings: rejected because checked-in SPIR-V and Renderer descriptor layouts would drift.
- Infer vector widths from offsets: rejected because `ERHIFormat::R32_Float` does not encode two- or three-component vertex inputs.
- Make shaders depend on executor implementation order: rejected because it invalidates safe parallel work and contract tests.

## Decision: Execute graph passes through a dedicated deferred executor

**Decision**: Add `FDeferredFrameExecutor`. It consumes a valid `FDeferredFramePlan`, compiled render graph, and explicit RHI bindings grouped by surface, directional lighting, point-volume lighting, spot-volume lighting, composition, optional forward-transparent handoff, and readback. It validates all bindings before beginning command recording, emits graph transitions immediately before their pass, and stops at the first failed stage.

**Rationale**: Planning and command execution have different failure and ownership boundaries. A dedicated executor prevents the renderer planner from owning GPU resources and avoids expanding the triangle-specific forward executor into an unrelated multi-pass dispatcher.

**Alternatives considered**:

- Record RHI commands from `FDeferredRenderer::PrepareFrame`: rejected because deterministic plan tests would require runtime resources.
- Reuse `FForwardFrameExecutor` by adding mode branches: rejected because its one-output/one-draw assumptions are deliberately narrow.
- Bypass the render graph for native validation: rejected because render graph integration is a core deliverable.

## Decision: Generalize the native offscreen session incrementally

**Decision**: Extend the existing Vulkan native context with a reusable offscreen session that owns real images, buffers, descriptors, shaders, pipelines, render passes, framebuffers, command/fence state, and staging readback. It exposes only RHI interface wrappers and normalized runtime/readback summaries to the caller. Renderer tests assemble `FDeferredFrameExecutionBindings` from those wrappers and invoke `FDeferredFrameExecutor`; the backend does not depend on Renderer.

**Rationale**: Feature 018 proved real Vulkan through a fixed triangle context, but that path is too narrow for multiple passes. Incremental generalization preserves working instance/device/Lavapipe setup while proving that the Renderer drives actual work through RHI objects.

**Alternatives considered**:

- Implement a raw `ExecuteDeferred` Vulkan function inside the backend: rejected because it would not prove the Renderer/RHI execution path.
- Convert every deterministic `FVulkan*` model object to native ownership in one feature: rejected as an unnecessarily broad backend rewrite.
- Create a second Vulkan backend: rejected because runtime selection and semantics would diverge.

## Decision: Use repository-owned deferred shader assets

**Decision**: Add reviewable GLSL sources and checked-in SPIR-V payloads for surface, directional light, point light, spot light, and composition stages under `Source/Renderer/Shaders/Deferred/`. SCons regenerates or validates them when shader tools are available; native execution rejects missing, malformed, stage-incompatible, or interface-incompatible payloads.

**Rationale**: This follows Feature 018's reproducible offline shader policy and keeps hosted builds independent of runtime compilation.

**Alternatives considered**:

- Runtime GLSL compilation: rejected by existing project policy and scope.
- Put shaders under the demo: rejected because deferred rendering is a reusable Renderer feature.
- Use one monolithic shader permutation for all passes: rejected because stage-specific interfaces and diagnostics become opaque.

## Decision: Validate native pixels with semantic sample probes

**Decision**: The Linux Lavapipe job renders the same fixed small offscreen reference scene under standard-Z and reversed-Z conventions, copies required intermediate targets and final LDR output to host-visible staging buffers, and evaluates named sample probes. Validation uses the clarified thresholds: final LDR color error at most `2/255` per channel, normalized depth error at most `1e-4`, decoded normalized world-normal dot product at least `0.999`, metallic/roughness error at most `1e-3`, and 8-bit UNorm ambient-occlusion error at most `2e-3`. Any non-finite value fails. At least 12 probes per convention cover background/far clear, each material semantic, directional lighting, point lighting, spot lighting, emissive-only, ambient-only, and local-light boundaries.

**Rationale**: Semantic probes are deterministic, cheap, and explain which stage failed. They avoid cross-driver whole-image golden noise while still validating real image, descriptor, pass, lighting, and readback behavior.

**Alternatives considered**:

- Byte-identical images: rejected by Q5 due to floating-point and format quantization.
- SSIM golden images: rejected because it can hide localized semantic errors and adds an image-analysis dependency.
- Validate only final color: rejected because a compensation error between surface and lighting stages could pass unnoticed.

## Decision: Treat the performance comparison as a reproducible baseline

**Decision**: Add a comparison harness that prepares equivalent forward and deferred workloads at `0`, `16`, `64`, and `256` local-light tiers. Each tier uses one warm-up phase and at least 100 measured frames, records median and p95 preparation/execution durations plus draw/pass/light counts, verifies input fingerprints, and reports any observed crossover. Timing does not gate feature success; malformed or non-equivalent comparisons do.

**Rationale**: Q3 explicitly rejects a universal speedup gate. Fixed workload tiers and fingerprints make the roadmap comparison useful without turning variable CI performance into a flaky correctness test.

**Alternatives considered**:

- Require deferred to be faster on Lavapipe: rejected because a software rasterizer is not representative and hosted load is noisy.
- Compare only one high-light scene: rejected because it cannot reveal scaling or crossover behavior.
- Publish timing without workload identity: rejected because incomparable inputs would produce misleading evidence.

## Decision: Keep the existing three-platform CI matrix and add deferred artifacts

**Decision**: Windows, macOS, and Linux build and run deterministic deferred tests. Linux additionally selects Lavapipe explicitly, runs native offscreen deferred execution/readback, and uploads a normalized pixel-validation report plus the comparison baseline. Windows/macOS native execution and visible screenshots are not completion gates.

**Rationale**: This exactly follows Q2 and the constitution while reusing the proven Feature 018 CI environment.

**Alternatives considered**:

- Require native execution on all hosted platforms: rejected by clarification.
- Add manual screenshots: rejected because the feature has no visible-demo requirement.
- Replace deterministic tests with native tests: rejected because failure injection and byte-stable planning remain important.
