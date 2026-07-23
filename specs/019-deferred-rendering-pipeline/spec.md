# Feature Specification: Deferred Rendering Pipeline

**Feature Branch**: `019-deferred-rendering-pipeline`  
**Created**: 2026-07-23  
**Status**: Draft  
**Input**: User description: "根据 roadmap 制定下一个阶段的 spec：Renderer Deferred Rendering Pipeline"

## Clarifications

### Session 2026-07-23

- Q: 本阶段完成时，Deferred Rendering Pipeline 应该验证到哪一层？ → A: 必须经过真实 RHI 离屏执行，并通过 readback 或参考样本验证实际像素结果；同时保留确定性 headless 测试。
- Q: 真实 RHI 离屏执行和像素 readback 验证必须在哪些平台通过？ → A: Linux Lavapipe CI 必须执行真实 Vulkan 离屏像素验证；Windows 和 macOS 执行编译及确定性 headless 测试。
- Q: Forward 与 deferred 性能对比是否必须证明 deferred 更快才能完成本阶段？ → A: 不要求 deferred 必须更快；性能报告作为可复现基线，必须记录各 light-count tier 的结果与 crossover 行为。
- Q: 现有材质系统的哪些语义必须由首版 deferred opaque 路径保留？ → A: 支持 base color、normal、metallic、roughness、depth、emissive 和 ambient occlusion；alpha 仅用于 masked coverage，透明混合继续使用 forward-transparent。
- Q: Linux Lavapipe 离屏 readback 与参考结果应采用哪种误差标准？ → A: 使用语义化容差：最终 LDR 颜色每通道误差不超过 2/255，归一化深度误差不超过 1e-4，解码法线点积不低于 0.999，metallic、roughness 和 occlusion 误差不超过 1e-3。

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Render Opaque Scenes Through Deferred Shading (Priority: P1)

An engine developer can select a deferred rendering path for an opaque scene and receive a complete frame in which surface properties are captured once, lighting is evaluated from those properties, and the final image is composed for the requested output.

**Why this priority**: This is the minimum viable deferred pipeline and establishes the alternative rendering path required by later many-light and global-illumination work.

**Independent Test**: Submit a representative view containing opaque objects with distinct material properties and directional, point, and spot lights, then verify the declared passes, intermediate surface data, lighting contribution, and composed output without relying on the forward opaque-lighting path.

**Acceptance Scenarios**:

1. **Given** a valid view, output, opaque draws, and supported materials, **When** a deferred frame is prepared, **Then** surface-data, lighting, and composition work is accepted in dependency-correct order.
2. **Given** surfaces with different base color, normal, metallic, roughness, depth, emissive, and ambient occlusion values, **When** the surface-data stage completes, **Then** each visible surface can be distinguished by every required deferred lighting input.
3. **Given** a valid deferred frame, **When** it executes, **Then** the final output contains the composed result and intermediate surface data is not exposed as the presentation result.
4. **Given** no accepted opaque geometry, **When** a deferred frame is prepared, **Then** unnecessary geometry and local-light work is omitted while a deterministic background output remains possible.
5. **Given** a supported real graphics runtime, **When** the deferred frame executes offscreen through the RHI, **Then** output readback or reference samples verify the expected surface and lighting result without requiring a visible window.

---

### User Story 2 - Scale Local Lighting Independently of Geometry (Priority: P2)

An engine developer can render the same opaque scene under many point and spot lights while each light affects only eligible pixels and the geometry stage is not repeated for every light.

**Why this priority**: Efficient many-light evaluation is the primary practical advantage of deferred rendering and the reason this path exists alongside forward rendering.

**Independent Test**: Prepare the same scene with increasing directional, point, and spot light counts, verify deterministic light acceptance and ordering, confirm bounded light regions for local lights, and compare the resulting work and timing against the existing forward path.

**Acceptance Scenarios**:

