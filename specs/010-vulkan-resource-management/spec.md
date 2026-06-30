# Feature Specification: Vulkan Resource Management

**Feature Branch**: `010-vulkan-resource-management`  
**Created**: 2026-06-30  
**Status**: Draft  
**Input**: User description: "根据roadmap制定下一个大feature的spec"

## Clarifications

### Session 2026-06-30

- Q: Should backend resource creation require a real Vulkan runtime/SDK, use only fallback behavior, or support both? → A: Real runtime available uses real resource path; unavailable runtime may use deterministic fallback allocation with diagnostics.
- Q: What should upload staging produce before command recording exists? → A: Upload staging records CPU-visible staging data and destination ranges; execution remains deferred to the command phase.
- Q: How should descriptor sets behave when a bound resource is invalidated? → A: Descriptor sets retain the binding record and report the bound resource as invalidated; later use or incompatible updates fail explicitly.
- Q: How should allocation failure be made deterministic for validation? → A: Tests may configure resource budget or allocation-count limits to trigger allocation failure and cleanup paths deterministically.
- Q: How should descriptor pool exhaustion behave? → A: Descriptor pools use configurable fixed capacity; descriptor set allocation fails explicitly when capacity is exhausted.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Create Backend Buffers and Textures (Priority: P1)

An engine developer can request backend-backed buffers and textures through the existing rendering hardware interface contracts and receive usable resource objects when the backend device supports the requested descriptions.

**Why this priority**: Buffers and textures are the first required GPU resources for visible rendering, uploads, descriptor binding, and later command recording. Without them, the Vulkan backend device cannot progress toward the triangle demo or renderer integration.

**Independent Test**: Can be tested by creating an active backend device, requesting valid buffer and texture descriptions, verifying the resulting objects preserve their descriptions and lifecycle state, and verifying invalid or unsupported descriptions return explicit failures without creating usable resources.

**Acceptance Scenarios**:

1. **Given** an active backend device and a valid buffer description, **When** a developer creates a buffer, **Then** the system returns a valid buffer object whose size, usage, lifecycle state, and backend allocation state can be queried through the existing resource contract.
2. **Given** an active backend device and a valid texture description, **When** a developer creates a texture, **Then** the system returns a valid texture object whose dimensions, format, usage, lifecycle state, and backend allocation state can be queried through the existing resource contract.
3. **Given** an invalid, unsupported, or resource-limit-exceeding buffer or texture description, **When** creation is requested, **Then** the system returns an explicit invalid-state, unsupported, out-of-memory, or failed result without creating a usable partial resource.

---

### User Story 2 - Manage Backend Resource Memory Safely (Priority: P1)

An engine developer can rely on the backend to allocate, track, release, and diagnose memory ownership for created buffers and textures.

**Why this priority**: Resource objects are only safe if their memory ownership is deterministic. Later upload, descriptor, command, and render graph work depends on resources being released cleanly and on allocation failures being reported explicitly.

**Independent Test**: Can be tested by creating and destroying multiple resources across supported, fallback, and test-limited allocation paths, checking that each created resource owns a valid allocation record, and confirming failed allocations do not leak usable objects.

**Acceptance Scenarios**:

1. **Given** enough backend memory is available, **When** a developer creates supported buffers and textures, **Then** each object reports a valid allocation record and remains usable until invalidated or the device shuts down.
2. **Given** allocation cannot be satisfied because of resource budget or allocation-count limits, **When** resource creation is requested, **Then** the system returns an explicit failure without exposing a partially usable buffer or texture.
3. **Given** resources exist when the backend device shuts down, **When** shutdown completes, **Then** every owned resource transitions to a non-usable lifecycle state and repeated cleanup remains safe.

---

### User Story 3 - Create and Use Backend Samplers (Priority: P1)

An engine developer can create sampler objects compatible with existing sampler descriptions and use them as bindable resources in later descriptor workflows.

**Why this priority**: Texture sampling is required for material systems and forward rendering. Samplers are smaller than buffers and textures, but they must obey the same lifecycle and unsupported-description behavior.

