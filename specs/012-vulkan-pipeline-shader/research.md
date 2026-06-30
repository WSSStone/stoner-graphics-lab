# Research: Vulkan Pipeline & Shader

**Feature**: 012-vulkan-pipeline-shader  
**Date**: 2026-06-30

## Decision: Use explicit shader interface metadata before reflection

**Rationale**: The current roadmap excludes runtime shader compilation and full material/shader systems from this feature. Explicit metadata supplied with shader module creation lets the backend validate descriptor layout compatibility deterministically without pretending that automatic reflection exists.

**Alternatives considered**:

- Defer descriptor/layout compatibility until the material system: rejected because pipeline creation tests would be too weak and command binding validation could accept incompatible layouts.
- Require automatic reflection now: rejected because it would add a shader tooling feature outside this roadmap slice.

## Decision: Prefer real runtime objects with deterministic fallback objects

**Rationale**: Earlier Vulkan phases already support explicit unsupported-runtime and deterministic fallback diagnostics. Continuing that pattern allows local and CI validation without a Vulkan runtime while still using real runtime creation when available.

**Alternatives considered**:

- Require real runtime for all shader and pipeline creation: rejected because it would make core validation unavailable on machines without Vulkan runtime support.
- Use fallback only in this phase: rejected because it would fail to advance the real backend path where runtime support exists.

## Decision: Keep graphics pipeline scope triangle-ready

**Rationale**: The next milestone needs graphics pipelines capable of validating a simple triangle path, not a fully general renderer. Triangle-ready state with vertex input, primitive topology, rasterization, depth/stencil, blend, multisample, and dynamic viewport/scissor requirements is enough to remove missing-pipeline diagnostics while retaining useful negative tests.

**Alternatives considered**:

- Color-only triangle pipeline: rejected because it under-tests common fixed-function state and would leave immediate renderer integration gaps.
- Full general graphics pipeline state coverage: rejected because subpass and advanced render target combinations belong to later renderer phases.

## Decision: Lightweight structural shader bytecode checks in fallback

**Rationale**: Fallback mode needs deterministic validation but cannot prove full bytecode semantics without a real runtime or reflection tooling. Structural checks catch empty/malformed payloads and declared-stage mismatches; real runtime creation provides stronger validation where available.

**Alternatives considered**:

- Full semantic bytecode validation offline: rejected because it would require a shader parser/validator outside this phase.
- Treat bytecode as opaque fallback data: rejected because it would make malformed payload tests too permissive.

## Decision: Process-local pipeline reuse only

**Rationale**: Process-local reuse supports deterministic repeated creation diagnostics and avoids premature choices around persistent cache file format, versioning, and invalidation.

**Alternatives considered**:

- Persistent disk pipeline cache: rejected because storage, invalidation, version compatibility, and platform path policy are not needed for this phase.
- No reuse behavior: rejected because the roadmap calls for pipeline cache support and later renderer/material systems will benefit from a stable process-local reuse contract.

## Decision: Extend existing RHI contracts rather than adding Vulkan-only public entry points

**Rationale**: The constitution requires RHI abstraction and multi-API readiness. Shader interface metadata, validation mode, runtime mode, and dynamic state requirements should live in RHI-level descriptions or queryable summaries so future backends can implement equivalent behavior.

**Alternatives considered**:

- Vulkan-only extension structs in public call sites: rejected because Renderer-facing code would learn backend-specific concepts.
- Hide all new behavior in Vulkan classes: rejected because command buffer binding and tests need backend-neutral observable contracts.
