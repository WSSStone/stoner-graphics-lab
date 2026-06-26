# Feature Specification: RHI Resource & Pipeline Interfaces

**Feature Branch**: `008-rhi-resource-pipeline`  
**Created**: 2026-06-26  
**Status**: Implemented  
**Input**: User description: "根据roadmap制定下一个大feature的spec"

## Clarifications

### Session 2026-06-26

- Q: Should pipeline layouts support a single descriptor set or multiple descriptor sets? → A: Multiple descriptor sets per pipeline layout; bindings use set index + binding slot.
- Q: Should RHI resource and pipeline objects expose explicit invalidation state? → A: Objects expose explicit lifecycle states including Valid and Invalidated.
- Q: What shader module payload detail belongs in this phase? → A: Store opaque payload identity plus entry point and stage; do not validate bytecode.
- Q: How should resource usage flags be validated? → A: Usage flags are composable, with explicit invalid incompatible combinations.
- Q: How much render pass subpass behavior belongs in this phase? → A: Support single-subpass render passes with attachment roles and load/store behavior.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Describe and Create RHI Resources (Priority: P1)

An engine developer can describe buffers, textures, and samplers through stable RHI contracts so renderers and backends agree on what resources exist, how they are intended to be used, and whether a creation request is valid.

**Why this priority**: Resource contracts are the foundation for every later rendering path. Vulkan resource management, render graph work, material systems, and frame rendering all need shared buffer and texture descriptions before any concrete backend implementation can be trusted.

**Independent Test**: Can be tested with a mock device that accepts valid resource descriptions, rejects invalid or unsupported descriptions, and returns resource objects whose descriptions, usage flags, dimensions, format, and lifecycle state can be queried without using a real graphics backend.

**Acceptance Scenarios**:

1. **Given** a valid buffer description for vertex, index, uniform, or storage use, **When** a developer requests a buffer, **Then** the system returns a resource object that preserves the requested size, usage intent, and initial lifecycle state.
2. **Given** a valid texture description for 1D, 2D, 3D, cube, or array usage, **When** a developer requests a texture, **Then** the system returns a resource object that preserves dimensions, format, mip levels, array layers, sample count, and usage intent.
3. **Given** a sampler description with valid filtering and address behavior, **When** a developer requests a sampler, **Then** the system returns a sampler object whose behavior can be queried consistently.
4. **Given** an invalid resource description such as zero size, zero dimensions, incompatible usage, unsupported format, or invalid mip/layer counts, **When** creation is requested, **Then** the system reports an explicit failure status without creating a usable object.

---

### User Story 2 - Bind Resources Through Pipeline Layouts and Descriptor Sets (Priority: P1)

An engine developer can define how shaders see resources by declaring pipeline layouts, descriptor bindings, and descriptor sets that bind buffers, textures, samplers, or combined texture-sampler resources without exposing backend-specific handles.

**Why this priority**: Resource binding is the bridge between resource objects and pipeline execution. Without a binding contract, later renderer and backend features cannot express material parameters, per-frame constants, storage buffers, or sampled textures.

**Independent Test**: Can be tested with mock pipeline layouts and descriptor sets that verify binding declarations, update valid bindings, reject incompatible resources, and preserve all declared binding metadata.

**Acceptance Scenarios**:

1. **Given** a pipeline layout with one or more descriptor set layouts and descriptor bindings, **When** a developer creates the layout, **Then** the layout exposes the expected set count, binding count, shader visibility, descriptor type, and array count.
2. **Given** a descriptor set created for a specific set index within a layout, **When** valid resources are written to matching bindings, **Then** the descriptor set records those bindings and reports a valid ready state.
3. **Given** a descriptor update that targets a missing binding, wrong resource type, invalid array index, or Invalidated resource, **When** the update is attempted, **Then** the system reports an explicit invalid-state or unsupported result.
4. **Given** multiple shader stages share a binding, **When** the binding is queried, **Then** the descriptor contract preserves all declared visibility flags.

---

### User Story 3 - Define Graphics and Compute Pipelines (Priority: P1)

An engine developer can describe graphics and compute pipeline state through RHI-level contracts, including shader modules, shader stages, pipeline layouts, vertex input, rasterization, blend, and depth-stencil behavior.

