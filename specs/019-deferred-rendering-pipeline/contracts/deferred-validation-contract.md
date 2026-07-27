# Contract: Deferred Validation

## Required Validation Matrix

| Platform | Build | Deterministic planner/graph/executor tests | Native Vulkan offscreen/readback | Visible screenshot |
|----------|-------|--------------------------------------------|----------------------------------|--------------------|
| Windows | Required in CI | Required in CI | Optional | Not required |
| macOS | Required in CI | Required in CI | Optional | Not required |
| Linux | Required in CI | Required in CI | Required in CI through Lavapipe | Not required |

Android and other mobile targets do not participate in Feature 019.

## Deterministic Test Requirements

Tests cover:

- valid empty, ambient-only, emissive-only, and representative lit frames;
- surface-layout semantics, format/extent/sample compatibility, standard-Z/reversed-Z far clears, and matching depth comparisons;
- opaque/masked acceptance and transparent forward handoff;
- invalid view, output, draw, material, shader binding, and surface input cases;
- valid/invalid directional, point, and spot lights;
- outside-view, boundary-touching, fully visible, camera-enclosing, and near-plane-intersecting local volumes;
- no local-light count cap and stable entity-identity tie-breaking;
- canonical pass/resource/access order and culling;
- complete and incomplete RHI execution bindings;
- descriptor, index, clear-value, transition, draw, and readback command order;
- injected graph, record, submit, readback, comparison, and cleanup failures;
- byte-stable normalized plans/diagnostics/reports across 20 repeated runs;
- final zero frame-owned resource assertions.

Existing project tests remain in the same run and must pass.

## Native Reference Scene

The Linux native scene uses a fixed small offscreen extent and deterministic camera, geometry, materials, and lights. The same scene executes once with standard-Z and once with reversed-Z, using convention-matched projection, inverse view-projection, far clear, depth comparison, and probe decode. It includes:

- background pixels;
- opaque surfaces spanning base-color, normal, metallic, roughness, emissive, and ambient-occlusion values;
- one masked coverage boundary;
- one directional light;
- point lights covering visible, outside-view, and camera-inside cases;
- spot lights covering visible, outside-cone, and near-plane-intersecting cases;
- ambient-only and emissive-only samples.

At least 18 unique named probes per depth convention must be decoded across intermediate surface targets and final LDR output, including the local-light probes `point-visible`, `point-outside-view`, `point-camera-inside`, `spot-visible`, `spot-outside-cone`, and `spot-near-plane`. The report lists each convention identity and every probe name, semantic, coordinate, expected value, observed value, error measure, threshold, and pass/fail state.

## Semantic Readback Thresholds

| Semantic | Pass threshold |
|----------|----------------|
| Final LDR RGB(A where asserted) | absolute error `<= 2/255` per asserted channel |
| Normalized depth | absolute error `<= 1e-4` |
| Decoded normalized world normal | dot(expected, observed) `>= 0.999` |
| Metallic | absolute error `<= 1e-3` |
| Roughness | absolute error `<= 1e-3` |
| Ambient occlusion in 8-bit UNorm | absolute error `<= 2e-3` |

Any non-finite expected/observed value, out-of-range coordinate, incompatible decode format, missing probe, duplicate probe identity, or threshold violation fails native validation.

## Forward/Deferred Comparison Profiles

Required local-light tiers: `0`, `16`, `64`, `256`.

Each tier:

1. Builds one normalized scene/view/material/light fingerprint.
2. Uses the same accepted opaque draws and equivalent supported lights for both strategies.
3. Runs a documented warm-up excluded from measurements.
4. Measures at least 100 frames per strategy.
5. Reports median and p95 preparation/execution duration.
6. Reports pass, geometry draw, local-light draw, accepted-light, and culled-light counts.
7. Verifies deferred surface geometry work does not increase with light count.
8. Classifies observed crossover or explicitly records that none was observed.

Timing values are evidence, not pass/fail thresholds. Mismatched fingerprints, accepted input counts, incomplete samples, non-finite/negative timings, or fewer than 100 measured frames invalidate the report and fail validation.

## Normalized Artifacts

Linux CI uploads:

```text
Validation/019/Linux/deferred-readback-report.txt
Validation/019/Linux/renderer-comparison-report.txt
```

The readback report includes:

- feature/commit/run identity;
- runtime mode and normalized adapter identity;
- software-device proof;
- surface layout/extent;
- standard-Z/reversed-Z convention identity, far clear, and comparison operation;
- pass/draw/light counts;
- every named probe and threshold result;
- primary diagnostics;
- peak and final live deferred object counts;
- final validation result.

The comparison report includes all profile fields defined above. Neither report contains native addresses, raw handles, machine-local absolute paths, or unstable timestamps in deterministic comparison fields.

## CI Pass/Fail Rules

- Windows, macOS, and Linux builds pass.
- All three jobs run the complete deterministic regression suite.
- Linux resolves and explicitly selects Lavapipe, then passes native offscreen deferred execution and semantic readback.
- At least 18 native probes per convention pass every clarified tolerance, including the six required local-light probe names.
- The four-tier comparison report is complete and input-equivalent; deferred need not be faster.
- Existing Core, RHI, Vulkan, Renderer, Application, scene/ECS, and triangle demo regressions remain passing.
- No required native run becomes success because a dependency/runtime is unavailable.
- Failed reports are uploaded when possible.
- Final deferred frame-owned live counts are zero.

## Completion Record

Feature completion references:

- CI run and commit identity;
- all three deterministic job outcomes;
- Linux Lavapipe native readback artifact and digest;
- renderer comparison artifact and digest;
- test/probe/profile counts;
- final live-resource result;
- any explicitly accepted temporary gap and follow-up task (none planned).