1. **Given** multiple valid directional lights, **When** lighting is prepared, **Then** each directional light is evaluated as a full-view contribution in deterministic order.
2. **Given** valid point and spot lights, **When** lighting is prepared, **Then** each local light declares a bounded influence region derived from its range and shape.
3. **Given** a local light whose influence does not intersect the active view, **When** the frame is prepared, **Then** that light contributes no lighting work and its omission is inspectable.
4. **Given** equivalent scene, view, material, and light inputs, **When** forward and deferred comparison runs are requested, **Then** both paths report comparable workload and timing measurements using the same warm-up and sampling policy.

---

### User Story 3 - Inspect and Diagnose Deferred Frames (Priority: P2)

An engine developer can inspect deferred frame structure, resource usage, light decisions, and failures without stepping through backend-specific graphics code.

**Why this priority**: A multi-pass renderer is difficult to extend or validate unless pass ordering, intermediate resources, rejected inputs, and failure ownership are observable and deterministic.

**Independent Test**: Exercise valid and invalid frame inputs, inspect the normalized frame report, and confirm that the first actionable diagnostic identifies the failed stage and subject while repeated equivalent runs remain identical.

**Acceptance Scenarios**:

1. **Given** a valid deferred frame, **When** diagnostics are requested, **Then** the report identifies accepted passes, intermediate resources, draw counts, light counts, culled lights, and composition output.
2. **Given** an incompatible material, missing output, invalid view, malformed light, or incompatible surface-data layout, **When** preparation occurs, **Then** the frame is rejected or the affected item is omitted according to documented policy with an actionable diagnostic.
3. **Given** equivalent inputs across repeated runs, **When** reports are compared, **Then** pass order, resource declarations, light order, culling decisions, result categories, and normalized diagnostics are identical.
4. **Given** execution fails after partial preparation, **When** cleanup completes, **Then** temporary frame resources are released and no later dependent stage claims success.

---

### User Story 4 - Preserve Existing Renderer Workflows (Priority: P3)

An engine maintainer can add and validate deferred rendering without changing the default forward workflow, breaking existing materials, or introducing platform-specific Renderer behavior.

**Why this priority**: Deferred rendering is an alternative, not a replacement; existing forward rendering and the triangle integration milestone must remain usable throughout development.

**Independent Test**: Run the existing Renderer and demo regressions, select forward and deferred paths independently, and execute automated build and headless validation on Windows, macOS, and Linux.

**Acceptance Scenarios**:

1. **Given** no deferred path is selected, **When** an existing forward frame is prepared and executed, **Then** its established behavior and output remain unchanged.
2. **Given** a material supported by both paths, **When** it is submitted to either renderer, **Then** its shared surface semantics have equivalent meaning without duplicating material authoring data.
3. **Given** a transparent draw, **When** a frame uses deferred rendering, **Then** the draw is handed to the established forward-transparent stage after deferred composition rather than being incorrectly written into opaque surface data.
4. **Given** any supported desktop CI platform, **When** automated validation runs, **Then** the project builds and deterministic deferred headless integration coverage completes without requiring visible presentation.
5. **Given** the Linux CI software graphics runtime, **When** native deferred validation runs, **Then** real Vulkan offscreen execution completes through Lavapipe and verifies output pixels by readback or deterministic reference samples.

### Edge Cases

- The active view or output extent is zero, empty, or changes between prepared frames.
- Required surface-data outputs use incompatible dimensions, sample counts, or semantic layouts.
- A material omits a required deferred surface input or uses a domain/blend mode that cannot participate in the opaque surface-data stage.
- Geometry overlaps at equal or nearly equal depth, including reversed winding, clipping, and off-screen bounds.
- A scene contains no geometry, no lights, only emissive geometry, or only ambient/background contribution.
- Directional, point, or spot light inputs contain non-finite values, negative intensity, non-positive range, invalid cone angles, or degenerate directions.
- A point or spot light intersects the camera near plane, encloses the camera, lies entirely behind the view, or touches only the view boundary.
- Light counts are zero, very large, or change sharply between consecutive frames.
- Multiple lights have identical influence and identity ordering keys.
- A surface-data, lighting, or composition stage fails after earlier temporary resources have been accepted.
- A transparent object is submitted alongside deferred-compatible opaque geometry.
- Forward and deferred comparison inputs differ in a way that would make the reported measurements misleading.
- A platform can compile the feature but has no available real graphics runtime for optional native execution.