**Why this priority**: Pipeline contracts complete the core RHI abstraction and unblock later Vulkan pipeline creation, render graph pass execution, material compilation, and the first forward rendering pipeline.

**Independent Test**: Can be tested with mock shader modules and pipeline objects that validate required stage combinations, required layouts, compatible render pass information, and queryable pipeline descriptions.

**Acceptance Scenarios**:

1. **Given** valid shader module descriptions and a pipeline layout, **When** a developer creates a compute pipeline, **Then** the system returns a compute pipeline object with exactly one compute stage and the requested layout.
2. **Given** valid vertex and fragment shader stages plus compatible graphics state, **When** a developer creates a graphics pipeline, **Then** the system returns a graphics pipeline object that preserves shader stages, layout, vertex input, rasterization, blend, and depth-stencil settings.
3. **Given** an invalid pipeline request such as missing required shader stages, duplicate incompatible stages, missing layout, unsupported format, or incompatible attachment state, **When** creation is requested, **Then** the system reports explicit failure without creating a usable pipeline.
4. **Given** a pipeline object created by a device, **When** consumers query its description, **Then** the result is stable and independent of any concrete graphics API.

---

### User Story 4 - Model Render Passes and Framebuffers (Priority: P2)

An engine developer can describe single-subpass render pass attachments, attachment roles, load/store behavior, and framebuffer attachments so later render graph and backend phases share a common frame target contract.

**Why this priority**: Render passes and framebuffers are needed by the first real draw path, but they depend on resource and pipeline descriptions being stable first.

**Independent Test**: Can be tested with mock render pass and framebuffer objects that validate attachment counts, compatible texture formats, dimensions, and lifecycle behavior without creating a real swapchain image.

**Acceptance Scenarios**:

1. **Given** a single-subpass render pass description with color and optional depth-stencil attachments, **When** a developer creates the render pass, **Then** the system preserves attachment formats, attachment roles, and load/store behavior.
2. **Given** compatible texture attachments and a render pass, **When** a developer creates a framebuffer, **Then** the system returns a framebuffer object with matching dimensions and attachment count.
3. **Given** mismatched attachment formats, dimensions, sample counts, or missing required attachments, **When** framebuffer creation is attempted, **Then** the system reports explicit failure without crashing.

---

### User Story 5 - Validate Resource and Pipeline Lifecycles With Mocks (Priority: P2)

An engine developer can run deterministic mock-based tests that exercise every new RHI resource and pipeline contract, including successful flows, invalid descriptions, incompatible bindings, and invalidated-object behavior.

**Why this priority**: This feature is contract-heavy. Mock validation provides confidence before real Vulkan resources and pipelines exist and prevents Renderer code from relying on undefined behavior.

**Independent Test**: Can be tested by running the project test executable and observing resource, binding, pipeline, render pass, and framebuffer lifecycle matrices pass without a graphics device.

**Acceptance Scenarios**:

1. **Given** all new public RHI resource and pipeline contracts, **When** the mock test suite runs, **Then** each contract has at least one success-path and one negative-path validation.
2. **Given** an object that has entered the explicit Invalidated lifecycle state, **When** a consumer attempts to use it in a binding, pipeline, render pass, or framebuffer operation, **Then** the system reports invalid state.
3. **Given** public RHI headers are reviewed in isolation, **When** they are included by a consumer, **Then** they do not require Backend, Renderer, Application, platform-window, or concrete graphics API details.

### Edge Cases

- What happens when a buffer has zero bytes, an unsupported usage combination, or an alignment requirement that cannot be satisfied?
- What happens when a texture has zero width/height/depth, invalid mip count, invalid array layer count, unsupported format, or cube dimensions that are not square?
- What happens when a resource usage request conflicts with the way the resource is later bound?
- What happens when a descriptor binding is updated with the wrong resource category, a missing resource, or an array element outside the declared range?
- What happens when a shader module has no declared stage, an unsupported stage, or is used in the wrong pipeline type?
- What happens when a graphics pipeline is missing a required stage or references attachments that do not match the declared render pass behavior?
- What happens when render pass attachments and framebuffer textures disagree on format, dimensions, sample count, or attachment count?
- What happens when resource, descriptor, pipeline, render pass, or framebuffer objects are queried or used after entering the Invalidated lifecycle state?
- What happens when advanced shader stages such as ray tracing or mesh shader stages are requested before those roadmap phases exist?
- What happens when public RHI resource headers are included without any backend, renderer, application, windowing, or graphics API headers?

