# Findings

## CR001-B01-F001: macOS builds emit 33 Vulkan aggregate-initialization warnings

- Severity: S2
- Status: Verified
- Requirement: CR-001 completion criterion: no unexplained compiler warnings before enabling -Werror or /WX
- Location: `Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp:240`
- Impact: Warning noise prevents warning-as-error enforcement and can hide newly introduced initialization defects.
- Evidence: Both clean Debug and Release gates pass but end with 33 -Wmissing-field-initializers warnings; see Evidence/gate-debug.json and gate-release.json.
- Resolution: Replaced partial Vulkan aggregate initialization with zero-initializing typed helpers and cleared project-owned cross-platform warnings; strict local Debug and Release builds pass.
- Verification: Verified at 8a328387: Windows, macOS, and Linux strict Debug jobs passed; all three strict Release jobs passed; no project compiler warnings escaped the error policy.
- Commit: `54e2599`

## CR001-B08-F001: Feature 019 native deferred validation is intermittent on local MoltenVK

- Severity: S2
- Status: Accepted
- Requirement: 019-FR-025 and 019-SC-002 semantic readback tolerances and deterministic validation evidence
- Location: `Tests/DeferredNativeIntegrationTests.cpp:104`
- Impact: A nondeterministic native gate can conceal a synchronization or initialization defect and makes local validation evidence unreliable.
- Evidence: The first post-build Debug test run failed native submission, probe validity, and report pass checks; two immediate repeats passed with 24 valid probes and zero live objects.
- Resolution: pending
- Verification: pending
- Commit: `pending`

## CR001-B01-F002: Build and CI lack Release, sanitizer, and warning-as-error gates

- Severity: S2
- Status: Verified
- Requirement: CR-001 completion requires Debug, Release, ASan/UBSan, clean warnings, and three-platform CI evidence
- Location: `site_scons/BuildConfig.py:12`
- Impact: Undefined behavior and Release-only defects can merge undetected, and the planned clean-warning policy is not mechanically enforced.
- Evidence: BuildConfig exposes only debug/release flags without sanitizer or strict-warning options; CI line 51 runs only default Debug.
- Resolution: Added validated strict and ASan/UBSan build controls, allow-listed CR gates, three-platform strict Release CI, and Linux sanitizer CI; all local profiles pass.
- Verification: Verified at 8a328387: three-platform strict Debug and Release builds, Linux ASan+UBSan tests, and mandatory Linux Lavapipe native validation all passed in CI run 30186757383.
- Commit: `54e2599`

## CR001-B01-F003: CI runs duplicate three-platform matrices for pull-request branch pushes

- Severity: S3
- Status: Verified
- Requirement: CR-001 Git and validation protocol requires meaningful batch-boundary evidence without redundant jobs
- Location: `.github/workflows/ci.yml:3`
- Impact: Every review push doubles hosted CI time, delays feedback, and makes check evidence harder to associate with the current head.
- Evidence: The workflow subscribes unconditionally to both push and pull_request; PR #4 produced two Linux, two macOS, and two Windows jobs for the same update.
- Resolution: Restricted branch push validation to master, retained pull-request validation, and ignored pure CodeReview/Runs state changes while preserving tool and engine CI.
- Verification: Verified at 8a328387: the head produced exactly one pull_request CI run and one pull_request Code Review Tools run, with no push-triggered duplicate matrix.
- Commit: `54e2599`

## CR001-B09-F001: Tests bypass Public/Private boundaries through global private include paths

- Severity: S2
- Status: Accepted
- Requirement: Constitution Principle II and roadmap public/private API boundary discipline
- Location: `Tests/SConscript:25`
- Impact: Tests can normalize dependencies on implementation details, weaken compile-time boundary enforcement, and make later refactors unnecessarily broad.
- Evidence: The single test target globally exposes Demo/StonerDemo/Private and Source/Application/Private; current tests directly include FWindowDriver.h and three demo-private headers.
- Resolution: pending
- Verification: pending
- Commit: `pending`

