# Contract: Forward Rendering Pipeline

## Scope

This contract describes the Renderer-layer public behavior for preparing forward frame plans, validating view/light/material/draw inputs, emitting render graph-compatible pass and resource declarations, selecting point lights, sorting transparent draws, reporting fallback behavior, and producing deterministic debug dumps. It is a C++ library contract, not a network, file format, command-line, backend execution, or presentation contract.

## Public Types

### `FForwardRenderer`

Coordinates forward frame preparation.

Required behavior:

- Accept a configuration, view data, output target summary, draw inputs, light inputs, and optional environment background.
- Prepare deterministic `FForwardFramePlan` outputs.
- Produce render graph-compatible pass/resource declaration summaries without requiring real graph execution.
- Reject invalid view or output data before pass declaration.
- Preserve deterministic diagnostics and dumps across repeated equivalent preparation.

### `FForwardFramePlan`

Represents one prepared forward rendering workload.

Required behavior:

- Expose validated view data, output target summary, pass order, accepted/rejected draws, accepted/rejected lights, render graph-compatible declarations, diagnostics, and debug dump text.
- Preserve the stage order: depth, opaque, sky/background, transparent.
- Represent no-geometry frames as valid clear/background plans.
- Represent geometry with no accepted lights as valid ambient-only fallback plans with diagnostics.

### `FForwardViewData`

Represents camera and viewport data.

Required behavior:

- Validate camera transform/view-projection information, viewport dimensions, and camera position.
- Reject non-finite or missing view data.
- Provide the view basis needed for point light influence scoring and transparent camera-space depth sorting.

### `FForwardOutputTarget`

Represents abstract color/depth frame outputs.

Required behavior:

- Store stable output identifiers, format intent, and extent.
- Reject missing color target or invalid extents.
- Avoid backend-specific resources, swapchain handles, platform windows, or graph-local handles.

### `FForwardLightSet`

Represents submitted and selected frame lights.

Required behavior:

- Accept at most one primary directional light.
- Validate directional and point light fields.
- Apply a configurable point light limit with default value 4.
- Sort point lights by deterministic influence, including distance and effectiveness, then accept the front N lights.
- Report accepted and rejected light counts.
- Emit exactly one ambient-only fallback diagnostic when valid geometry has no accepted lights.

### `FMeshDrawCommand`

Represents a reusable Renderer-level draw description.

Required behavior:

- Store stable object id, mesh id, material binding summary, pass participation, sort keys, and material resource requirements.
- Reject draws with invalid material bindings or incompatible material domain/blend behavior.
- Preserve stable identity and ordering across repeated equivalent frame preparation.
- Avoid backend command buffers or graphics API command records.

### `FForwardRenderGraphDeclaration`

Represents render graph-compatible declaration output.

Required behavior:

- Expose pass summaries for depth, opaque, sky/background, and transparent work as applicable.
- Expose resource declaration summaries for output, depth, material, and environment requirements.
- Expose access declaration summaries matching pass/resource relationships.
- Be inspectable and testable without executing a render graph or recording backend commands.

### `FForwardDiagnostics`

Collects validation, selection, sorting, fallback, declaration, and inspection diagnostics.

Required behavior:

- Include stable diagnostic codes, severity, category, subject name, and human-readable message.
- Identify invalid view data, output targets, material bindings, PBR surface inputs, excessive lights, incompatible transparent draws, and fallback behavior.
- Preserve deterministic ordering.

## Frame Preparation Contract

Input:

- Forward renderer configuration.
- View data.
- Output target summary.
- Opaque and transparent draw candidates.
- Material/shader binding summaries and resource requirements.
- Directional and point light candidates.
- Optional sky/background input.

Output:

- Success with a prepared frame plan, render graph-compatible declarations, diagnostics, and deterministic debug dump.
- Failure with deterministic diagnostics and no render-ready frame plan when required view or output data is invalid.

Required validation:

- Detect invalid view data.
- Detect invalid or missing output target summaries.
- Detect material bindings that cannot participate in requested forward passes.
- Detect incomplete full PBR-style surface inputs.
- Detect multiple primary directional light candidates.
- Detect invalid point light ranges, positions, intensities, or identifiers.
- Detect incompatible transparent material use.
- Preserve stable ordering for accepted/rejected records.

## Pass Declaration Contract

Required behavior:

- Depth declaration is present before opaque declaration when opaque draws exist.
- Opaque declaration includes accepted opaque draw summaries and material resource requirements.
- Sky/background declaration is present when configured or when clear/background behavior is required.
- Transparent declaration follows opaque and sky/background declarations when transparent draws exist.
- Declarations must not require real GPU execution, backend command buffers, or window presentation.

## Light Selection Contract

Preconditions:

- View data is valid.
- Point light candidates have valid positions, ranges, and intensity values.
- Point light limit is configured or defaults to 4.

Required behavior:

- Compute deterministic influence scores using distance and effectiveness.
- Accept the front N point lights after influence sorting.
- Apply stable tie-breakers for equal influence scores.
- Report accepted and rejected lights in deterministic order.
- Allow zero accepted lights only with ambient-only fallback when geometry remains valid.

## Transparent Sorting Contract

Preconditions:

- View data is valid.
- Transparent draw candidates have valid material bindings and stable object/material identities.

Required behavior:

- Sort transparent draws by camera-space depth from farthest to nearest.
- Break equal-depth ties by stable material id, then stable object id.
- Produce identical ordering and dumps for repeated equivalent inputs.
- Do not rely on caller submission order for final ties.

## PBR Surface Input Contract

Required behavior:

- Validate base color, metallic, roughness, normal-related input, occlusion, emissive, alpha, and extension slots for forward-compatible materials.
- Report missing or type-incompatible surface inputs with deterministic diagnostics.
- Preserve extension slot names in stable order.
- Keep material resource references abstract and compatible with render graph declaration.

## Debug Dump Contract

Required behavior:

- Produce deterministic human-readable text output.
- Include frame summary, pass order, view summary, accepted/rejected draws, light selection, resource declarations, fallback decisions, and diagnostics.
- Produce byte-identical output across at least 20 repeated preparations of unchanged inputs.
- Avoid pointer values, backend handles, platform handles, and unordered iteration artifacts.

## Boundary Rules

- Public Renderer forward contracts must not include Vulkan, Metal, DX12, DirectX, OpenGL, GLES, WebGL, platform-window, swapchain, or backend-specific concepts.
- Forward frame plans must not own live RHI resources, backend command buffers, or render graph local handles.
- Real GPU execution, swapchain/window presentation, shadow mapping, post-processing, deferred rendering, Application-layer scene ownership, runtime shader compilation, shader source parsing, and local shader file scanning/loading are out of scope.
