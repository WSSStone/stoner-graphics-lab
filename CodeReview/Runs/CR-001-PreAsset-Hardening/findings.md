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
- Status: Verified
- Requirement: 004 data-model interpolation endpoints, FR-005, FR-011, and User Story 1 require predictable interpolation and boundary coverage
- Location: `Source/Core/Public/Core/FMath.h:44`
- Impact: Endpoint interpolation can turn valid finite values into non-finite results, breaking the documented scalar invariant and propagating invalid values into future animation, color, and rendering interpolation.
- Evidence: A C++20 Debug and Release probe with finite A=FLT_MAX and B=-FLT_MAX produces NaN at Alpha=0 and -infinity at Alpha=1 instead of A and B.
- Resolution: Replaced overflow-prone interpolation arithmetic with C++20 std::lerp and added opposite-FLT_MAX endpoint and midpoint regressions.
- Verification: Independent scalar/vector/color parent-current probes and local strict/sanitizer gates passed in B02-S09; final CI 30195707555 passed Windows, macOS, Linux Debug/Release and Linux ASan/UBSan at the batch boundary.
- Commit: `e077419`

## CR001-B02-F005: Safe vector normalization collapses large finite directions to zero

- Severity: S2
- Status: Verified
- Requirement: 004-FR-001, FR-011, Vector validation rules, and T013 require correct safe normalization for finite vectors and boundary inputs
- Location: `Source/Core/Public/Core/FVector2.h:86`
- Impact: Valid finite directions can be erased, corrupting planes, light directions, camera-facing data, and any downstream operation that relies on normalized vectors.
- Evidence: Debug and Release probes normalize axis vectors with FLT_MAX components; FVector2, FVector3, and FVector4 all return zero vectors because LengthSquared overflows before division.
- Resolution: Changed vector length to overflow-resistant hypot and safe normalization to finite-validated, scale-resistant normalization across FVector2/3/4, with large-axis and diagonal tests.
- Verification: Independent scalar/vector/color parent-current probes and local strict/sanitizer gates passed in B02-S09; final CI 30195707555 passed Windows, macOS, Linux Debug/Release and Linux ASan/UBSan at the batch boundary.
- Commit: `e077419`

## CR001-B02-F006: Invalid numeric math contract and verification are missing

- Severity: S2
- Status: Verified
- Requirement: 004 edge case 68, FR-011, FR-012, T013, and T040 require documented and verified NaN/infinity and invalid-input behavior
- Location: `Tests/CoreMathTests.cpp:75`
- Impact: Callers and optimized implementations have no stable invalid-input contract, and the green suite cannot detect non-finite propagation or cross-platform policy drift.
- Evidence: The claimed infinity test only queries IsFinite on one component; public normalization emits NaN, negative tolerance rejects self-equality, and NaN color channels silently convert to 255, while public comments define none of these policies.
- Resolution: Documented and enforced finite non-negative near tolerances, Zero fallback for invalid vector normalization, and deterministic NaN/infinity color conversion; replaced nominal checks with behavioral coverage.
- Verification: Independent scalar/vector/color parent-current probes and local strict/sanitizer gates passed in B02-S09; final CI 30195707555 passed Windows, macOS, Linux Debug/Release and Linux ASan/UBSan at the batch boundary.
- Commit: `e077419`

## CR001-B02-F007: Quaternion normalization and equivalence break the public rotation contract

- Severity: S2
- Status: Verified
- Requirement: 004-FR-003, FR-009, FR-011, data-model quaternion validation, and SC-006
- Location: `Source/Core/Public/Core/FQuat.h:59`
- Impact: Finite orientations can be silently erased, invalid input can reach render-facing directions, and equivalent rotations cannot be compared with the documented tolerance-aware behavior.
- Evidence: Debug and Release probe: FQuat(FLT_MAX,0,0,FLT_MAX).GetSafeNormal() returns identity; an infinite component propagates non-finite RotateVector output; a unit quaternion and its negation return false from NearlyEquals.
- Resolution: Made quaternion length/normalization/inverse scale-resistant, defined invalid-input identity fallback, and made NearlyEquals accept q/-q rotation equivalence; added direct Debug/Release regressions.
- Verification: Independent spatial-math parent-current probes and local strict/sanitizer gates passed in B02-S12; final CI 30195707555 passed Windows, macOS, Linux Debug/Release and Linux ASan/UBSan at the batch boundary.
- Commit: `70cacb7`

## CR001-B02-F008: Spatial transform composition and inversion report incorrect success for valid and invalid inputs

