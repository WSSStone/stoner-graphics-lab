# Feature Specification: Forward Rendering Pipeline

**Feature Branch**: `015-forward-rendering-pipeline`  
**Created**: 2026-07-02  
**Status**: Implemented  
**Input**: User description: "根据roadmap制定下一个大feature的spec"

## Clarifications

### Session 2026-07-02

- Q: What point light budget should the first forward renderer guarantee? → A: Configurable, default 4; order lights by deterministic influence such as distance and effectiveness, then accept the front N lights.
- Q: What execution boundary should this forward rendering phase guarantee? → A: Produce validated frame plans and render graph-compatible pass/resource declarations, but do not require real GPU execution or presentation.
- Q: What material surface input set should the first forward PBR path validate? → A: Full PBR-style surface set including occlusion, emissive, alpha, and extension slots.
- Q: What transparent draw ordering strategy should the forward renderer guarantee? → A: Sort by camera-space depth descending, then stable material id, then stable object id.
- Q: What should happen when a frame has no accepted lights? → A: Allow preparation with constant ambient-only fallback and emit a diagnostic.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Render Lit Opaque Geometry (Priority: P1)

As an engine developer, I need the renderer to produce a complete forward-rendered frame for opaque scene geometry so that the engine can move from data preparation to visible material and lighting output.

**Why this priority**: Opaque lit rendering is the minimum valuable slice for the next integration milestone and validates that the render graph and material systems can cooperate in a frame-level renderer.

**Independent Test**: Can be tested by declaring a camera, one or more opaque mesh items, material data, and at least one light, then requesting a frame plan and render graph-compatible pass/resource declarations, confirming that depth preparation, opaque rendering, material binding, lighting data, and final color output are all represented deterministically without requiring real GPU execution or presentation.

**Acceptance Scenarios**:

1. **Given** a scene with valid opaque geometry, valid material data, a camera, and one directional light, **When** a forward frame is prepared, **Then** the system produces a deterministic frame plan and render graph-compatible declarations with depth preparation before opaque lighting and a final color target that records all required geometry.
2. **Given** multiple opaque objects sharing the same material and compatible render state, **When** a forward frame is prepared repeatedly, **Then** the system produces stable draw ordering and stable reusable draw descriptions across runs.
3. **Given** an opaque object with missing or invalid material information, **When** the frame is validated, **Then** the object is rejected from the draw plan with a deterministic diagnostic that identifies the missing requirement.

---

### User Story 2 - Evaluate Basic Material Lighting (Priority: P2)

As a rendering developer, I need forward rendering to account for common material surface properties and basic lights so that scene output can communicate shape, roughness, metallic response, and light placement before advanced effects are added.

**Why this priority**: Lighting and physically meaningful material inputs are the main user-visible value of a forward renderer and unlock practical material validation.

**Independent Test**: Can be tested by preparing representative material inputs, one directional light, several point lights within the configured limit, and confirming the resulting frame description includes the expected light and material contributions without depending on shadows or post-processing.

**Acceptance Scenarios**:

1. **Given** opaque surfaces with base color, metallic, roughness, normal-related material data, occlusion, emissive, alpha, and extension-slot material data, **When** the forward renderer prepares an opaque lighting pass, **Then** the pass records all required surface inputs and rejects surfaces that lack mandatory data.
2. **Given** a scene with one directional light and a configured number of point lights, **When** lighting data is prepared, **Then** every accepted light is represented in a deterministic order with validated intensity, position or direction, range, and color.
3. **Given** more point lights than the configured per-frame limit, **When** lighting data is prepared, **Then** the system sorts lights by deterministic influence such as distance and effectiveness, accepts the front N lights, and reports the discarded lights through diagnostics.

---

### User Story 3 - Compose Sky and Transparent Geometry (Priority: P3)

As an engine developer, I need the forward renderer to include environment background and transparent object ordering so that common scenes can be composed correctly enough for demos and regression tests.

**Why this priority**: Sky and transparency complete the basic forward frame structure while staying below the complexity of shadows, post-processing, and deferred rendering.

**Independent Test**: Can be tested by preparing a scene with a sky environment and transparent objects at different camera-space depths, then verifying the frame plan places the environment and transparent work in the expected order and sorts transparent draws by camera-space depth from farthest to nearest for the active view, using stable material and object identifiers as tie-breakers.

**Acceptance Scenarios**:

