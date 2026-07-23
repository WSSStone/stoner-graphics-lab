# Contract: Deferred Renderer

## Purpose

Define the backend-neutral public behavior of Feature 019: strategy selection, frame inputs, surface semantics, draw/material acceptance, light handling, pass order, transparent handoff, diagnostics, and deterministic frame reports. This contract contains no Vulkan types or native handles.

## Strategy Contract

- `FForwardRenderer` remains the established default and does not allocate or declare deferred-only resources.
- `FDeferredRenderer` is selected explicitly by a caller that already owns scene collection and the active view/output.
- Both strategies reuse existing material/instance data and stable draw/light identities where semantics overlap.
- Selecting one strategy does not mutate the other strategy's configuration or cached plan state.
- Renderer strategy selection is outside scene/ECS traversal and outside backend-device ownership.

## Frame Input Contract

`FDeferredFrameInputs` contains:

- one valid active view with finite matrices, camera position, viewport, and positive extent;
- one stable final output target compatible with the active extent;
- ordered opaque/masked and transparent draw candidates;
- directional, point, and spot light candidates with stable identities;
- finite ambient/background contribution;
- one stable frame identity.

Preparation rejects the frame before graph declaration when view, output, extent, matrix/depth convention, or surface-layout compatibility is invalid.

## Surface Semantic Contract

The required semantic values are:

| Semantic | Range/space |
|----------|-------------|
| Base color | finite linear RGB, each channel in `[0, 1]` before lighting |
| View normal | finite normalized view-space vector |
| Metallic | finite scalar in `[0, 1]` |
| Roughness | finite scalar in `[0, 1]` |
| Emissive | finite non-negative linear RGB |
| Ambient occlusion | finite scalar in `[0, 1]` |
| Depth | finite normalized depth compatible with the active inverse projection |

Masked alpha is finite in `[0, 1]` and controls coverage against the material cutoff. It is not consumed as a lighting semantic. Transparent blend alpha remains in the existing forward-transparent path.

## Material and Draw Acceptance

- Opaque and compatible masked surface materials are accepted when all required semantic inputs and deferred shader bindings are available.
- Transparent materials are never accepted by the surface-data stage; when transparent handoff is enabled, they are prepared by the existing forward-transparent ordering after deferred composition.
- Unsupported material domain, blend mode, shader permutation, extension requirement, missing semantic, non-finite value, invalid transform, or invalid bounds produces one stable rejected-draw record.
- Unsupported extension slots do not silently change meaning between forward and deferred; the material is rejected with the unsupported slot identified.
- Equivalent accepted draws have deterministic order with stable object/entity identity as the final tie-breaker.

## Light Acceptance and Ordering

Directional light requirements:

- finite non-negative color and intensity;
- finite normalized non-degenerate direction;
- one full-view contribution per accepted light.

Point light requirements:

- directional common fields where applicable;
- finite position and positive finite range;
- spherical influence bounds.

Spot light requirements:

- finite position, positive finite range, and normalized non-degenerate direction;
- `0 <= InnerConeAngle <= OuterConeAngle < 90 degrees`;
- conical influence bounds.

Policy:

- There is no deferred local-light count cap.
- Invalid lights are rejected; zero-contribution or outside-view lights are omitted with a stable reason.
- Accepted local lights classify camera-outside, camera-inside, or near-plane-intersecting volume mode.
- Ordering is directional, point, spot; then deterministic influence key; then stable entity identity.
- Every valid view-affecting submitted light appears exactly once in accepted work.

## Canonical Pass Contract

The canonical logical order is:

1. `SurfaceData`
2. `DirectionalLighting` when accepted directional lights exist
3. `PointLightVolumes` when accepted point lights exist
4. `SpotLightVolumes` when accepted spot lights exist
5. `Composition`
6. `ForwardTransparency` when accepted transparent draws exist
7. `ValidationReadback` only when requested

Rules:

- Surface geometry visibility/depth work occurs once and never once per light.
- Empty lighting and transparent stages may be omitted deterministically.
- Composition produces exactly one final output even with no geometry or no lights.
- Ambient-only and emissive-only frames are valid.
- No stage reads a resource before an imported declaration or prior write.
- Graph pass/resource/access ordering is inspectable before execution.

## Frame Plan Result

Successful preparation returns a valid `FDeferredFramePlan` containing:

- layout compatibility identity;
- accepted/rejected draw records;
- accepted/culled/rejected lights by type;
- ordered pass records and resource semantics;
- optional transparent handoff;
- normalized input fingerprint;
- ordered diagnostics and deterministic debug dump.

Failed preparation returns an invalid plan, first actionable error, rejected subject identity, and no executable graph declaration.

## Determinism Contract

For equivalent normalized inputs, 20 repeated preparations must produce identical:

- layout identity;
- pass/resource/access order;
- accepted/rejected draw order and reasons;
- accepted/culled/rejected light order and reasons;
- transparent handoff order;
- result categories, diagnostics, fingerprints, and debug dump bytes.

Allocator addresses, unordered-container iteration order, native handles, wall-clock timing, and platform path separators cannot influence these fields.

## Human-Readable Report

The normalized deferred frame report includes:

- feature and frame identity;
- result and validation state;
- surface semantic/format summary;
- canonical pass order and culling state;
- accepted/rejected draw counts;
- accepted/culled/rejected light counts by type and volume mode;
- transient/imported resource summaries;
- transparent handoff count;
- final composition state;
- stable diagnostics.

The report contains no native addresses or backend object handles.