## CR001-B02-F001: FName public states violate text/hash identity invariants

- Severity: S2
- Status: Verified
- Requirement: 003-FR-003 and Engine Name validation rules require text-derived identity and correct equality
- Location: `Source/Core/Public/Core/FName.h:34`
- Impact: Valid constructible or moved-from FName values can violate equality consistency, making identifier comparisons and future hashed lookup behavior depend on stale or caller-forged hash state.
- Evidence: A focused C++20 probe prints '1 0 0': a moved-from FName reports empty but is unequal to the default empty name, and two public synthetic names with identical text but different supplied hashes compare unequal.
- Resolution: Added explicit FName move construction and assignment that re-derive hashes for destination and moved-from source text; replaced the escaping forged-hash factory with a non-escaping common-hash comparator.
- Verification: Verified at remote head 2a97649: Windows/macOS/Linux strict Debug and deterministic tests, three-platform strict Release, Linux ASan+UBSan, Linux Lavapipe native validation, and local Core 60/0 all pass; focused move/collision probe is 1 1 1 and no forged FName factory remains.
- Commit: `0bfcdec`

## CR001-B02-F002: Core foundation tests do not validate moved-from value invariants

- Severity: S2
- Status: Verified
- Requirement: 003-FR-008 and the feature edge cases require meaningful move and boundary verification
- Location: `Tests/CoreFoundationTests.cpp:64`
- Impact: The required boundary coverage reports success without checking behavior and cannot detect identity invariant regressions in a foundational value type.
- Evidence: The only moved-from FString assertion is 'IsEmpty() || !IsEmpty()', which is tautologically true; FName copy/move behavior is not tested, allowing the reproduced stale-hash equality defect to pass all gates.
- Resolution: Replaced the tautological moved-from FString assertion and added collision, copy, equality-law, move-construction, and move-assignment invariant coverage for FName.
- Verification: Verified at remote head 2a97649: all hosted test/build checks pass; local Core 60/0 exercises meaningful FString reassignment plus FName collision, equality-law, copy, move-construction, and move-assignment invariants, and the tautological assertion is absent.
- Commit: `0bfcdec`

## CR001-B02-F003: Aligned allocation size arithmetic wraps into undersized success

- Severity: S1
- Status: Verified
- Requirement: 003-FR-005, 003-FR-008, User Story 2 scenario 3, and Memory Utility validation require invalid or unsupported requests to fail deterministically without corruption
- Location: `Source/Core/Private/FMemory.cpp:43`
- Impact: A caller receives a tiny allocation for a near-SIZE_MAX request and may write the promised byte count, causing immediate out-of-bounds memory corruption.
- Evidence: A focused C++20 probe calls AllocateAligned(SIZE_MAX, 16); on macOS arm64 it returns non-null because Size + Alignment - 1 + sizeof(void*) wraps to 22 before malloc.
- Resolution: Added a representability guard before aligned-allocation overhead arithmetic, retained checked padding in address alignment, documented public alignment/pairing rules, and added overflow-boundary regressions.
- Verification: Verified at remote head 365fdb9: Windows/macOS/Linux strict headless tests, three-platform strict Release, Linux ASan+UBSan, Linux Lavapipe native/readback, and CR tools all pass; local Core is 62/0 and the focused probe changed from non-null/1 to null/0.
- Commit: `60689e1`

## CR001-B02-F004: Lerp violates finite endpoint guarantees through intermediate overflow