1. **Given** a scene with a configured sky or environment background, **When** a forward frame is prepared, **Then** the frame includes a background contribution that does not overwrite accepted opaque geometry.
2. **Given** transparent geometry at distinct depths from the active camera, **When** transparent rendering is prepared, **Then** transparent draw descriptions are sorted from back to front by camera-space depth, with stable material id and stable object id tie-breakers.
3. **Given** a transparent object whose material is not compatible with transparent rendering, **When** the frame is validated, **Then** the object is excluded from the transparent pass with a deterministic diagnostic.

### Edge Cases

- A scene with no renderable geometry still produces a valid frame plan containing clear/background behavior and no geometry draws.
- A scene with geometry but no accepted lights produces deterministic diagnostics and a valid constant ambient-only fallback.
- Transparent objects with identical camera-space depth use stable material id and stable object id tie-breakers so repeated runs produce identical dumps.
- Invalid camera/view data prevents frame preparation and reports the exact missing or invalid view requirement.
- Invalid or missing output targets are rejected before any pass ordering is finalized.
- Transparent geometry that references an invalid or opaque-only material is rejected without affecting unrelated valid transparent objects.
- Point light counts above the configured limit are sorted by deterministic influence such as distance and effectiveness, then the front N lights are accepted and the remainder are reported without aborting the whole frame when at least one valid light remains.
- Resource requirements produced by material bindings must remain compatible with render graph declaration and must not introduce backend-specific resource handles into renderer-level data.

## Architecture & Design Constraints *(mandatory)*

- **RHI Abstraction**: The feature MUST NOT bypass the RHI layer to call graphics APIs directly.
- **Render Graph Integration**: The feature MUST express frame work as render graph-compatible declarations with deterministic pass ordering, resource requirements, and diagnostics, without requiring real GPU execution or presentation in this phase.
- **Layer Boundaries**: The feature MUST remain in the Renderer layer and consume existing material, shader, and render graph capabilities without introducing Application-layer scene ownership.
- **Design Patterns**: The feature MUST keep frame orchestration, view data, light data, material binding, draw preparation, and diagnostics as separable responsibilities.
- **Advanced Graphics Compatibility**: The feature MUST leave clear extension points for future deferred rendering, shadow mapping, meshlet rendering, ray tracing, and global illumination without requiring those features now.
- **Naming Conventions**: The feature's externally visible design MUST adhere to PascalCase, UnrealEngine5-style naming conventions.
- **Cross-Platform Compatibility**: The feature MUST compile and run on all supported platforms (Windows, macOS, Linux), with platform-specific behavior isolated behind existing abstraction layers.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST prepare a deterministic forward frame plan and render graph-compatible pass/resource declarations for a view, output target, scene draw inputs, material bindings, and light inputs.
- **FR-002**: System MUST include a depth preparation stage that runs before opaque geometry lighting whenever opaque geometry is present.
- **FR-003**: System MUST include an opaque geometry stage that accepts compatible opaque material bindings and rejects invalid or incomplete bindings with deterministic diagnostics.
- **FR-004**: System MUST support a full PBR-style metallic-roughness material input set for opaque surfaces, including base color, metallic, roughness, normal-related inputs, occlusion, emissive, alpha, and extension slots.
- **FR-005**: System MUST support exactly one primary directional light contribution per frame and report a clear diagnostic when more than one candidate is submitted.
- **FR-006**: System MUST support a configurable point light contribution limit with a default value of 4 lights per frame, and when submitted lights exceed the configured limit, the system MUST sort lights by deterministic influence such as distance and effectiveness before accepting the front N lights.
- **FR-007**: System MUST prepare validated view data for each frame, including camera transform, view-projection information, viewport dimensions, and camera position.
- **FR-008**: System MUST prepare validated light data for each frame, including stable influence ordering and validation of color, intensity, range, position, and direction fields as applicable.
- **FR-009**: System MUST support an environment background stage that can render a simple sky contribution when configured and otherwise falls back to a deterministic clear/background behavior.
- **FR-010**: System MUST support a transparent geometry stage for compatible transparent materials after opaque geometry and environment background preparation.
- **FR-011**: System MUST sort transparent draw descriptions by camera-space depth from farthest to nearest for the active view, then by stable material id and stable object id for ties.
- **FR-012**: System MUST produce reusable draw descriptions for mesh/material/view combinations so repeated frame preparation can demonstrate stable draw identity and ordering.
- **FR-013**: System MUST integrate material resource requirements into frame resource declarations without exposing backend-specific resource handles at the Renderer feature boundary.
- **FR-014**: System MUST provide deterministic diagnostics for invalid view data, invalid output targets, unsupported material domains or blends, missing shader/material bindings, incomplete PBR surface inputs, excessive lights, and incompatible transparent objects.
- **FR-015**: System MUST provide a human-readable frame debug dump that includes accepted passes, rejected items, draw counts, light counts, resource requirements, and ordering decisions.
- **FR-016**: System MUST allow the forward frame plan to be tested without requiring a real display window or user input system.
- **FR-017**: System MUST preserve deterministic behavior across repeated preparation of equivalent frame inputs.
- **FR-018**: System MUST allow frames with geometry but no accepted lights to prepare with a constant ambient-only fallback and a deterministic diagnostic.
- **FR-019**: System MUST exclude real GPU execution, presentation to a window or swapchain, shadow mapping, post-processing, deferred rendering, and application-level scene ownership from this feature.