## Architecture & Design Constraints *(mandatory)*

- **RHI Abstraction**: This feature defines RHI resource and pipeline contracts. It MUST NOT require Renderer, Application, Backend, Vulkan, Metal, DX12, OpenGL, platform-window, or concrete graphics API details in public RHI-facing behavior.
- **Design Patterns**: Resource objects, binding layouts, descriptor sets, shader modules, pipeline states, render passes, and framebuffers MUST remain separate responsibilities. The feature MUST avoid a single catch-all rendering object that owns unrelated concepts.
- **Advanced Graphics**: The contracts MUST leave room for future ray tracing, meshlet, compute-heavy, and global illumination workflows without implementing those advanced pipelines in this phase.
- **Naming Conventions**: Public concepts MUST follow the project's UE5-style naming conventions, including interface, enum, flag, and value-object naming consistent with existing RHI core contracts.
- **Cross-Platform Compatibility**: The behavior MUST be satisfiable consistently on Windows, macOS, and Linux through backend implementations, with no platform-specific public resource or pipeline dependency.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST define buffer resource contracts that expose size, usage intent, lifecycle state, and creation outcome for vertex, index, uniform, and storage buffer use cases.
- **FR-002**: System MUST define texture resource contracts that expose texture dimension type, width, height, depth, mip levels, array layers, sample count, format, usage intent, lifecycle state, and creation outcome.
- **FR-003**: System MUST define sampler contracts that expose filtering, address behavior, comparison behavior, and creation outcome.
- **FR-004**: System MUST define portable, composable usage classifications for buffers and textures, including combinations required for rendering, compute, copying, sampling, storage, and presentation-adjacent workflows.
- **FR-005**: System MUST reject invalid or unsupported resource descriptions through explicit status values without returning usable resource objects.
- **FR-005a**: System MUST explicitly reject incompatible usage flag combinations while allowing valid multi-usage resources needed by renderer and render graph workflows.
- **FR-006**: System MUST define shader module contracts that expose declared shader stage, entry point identity, opaque payload identity, lifecycle state, and creation outcome without requiring bytecode validation or a concrete shader compiler in this phase.
- **FR-007**: System MUST define shader stage classifications for vertex, fragment/pixel, compute, and common future-visible stages while explicitly reporting unsupported stages that are outside this feature's scope.
- **FR-008**: System MUST define pipeline layout contracts that describe one or more descriptor set layouts, including set indices, descriptor bindings, descriptor types, binding slots, array counts, and shader stage visibility.
- **FR-009**: System MUST define descriptor set contracts that can bind buffers, textures, samplers, and combined texture-sampler resources according to a specific descriptor set index within a pipeline layout.
- **FR-010**: System MUST reject descriptor updates that target missing bindings, incompatible descriptor types, invalid array indices, unsupported resource usage, or invalidated resources.
- **FR-011**: System MUST define graphics pipeline contracts that describe shader stages, pipeline layout, vertex input, rasterization behavior, blending behavior, depth-stencil behavior, primitive topology, and compatible render target information.
- **FR-012**: System MUST define compute pipeline contracts that describe exactly one compute shader stage and one pipeline layout.
- **FR-013**: System MUST reject invalid pipeline descriptions such as missing required stages, incompatible stage combinations, missing layouts, unsupported formats, or attachment incompatibilities.
- **FR-014**: System MUST define single-subpass render pass contracts for attachment descriptions, load/store behavior, and color/depth-stencil attachment roles.
- **FR-015**: System MUST define framebuffer contracts that associate texture attachments with a compatible render pass and expose dimensions, attachment count, and lifecycle state.
- **FR-016**: System MUST reject framebuffer creation when attachments do not match render pass expectations for count, format, dimensions, sample count, or lifecycle validity.
- **FR-017**: System MUST ensure the rendering device remains the authoritative factory/owner for resources, layouts, descriptor sets, shader modules, pipelines, render passes, and framebuffers.
- **FR-018**: System MUST expose explicit lifecycle states for resource and pipeline-family objects, including at least Valid and Invalidated, and MUST reject use of Invalidated objects through explicit status values.
- **FR-019**: System MUST provide deterministic mock-test behavior for every public RHI resource and pipeline contract introduced by this feature.
- **FR-020**: System MUST preserve all existing RHI core device, command queue, command buffer, fence, semaphore, swapchain, result, queue type, and format behavior.
- **FR-021**: System MUST keep real graphics backend calls, shader compilation, memory allocation strategy, resource upload scheduling, and render graph execution outside this feature.

