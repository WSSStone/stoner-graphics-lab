# Feature Specification: Vulkan Pipeline & Shader

**Feature Branch**: `012-vulkan-pipeline-shader`  
**Created**: 2026-06-30  
**Status**: Delivered  
**Input**: User description: "根据roadmap制定下一个大feature的spec"

## Clarifications

### Session 2026-06-30

- Q: How should shader resource interface compatibility be represented before full shader reflection/material system exists? → A: Shader modules carry explicit interface metadata supplied at creation time; pipeline creation validates it against pipeline layout.
- Q: How should shader and pipeline creation behave when a real backend runtime is unavailable? → A: Real runtime creates real shader and pipeline objects when available; unavailable runtime uses deterministic fallback objects with explicit diagnostics.
- Q: What graphics pipeline state scope is required for this phase? → A: Minimal triangle-ready graphics pipeline plus validation for vertex input, primitive topology, rasterization, depth/stencil, blend, multisample, and dynamic viewport/scissor.
- Q: How strict should shader bytecode validation be when real runtime validation is unavailable? → A: Validate shader bytecode with lightweight structural checks plus real-runtime creation when available; fallback accepts structurally valid bytecode with diagnostics.
- Q: What pipeline cache persistence scope is required for this phase? → A: Process-local cache/reuse only; no persistent disk cache in this phase.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Load Shader Modules for Backend Pipelines (Priority: P1)

An engine developer can provide precompiled shader bytecode and explicit shader interface metadata to the backend and receive a validated shader module that can be inspected, retained, rejected, or invalidated predictably.

**Why this priority**: Pipelines cannot be created until shader inputs are represented safely. This is the first required step toward making previously recorded draw and dispatch commands meaningful.

**Independent Test**: Can be tested by creating an active backend device, loading structurally valid shader bytecode for supported stages with matching interface metadata, checking queryable stage and lifecycle information, and verifying empty, structurally malformed, unsupported, wrong-stage, metadata-incompatible, or post-shutdown inputs fail explicitly.

**Acceptance Scenarios**:

1. **Given** an active backend device, structurally valid precompiled shader bytecode for a supported stage, and valid explicit interface metadata, **When** a developer creates a shader module, **Then** the system returns a valid real-runtime or deterministic fallback module whose stage, interface summary, lifecycle state, validation mode, and diagnostics can be queried.
2. **Given** empty, structurally malformed, incompatible, or unsupported shader bytecode, **When** shader module creation is requested, **Then** the system returns an explicit failure without exposing a usable partial shader module.
3. **Given** a shader module exists when the backend device shuts down, **When** shutdown completes, **Then** the module becomes invalid and later use is rejected predictably.

---

### User Story 2 - Define Pipeline Layouts for Resource Binding (Priority: P1)

An engine developer can create pipeline layouts from declared descriptor set layouts and push-constant-style ranges so future draw and dispatch work has a validated resource binding contract.

**Why this priority**: Pipeline layouts connect the resource/descriptor phase to shader execution. Without layout validation, pipelines cannot safely reference resources or later bind descriptor sets.

**Independent Test**: Can be tested by creating descriptor layouts from the existing resource pipeline contracts, combining them into valid pipeline layouts, and checking duplicate sets, incompatible stage visibility, invalid ranges, missing layouts, and invalidated dependencies.

**Acceptance Scenarios**:

1. **Given** valid descriptor layout declarations, **When** a developer creates a pipeline layout, **Then** the system returns a valid layout preserving the expected set order, stage visibility, and lifecycle state.
2. **Given** duplicate set declarations, invalid ranges, missing dependencies, or invalidated descriptor layouts, **When** pipeline layout creation is requested, **Then** the system rejects the request with a clear failure reason.
3. **Given** a pipeline layout is referenced by an existing pipeline, **When** the backend device shuts down, **Then** both layout and dependent pipeline objects become invalid in a deterministic order.

---

### User Story 3 - Create Graphics Pipelines for Render Pass Drawing (Priority: P1)

An engine developer can create a triangle-ready graphics pipeline from compatible shader modules, pipeline layout, render target compatibility, and common fixed-function drawing state, then use that pipeline to validate graphics command recording.