**Independent Test**: Can be tested by creating supported sampler descriptions, verifying the sampler preserves its filtering and addressing behavior through the existing contract, and confirming unsupported combinations are rejected.

**Acceptance Scenarios**:

1. **Given** a valid sampler description, **When** a developer creates a sampler, **Then** the system returns a valid sampler object with queryable lifecycle state and preserved sampling behavior.
2. **Given** an unsupported sampler mode combination, **When** creation is requested, **Then** the system returns unsupported without creating a usable sampler.
3. **Given** the backend device has been shut down, **When** sampler creation is requested, **Then** the system returns invalid-state without creating a usable object.

---

### User Story 4 - Bind Resources Through Descriptor Sets (Priority: P1)

An engine developer can allocate backend descriptor sets from declared layouts and bind buffers, textures, samplers, and combined texture-sampler pairs according to the existing descriptor contract.

**Why this priority**: Descriptor binding is the bridge between resources and future pipelines. Even before command recording and shader execution are implemented, the backend must validate binding compatibility and lifecycle rules.

**Independent Test**: Can be tested by creating resources, a descriptor layout, and descriptor sets from a fixed-capacity descriptor pool; updating each supported binding kind; and verifying type mismatches, missing bindings, invalid array indices, invalidated resources, pool exhaustion, and post-shutdown updates are rejected.

**Acceptance Scenarios**:

1. **Given** a valid descriptor layout and active backend device, **When** a descriptor set is allocated for a declared set index, **Then** the system returns a valid descriptor set that can report its layout and bound resource kinds.
2. **Given** valid buffer, texture, sampler, and combined texture-sampler bindings, **When** the descriptor set is updated, **Then** each binding records the expected resource kind and remains queryable.
3. **Given** a binding type mismatch, missing binding, invalid array index, invalidated resource, or device shutdown, **When** an update is attempted, **Then** the system returns an explicit failure without changing unrelated bindings.
4. **Given** a descriptor set already references a resource that later becomes invalidated, **When** a developer queries the binding, **Then** the descriptor set retains the binding record and reports that the referenced resource is no longer valid.
5. **Given** a descriptor pool has reached its configured set capacity, **When** another descriptor set allocation is requested, **Then** the system returns an explicit failure without changing existing descriptor sets.

---

### User Story 5 - Stage Resource Upload Requests for Later Submission (Priority: P2)

An engine developer can prepare upload requests for buffers and textures and receive validated upload records with CPU-visible staging data and destination ranges that can be consumed by the future command submission phase.

**Why this priority**: Real rendering needs data movement from CPU-visible inputs to backend resources, but command recording and queue submission are separate roadmap phases. This stage should validate upload intent and preserve enough metadata for the next phase without pretending execution occurred.

**Independent Test**: Can be tested by creating upload requests for valid buffers and textures, verifying byte ranges and texture regions are validated, and confirming requests are recorded as pending rather than executed.

**Acceptance Scenarios**:

1. **Given** a valid buffer and a valid source data range, **When** an upload is requested, **Then** the system records CPU-visible staging data for a pending buffer upload with explicit byte range, destination, and lifecycle state.
2. **Given** a valid texture and a valid source region, **When** an upload is requested, **Then** the system records CPU-visible staging data for a pending texture upload with explicit region, format compatibility, and lifecycle state.
3. **Given** an invalid range, incompatible format, missing source data, invalidated destination, or unsupported upload path, **When** an upload is requested, **Then** the system returns an explicit failure without modifying resource contents.

### Edge Cases

- What happens when a buffer size is zero, exceeds device limits, or uses unsupported usage combinations?
- What happens when a texture has invalid dimensions, unsupported format, incompatible usage, invalid mip levels, invalid array layers, or unsupported sample count?
- What happens when resource creation is requested after backend device shutdown?
- What happens when resource allocation succeeds partially and later creation steps fail?
- What happens when memory budget or allocation count limits are reached?
- What happens when a resource is invalidated while still referenced by a descriptor set?
- What happens when descriptor updates target a missing binding, wrong descriptor type, or invalid array index?
- What happens when a descriptor set allocation is requested for a layout set that does not exist?
- What happens when descriptor pool capacity is exhausted?
- What happens when upload requests target invalidated resources or ranges outside the resource bounds?
- What happens when the backend runtime is unavailable but RHI resource contracts are still tested through explicit unsupported behavior?
- What happens when repeated create/invalidate/shutdown cycles happen in the same process?