- Severity: S1
- Status: Verified
- Requirement: 004-FR-002, FR-004, FR-009, FR-011; data-model matrix/transform validation; 017 preserve-world reparent contract
- Location: `Source/Core/Public/Core/FTransform.h:44; Source/Core/Public/Core/FMatrix4x4.h:161`
- Impact: Scene hierarchy PreserveWorld reparent uses FTransform::TryInverse and operator*, so ordinary rotated non-uniform parent transforms can corrupt local/world state; callers can also treat NaN inverses as valid.
- Evidence: Debug and Release probe: parent non-uniform scale plus child rotation makes sequential and operator* point transforms disagree; TryInverse then fails round-trip. Zero matrix with negative tolerance and a matrix containing NaN both return success and emit NaN. Zero-scale FTransform with negative tolerance also returns success.
- Resolution: Replaced infallible TRS composition with exact matrix-checked TryCompose/TryInverse/TryRelativeTo, added orthogonal TRS decomposition, rejected shear truthfully, and made Scene hierarchy validation transactional with stable InvalidHierarchyOperation diagnostics.
- Verification: Independent spatial-math parent-current probes and local strict/sanitizer gates passed in B02-S12; final CI 30195707555 passed Windows, macOS, Linux Debug/Release and Linux ASan/UBSan at the batch boundary.
- Commit: `70cacb7`

## CR001-B02-F009: Geometry primitives lack finite-safe construction, tolerance, and extreme-bound behavior

- Severity: S2
- Status: Verified
- Requirement: 004-FR-007, FR-009, FR-011, FR-012; data-model box/sphere/plane validation
- Location: `Source/Core/Public/Core/FBox.h:75; Source/Core/Public/Core/FSphere.h:18; Source/Core/Public/Core/FPlane.h:23`
- Impact: Bounds/culling and spatial classification can become mathematically wrong or non-finite from finite source data, while invalid numeric input is accepted without a documented deterministic policy.
- Evidence: Debug and Release probe: FPlane((0,0,2),2) reports signed distance -1 at z=1 because normal normalization does not rescale Distance; FBox(FLT_MAX,FLT_MAX) center is non-finite; +infinity radius sphere is valid and a finite large sphere reports FLT_MAX point contained after squared-distance overflow; negative plane tolerance changes an on-plane result to Front.
- Resolution: Added finite-safe box/sphere/plane construction and queries, overflow-resistant bounds and containment, normalized plane equation coefficients together, and deterministic invalid tolerance behavior with regressions.
- Verification: Independent spatial-math parent-current probes and local strict/sanitizer gates passed in B02-S12; final CI 30195707555 passed Windows, macOS, Linux Debug/Release and Linux ASan/UBSan at the batch boundary.
- Commit: `70cacb7`

## CR001-B02-F010: Global severity filtering evaluates suppressed log arguments

- Severity: S2
- Status: Verified
- Requirement: Feature 005 FR-016 macro-level early-out
- Location: `Source/Core/Public/Core/SGLog.h:18`
- Impact: Globally filtered logs still execute arbitrary argument expressions and enter FLog::LogMessage, violating the zero-side-effect and single-comparison contract.
- Evidence: Debug and Release probes set global=Warning/category=Verbose then issue Info with ++sideEffect; both print side_effect_count=1 and exit 1.
- Resolution: SG_LOG now intersects atomic category/global severity masks before one final bit comparison; Debug and optimized Release probes preserve side_effect_count=0 and maintained coverage asserts the global path.
- Verification: Independent logging probes and local strict/sanitizer gates passed in B02-S15; final CI 30195707555 passed maintained logging coverage on Windows, macOS, Linux and Linux ASan/UBSan at the batch boundary.
- Commit: `8303045d6b977ecc873033a2da3100756f347055`

## CR001-B02-F011: Runtime logging thresholds contain unsynchronized data races

- Severity: S2
- Status: Verified
- Requirement: Feature 005 runtime-mutable category/global filtering, FR-013 concurrency safety, and research atomic-threshold decision
- Location: `Source/Core/Public/Core/FLogCategory.h:50; Source/Core/Private/FLog.cpp:17`
- Impact: Concurrent threshold reconfiguration and logging causes C++ undefined behavior, so filtering and diagnostics may be corrupted or optimized unpredictably.
- Evidence: macOS ThreadSanitizer independently reports races between FLogCategory::Set/GetMinSeverity on LogCore and FLog::Set/GetGlobalMinSeverity on GGlobalMinSeverity.
- Resolution: Category and global thresholds now use relaxed std::atomic storage; maintained concurrent access coverage passes and both independent pre-fix TSan reproducers now exit 0 without reports.
- Verification: Independent logging probes and local strict/sanitizer gates passed in B02-S15; final CI 30195707555 passed maintained logging coverage on Windows, macOS, Linux and Linux ASan/UBSan at the batch boundary.
- Commit: `8303045d6b977ecc873033a2da3100756f347055`

