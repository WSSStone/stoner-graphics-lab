# Feature 028 Affected Regression Gate

Captured through 2026-08-25 from final hosted evidence head
`b7c89d6a5bbf92775db3b9f05af4d57e9bd5dc34`, with direct code authority
`ffdc1a73994c8fb47971d8033628aba831af669d`, on branch
`028-production-content-acceptance`.

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

Hosted run `32818269789` passed Windows Vulkan, Linux Lavapipe Vulkan, macOS
arm64 Metal, macOS x86_64 Metal, ASan/UBSan, and TSan. Its four independent
consumer jobs downloaded and revalidated every immutable regular artifact.

## Script And Architecture Suites

The following validation-runner tests passed in the `godot` conda environment:

- Feature 028 production runner: 34 tests;
- Feature 028 workflow contract: 5 tests;
- Feature 028 comparison/corpus/privacy/report/acquisition scripts: 34 tests;
- Metal validation runner: 19 tests;
- AssetCooker validation runner: 6 tests;
- Runtime Asset Manager validation runner: 4 tests;
- static-model validation runner: 4 tests;
- Deferred validation runner: 9 tests; and
- architecture verifier tests: 14 tests.

The architecture verifier passed Asset graphics boundaries, runtime-to-Tools
isolation, backend call isolation, private Objective-C++ ownership,
validation-only FLIP ownership, Tools-only SPIRV-Cross, public API leakage,
Feature 028 exclusions, and the 1,600-line Demo composition-root budget.

## Native Metal And Vulkan Regression

The clean predecessor Metal 1,000-cycle hardware run passed both v2 workloads with
native backend proof, Deferred and Forward submission, seven final GPU
readbacks, 2,000 capture records per workload, 20 semantic probes, accepted
exact baselines, FLIP 0, terminal owner count zero, stale-handle rejection, and
the 16 MiB RSS gate. Lantern post-warm-up growth was zero; Sponza growth was
3,620,864 bytes.

Final-binary 20-frame visible Vulkan gates passed Lantern v2 and Sponza v2 with
40 capture records, seven final GPU readbacks, 20 semantic probes, accepted
exact MoltenVK baselines, FLIP 0, zero terminal owners, and stale-handle
rejection. The full predecessor-revision M4 Pro Vulkan hardware profile passed
1,000 cycles per workload; final physical CI owns the same-revision 1,000-cycle
reconfirmation. The final revision adds one unmeasured Deferred/Forward native
prime that submits, waits, releases every owner, rejects stale handles, and
reinitializes before the unchanged declared lifecycle sequence. Strict
Debug/Release, complete Vulkan/Metal regressions, runner 34/34, and final-binary
20-frame visible gates passed after that change without altering formal capture,
readback, or cycle counts.