## Architecture & Design Constraints *(mandatory)*

- **RHI Abstraction**: Renderer and Application-facing behavior MUST remain expressed through existing resource, descriptor, and device contracts. Backend-specific allocation details MUST NOT leak into Renderer-facing contracts.
- **Design Patterns**: Buffer, texture, sampler, memory allocation, descriptor allocation, descriptor updates, and upload staging responsibilities MUST remain separate and testable. The feature MUST avoid a single catch-all resource manager with hidden ownership rules.
- **Advanced Graphics**: Resource descriptions and descriptor binding behavior MUST leave room for future render graph, ray tracing, meshlet, global illumination, and bindless-style resource models without implementing those features in this phase.
- **Naming Conventions**: Public project-facing concepts introduced by this feature MUST follow the project's UE5-style naming conventions.
- **Cross-Platform Compatibility**: The backend MUST support supported desktop environments where the required graphics runtime is available and MUST report explicit unsupported status where runtime, format, memory, or allocation capabilities are absent.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST allow creation of backend-backed buffers through the existing resource creation contract when the backend device is active and the requested description is valid and supported, using the real resource path when runtime support is available and a deterministic fallback allocation path with diagnostics when it is unavailable.
- **FR-002**: System MUST reject invalid or unsupported buffer descriptions with explicit results and without returning usable partial buffer objects.
- **FR-003**: System MUST allow creation of backend-backed textures through the existing resource creation contract when the backend device is active and the requested description is valid and supported, using the real resource path when runtime support is available and a deterministic fallback allocation path with diagnostics when it is unavailable.
- **FR-004**: System MUST reject invalid or unsupported texture descriptions with explicit results and without returning usable partial texture objects.
- **FR-005**: System MUST allow creation of backend-backed samplers for supported sampler descriptions and reject unsupported sampler mode combinations explicitly.
- **FR-006**: System MUST expose resource lifecycle state consistently for buffers, textures, samplers, descriptor sets, and upload records.
- **FR-007**: System MUST track backend allocation ownership for buffers and textures so that successful resources can be released or invalidated deterministically.
- **FR-008**: System MUST report allocation failure, memory budget exhaustion, unsupported memory requirements, or partial creation failure without leaving usable partial resources.
- **FR-008a**: System MUST allow validation to configure resource budget or allocation-count limits so allocation failure and cleanup behavior can be tested deterministically.
- **FR-009**: System MUST invalidate or release all owned resource allocations when the backend device shuts down.
- **FR-010**: System MUST reject buffer, texture, sampler, descriptor, and upload requests after backend device shutdown with invalid-state status.
- **FR-011**: System MUST allocate descriptor sets only for declared descriptor layouts and set indices from descriptor pools with configurable fixed capacity.
- **FR-011a**: System MUST reject descriptor set allocation with an explicit failure when descriptor pool capacity is exhausted, without invalidating existing descriptor sets.
- **FR-012**: System MUST support descriptor updates for buffers, textures, samplers, and combined texture-sampler resources according to the existing descriptor contract.
- **FR-013**: System MUST reject descriptor updates when the binding does not exist, the descriptor type is incompatible, the array index is invalid, the resource is missing, or the resource lifecycle is no longer valid.
- **FR-014**: System MUST preserve existing bound resource information so tests and future renderer code can query descriptor update outcomes deterministically, including retained binding records whose referenced resources have become invalidated.
- **FR-015**: System MUST allow validated buffer upload requests to record CPU-visible staging data and destination byte ranges for future command execution without claiming the upload has already executed.
- **FR-016**: System MUST allow validated texture upload requests to record CPU-visible staging data and destination regions for future command execution without claiming the upload has already executed.
- **FR-017**: System MUST reject upload requests with missing source data, out-of-bounds ranges, incompatible texture regions, invalidated destinations, or unsupported transfer paths.
- **FR-018**: System MUST preserve existing Core, RHI core, RHI resource/pipeline, and Vulkan device/swapchain test outcomes while adding backend resource validation.
- **FR-019**: System MUST provide deterministic test coverage for success paths and negative paths for resource creation, real-runtime or fallback allocation diagnostics, allocation failure, sampler creation, descriptor allocation, descriptor update, upload staging, lifecycle invalidation, and shutdown.
- **FR-020**: System MUST keep command recording, queue execution of uploads, shader compilation, pipeline creation, and render graph scheduling outside this feature's delivered scope.