**Why this priority**: Graphics pipelines are the last backend piece required before a triangle draw can become a real rendering path instead of a placeholder command.

**Independent Test**: Can be tested by creating a minimal compatible render target scope, valid shader modules, a compatible pipeline layout, vertex input, primitive topology, rasterization, depth/stencil, blend, multisample, and dynamic viewport/scissor state, then creating a graphics pipeline and verifying invalid shaders, incompatible layouts, invalid state combinations, incompatible render target descriptions, and shutdown invalidation.

**Acceptance Scenarios**:

1. **Given** valid shader modules, layout, render target compatibility, and triangle-ready graphics state, **When** a developer creates a graphics pipeline, **Then** the system returns a valid real-runtime or deterministic fallback pipeline with queryable compatibility, lifecycle, and runtime-mode information.
2. **Given** a graphics command buffer inside a compatible render pass scope, **When** a valid graphics pipeline is bound before draw commands, **Then** the recorded draw work is no longer reported as missing pipeline binding.
3. **Given** incompatible shader stages, missing layout, invalid vertex input, invalid raster/depth/blend state, incompatible render target descriptions, or invalidated dependencies, **When** graphics pipeline creation or binding is attempted, **Then** the system returns an explicit failure without changing unrelated command state.

---

### User Story 4 - Create Compute Pipelines for Dispatch Work (Priority: P2)

An engine developer can create a compute pipeline from a compatible compute shader module and pipeline layout, then use that pipeline to validate compute command recording.

**Why this priority**: Compute support is needed by later render graph, material, post-processing, meshlet, and global illumination work. It can be delivered after graphics pipeline support because the first visible rendering milestone depends on graphics first.

**Independent Test**: Can be tested by creating valid and invalid compute shader modules and layouts, creating compute pipelines, binding them to compute-capable command buffers, and verifying dispatch commands report bound-pipeline status correctly.

**Acceptance Scenarios**:

1. **Given** a valid compute shader module and compatible pipeline layout, **When** a developer creates a compute pipeline, **Then** the system returns a valid real-runtime or deterministic fallback pipeline with queryable compatibility, lifecycle, and runtime-mode information.
2. **Given** a compute-capable command buffer, **When** a valid compute pipeline is bound before dispatch commands, **Then** the recorded dispatch work is no longer reported as missing pipeline binding.
3. **Given** a non-compute shader, missing layout, incompatible descriptor requirements, unsupported queue capability, or invalidated dependency, **When** compute pipeline creation or binding is attempted, **Then** the system rejects the request explicitly.

---

### User Story 5 - Reuse Pipeline Creation Results Deterministically (Priority: P3)

An engine developer can observe whether compatible pipeline creation requests reuse previous preparation work within the current process and whether failed or invalidated requests avoid polluting reusable state.

**Why this priority**: Pipeline creation can be expensive, and later renderer features will repeatedly request equivalent pipelines. This feature should establish deterministic cache and diagnostic behavior before higher-level material systems depend on it.

**Independent Test**: Can be tested by issuing repeated equivalent and non-equivalent pipeline creation requests in the same process, comparing cache-hit diagnostics, invalidating dependencies, and confirming failed requests do not become reusable successful entries. No persistent disk cache behavior is required.

**Acceptance Scenarios**:

1. **Given** a graphics or compute pipeline description equivalent to one already created successfully in the current process, **When** the same pipeline is requested again, **Then** the system reports deterministic process-local reuse or cache-hit diagnostics without changing externally visible behavior.
2. **Given** a failed, unsupported, or invalidated pipeline creation request, **When** an equivalent request is made later after correcting the issue, **Then** the system evaluates it normally rather than reusing stale failure state.
3. **Given** the backend device shuts down, **When** pipeline cache or reusable state is queried afterward, **Then** invalidation is explicit and no stale pipeline object is reported as usable.

### Edge Cases