## CR001-B02-F012: Fatal logging contract is not exercised by the test suite

- Severity: S2
- Status: Verified
- Requirement: Feature 005 FR-004, FR-012, SC-009, T011, T013, and quickstart isolated termination validation
- Location: `Tests/LoggingAssertionTests.cpp:360; Tests/LoggingAssertionTests.cpp:432`
- Impact: Fatal routing or termination can regress while the checked-complete tasks and public-entry coverage gate continue to pass.
- Evidence: Routing loop explicitly skips Fatal, and TestFatalLogBehavior emits Error instead. A release child probe shows actual Fatal writes to stderr and aborts with exit 134, behavior absent from the suite.
- Resolution: The test executable now exposes a dedicated Fatal child mode; POSIX fork/exec and Windows CreateProcess harnesses capture stderr and require abnormal termination before the fallback return. Debug and Release suites pass, and task/contract text now matches the clarified abort behavior.
- Verification: Independent logging probes and local strict/sanitizer gates passed in B02-S15; final CI 30195707555 passed maintained logging coverage on Windows, macOS, Linux and Linux ASan/UBSan at the batch boundary.
- Commit: `8303045d6b977ecc873033a2da3100756f347055`

## CR001-B02-F013: Assertion handler replacement races with assertion dispatch

- Severity: S2
- Status: Verified
- Requirement: Feature 005 replaceable assertion handler contract and Core diagnostic thread-safety
- Location: `Source/Core/Private/FLog.cpp:17; Source/Core/Private/FLog.cpp:93; Source/Core/Private/FLog.cpp:178`
- Impact: Replacing the handler while any worker reports an assertion is C++ undefined behavior and can dispatch through a torn or stale function pointer.
- Evidence: A release ThreadSanitizer probe reports a data race between FLog::SetAssertionHandler and FLog::HandleAssertionFailure on global GAssertionHandler.
- Resolution: Assertion handler selection now uses relaxed atomic function-pointer loads/stores; maintained concurrent dispatch coverage passes and the original post-fix TSan probe exits 0 without a race report.
- Verification: Independent assertion parent-current probes and local strict/sanitizer gates passed in B02-S18; final CI 30195707555 passed maintained assertion coverage on MSVC, Apple Clang, GCC and Linux ASan/UBSan at the batch boundary.
- Commit: `76063b27d6f3cbbb79fbcd488897af33a9504054`

## CR001-B02-F014: Assertion build-mode and default-break contracts lack durable coverage

- Severity: S2
- Status: Verified
- Requirement: Feature 005 FR-008 through FR-011, SC-003 through SC-005, SC-009, and quickstart checks 5-7
- Location: `Tests/LoggingAssertionTests.cpp:646; Tests/LoggingAssertionTests.cpp:684; Tests/LoggingAssertionTests.cpp:719`
- Impact: Debugger-break, compile-out, or Release VERIFY regressions can pass the claimed public-entry and build-mode coverage gates on all CI platforms.
- Evidence: Maintained tests always install a custom handler, never execute SG_DEBUG_BREAK, use no side effect in SG_CHECK/SG_CHECKF Release checks, and omit the Release false-SG_VERIFY no-handler assertion. External probes show Debug SIGTRAP and correct Release stripping, but that evidence is not maintained.
- Resolution: The maintained child harness now exercises the default assertion handler in Debug and Release; side-effect counters prove SG_CHECK/SG_CHECKF stripping and false SG_VERIFY non-dispatch in Release.
- Verification: Independent assertion parent-current probes and local strict/sanitizer gates passed in B02-S18; final CI 30195707555 passed maintained assertion coverage on MSVC, Apple Clang, GCC and Linux ASan/UBSan at the batch boundary.
- Commit: `76063b27d6f3cbbb79fbcd488897af33a9504054`

## CR001-B02-F015: GCC debug break is not resumable as required

