# Feature Specification: Material & Shader System

**Feature Branch**: `014-material-shader-system`  
**Created**: 2026-07-01  
**Status**: Implemented  
**Input**: User description: "根据roadmap制定下一个大feature的spec"

## Clarifications

### Session 2026-07-01

- Q: What shader library source scope should this feature support? → A: Explicit in-memory registration of precompiled shader records only.
- Q: What kind of texture/resource references should material parameters store? → A: Abstract Renderer-level resource references resolved later by render graph or pipeline code.
- Q: How should material instance inheritance work? → A: Material instances may inherit from other material instances, with cycle detection.
- Q: How should shader permutation flags be validated? → A: Each shader record declares allowed permutation flags; unknown flags are validation errors.
- Q: What inspection output should this feature provide? → A: Deterministic human-readable text dumps for materials, instances, shader records, and diagnostics.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Define Reusable Materials (Priority: P1)

As a rendering developer, I need to define a reusable material that names its shader choice, render behavior, material domain, blend behavior, and typed parameters so that future render pipelines can bind surface behavior consistently across objects.

**Why this priority**: Materials are the primary authoring unit for the renderer. Without this, later forward rendering cannot reliably choose shaders or bind per-surface data.

**Independent Test**: Can be fully tested by creating materials for opaque surface, masked surface, translucent surface, post-process, UI, and decal usage, then inspecting that each material reports its domain, blend behavior, render state, shader reference, and parameter set deterministically.

**Acceptance Scenarios**:

1. **Given** a valid material definition with shader reference, domain, blend mode, render behavior, and parameters, **When** the material is validated, **Then** it is accepted and exposes the same values on repeated inspection.
2. **Given** a material with an unsupported combination of domain and blend behavior, **When** the material is validated, **Then** validation fails with a diagnostic that identifies the material and the conflicting choices.
3. **Given** multiple materials declared in different orders, **When** their summaries are inspected, **Then** each material remains independently identifiable and deterministic.

---

### User Story 2 - Override Parameters Per Material Instance (Priority: P2)

As a rendering developer, I need material instances to override selected parent material parameters without duplicating the full material definition so that many objects can share shader behavior while varying colors, scalar controls, vectors, and texture bindings.

**Why this priority**: Material instances are required for practical scene usage and prevent a small parameter change from creating a completely separate material definition.

**Independent Test**: Can be fully tested by creating a base material and several instances that override different subsets of parameters, then resolving the effective values for each instance.

**Acceptance Scenarios**:

1. **Given** a material instance with no overrides, **When** its effective parameters are resolved, **Then** the values match the full inherited parent chain exactly.
2. **Given** a material instance that overrides a scalar, vector, and texture parameter, **When** its effective parameters are resolved, **Then** the nearest override replaces inherited values while non-overridden values continue to inherit from earlier parents.
3. **Given** an instance override for a parameter that the root material does not define, **When** the instance is validated, **Then** validation fails with a diagnostic naming the invalid parameter.

---

### User Story 3 - Select Shader Variants Deterministically (Priority: P3)

As a rendering developer, I need shader library records and shader permutations to describe available variants and choose a deterministic shader option for a material so that material binding remains reproducible and inspectable.

**Why this priority**: Materials cannot be connected to render passes without a stable way to identify shader variants and their required parameters.

**Independent Test**: Can be fully tested by registering shader records with variant flags, requesting permutations for several material configurations, and confirming stable selection and clear rejection for missing variants.

**Acceptance Scenarios**:

1. **Given** a shader library with a matching shader variant, **When** a material requests that variant, **Then** the selected shader identity is stable across repeated queries.
2. **Given** a material that requests a shader variant that is not available, **When** shader selection is attempted, **Then** selection fails with a diagnostic that includes the material and requested variant summary.
3. **Given** two equivalent permutation requests with flags supplied in different orders, **When** the library resolves them, **Then** they produce the same selected variant identity.

---

### User Story 4 - Declare Render Graph Resource Needs (Priority: P4)

As a rendering developer, I need materials and material instances to report the texture and resource needs implied by their effective parameters so that render graph passes can declare those requirements before execution.

**Why this priority**: This connects the material system to the completed render graph foundation without implementing a full forward renderer yet.

**Independent Test**: Can be fully tested by resolving a material instance with texture parameters and verifying that its resource requirement summary can be consumed by a render graph declaration flow.