### Key Entities *(include if feature involves data)*

- **Buffer Resource**: Represents a GPU-addressable byte range with declared size, composable usage intent, lifecycle state, and creation result. Lifecycle state includes Valid and Invalidated.
- **Texture Resource**: Represents a one-, two-, three-dimensional, cube, or array image with dimensions, format, mip/layer information, composable usage intent, lifecycle state, and creation result. Lifecycle state includes Valid and Invalidated.
- **Sampler**: Represents texture sampling behavior such as filtering and address modes. Lifecycle state includes Valid and Invalidated.
- **Shader Module**: Represents an opaque shader payload identity, entry point, and declared stage without requiring bytecode validation or backend-specific shader objects. Lifecycle state includes Valid and Invalidated.
- **Pipeline Layout**: Represents the binding contract between shader stages and one or more descriptor sets.
- **Descriptor Binding**: Represents one declared set index, binding slot, resource category, array count, and shader stage visibility.
- **Descriptor Set**: Represents resource assignments that satisfy one set index within a pipeline layout. Lifecycle state includes Valid and Invalidated.
- **Graphics Pipeline**: Represents fixed and programmable graphics state used for draw workflows. Lifecycle state includes Valid and Invalidated.
- **Compute Pipeline**: Represents compute shader execution state. Lifecycle state includes Valid and Invalidated.
- **Render Pass**: Represents attachment roles and load/store behavior for a single-subpass render target flow. Lifecycle state includes Valid and Invalidated.
- **Framebuffer**: Represents concrete texture attachments compatible with a render pass. Lifecycle state includes Valid and Invalidated.
- **Resource/Pipeline Result**: Represents explicit success, invalid-state, unsupported, unavailable, or failed outcomes for creation and update operations.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A developer can validate buffer, texture, sampler, shader module, pipeline layout, descriptor set, graphics pipeline, compute pipeline, render pass, and framebuffer contracts through mock tests in under 5 minutes using the project test executable.
- **SC-002**: Mock-based tests cover 100% of public RHI resource and pipeline contracts introduced by this feature, with at least one success path and one negative path for each contract.
- **SC-003**: Invalid resource descriptions, incompatible descriptor updates, invalid pipeline descriptions, and incompatible framebuffer attachments are detected without process crashes in all test scenarios.
- **SC-004**: A renderer-facing smoke test can describe resources, bind them through a descriptor set, create a compatible pipeline, create a render pass/framebuffer pair, and validate the complete mock setup with zero backend-specific dependencies.
- **SC-005**: Public RHI resource and pipeline behavior can be reviewed without reference to any concrete graphics API, backend implementation, platform-specific presentation implementation, or renderer implementation.
- **SC-006**: The feature preserves all existing RHI core test outcomes while adding resource and pipeline validation coverage.

## Assumptions

- The existing RHI core interfaces feature is complete and provides device ownership, explicit result/status values, queue classifications, portable formats, and mock-test conventions.
- This roadmap phase is Phase 007, while the Speckit directory and branch use `008` because `specs/007-rhi-core-interfaces` already exists.
- Resource and pipeline interfaces are contract-only in this phase; real GPU memory allocation, shader bytecode validation, shader compilation, backend object creation, and data upload are deferred to Vulkan/backend phases.
- Ray tracing pipeline contracts, mesh shader pipeline contracts, and advanced global illumination-specific resource contracts are deferred to their roadmap phases.
- Render pass and framebuffer contracts should be sufficient for early forward rendering and Vulkan planning, while full multi-subpass dependency modeling and render graph execution remain later renderer/backend concerns.
- Mock implementations are sufficient for acceptance testing and must not require a real GPU, native window, swapchain image, platform surface, or graphics API runtime.