### Key Entities *(include if feature involves data)*

- **Backend Buffer**: Represents a backend-owned buffer resource with size, usage, lifecycle, and allocation ownership.
- **Backend Texture**: Represents a backend-owned texture resource with dimension, extent, mip levels, array layers, format, usage, lifecycle, and allocation ownership.
- **Backend Sampler**: Represents a sampling state object with lifecycle and supported filtering/addressing behavior.
- **Resource Allocation**: Represents the ownership record for memory associated with a buffer or texture, including whether creation succeeded, failed because of configured limits, failed for runtime reasons, or was released.
- **Descriptor Pool**: Represents configurable fixed backend capacity from which descriptor sets are allocated and reclaimed, including explicit exhaustion state.
- **Descriptor Set**: Represents a set of resource bindings compatible with a declared layout and queryable by binding slot and array index.
- **Bound Resource Record**: Represents a descriptor binding result, including resource kind, binding slot, array index, and whether the currently referenced resource lifecycle remains valid.
- **Upload Request**: Represents a validated pending data transfer into a buffer or texture, including CPU-visible staging data, destination, range or region, and execution state.
- **Resource Creation Result**: Represents explicit success, unsupported, invalid-state, out-of-memory, unavailable, or failed outcomes for resource operations.
- **Resource Diagnostics**: Represents allocation mode, fallback allocation reasons, allocation failure reasons, unsupported format or usage reasons, descriptor update reasons, and upload rejection reasons.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In a supported development environment, a developer can create at least one valid buffer, texture, and sampler from an active backend device and query their descriptions, lifecycle states, and real-or-fallback allocation diagnostics in under 30 seconds using the project verification flow.
- **SC-002**: Invalid or unsupported buffer, texture, and sampler descriptions are rejected with explicit results in 100% of covered negative cases without crashes or usable partial objects.
- **SC-003**: Resource allocation failure triggered by configured budget or allocation-count limits and device shutdown paths leave zero usable stale resources in the covered lifecycle tests.
- **SC-004**: Descriptor validation covers at least one success path and one negative path each for buffer, texture, sampler, combined texture-sampler, missing binding, wrong type, invalid array index, descriptor pool exhaustion, invalidated resource updates, and queries of retained bindings whose resources later become invalidated.
- **SC-005**: Upload staging validation covers at least one success path and one negative path each for buffer uploads and texture uploads, including preservation of CPU-visible staging data and destination ranges, without requiring command recording or queue execution.
- **SC-006**: Existing Core, RHI core, RHI resource/pipeline, and Vulkan device/swapchain tests continue to pass after backend resource management is added.
- **SC-007**: Renderer-facing code can interact with backend buffers, textures, samplers, descriptor sets, and upload records through established contracts without depending on backend-specific allocation details.

## Assumptions

- The Vulkan backend device and swapchain feature is complete enough to create an active backend device, expose capabilities, shut down cleanly, and report unsupported runtime states.
- Existing RHI resource and descriptor contracts define the public behavior for buffers, textures, samplers, descriptor layouts, descriptor sets, lifecycle state, and object results.
- This feature targets the roadmap's Phase 009, while the Speckit feature directory uses `010` because existing spec directories already occupy numbers through `009`.
- Resource creation is expected to use real backend allocation in supported runtime environments and deterministic fallback allocation with diagnostics where runtime support is absent but the resource contract can still be validated.
- Upload requests are validated and recorded as pending work with CPU-visible staging data in this phase; actual transfer command recording and queue execution are deferred to the command submission phase.
- Shader modules, graphics pipelines, compute pipelines, render passes, framebuffers, and render graph scheduling remain outside this feature except where existing tests must continue to pass.