**Acceptance Scenarios**:

1. **Given** a material with texture parameters, **When** its resource needs are requested, **Then** each required texture parameter is reported with a stable name and access intent.
2. **Given** a material instance that overrides a texture parameter, **When** resource needs are requested, **Then** the overridden texture is reported instead of the parent value.
3. **Given** a material with no texture parameters, **When** resource needs are requested, **Then** the result is empty and no error is reported.

### Edge Cases

- Materials with duplicate parameter names must be rejected with diagnostics that identify the duplicate names.
- Material instances must reject overrides whose type differs from the parent parameter type.
- Material instance inheritance cycles must be rejected before effective parameters are resolved.
- Missing shader records, missing shader variants, and missing required parameters must fail before a material is considered ready for rendering.
- Unknown permutation flags must fail validation before shader variant selection.
- Domain and blend behavior combinations that are not supported by this feature must fail validation rather than being silently accepted.
- Empty material or shader libraries must produce deterministic "not found" diagnostics.
- Repeated validation and inspection of the same material set must produce stable ordering and text output.
- Text dumps must remain stable when material, instance, shader record, and diagnostic inputs are unchanged.

## Architecture & Design Constraints *(mandatory)*

- **RHI Abstraction**: The feature MUST NOT bypass the RHI layer to call Graphics APIs directly.
- **Design Patterns**: The feature MUST avoid God-classes and utilize Strategy/Composite patterns for orthogonal responsibilities.
- **Advanced Graphics**: The feature MUST consider compatibility with Ray Tracing, Meshlets, and Global Illumination pipelines.
- **Naming Conventions**: The feature's code design MUST adhere to PascalCase, UnrealEngine5-style naming conventions.
- **Cross-Platform Compatibility**: The feature MUST compile and run on all supported platforms (Windows, macOS, Linux). Platform-specific code MUST be isolated behind abstraction layers or conditional compilation guards.
- **Render Graph Compatibility**: Material resource requirements MUST be expressible in a form that render graph passes can declare before execution.
- **Precompiled Shader Scope**: The feature MUST manage shader records and variants that already exist; compiling shader source at runtime is outside this feature.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST allow developers to define materials with a shader reference, material domain, blend mode, render behavior summary, and typed parameter collection.
- **FR-002**: System MUST support material domains for surface, post-process, UI, and decal use cases.
- **FR-003**: System MUST support blend behavior categories for opaque, translucent, additive, and masked use cases.
- **FR-004**: System MUST support typed material parameters for scalar values, vector values, color-like values, and abstract Renderer-level texture/resource references.
- **FR-005**: System MUST reject duplicate parameter names within a material definition.
- **FR-006**: System MUST validate that each material's domain, blend mode, shader reference, and required parameters form a supported combination.
- **FR-007**: System MUST allow material instances to reference either a base material or another material instance and override selected parameters.
- **FR-008**: System MUST resolve effective material instance parameters by walking the parent chain and applying the nearest override for each parameter.
- **FR-009**: System MUST reject material instance overrides for parameters not defined by the root material.
- **FR-010**: System MUST reject material instance overrides whose value type does not match the parent parameter type.
- **FR-011**: System MUST maintain a shader library that records explicitly registered precompiled shader identities, allowed permutation flags, available variants, required parameter expectations, and human-readable diagnostics.
- **FR-012**: System MUST represent shader permutations as deterministic feature-flag sets so equivalent requests resolve to the same identity regardless of input order.
- **FR-013**: System MUST select a shader variant for a material when all requested permutation flags and shader requirements are available.
- **FR-014**: System MUST reject shader selection when the requested shader, requested permutation, or requested permutation flag is unavailable, including a diagnostic that identifies the requested shader and permutation.
- **FR-015**: System MUST expose material and material-instance resource requirements in a stable form suitable for render graph pass declaration.
- **FR-016**: System MUST NOT require material parameters to hold live rendering resources or graph-local handles; resource references MUST remain stable until resolved by later render graph or pipeline code.
- **FR-017**: System MUST detect and reject material instance inheritance cycles with diagnostics that identify the cycle participants.
- **FR-018**: System MUST validate requested permutation flags against the target shader record before selecting a shader variant.
- **FR-019**: System MUST provide deterministic human-readable text dumps for materials, instances, shader library records, permutation selections, validation failures, diagnostics, and resource requirements.
- **FR-020**: System MUST keep inspection dump ordering stable for unchanged material, instance, shader record, and diagnostic inputs.
- **FR-021**: System MUST support clearing or invalidating material and shader records so stale objects cannot be accidentally treated as valid.
- **FR-022**: System MUST include tests for valid material definitions, invalid combinations, instance inheritance, inheritance cycles, override failures, shader variant selection, missing variants, unknown permutation flags, render graph resource declarations, deterministic text dumps, and reset/invalidation behavior.

