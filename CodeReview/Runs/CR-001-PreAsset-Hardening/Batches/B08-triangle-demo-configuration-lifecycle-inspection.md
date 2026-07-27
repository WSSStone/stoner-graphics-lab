# B08-S01: Triangle Demo Configuration And Lifecycle Inspection

## Scope

Inspected Feature 018 triangle demo configuration parsing, executable entry,
application lifecycle root, initialization order, failure ownership, shutdown,
and deterministic tests.

Production files read:

- `Demo/StonerDemo/Private/FDemoConfiguration.h`
- `Demo/StonerDemo/Private/FDemoConfiguration.cpp`
- `Demo/StonerDemo/Private/FStonerDemoApplication.h`
- `Demo/StonerDemo/Private/FStonerDemoApplication.cpp`
- `Demo/StonerDemo/Private/Main.cpp`

Supporting files read:

- `Tests/TriangleDemoIntegrationTests.cpp`
- `specs/018-triangle-demo-integration/spec.md`
- `specs/018-triangle-demo-integration/plan.md`
- `specs/018-triangle-demo-integration/data-model.md`
- `specs/018-triangle-demo-integration/contracts/triangle-demo-runtime-contract.md`
- `specs/018-triangle-demo-integration/contracts/triangle-demo-validation-contract.md`

## Requirements Checked

- Feature 018 FR-001: standalone demo executable.
- Feature 018 FR-014: interactive default and bounded validation frame budget.
- Feature 018 FR-015: normal exit, partial initialization failure, and fatal
  frame failure release resources in dependency-safe order.
- Feature 018 FR-016: diagnostics identify lifecycle stage without unstable
  native addresses.
- Feature 018 FR-019: bounded validation reports memory/resource status.
- Runtime contract: invalid configuration returns exit code 2 before native
  initialization.
- Runtime contract: first non-success outcome owns the final exit code.
- Shutdown contract: shutdown is idempotent and tolerates partially initialized
  prior steps.

## Finding

### `CR001-B08-F002`

`FStonerDemoApplication::Initialize` creates visible/native resources before
late shader and presentation-resource validation. If those late startup checks
fail, `Initialize` returns `InitializationFailed` with lifecycle `Failed` but
does not invoke shutdown or a local cleanup path. `Run` does call `Shutdown`
after `Initialize`, but `Initialize` is public and existing tests call it
directly.

Impact: focused initialization callers and tests can observe a failed demo
application that still owns partially initialized window/native resources until
an explicit `Shutdown` or destructor. This weakens the Feature 018 partial
startup cleanup contract and can hide resource lifetime defects.

Status: Accepted, S2.

## Non-Findings

- `FDemoConfiguration::Parse` rejects unknown options, missing option values,
  malformed numeric values, zero bounded frame budgets, invalid extents, empty
  shader directories, empty bounded validation output paths, and profiles with
  fewer than ten post-warmup memory samples.
- Profile defaults match the validation contract: deterministic headless uses
  4096 frames, 512 warm-up frames, 128-frame sampling, 16 MiB absolute growth,
  and 5% relative growth; native bounded modes use the larger native profile.
- `Main.cpp` maps configuration errors to exit code 2 before constructing the
  demo application.
- `Run` records startup time before initialization, dispatches by runtime mode,
  calls `Shutdown` after success or failure, preserves first-failure exit-code
  ownership, and writes bounded validation reports only after shutdown.
- `Shutdown` clears frame contexts, presentation state, triangle resources,
  native context, and window in reverse dependency order and is idempotent.
- Diagnostics stable text is covered by tests and avoids native address text.