- What happens when shader bytecode is empty, structurally malformed, uses an unsupported stage, or is incompatible with the declared stage?
- What happens when pipeline layout declarations contain duplicate descriptor sets, invalid binding visibility, missing dependencies, or out-of-range small constant data regions?
- What happens when graphics pipeline creation lacks required shader stages or provides stage combinations that cannot form a drawable program?
- What happens when vertex input, primitive topology, rasterization, multisample, depth/stencil, or blend state conflicts with the selected render target compatibility?
- What happens when dynamic viewport or scissor state is missing, invalid, or incompatible with the command buffer's recorded state?
- What happens when render pass or framebuffer compatibility changes after pipeline creation?
- What happens when a command buffer tries to bind a graphics pipeline outside graphics-capable recording or outside a compatible render pass scope?
- What happens when a command buffer tries to bind a compute pipeline on an incompatible queue or while not recording?
- What happens when draw or dispatch is recorded with no pipeline, the wrong pipeline kind, or an invalidated pipeline?
- What happens when pipeline creation reaches configured capacity, cache storage, or deterministic failure limits?
- What happens when the required runtime is unavailable but shader and pipeline contracts still need deterministic validation?
- What happens when repeated shader, layout, pipeline, bind, draw, dispatch, reset, and shutdown cycles happen in the same process?

## Architecture & Design Constraints *(mandatory)*

- **RHI Abstraction**: Renderer and Application-facing behavior MUST remain expressed through existing shader, pipeline, layout, descriptor, render pass, framebuffer, command buffer, queue, and synchronization contracts. Backend-specific pipeline creation details MUST NOT leak into Renderer-facing contracts.
- **Design Patterns**: Shader module lifetime, pipeline layout validation, graphics pipeline creation, compute pipeline creation, pipeline reuse, and command binding validation responsibilities MUST remain separate and testable. The feature MUST avoid a single catch-all pipeline manager with hidden ownership rules.
- **Advanced Graphics**: Pipeline and layout behavior MUST leave room for future render graph scheduling, material permutations, meshlet workloads, ray tracing pipelines, and global illumination compute workloads without implementing those higher-level systems in this phase.
- **Naming Conventions**: Public project-facing concepts introduced by this feature MUST follow the project's UE5-style naming conventions.
- **Cross-Platform Compatibility**: The backend MUST support supported desktop environments where the required graphics runtime is available and MUST report explicit unsupported status where runtime, shader, layout, pipeline, or command-binding capabilities are absent.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST allow shader module creation for active backend devices when precompiled shader bytecode is structurally valid, supported, matches the declared shader stage, and includes explicit shader interface metadata, using real runtime objects when available and deterministic fallback objects with explicit diagnostics when unavailable.
- **FR-002**: System MUST reject empty, structurally malformed, unsupported, wrong-stage, metadata-incompatible, or post-shutdown shader module requests with explicit results and without returning usable partial modules.
- **FR-003**: System MUST expose shader module lifecycle state, interface summary, validation mode, and diagnostics consistently so later pipeline creation can distinguish valid, invalidated, unsupported, and failed shader inputs.
- **FR-003a**: System MUST use lightweight structural bytecode checks when real runtime validation is unavailable and MUST rely on real runtime creation validation when the backend runtime is available.
- **FR-004**: System MUST allow pipeline layout creation from valid descriptor layout declarations and small constant-data ranges while preserving queryable set order, stage visibility, and lifecycle state.
- **FR-005**: System MUST reject pipeline layout creation when descriptor declarations are duplicated, missing, incompatible, invalidated, or contain invalid range definitions.
- **FR-006**: System MUST allow triangle-ready graphics pipeline creation when shader modules, explicit shader interface metadata, pipeline layout, render target compatibility, vertex input, primitive topology, rasterization, depth/stencil, blend, multisample, and dynamic viewport/scissor requirements are valid and mutually compatible, using real runtime objects when available and deterministic fallback objects with explicit diagnostics when unavailable.
- **FR-007**: System MUST reject graphics pipeline creation when required shader stages are missing, shader stages are incompatible, shader interface metadata does not match the layout, vertex input is invalid, primitive topology is invalid, rasterization state is invalid, depth/stencil state is incompatible, blend state is incompatible, multisample state is invalid, dynamic viewport/scissor requirements are invalid, render target compatibility is missing or incompatible, or dependencies are invalidated.
- **FR-008**: System MUST allow compute pipeline creation when a valid compute shader module with compatible explicit interface metadata and a compatible pipeline layout are provided, using real runtime objects when available and deterministic fallback objects with explicit diagnostics when unavailable.
- **FR-009**: System MUST reject compute pipeline creation when the shader stage is not compute, shader interface metadata does not match the layout, queue capability is unsupported, or dependencies are invalidated.
- **FR-010**: System MUST expose graphics and compute pipeline lifecycle, compatibility summaries, runtime mode, and creation diagnostics in a deterministic way for tests and later renderer code.
- **FR-011**: System MUST allow valid graphics pipelines to be bound to compatible recording command buffers in compatible graphics render pass scope.
- **FR-012**: System MUST allow valid compute pipelines to be bound to compatible recording command buffers with compute capability.
- **FR-013**: System MUST reject pipeline binding when the command buffer is not recording, the queue capability is incompatible, the pipeline kind is wrong for the command category, the render pass scope is missing or incompatible, or the pipeline has been invalidated.
- **FR-014**: System MUST update draw and indexed-draw validation so compatible bound graphics pipelines remove missing-pipeline diagnostics while incompatible, missing, or invalidated pipeline state remains explicit.
- **FR-015**: System MUST update dispatch validation so compatible bound compute pipelines remove missing-pipeline diagnostics while incompatible, missing, or invalidated pipeline state remains explicit.
- **FR-016**: System MUST provide deterministic process-local reuse or cache diagnostics for equivalent successful graphics and compute pipeline creation requests.
- **FR-016a**: System MUST keep persistent disk pipeline cache load, save, versioning, and invalidation behavior outside this feature's delivered scope.
- **FR-017**: System MUST ensure failed, unsupported, and invalidated pipeline creation requests do not become reusable successful pipeline entries.
- **FR-018**: System MUST invalidate or release owned shader modules, pipeline layouts, graphics pipelines, compute pipelines, and reusable pipeline state when the backend device shuts down.
- **FR-019**: System MUST reject shader, layout, pipeline, binding, draw validation, dispatch validation, and cache queries after backend device shutdown with invalid-state status.
- **FR-020**: System MUST preserve existing Core, RHI, Vulkan device/swapchain, Vulkan resource management, and Vulkan command recording/submission test outcomes while adding pipeline and shader validation.
- **FR-021**: System MUST provide deterministic test coverage for shader module success and rejection paths, pipeline layout validation, graphics pipeline creation and binding, compute pipeline creation and binding, cache/reuse behavior, draw/dispatch missing-pipeline diagnostics, dependency invalidation, configured failure limits, and shutdown invalidation.
- **FR-022**: System MUST keep runtime shader compilation from source languages, material systems, render graph scheduling, mesh shader pipelines, ray tracing pipelines, visible swapchain presentation, and full triangle demo application flow outside this feature's delivered scope.