- Severity: S2
- Status: Verified
- Requirement: Feature 005 FR-011, cross-platform constraint, contract SG_DEBUG_BREAK, and research Decision 5
- Location: `Source/Core/Public/Core/SGPlatformBreak.h:19`
- Impact: Linux GCC assertions stop on a trap instruction that repeats or requires manual instruction-pointer repair, violating the soft-stop assertion contract and differing from MSVC/Clang behavior.
- Evidence: The GCC branch expands to __builtin_trap even though Feature 005 research explicitly rejects that intrinsic as non-resumable; the clarified contract requires a debugger break from which execution can continue.
- Resolution: GCC/POSIX now raises SIGTRAP instead of using __builtin_trap; Clang and MSVC retain resumable debugger intrinsics, and Feature 005 artifacts document the corrected platform mapping.
- Verification: Independent assertion parent-current probes and local strict/sanitizer gates passed in B02-S18; final CI 30195707555 passed maintained assertion coverage on MSVC, Apple Clang, GCC and Linux ASan/UBSan at the batch boundary.
- Commit: `76063b27d6f3cbbb79fbcd488897af33a9504054`

## CR001-B02-F016: Mobile targets are silently classified as supported desktop platforms

- Severity: S2
- Status: Verified
- Requirement: Feature 006 FR-001 and FR-002 require exactly one supported Windows/macOS/Linux identity and a clear failure for unsupported platforms
- Location: `Source/Core/Public/Core/SGPlatform.h:3`
- Impact: Android and non-macOS Apple builds can enter desktop platform branches and compile against incorrect APIs while advertising a supported desktop identity
- Evidence: B02-S19 compile probes classified __ANDROID__ as SG_PLATFORM_LINUX and TARGET_OS_IPHONE as SG_PLATFORM_MAC, while a generic unknown target correctly failed compilation
- Resolution: SGPlatform now rejects Android before Linux and accepts only TARGET_OS_OSX within the Apple/Mach family. The SCons test target runs a maintained compiler matrix proving Windows/macOS/Linux success and Android/iOS/unknown diagnostic failure.
- Verification: Independent platform-matrix and Mach ownership parent-current probes passed in B02-S21; final CI 30195707555 passed Windows, macOS, Linux Debug/Release and Linux ASan/UBSan at the batch boundary.
- Commit: `7a78cc6db8ed80c4b6d373cd932677850f58abbe`

## CR001-B02-F017: macOS available-memory query leaks a Mach host send-right reference

- Severity: S2
- Status: Verified
- Requirement: Feature 006 FR-003 requires safe available-memory reporting through the Core platform boundary
- Location: `Source/Core/Private/FPlatformMisc.cpp:53`
- Impact: Long-running repeated queries consume Mach port user references and can eventually exhaust or saturate the process right-reference count
- Evidence: B02-S19 macOS ownership probe called GetAvailableMemoryBytes 1024 times and observed host send urefs grow from 1 to 1025 (delta 1024)
- Resolution: The macOS available-memory path now balances mach_host_self with mach_port_deallocate before every later return. A maintained 1,024-query Mach uref regression and the original focused probe both preserve the reference count.
- Verification: Independent platform-matrix and Mach ownership parent-current probes passed in B02-S21; final CI 30195707555 passed Windows, macOS, Linux Debug/Release and Linux ASan/UBSan at the batch boundary.
- Commit: `7a78cc6db8ed80c4b6d373cd932677850f58abbe`

## CR001-B02-F018: Concurrent file truncation is reported as a successful full read

- Severity: S2
- Status: Verified
- Requirement: 006 FR-009; data-model file failure rules; core-platform API verification contract
- Location: `Source/Core/Private/FPlatformFileSystem.cpp:69`
- Impact: A caller can accept a partial read padded by value-initialized bytes as a complete payload, violating byte preservation and failure reporting
- Evidence: ASan/UBSan probe truncated a 256 MiB regular file to one byte after sizing; ReadFile returned true with a 268435456-byte output while the final file size was 1
- Resolution: ReadFile now delegates to a bounded exact-read helper that catches allocation/stream exceptions, requires gcount to equal the requested size, and clears output on every failure; maintained exact, short, empty, and stale-output tests cover the contract.
- Verification: Independent parent/current truncation probe changed from false success with 268435456 bytes to 12/12 clean failures with cleared output; strict local gates and CI 30195707555 including Linux ASan/UBSan passed.
- Commit: `1a3c4de2a8bd2e45c22777c778f087ed82192fa4`

## CR001-B02-F019: POSIX module validation permits loader-search bypass

