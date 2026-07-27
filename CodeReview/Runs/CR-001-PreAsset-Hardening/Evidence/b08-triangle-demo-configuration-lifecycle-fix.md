# B08-S02 Evidence: Triangle Demo Configuration And Lifecycle Fix

Step: `B08-S02`.

Finding fixed:

- `CR001-B08-F002`: Public demo Initialize can leave partial native resources
  after late startup failure.

Resolution:

- Late initialization failures after potential native/window allocation now
  clean up through `Shutdown`.
- Shader validation failure and visible presentation resource failure use a
  common initialization failure path that records the primary stage before
  cleanup.
- Upload and pipeline initialization failure injection paths call `Shutdown`
  before returning.
- Public direct `Initialize` failure now leaves lifecycle `Stopped`, with
  subsequent shutdown remaining idempotent.

Regression evidence:

- `[PASS] Triangle demo rejects swapped shader stages and cleans partial initialization`
- `[PASS] Triangle demo normal shutdown remains idempotent after initialized state`
- `[PASS] Triangle demo injected failures preserve first-stage exit ownership`

Gate evidence:

- `git diff --check -- Demo/StonerDemo/Private/FStonerDemoApplication.h Demo/StonerDemo/Private/FStonerDemoApplication.cpp Tests/TriangleDemoIntegrationTests.cpp`: passed.
- `scons config=debug`: passed.
- `Build/Mac/Debug/Tests/StonerTest`: passed, with output captured in
  `CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/output/b08-triangle-lifecycle-fix-stonertest.txt`.
- `CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/gate-fallback-strict.json`
  records `fallback-strict` passed at `2026-07-27T08:49:42+00:00`.

Code commit:

- `3cb5420`: `fix(demo): clean partial initialization failures`
