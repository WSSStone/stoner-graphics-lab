# Research: Forward Rendering Pipeline

## Decision: First forward renderer stops at frame plans and render graph-compatible declarations

**Rationale**: The clarified scope requires validated frame plans and pass/resource declarations, but not real GPU execution or presentation. This keeps the feature aligned with current Renderer foundations and avoids depending on the later window/input and triangle demo phases.

**Alternatives considered**:

- Debug dumps only: rejected because the roadmap and spec require render graph integration, not just inspection.
- End-to-end RHI command submission: rejected because it would expand this phase into backend execution behavior.
- Window or swapchain presentation: rejected because presentation belongs to later Application/demo integration work.

## Decision: Forward pass order is depth, opaque, sky/background, transparent

**Rationale**: This order satisfies the spec's visible-frame composition needs while remaining simple enough for deterministic pass declaration. Depth first supports opaque ordering and future optimization, opaque lighting handles the main material path, sky/background fills unoccupied color, and transparent work composes last.

**Alternatives considered**:

- Sky before opaque: rejected because it complicates the guarantee that background does not overwrite opaque geometry.
- Transparent before sky: rejected because the sky/background should be established before transparent blending-style composition.
- Single combined pass: rejected because it hides pass ordering and resource requirements from render graph validation.

## Decision: Full PBR-style material inputs are validated at the forward planning layer

**Rationale**: The user selected the broader PBR-style surface set: base color, metallic, roughness, normal-related data, occlusion, emissive, alpha, and extension slots. Validating this set during frame planning ensures missing material inputs are caught before graph declaration output is treated as render-ready.

**Alternatives considered**:

- Minimal base color/metallic/roughness only: rejected because it would under-specify the selected PBR acceptance target.
- Push all validation to shader binding: rejected because forward rendering needs to classify surfaces and diagnostics at frame-preparation time.

## Decision: Point light selection is configurable, defaults to 4, and uses deterministic influence ordering

**Rationale**: Defaulting to 4 point lights keeps the first forward path compact, while configurability allows tests to exercise light limits. Influence ordering by distance/effectiveness selects the most relevant lights and produces deterministic accepted/rejected records.

**Alternatives considered**:

- Fixed limit of 8: rejected because the user requested 4 as the default and configurable behavior.
- Submission order: rejected because it does not reflect lighting relevance and is fragile across caller ordering.
- Accept all lights: rejected because it weakens validation of the configured limit and light packing behavior.

## Decision: Transparent draw order uses camera-space depth descending, material id, object id

**Rationale**: Camera-space depth gives a clear back-to-front rule for the active view. Stable material and object identifiers make equal-depth cases deterministic without introducing Application-layer scene ownership or user-specified priority in this phase.

**Alternatives considered**:

- World-space distance: rejected because camera-space depth better matches view-dependent composition.
- User-provided transparent priority: rejected because it introduces an extra policy not required for the first forward path.
- Preserve submitted order for ties: rejected because it makes dumps depend on caller iteration order.

## Decision: No accepted lights produce a constant ambient-only fallback with diagnostics

**Rationale**: The user selected ambient-only fallback. This keeps valid geometry testable in no-light scenes, avoids treating no-light scenes as hard failures, and still reports the fallback so authors know the lighting path was degraded.

**Alternatives considered**:

- Reject geometry preparation: rejected because it would make no-light scenes unusable for renderer smoke tests.
- Fully unlit fallback: rejected because ambient-only is more informative and closer to forward lighting behavior.
- Sky/environment lighting fallback: rejected because it would blur background contribution with material lighting.

## Decision: Mesh draw commands are reusable Renderer-level descriptions

**Rationale**: The roadmap calls for cached draw commands, but the clarified phase does not require backend command submission. Reusable Renderer-level draw descriptions provide stable identity, sorting, material binding, pass participation, and future command-recording inputs without storing backend command buffers.

**Alternatives considered**:

- Backend command records: rejected because that would leak execution concerns into the Renderer public contract.
- Per-frame-only draw objects with no identity: rejected because stable draw identity is needed for deterministic dumps and reuse tests.

## Decision: Diagnostics and debug dumps are first-class outputs

**Rationale**: The feature is primarily headless and test-driven, so deterministic diagnostics and human-readable dumps are the observable proof of behavior. Byte-stable dumps support regression tests for pass order, draw order, light acceptance, resource requirements, and fallback decisions.

**Alternatives considered**:

- Visual validation only: rejected because this phase does not require real presentation.
- Unstructured log text only: rejected because tests need stable diagnostic categories and subject identities.
