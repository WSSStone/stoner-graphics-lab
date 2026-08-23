# Feature 028 Affected Regression Gate

Captured on 2026-08-22 from branch
`028-production-content-acceptance` plus the current Feature 028 worktree.

## Engine Suites

The strict Release `StonerTest` executable passed the affected Feature 018-027
and Feature 028 deterministic/regression surface:

- triangle Demo, Deferred, and Forward;
- Asset Core, KTX2, Material/Shader, and static-model;
- AssetCooker production texture, target profile, clean, DDC, and determinism;
- Runtime Asset Manager;
- Renderer aggregate static-model, material-asset, and texture realization;
- Vulkan and Metal backend suites; and
- production corpus, cook graph, Demo, image-acceptance, and failure contracts.

The publication-dependent `production-content-equivalence` and
`production-content-strict-runtime` suites passed through the authoritative
regular Metal runner with their required explicit source, publication, lease,
generation, and target-profile inputs. They intentionally reject invocation
without those inputs.

## Script And Architecture Suites

The following validation-runner tests passed in the `godot` conda environment:

- Feature 028 production scripts: 65 tests;
- Metal validation runner: 17 tests;
- AssetCooker validation runner: 4 tests;
- Runtime Asset Manager validation runner: 4 tests;
- static-model validation runner: 4 tests;
- Deferred validation runner: 9 tests; and
- architecture verifier tests: 14 tests.

The architecture verifier passed Asset graphics boundaries, runtime-to-Tools
isolation, backend call isolation, private Objective-C++ ownership,
validation-only FLIP ownership, Tools-only SPIRV-Cross, public API leakage,
Feature 028 exclusions, and the 1,600-line Demo composition-root budget.

## Native Metal Regression

The post-refactor 20-cycle visible Metal run passed native backend proof,
Deferred and Forward submission, seven final GPU readbacks, 40 capture records,
20 semantic probes, terminal owner count zero, stale-handle rejection, and the
16 MiB RSS gate. Attachment and window-capture digests were identical to the
pre-refactor candidate run. Image selection stopped only at the required
`baseline-state-not-accepted` boundary.