## Architecture & Design Constraints *(mandatory)*

- **RHI Abstraction**: Renderer behavior MUST NOT bypass the RHI layer or expose backend-specific objects in deferred public contracts. Graphics execution MUST use existing backend-neutral resource, pipeline, command, synchronization, and render-pass capabilities.
- **Render Graph Ownership**: Surface-data, lighting, and composition work MUST be declared through the existing render graph so ordering, lifetimes, transitions, culling, and transient-resource decisions remain centrally inspectable.
- **Renderer Strategy Boundary**: Forward and deferred rendering MUST remain selectable strategies sharing stable view, material, draw, light, and output semantics where those concepts overlap. Neither strategy may own application scene traversal or backend device lifetime.
- **Responsibility Separation**: The feature MUST keep frame input validation, surface-data declaration, lighting declaration, composition, execution binding, diagnostics, and performance reporting as separable responsibilities rather than concentrating them in one renderer class.
- **Advanced Graphics**: Deferred contracts MUST leave explicit extension points for later tiled or clustered light assignment, shadow inputs, SSAO, SSR, ray tracing, and global illumination without implementing those features here.
- **Naming Conventions**: Public code design MUST adhere to PascalCase, Unreal Engine-style naming conventions.
- **Cross-Platform Compatibility**: The feature MUST compile and provide equivalent deterministic Renderer behavior on Windows, macOS, and Linux. Platform-specific graphics availability MUST remain an execution capability rather than changing frame semantics.
- **Automated Cross-Platform Validation**: The feature MUST include or update automated Windows, macOS, and Linux build and deterministic headless integration validation. Linux CI MUST additionally execute the real Vulkan deferred path offscreen through Lavapipe and validate its pixels; native offscreen execution is not a completion gate on Windows or macOS for this feature.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The Renderer MUST offer deferred rendering as an explicit alternative to the existing forward path without changing the established default path.
- **FR-002**: The deferred path MUST accept a validated active view, output target, opaque draw set, material bindings, and directional, point, and spot light inputs.
- **FR-003**: The deferred path MUST prepare a deterministic surface-data stage that records, for each visible opaque surface, base color, view-independent normal information, metallic value, roughness value, emissive contribution, ambient occlusion, and depth sufficient for later position reconstruction.
- **FR-004**: The surface-data contract MUST define each stored semantic, value range, coordinate space, clear value, precision expectation, and compatibility rule while allowing concrete storage formats to be selected during planning.
- **FR-005**: Deferred-compatible materials MUST reuse the existing material and instance authoring data; the renderer MUST report unsupported domains, blend modes, permutations, or missing required surface inputs before execution.
- **FR-006**: Opaque and compatible masked geometry MUST participate in the surface-data stage, with alpha used only to determine masked coverage; transparent blended geometry MUST be excluded from that stage and remain eligible for the established forward-transparent stage after deferred composition.
- **FR-007**: Geometry visibility and depth resolution MUST occur once per deferred frame and MUST NOT be repeated independently for every accepted light.
- **FR-008**: The deferred path MUST prepare a lighting accumulation stage that reads the declared surface data and evaluates every accepted directional, point, and spot light without applying the forward renderer's local-light count limit.
- **FR-009**: Directional lights MUST contribute across the active view; point and spot lights MUST declare bounded influence regions from validated light shape and range data.
- **FR-010**: Local lights that cannot affect the active view MUST be omitted before execution, and accepted and omitted lights MUST use stable entity identity as the final ordering tie-breaker when scene entities are available.
- **FR-011**: The lighting stage MUST preserve independent diffuse and specular material response using the existing metallic-roughness and ambient-occlusion surface semantics and MUST support deterministic ambient-only, emissive-only, and no-light outcomes.
- **FR-012**: The deferred path MUST prepare a composition stage that combines accumulated lighting with required surface contributions into exactly one final renderer output suitable for downstream presentation or later post-processing.
- **FR-013**: Transparent forward work, when present, MUST execute after deferred composition against the same active view and final output, with deterministic cross-path ordering.
- **FR-014**: Surface-data, lighting, composition, and optional transparent work MUST be represented as render graph passes with explicit resource reads, writes, ordering dependencies, and culling eligibility.
- **FR-015**: Intermediate resources MUST declare compatible extents, semantic layouts, access states, and lifetimes; incompatible bindings MUST fail before dependent work executes.
- **FR-016**: Equivalent frame inputs MUST produce deterministic pass order, draw order, light order, resource declarations, culling decisions, diagnostics, and normalized debug reports.
- **FR-017**: Diagnostics MUST identify invalid view/output data, rejected draws/materials/lights, incompatible intermediate resources, light culling decisions, failed stages, and cleanup outcomes without exposing unstable native addresses.
- **FR-018**: The feature MUST provide a human-readable deferred frame report containing pass order, resource semantics, accepted and rejected draw counts, accepted and culled light counts by type, composition state, and stable result categories.
- **FR-019**: The feature MUST provide a reproducible forward-versus-deferred comparison using identical scene, view, output, material, and light inputs, with documented warm-up, sample count, workload counts, per-light-count-tier summary timing statistics, and observed crossover behavior; deferred rendering is not required to outperform forward rendering for feature completion.
- **FR-020**: Comparison reporting MUST detect and reject non-equivalent inputs or incomplete runs rather than presenting them as valid performance evidence.
- **FR-021**: Automated tests MUST cover frame preparation, render graph declaration, material compatibility, all supported light types, local-light influence bounds, deterministic ordering, composition, failure handling, and cleanup without requiring visible presentation.
- **FR-022**: Automated validation MUST build and run deterministic deferred headless integration coverage on Windows, macOS, and Linux, while preserving all existing Core, RHI, backend, Renderer, Application, scene/ECS, and triangle demo regression outcomes.
- **FR-023**: Tiled or clustered deferred rendering, shadows, SSAO, SSR, temporal effects, anti-aliasing, decals, custom post-processing, new material authoring workflows, new graphics backends, scene serialization, and editor tooling MUST remain outside this feature.
- **FR-024**: Linux CI MUST execute the deferred path through real RHI offscreen resources using the Lavapipe Vulkan runtime and MUST verify resulting pixels through output readback or deterministic reference samples; this native execution coverage complements rather than replaces three-platform deterministic headless tests and is not required on Windows or macOS for feature completion.
- **FR-025**: Native offscreen validation MUST compare finite readback values by semantic: final LDR color error MUST be no greater than 2/255 per channel, normalized depth error no greater than 1e-4, decoded-normal dot product no less than 0.999, and metallic, roughness, and ambient-occlusion error no greater than 1e-3; values outside these bounds or any non-finite result MUST fail validation.