### Key Entities

- **Forward Frame**: Represents one prepared rendering workload for a view, including output target, pass order, accepted draws, light data, environment behavior, diagnostics, render graph-compatible declarations, and resource requirements.
- **View Data**: Represents camera and viewport information needed to prepare frame-visible work.
- **Light Data**: Represents accepted directional and point light inputs, validation state, and stable ordering.
- **Mesh Draw Description**: Represents a reusable draw-ready summary for a mesh, material binding, view relevance, sort key, and pass participation.
- **Environment Background**: Represents the sky or fallback background contribution for a frame.
- **Transparent Draw Set**: Represents transparent objects accepted for the frame and sorted by camera-space depth, stable material id, and stable object id for back-to-front composition.
- **Frame Diagnostic**: Represents deterministic validation and preparation feedback for accepted, rejected, or limited frame inputs.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A representative scene with one camera, one directional light, at least four opaque objects, and two transparent objects can be prepared into a complete forward frame plan in under 1 second on a development machine.
- **SC-002**: Repeating frame preparation 20 times with identical inputs produces identical pass order, draw order, diagnostics, and debug dump output in 100% of runs.
- **SC-003**: The system reports actionable diagnostics for at least 10 distinct invalid input cases, including invalid view data, missing material binding, incomplete PBR surface inputs, incompatible transparency, excessive point lights, and missing output target.
- **SC-004**: A frame containing more point lights than the configured limit still prepares successfully when at least one valid light and one valid draw remain, defaults to accepting 4 point lights unless configured otherwise, and reports exactly how many lights were accepted and rejected.
- **SC-005**: Transparent objects with distinct camera-space depths are ordered back-to-front correctly in 100% of tested representative cases, and equal-depth ties remain deterministic by material id and object id.
- **SC-006**: Frame preparation and render graph-compatible declaration output can be verified in automated tests without real GPU execution, opening a window, or requiring interactive input.
- **SC-007**: A frame with valid geometry and no accepted lights prepares successfully with exactly one ambient-only fallback diagnostic in 100% of tested representative cases.
- **SC-008**: Renderer boundary validation confirms this feature introduces no direct graphics API calls or backend-specific resource handles into the Renderer-level public contract.

## Assumptions

- This spec corresponds to roadmap Phase 014, but uses Speckit directory number `015` because `014-material-shader-system` is already the current completed feature directory.
- Existing render graph, material, shader, resource requirement, diagnostics, and RHI contract work from prior phases is available and remains the foundation for this feature.
- The initial forward renderer targets deterministic frame preparation and render graph-compatible pass/resource declarations before real GPU execution or full visual presentation through a windowed application.
- A simple sky or environment contribution is sufficient; shadow mapping, post-processing, and deferred rendering remain out of scope.
- The point light count is configurable, defaults to 4 per frame, and uses deterministic influence ordering based on factors such as distance and effectiveness before accepting the front N lights.
- Frames with geometry but no accepted lights use a constant ambient-only fallback and report that fallback through diagnostics.
- Basic precompiled shader assets or shader records may be used for validation, but runtime shader authoring, shader compilation, and shader source scanning are out of scope.

## Implementation Notes

- Implemented Renderer-level forward frame planning through `FForwardRenderer`, `FForwardFramePlan`, `FForwardViewData`, `FForwardLightData`, `FMeshDrawCommand`, `FForwardRenderGraphDeclaration`, and `FForwardDiagnostics`.
- Implementation remains headless and backend-agnostic: it prepares validated frame plans, pass/resource declaration summaries, deterministic diagnostics, and debug dumps without real GPU execution or presentation.
- Verification is recorded in `quickstart.md`; `conda run -n godot scons`, `Build/Mac/Debug/Tests/StonerTest`, 20-run dump stability, representative under-1-second preparation, and boundary isolation checks passed on 2026-07-03.