### Key Entities *(include if feature involves data)*

- **Shader Module**: Represents structurally validated precompiled shader bytecode for one shader stage, with explicit interface metadata, validation mode, lifecycle state, and diagnostics.
- **Shader Interface Metadata**: Represents declared resource binding and small constant-data requirements supplied with a shader module before full shader reflection exists.
- **Pipeline Layout**: Represents the resource binding contract used by graphics and compute pipelines, including descriptor set order, visibility, and small constant-data ranges.
- **Graphics Pipeline**: Represents validated triangle-ready drawable pipeline state compatible with shader modules, layout, render target scope, vertex input, primitive topology, rasterization, depth/stencil, blend, multisample, and dynamic viewport/scissor requirements.
- **Compute Pipeline**: Represents validated dispatch pipeline state compatible with a compute shader module and pipeline layout.
- **Pipeline Compatibility Summary**: Represents the render target, shader stage, layout, queue, and command-binding constraints needed to validate use of a pipeline.
- **Pipeline Reuse Record**: Represents whether a pipeline creation request was newly created, reused within the current process, rejected, invalidated, or unavailable.
- **Pipeline Binding State**: Represents the currently bound graphics or compute pipeline on a command buffer and the validation status used by draw or dispatch recording.
- **Pipeline Diagnostics**: Represents creation, reuse, dependency, compatibility, binding, missing-pipeline, and invalidation reasons in a deterministic format.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In a supported development environment, a developer can create valid real-runtime or deterministic fallback shader modules, a pipeline layout, one graphics pipeline, one compute pipeline, bind each compatible pipeline to a command buffer, and record validated draw or dispatch work in under 60 seconds using the project verification flow.
- **SC-002**: Shader module validation rejects 100% of covered empty, structurally malformed, unsupported, wrong-stage, metadata-incompatible, and post-shutdown inputs without crashes or usable partial modules.
- **SC-003**: Pipeline layout validation covers at least one success path and one negative path each for valid descriptor declarations, duplicate declarations, missing dependencies, invalid ranges, and invalidated dependency use.
- **SC-004**: Graphics pipeline validation covers at least one success path and one negative path each for required shader stages, incompatible shader stages, invalid layout requirements, invalid vertex input, invalid primitive topology, invalid rasterization state, incompatible depth/stencil state, incompatible blend state, invalid multisample state, invalid dynamic viewport/scissor requirements, incompatible render target scope, invalidated dependencies, and configured creation failure.
- **SC-005**: Compute pipeline validation covers at least one success path and one negative path each for valid compute shader use, wrong-stage shader use, invalid layout requirements, unsupported queue capability, invalidated dependencies, and configured creation failure.
- **SC-006**: Command recording validation reports missing, compatible, incompatible, wrong-kind, and invalidated pipeline binding states for draw, indexed draw, and dispatch without changing unrelated recorded command state.
- **SC-007**: Equivalent successful graphics and compute pipeline requests within the same process produce deterministic reuse diagnostics, while failed and invalidated requests are never reported as reusable successful entries.
- **SC-008**: Runtime-unavailable validation covers at least one shader module, graphics pipeline, compute pipeline, bind, draw, and dispatch path where deterministic fallback diagnostics clearly state that no real runtime execution occurred.
- **SC-009**: Existing Core, RHI, Vulkan device/swapchain, Vulkan resource management, and Vulkan command recording/submission tests continue to pass after pipeline and shader support is added.
- **SC-010**: Renderer-facing code can interact with backend shader modules, pipeline layouts, graphics pipelines, compute pipelines, and command binding validation through established contracts without depending on backend-specific creation details.