### Key Entities

- **Deferred Renderer**: The selectable Renderer strategy that validates inputs and coordinates surface-data, lighting, composition, optional transparent handoff, diagnostics, and frame execution.
- **Deferred Frame Inputs**: The active view, final output, draw descriptions, shared material bindings, light set, background or ambient contribution, and stable identities for one frame.
- **Surface Data Layout**: The semantic contract for base color, normal, metallic, roughness, emissive, ambient occlusion, and depth, including value ranges, coordinate spaces, clear values, precision expectations, and compatibility identity.
- **Deferred Frame Plan**: The deterministic accepted work, pass ordering, resource requirements, draw/light decisions, and final composition state for one frame.
- **Deferred Light Record**: A validated directional, point, or spot light with stable identity, photometric inputs, shape/range data, influence bounds, acceptance state, and rejection or culling reason.
- **Deferred Graph Declaration**: The render graph-compatible passes, resources, accesses, dependencies, and outputs derived from a deferred frame plan.
- **Deferred Diagnostic**: A stable record of stage, severity, result category, subject identity, and actionable reason.
- **Renderer Comparison Report**: The normalized workload description, warm-up and sampling policy, forward and deferred measurements, validity state, and summary statistics for one equivalent-scene comparison.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A representative scene containing at least 100 opaque draws, 1 directional light, 64 point lights, and 16 spot lights prepares a complete deferred frame with one surface-data sequence, deterministic light work, and one composition result in 100% of 20 repeated runs.
- **SC-002**: For at least 12 readback or reference samples produced by real RHI offscreen execution and spanning distinct base colors, normals, metallic values, roughness values, depths, emissive values, and ambient-occlusion values, 100% of final LDR color channels are within 2/255 of reference, normalized depths are within 1e-4, decoded-normal dot products are at least 0.999, metallic/roughness/occlusion values are within 1e-3, and no compared value is non-finite.
- **SC-003**: Local-light boundary tests covering outside-view, boundary-touching, camera-enclosing, near-plane-intersecting, and fully visible point/spot lights produce the expected accepted or culled result in 100% of cases.
- **SC-004**: Repeating equivalent frame preparation 20 times produces identical pass order, draw order, light order, resource declarations, culling decisions, result categories, and normalized diagnostic output in all 20 runs.
- **SC-005**: Every tested invalid view, output, material, surface-data layout, directional light, point light, and spot light case identifies the primary rejected subject and reason in the first actionable error diagnostic.
- **SC-006**: A reproducible comparison report is generated for equivalent forward and deferred scenes at no fewer than 4 local-light-count tiers, includes at least 100 measured frames after warm-up per tier, and rejects 100% of deliberately mismatched comparison inputs.
- **SC-007**: Across all measured light-count tiers, the report demonstrates that deferred geometry work remains constant as accepted local-light count increases and records comparative timing and any observed crossover without treating the absence of a deferred performance win as a validation failure.
- **SC-008**: Windows, macOS, and Linux automated jobs all build the feature and pass deterministic headless deferred integration coverage, and the Linux job additionally passes real Vulkan offscreen execution and pixel validation through Lavapipe.
- **SC-009**: 100% of existing regression tests remain passing, and selecting the existing forward path produces no deferred passes or deferred-only resource requirements.
- **SC-010**: All covered successful, rejected-input, partial-execution-failure, and shutdown cases finish without a hang or crash and leave zero live deferred frame-owned resources after cleanup.