### Key Entities

- **Material**: A reusable rendering description containing shader identity, domain, blend behavior, render behavior summary, and default parameters.
- **Material Instance**: A child material description that references a parent material or parent material instance and overrides selected parameter values.
- **Material Parameter Set**: A named collection of typed parameter values used by materials and instances; texture/resource values are abstract Renderer-level references, not live resources.
- **Shader Library**: A registry of shader records, allowed permutation flags, variants, parameter expectations, and availability status.
- **Shader Permutation**: A deterministic feature-flag set used to select a shader variant; flags are valid only when declared by the target shader record.
- **Material Resource Requirement**: A stable description of texture or resource needs that can be declared by render graph passes.
- **Validation Diagnostic**: A structured message describing why a material, instance, shader request, or resource requirement is invalid.
- **Inspection Dump**: A deterministic human-readable text summary of material, instance, shader record, permutation, resource requirement, and diagnostic state.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Developers can define and validate at least 8 representative materials covering all supported domains and blend behaviors in the project test workflow.
- **SC-002**: At least 95% of invalid material, instance, or shader selection cases in the test suite produce diagnostics that identify the failing material or shader request.
- **SC-003**: Equivalent shader permutation requests produce identical selected variant identities across at least 20 repeated resolutions.
- **SC-004**: A representative material instance set with at least 5 parent materials, 10 instances, 4 parameter types, and 3 shader permutations can be validated, inspected, and resource-summarized in under 60 seconds through the project verification workflow.
- **SC-005**: Material resource requirement summaries can be used to declare a render graph pass in a headless test without requiring visible presentation.
- **SC-006**: Repeated text dump inspection of the same material library produces byte-identical output across at least 20 runs.

## Assumptions

- The next roadmap feature is Phase 013: Renderer Material & Shader System, following the completed render graph foundation.
- Speckit numbering is independent from roadmap phase numbering; this feature uses `014-material-shader-system` because `013-render-graph-foundation` already exists in `specs/`.
- Shader source compilation, live shader reloading, and local file scanning/loading are outside this feature; shader records are explicitly registered in memory and represent pre-existing compiled shader variants.
- PBR-specific material models are reserved for the forward rendering phase; this feature focuses on the generic material, instance, parameter, shader selection, and render graph declaration contracts.
- Visual material editing tools are outside this feature.
- Tests may use headless/mock rendering resources and do not require a physical graphics device or visible window.
- Resource reference resolution into live resources or graph-local handles is deferred to later render graph or pipeline integration.

## Implementation Status

Implemented on 2026-07-01 on branch `014-material-shader-system`.

- Added Renderer public contracts for `FMaterial`, `FMaterialInstance`, `FMaterialParameterSet`, `FMaterialResourceRequirement`, `FShaderLibrary`, `FShaderPermutation`, `FMaterialShaderBinding`, and material diagnostics.
- Added private implementations for deterministic parameter ordering, material validation, instance inheritance and cycle detection, shader record registration, per-record permutation validation, variant lookup, required parameter validation, abstract resource requirement extraction, invalidation, and text dumps.
- Added `Tests/RendererMaterialShaderTests.cpp` with coverage for representative valid materials, invalid domain/blend combinations, duplicate parameters, instance override precedence, unknown/type-mismatched overrides, inheritance cycles, invalidated parents, shader selection success and failures, resource requirement extraction, live-resource rejection, render graph declaration smoke flow, repeated deterministic resolution, and the 60-second representative scenario.
- Verification passed: `conda run -n godot scons`; `Build/Mac/Debug/Tests/StonerTest`; Renderer material/backend boundary scan with `rg -n "Vulkan|Metal|DX12|DirectX|OpenGL|GLES|WebGL|Vk[A-Z]|ID3D|MTL" Source/Renderer/Public/Renderer Source/Renderer/Private` returned no matches.