- Severity: S2
- Status: Fixed
- Requirement: 004 data-model interpolation endpoints, FR-005, FR-011, and User Story 1 require predictable interpolation and boundary coverage
- Location: `Source/Core/Public/Core/FMath.h:44`
- Impact: Endpoint interpolation can turn valid finite values into non-finite results, breaking the documented scalar invariant and propagating invalid values into future animation, color, and rendering interpolation.
- Evidence: A C++20 Debug and Release probe with finite A=FLT_MAX and B=-FLT_MAX produces NaN at Alpha=0 and -infinity at Alpha=1 instead of A and B.
- Resolution: Replaced overflow-prone interpolation arithmetic with C++20 std::lerp and added opposite-FLT_MAX endpoint and midpoint regressions.
- Verification: pending
- Commit: `e077419`

## CR001-B02-F005: Safe vector normalization collapses large finite directions to zero

- Severity: S2
- Status: Fixed
- Requirement: 004-FR-001, FR-011, Vector validation rules, and T013 require correct safe normalization for finite vectors and boundary inputs
- Location: `Source/Core/Public/Core/FVector2.h:86`
- Impact: Valid finite directions can be erased, corrupting planes, light directions, camera-facing data, and any downstream operation that relies on normalized vectors.
- Evidence: Debug and Release probes normalize axis vectors with FLT_MAX components; FVector2, FVector3, and FVector4 all return zero vectors because LengthSquared overflows before division.
- Resolution: Changed vector length to overflow-resistant hypot and safe normalization to finite-validated, scale-resistant normalization across FVector2/3/4, with large-axis and diagonal tests.
- Verification: pending
- Commit: `e077419`

## CR001-B02-F006: Invalid numeric math contract and verification are missing

- Severity: S2
- Status: Fixed
- Requirement: 004 edge case 68, FR-011, FR-012, T013, and T040 require documented and verified NaN/infinity and invalid-input behavior
- Location: `Tests/CoreMathTests.cpp:75`
- Impact: Callers and optimized implementations have no stable invalid-input contract, and the green suite cannot detect non-finite propagation or cross-platform policy drift.
- Evidence: The claimed infinity test only queries IsFinite on one component; public normalization emits NaN, negative tolerance rejects self-equality, and NaN color channels silently convert to 255, while public comments define none of these policies.
- Resolution: Documented and enforced finite non-negative near tolerances, Zero fallback for invalid vector normalization, and deterministic NaN/infinity color conversion; replaced nominal checks with behavioral coverage.
- Verification: pending
- Commit: `e077419`

## CR001-B02-F007: Quaternion normalization and equivalence break the public rotation contract

- Severity: S2
- Status: Fixed
- Requirement: 004-FR-003, FR-009, FR-011, data-model quaternion validation, and SC-006
- Location: `Source/Core/Public/Core/FQuat.h:59`
- Impact: Finite orientations can be silently erased, invalid input can reach render-facing directions, and equivalent rotations cannot be compared with the documented tolerance-aware behavior.
- Evidence: Debug and Release probe: FQuat(FLT_MAX,0,0,FLT_MAX).GetSafeNormal() returns identity; an infinite component propagates non-finite RotateVector output; a unit quaternion and its negation return false from NearlyEquals.
- Resolution: Made quaternion length/normalization/inverse scale-resistant, defined invalid-input identity fallback, and made NearlyEquals accept q/-q rotation equivalence; added direct Debug/Release regressions.
- Verification: pending
- Commit: `70cacb7`

## CR001-B02-F008: Spatial transform composition and inversion report incorrect success for valid and invalid inputs

- Severity: S1
- Status: Fixed
- Requirement: 004-FR-002, FR-004, FR-009, FR-011; data-model matrix/transform validation; 017 preserve-world reparent contract
- Location: `Source/Core/Public/Core/FTransform.h:44; Source/Core/Public/Core/FMatrix4x4.h:161`
- Impact: Scene hierarchy PreserveWorld reparent uses FTransform::TryInverse and operator*, so ordinary rotated non-uniform parent transforms can corrupt local/world state; callers can also treat NaN inverses as valid.
- Evidence: Debug and Release probe: parent non-uniform scale plus child rotation makes sequential and operator* point transforms disagree; TryInverse then fails round-trip. Zero matrix with negative tolerance and a matrix containing NaN both return success and emit NaN. Zero-scale FTransform with negative tolerance also returns success.
- Resolution: Replaced infallible TRS composition with exact matrix-checked TryCompose/TryInverse/TryRelativeTo, added orthogonal TRS decomposition, rejected shear truthfully, and made Scene hierarchy validation transactional with stable InvalidHierarchyOperation diagnostics.
- Verification: pending
- Commit: `70cacb7`