## Assumptions

- The primary users are engine developers comparing and extending renderer strategies, not end users selecting a graphics option through a settings screen.
- The existing forward renderer, material system, render graph, RHI execution contracts, scene collection ordering, and triangle integration provide the baseline abstractions and regression suite.
- Roadmap Phase 018 maps to SpecKit feature number 019 because the completed triangle milestone already occupies feature number 018.
- The first deferred version targets one active view and one final output per frame.
- Opaque and compatible masked materials use deferred shading; transparent materials continue through the existing forward-transparent path after composition.
- Base color, normal, metallic, roughness, emissive, ambient occlusion, and depth are required surface semantics. Alpha controls masked coverage but is not a deferred lighting input, and transparent blending remains in the forward-transparent path. Planning research will select concrete attachment count, channel packing, and precision based on compatibility, memory, and quality evidence.
- Directional lights use full-view lighting work. Point and spot lights use bounded influence work; tiled and clustered light assignment remain future optimizations.
- "Processes all lights" means every valid, view-affecting submitted light is represented in the frame plan; it does not require work for lights proven unable to affect the active view.
- The performance comparison is an engineering baseline artifact, not a universal frame-rate guarantee or a requirement that deferred rendering outperform forward rendering in this feature. It uses equivalent inputs, fixed warm-up and sampling rules, and reports per-tier workload counts, timing statistics, and observed crossover behavior.
- Deterministic headless validation is required on Windows, macOS, and Linux. Linux Lavapipe CI is the required real RHI offscreen execution and pixel-validation environment; native execution on Windows/macOS, visible-window integration, and screenshot evidence remain outside this feature's completion gates.
- Android and other mobile platforms are outside the currently supported platform set for this feature.
