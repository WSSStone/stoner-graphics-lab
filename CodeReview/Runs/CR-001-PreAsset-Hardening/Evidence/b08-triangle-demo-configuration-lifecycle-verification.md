# B08-S03 Evidence: Triangle Demo Configuration And Lifecycle Verification

Step: `B08-S03`.

Finding verified:

- `CR001-B08-F002`: Public demo Initialize can leave partial native resources
  after late startup failure.

Static verification evidence:

- `Demo/StonerDemo/Private/FStonerDemoApplication.h:68` declares the private
  initialization failure cleanup helper.
- `Demo/StonerDemo/Private/FStonerDemoApplication.cpp:130` implements the
  helper and `Demo/StonerDemo/Private/FStonerDemoApplication.cpp:138` calls
  `Shutdown`.
- `Demo/StonerDemo/Private/FStonerDemoApplication.cpp:212` routes shader
  payload validation failure through the cleanup helper.
- `Demo/StonerDemo/Private/FStonerDemoApplication.cpp:220` and
  `Demo/StonerDemo/Private/FStonerDemoApplication.cpp:225` clean up injected
  upload and pipeline startup failures.
- `Demo/StonerDemo/Private/FStonerDemoApplication.cpp:241` routes visible
  presentation resource creation failure through the cleanup helper.

Regression evidence:

- `Tests/TriangleDemoIntegrationTests.cpp:120` asserts direct swapped-shader
  `Initialize` failure leaves lifecycle `Stopped`.
- `Tests/TriangleDemoIntegrationTests.cpp:121` asserts a subsequent `Shutdown`
  succeeds idempotently.
- `CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/output/b08-triangle-lifecycle-fix-stonertest.txt`
  records `[PASS] Triangle demo rejects swapped shader stages and cleans partial initialization`.

Gate evidence:

- `CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/gate-fallback-strict.json`
  records `fallback-strict` passed at `2026-07-27T08:53:13+00:00`.

Result:

- `CR001-B08-F002` is verified.