## CR001-B02-F009: Geometry primitives lack finite-safe construction, tolerance, and extreme-bound behavior

- Severity: S2
- Status: Fixed
- Requirement: 004-FR-007, FR-009, FR-011, FR-012; data-model box/sphere/plane validation
- Location: `Source/Core/Public/Core/FBox.h:75; Source/Core/Public/Core/FSphere.h:18; Source/Core/Public/Core/FPlane.h:23`
- Impact: Bounds/culling and spatial classification can become mathematically wrong or non-finite from finite source data, while invalid numeric input is accepted without a documented deterministic policy.
- Evidence: Debug and Release probe: FPlane((0,0,2),2) reports signed distance -1 at z=1 because normal normalization does not rescale Distance; FBox(FLT_MAX,FLT_MAX) center is non-finite; +infinity radius sphere is valid and a finite large sphere reports FLT_MAX point contained after squared-distance overflow; negative plane tolerance changes an on-plane result to Front.
- Resolution: Added finite-safe box/sphere/plane construction and queries, overflow-resistant bounds and containment, normalized plane equation coefficients together, and deterministic invalid tolerance behavior with regressions.
- Verification: pending
- Commit: `70cacb7`

## CR001-B02-F010: Global severity filtering evaluates suppressed log arguments

- Severity: S2
- Status: Accepted
- Requirement: Feature 005 FR-016 macro-level early-out
- Location: `Source/Core/Public/Core/SGLog.h:18`
- Impact: Globally filtered logs still execute arbitrary argument expressions and enter FLog::LogMessage, violating the zero-side-effect and single-comparison contract.
- Evidence: Debug and Release probes set global=Warning/category=Verbose then issue Info with ++sideEffect; both print side_effect_count=1 and exit 1.
- Resolution: pending
- Verification: pending
- Commit: `pending`

## CR001-B02-F011: Runtime logging thresholds contain unsynchronized data races

- Severity: S2
- Status: Accepted
- Requirement: Feature 005 runtime-mutable category/global filtering, FR-013 concurrency safety, and research atomic-threshold decision
- Location: `Source/Core/Public/Core/FLogCategory.h:50; Source/Core/Private/FLog.cpp:17`
- Impact: Concurrent threshold reconfiguration and logging causes C++ undefined behavior, so filtering and diagnostics may be corrupted or optimized unpredictably.
- Evidence: macOS ThreadSanitizer independently reports races between FLogCategory::Set/GetMinSeverity on LogCore and FLog::Set/GetGlobalMinSeverity on GGlobalMinSeverity.
- Resolution: pending
- Verification: pending
- Commit: `pending`

## CR001-B02-F012: Fatal logging contract is not exercised by the test suite

- Severity: S2
- Status: Accepted
- Requirement: Feature 005 FR-004, FR-012, SC-009, T011, T013, and quickstart isolated termination validation
- Location: `Tests/LoggingAssertionTests.cpp:360; Tests/LoggingAssertionTests.cpp:432`
- Impact: Fatal routing or termination can regress while the checked-complete tasks and public-entry coverage gate continue to pass.
- Evidence: Routing loop explicitly skips Fatal, and TestFatalLogBehavior emits Error instead. A release child probe shows actual Fatal writes to stderr and aborts with exit 134, behavior absent from the suite.
- Resolution: pending
- Verification: pending
- Commit: `pending`
