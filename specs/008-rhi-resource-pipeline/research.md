# Research: RHI Resource & Pipeline Interfaces

**Feature**: 008-rhi-resource-pipeline  
**Date**: 2026-06-26

## Decision: Use focused public RHI contracts with mock-only behavior in tests

**Rationale**: The feature is the second RHI contract slice and must unblock backend and renderer planning without adding fake production backends. Public headers define portable contracts; deterministic mocks live in `Tests/RHICoreTests.cpp` and validate lifecycle, descriptors, pipelines, and render target compatibility.

**Alternatives considered**:
- Add a mock backend under `Source/RHI/Private`: rejected because it risks becoming accidental production architecture.
- Wait for Vulkan backend before defining resources: rejected because Renderer and Backend need a shared contract first.

## Decision: Device remains the authoritative factory for all new objects

**Rationale**: The previous RHI core feature established device-owned creation. Extending that rule to resources, layouts, descriptor sets, shader modules, pipelines, render passes, and framebuffers keeps capability checks and shutdown invalidation centralized.

**Alternatives considered**:
- Free functions for resource creation: rejected because creation would not be tied to device capabilities.
- Direct constructors as public creation path: rejected because invalid descriptions and unsupported combinations need explicit creation status.

## Decision: Use composable usage flags with explicit invalid combinations

**Rationale**: Real renderer and render graph workflows need resources that can participate in multiple roles, such as copy destination plus sampled texture, or storage buffer plus copy source. Explicit invalid-combination tests keep validation meaningful without over-constraining future backends.

**Alternatives considered**:
- Single primary usage per resource: rejected because it is too narrow for render graph and upload workflows.
- Accept all usage combinations: rejected because it removes useful negative-path validation from the RHI contract.

## Decision: Pipeline layouts support multiple descriptor sets

**Rationale**: Set-indexed layouts fit modern explicit APIs and common material architectures. They allow separate global, material, object, and pass resource sets while still allowing MVP tests to use a single set.

**Alternatives considered**:
- Single descriptor set per layout: rejected because future material/render graph integration would likely require reworking the contract.
- Descriptor declarations without descriptor set objects: rejected because descriptor update validation is part of this feature's value.

## Decision: Shader modules carry opaque payload identity, entry point, and stage only

**Rationale**: This feature should not validate shader bytecode or integrate a compiler. Opaque payload identity is enough for pipeline contracts, mock validation, and future backend mapping.

**Alternatives considered**:
- Debug name only: rejected because pipeline tests need a stable payload identity beyond human-readable naming.
- Store and validate byte payloads: rejected because shader compilation/validation belongs to later backend or material/shader phases.

## Decision: Resource and pipeline-family objects expose explicit lifecycle states

**Rationale**: The spec requires invalidated-object behavior. Explicit `Valid` and `Invalidated` states make shutdown, release, descriptor update, framebuffer attachment, and pipeline use negative paths deterministic in tests.

**Alternatives considered**:
- Rely only on shared pointer lifetime: rejected because it cannot express device shutdown or invalidated resources while references still exist.
- Only device shutdown invalidates all objects: rejected because individual invalidation is needed for lifecycle matrix coverage.

## Decision: Render pass contracts are single-subpass in this phase

**Rationale**: Single-subpass render passes with attachment roles and load/store behavior are enough for early forward rendering and Vulkan planning. Full multi-subpass dependency modeling is better deferred until render graph and backend execution semantics are clearer.

**Alternatives considered**:
- Attachment list only: rejected because pipelines and framebuffers need a render target compatibility concept.
- Full multi-subpass graph now: rejected as premature complexity before render graph execution exists.

## Decision: Public RHI resource headers remain backend and renderer independent

**Rationale**: Constitution requires strict layering. The contracts must be reviewable and testable without Vulkan, DX12, Metal, OpenGL, Renderer, Application, native windowing, or platform surface dependencies.

**Alternatives considered**:
- Mirror Vulkan descriptors and render passes directly: rejected because this would leak one backend's model into the RHI.
- Couple framebuffer/swapchain image handling to Application windowing: rejected because swapchain/window integration is handled by separate phases.
