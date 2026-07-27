# B08-S01 Evidence: Triangle Demo Configuration And Lifecycle Inspection

Step: `B08-S01`.

Files inspected:

- `Demo/StonerDemo/Private/FDemoConfiguration.h`
- `Demo/StonerDemo/Private/FDemoConfiguration.cpp`
- `Demo/StonerDemo/Private/FStonerDemoApplication.h`
- `Demo/StonerDemo/Private/FStonerDemoApplication.cpp`
- `Demo/StonerDemo/Private/Main.cpp`
- `Tests/TriangleDemoIntegrationTests.cpp`
- `specs/018-triangle-demo-integration/spec.md`
- `specs/018-triangle-demo-integration/plan.md`
- `specs/018-triangle-demo-integration/data-model.md`
- `specs/018-triangle-demo-integration/contracts/triangle-demo-runtime-contract.md`
- `specs/018-triangle-demo-integration/contracts/triangle-demo-validation-contract.md`

Requirement evidence:

- `specs/018-triangle-demo-integration/spec.md:64` requires initialization
  failures to identify the failed stage, release previously created resources,
  and exit with a failure result.
- `specs/018-triangle-demo-integration/spec.md:130` requires partial
  initialization failure to release owned resources in dependency-safe order.
- `specs/018-triangle-demo-integration/contracts/triangle-demo-runtime-contract.md:185`
  defines shutdown as idempotent after it starts.
- `specs/018-triangle-demo-integration/contracts/triangle-demo-runtime-contract.md:198`
  requires shutdown steps to tolerate partially initialized prior steps.

Finding evidence:

- `Demo/StonerDemo/Private/FStonerDemoApplication.cpp:155` to
  `Demo/StonerDemo/Private/FStonerDemoApplication.cpp:193` can allocate window
  and/or native context resources before shader validation.
- `Demo/StonerDemo/Private/FStonerDemoApplication.cpp:198` to
  `Demo/StonerDemo/Private/FStonerDemoApplication.cpp:202` returns
  `InitializationFailed` on invalid shader payloads without cleanup.
- `Demo/StonerDemo/Private/FStonerDemoApplication.cpp:214` to
  `Demo/StonerDemo/Private/FStonerDemoApplication.cpp:221` returns
  `InitializationFailed` on visible presentation resource creation failure
  without cleanup.
- `Demo/StonerDemo/Private/FStonerDemoApplication.cpp:451` to
  `Demo/StonerDemo/Private/FStonerDemoApplication.cpp:464` shows `Run` does
  call `Shutdown`, but `Initialize` remains a public method.
- `Tests/TriangleDemoIntegrationTests.cpp:118` covers swapped shader stage
  rejection through direct `Initialize`, but does not assert cleanup or stopped
  lifecycle after the failure.

Finding recorded:

- `CR001-B08-F002`: Public demo Initialize can leave partial native resources
  after late startup failure.
- Severity: S2.
- Disposition: Accepted.

Next step:

- Fix `CR001-B08-F002` in B08-S02 by adding a cleanup path for late
  initialization failures and focused regression coverage for direct
  `Initialize` failure cleanup.