- Severity: S2
- Status: Verified
- Requirement: 006 FR-010; explicit-path clarification; core-platform API contract
- Location: `Source/Core/Private/FPlatformProcess.cpp:20`
- Impact: Callers that expect explicit-path-only loading can instead resolve a module through mutable platform loader search paths
- Evidence: On macOS, a module name containing only a backslash passed HasExplicitPathMarker and loaded from DYLD_LIBRARY_PATH without any slash in the supplied name
- Resolution: Dynamic module validation now uses native std::filesystem path grammar; POSIX requires a real parent path, Windows accepts native parent/root syntax, and LoadLibraryW preserves the UTF-8 engine path through the native wide path.
- Verification: Independent parent/current module probe changed from POSIX loader-search success to explicit-path rejection; maintained native-path tests compiled and passed on Windows, macOS, and Linux in CI 30195707555.
- Commit: `1a3c4de2a8bd2e45c22777c778f087ed82192fa4`

## CR001-B02-F020: Dynamic module handle copies permit stale use and repeated release

- Severity: S2
- Status: Verified
- Requirement: 006 data-model dynamic module validation and state transitions; FR-011 managed invalid-handle behavior
- Location: `Source/Core/Public/Core/FPlatformProcess.h:8`
- Impact: The public ownership type cannot enforce release exactly once and exposes stale symbol lookup and platform-dependent repeated-close behavior
- Evidence: ASan/UBSan probe reports copyable=1; after freeing the original handle, its copy still reports valid and FreeDynamicModule invokes a second dlclose on the stale native handle
- Resolution: FDynamicModuleHandle is now an opaque move-only owner with noexcept transfer, RAII destruction, private native state, const-reference symbol lookup, and idempotent explicit release; compile-time traits and runtime transfer tests preserve exactly-once ownership.
- Verification: Independent parent/current probe proves the public handle changed from copyable aliasing to move-only ownership with transfer, symbol lookup, and idempotent release; compile-time/runtime tests passed on all supported hosts in CI 30195707555.
- Commit: `1a3c4de2a8bd2e45c22777c778f087ed82192fa4`

## CR001-B03-F001: Runtime snapshot live-object total can wrap to zero

- Severity: S2
- Status: Verified
- Requirement: 018-FR-019; 018-SC-009; 018-T006
- Location: `Source/RHI/Public/RHI/FRHIRuntimeSnapshot.h:15`
- Impact: Unsigned 32-bit aggregation can falsely certify leak-free shutdown when non-zero category counts wrap modulo 2^32, weakening the stable runtime snapshot and final-zero resource gate.
- Evidence: A strict standalone C++20 probe sets LiveInstances=UINT32_MAX and LiveDevices=1; GetTotalLiveObjectCount() returns 0 (probe exit 3), while successful endurance validation treats zero as proof that no demo-owned resources remain.
- Resolution: GetTotalLiveObjectCount now returns uint64 and promotes the first operand before summation; per-category uint32 diagnostics remain unchanged. Regression coverage proves UINT32_MAX+1 equals 4294967296 instead of zero.
- Verification: Independent same-source parent/current probe verified exact behavior: parent blob dabb110 returns total=0 and exit 3 for UINT32_MAX+1; current returns 4294967296 and exit 0. Strict fallback, strict Release, and ASan/UBSan records pass; repository call sites perform comparison or stream output with no uint32 truncating assignment.
- Commit: `98c97a5`

## CR001-B03-F002: Native execution proof accepts a deterministic request

- Severity: S2
- Status: Verified
- Requirement: 018-FR-003; 018-T006; 018-T009; triangle-demo-validation-contract native-required gate
- Location: `Source/RHI/Public/RHI/FRHIRuntimeSnapshot.h:32`
- Impact: A contradictory snapshot can falsely satisfy backend-neutral native proof, allowing a deterministic request to be represented as successful native execution and undermining the no-silent-fallback milestone gate.
- Evidence: A strict standalone C++20 probe sets RequestedMode=Deterministic, ObjectMode=RealRuntime, LiveInstances=1, and LiveDevices=1; ProvesNativeExecution() returns true (probe exit 3). The validation contract requires native-required runs to fail when proof reports deterministic mode.
- Resolution: ProvesNativeExecution now requires an explicit Native or NativeHeadless request in addition to RealRuntime and non-zero instance/device counts. Tests cover both positive native modes and deterministic, fallback, missing-instance, and missing-device negatives.
- Verification: Independent same-source parent/current probe verified exact behavior: parent accepts the contradictory deterministic/real-runtime snapshot (proof=1, exit 3); current rejects it (proof=0, exit 0). Positive Native and NativeHeadless plus fallback and zero-count negatives pass in the strict 757-line deterministic suite.
- Commit: `98c97a5`