## Assumptions

- Vulkan command recording and submission are complete enough to allocate command buffers, begin/end recording, validate render pass scope, submit work, and report missing-pipeline diagnostics.
- Vulkan resource management is complete enough to create descriptor layouts, descriptor sets, buffers, textures, samplers, render pass-compatible resources, and lifecycle states needed by pipeline validation.
- This feature targets the roadmap's Phase 011, while the Speckit feature directory uses `012` because existing spec directories already occupy numbers through `011`.
- Shader inputs are precompiled bytecode in this feature; runtime compilation from source shader languages and automatic reflection are intentionally deferred.
- Shader resource interface compatibility is represented by explicit metadata supplied at shader module creation time and validated against pipeline layouts during pipeline creation.
- Fallback shader validation checks only lightweight structural validity and declared metadata compatibility; complete bytecode semantic validation is delegated to real runtime creation when available.
- Graphics and compute pipelines should be validated and bindable in command recording before a full renderer, material system, render graph, or visible triangle demo exists.
- Graphics pipeline scope is triangle-ready rather than fully general: it must cover common fixed-function validation needed for the first visible rendering milestone while deferring advanced render target and subpass combinations.
- Render pass and framebuffer support may remain minimal as long as graphics pipeline compatibility and command binding validation are deterministic.
- Real runtime objects are preferred whenever the backend runtime is available; deterministic fallback objects exist only to preserve contract validation and must disclose that no real runtime execution occurred.
- Pipeline reuse behavior is limited to process-local diagnostics and object reuse; persistent disk cache storage is deferred.
- Multi-threaded pipeline creation, asynchronous compilation, pipeline libraries, mesh shader pipelines, ray tracing pipelines, and material permutations are outside the first delivered scope.
