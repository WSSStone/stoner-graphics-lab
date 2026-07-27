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

## CR001-B03-F003: Surface-aware swapchain creation silently falls back to headless

- Severity: S2
- Status: Verified
- Requirement: 018-FR-003; 018-FR-009; 018-T012; triangle-demo-runtime-contract presentation contract
- Location: `Source/RHI/Public/RHI/IRHIDevice.h:92`
- Impact: Callers and future backends can receive false presentation support and a successful object that does not satisfy the requested surface, extent, format, or presentation policy, undermining native-versus-headless runtime truth.
- Evidence: A strict standalone C++20 probe calls CreateSwapchain through IRHIDevice& with a null surface and an invalid zero-extent FRHISwapchainDesc; the default overload returns Success and a non-null headless object. The adapter ignores surface, width, height, preferred format, and VSync, forwarding only FramesInFlight.
- Resolution: The surface-aware device compatibility overload now returns Unsupported with no object instead of forwarding only FramesInFlight to the headless factory. Base-reference tests cover valid and invalid descriptors and prove no false presentation object is created.
- Verification: A same-source strict verifier compiled against exact parent 0e1ccf1 reproduces false surface-aware success (surface_fixed=0), while current code returns Unsupported with no object (surface_fixed=1). Parent header/test exports match Git blobs, fix scope is four expected files, no production call relies on the fallback, and fresh fallback-strict passes.
- Commit: `b29f466`

## CR001-B03-F004: Synchronization failures leave partial queue and swapchain transitions

- Severity: S2
- Status: Verified
- Requirement: 007-FR-005; 007-FR-009; 007-SC-003; 018-FR-009; triangle-demo-runtime-contract native frame order
- Location: `Source/RHI/Public/RHI/IRHISwapchain.h:33; Tests/RHICoreTests.cpp:919`
- Impact: A caller that follows the returned failure cannot safely retry or recover: an image, dependency, or command buffer may already have changed ownership/state, allowing deadlock, stale acquisition, false submission accounting, or an unsignaled completion path.
- Evidence: The retained probe shows synchronized acquire returning InvalidState with the swapchain already Acquired; invalid present consumes its wait semaphore; a two-wait mock submission returns NotReady after consuming the first wait; and a failed output signal returns InvalidState after marking the command Submitted and incrementing submission count while its fence remains unsignaled.
- Resolution: Synchronized swapchain compatibility defaults now fail closed; the mock swapchain explicitly validates before commit, and the mock queue preflights the complete wait/signal/fence set before any mutation. Regression tests prove failed acquire, present, wait, and signal paths preserve all observable states.
- Verification: The same strict verifier reproduces all four parent partial-transition failures for compatibility acquire/present and mock queue wait/signal paths, then proves all four current operations preserve state on failure. Current Source/Tests match b29f466, 10 maintained assertions pass in a fresh 770-line strict suite, and retained ASan/UBSan evidence passes.
- Commit: `b29f466`

## CR001-B03-F005: Clear-value render-pass overload can report success while discarding clears

- Severity: S2
- Status: Verified
- Requirement: 018-FR-008; 018-T008; 018-T012; triangle-demo-runtime-contract required command capabilities
- Location: `Source/RHI/Public/RHI/IRHICommandBuffer.h:144`
- Impact: A legacy or future backend can claim successful frame recording while leaving color/depth attachments uncleared or stale, violating backend-neutral execution semantics and making rendered output nondeterministic.
- Evidence: A strict standalone legacy command-buffer probe supplies a non-empty FRHIRenderPassClearValues through IRHICommandBuffer&. The default overload returns Success and calls only the old overload, even with null render-pass/framebuffer arguments; no clear value is observed.
- Resolution: The explicit-clear command-buffer compatibility overload now returns Unsupported without delegating to a legacy render-pass method. A base-reference regression proves non-empty clears cannot return false success or invoke the legacy path; existing explicit-clear overrides continue to compile and pass.
- Verification: The same legacy command-buffer verifier shows the exact parent delegates non-empty clear values and reports success (clear_fixed=0), while current code returns Unsupported without invoking the legacy method (clear_fixed=1). Production clear callers resolve to the Vulkan override, strict Debug/Release compile, and fresh deterministic coverage passes.
- Commit: `b29f466`

## CR001-B03-F006: Resource validators accept undefined usage and enum domains

- Severity: S2
- Status: Verified
- Requirement: 008-FR-003; 008-FR-004; 008-FR-005; 008-FR-005a; 008-FR-019; rhi-resource-pipeline-api unsupported status contract
- Location: `Source/RHI/Public/RHI/ERHIResourceUsage.h:8; Source/RHI/Public/RHI/FRHIBufferDesc.h:27; Source/RHI/Public/RHI/FRHITextureDesc.h:34; Source/RHI/Public/RHI/FRHISamplerDesc.h:19`
- Impact: Undefined or contract-incompatible values cross the authoritative device boundary as successful resources, allowing backend conversion code to receive states with no portable meaning and defeating explicit Unsupported/InvalidState reporting.
- Evidence: A strict standalone probe shows public validators and FMockDevice factories accept a buffer with bit 31 usage, a buffer with ERHIMemoryAccess(255), a texture with bit 31 usage, TextureUsage::Vertex, ERHISampleCount(3), and a sampler with filter/address value 255; each returns a usable object.
- Resolution: Closed buffer/texture usage masks and recognized memory, sample, and sampler enum domains now reject undefined values; maintained helper/factory parity tests and the positive probe pass.
- Verification: A same-source strict verifier compiled against exact parent d8c59b7 reports 0/12 closed-domain rejections while current reports 12/12; parent export blobs match Git, invalid casts remain test-only, Vulkan consumes the public validators, and fresh fallback-strict passes.
- Commit: `ca68ed4`

## CR001-B03-F007: Texture validation accepts impossible mip chains

- Severity: S2
- Status: Verified
- Requirement: 008-FR-002; 008-FR-005; 008-T033; texture resource contract invalid mip/layer counts
- Location: `Source/RHI/Public/RHI/FRHITextureDesc.h:34`
- Impact: The RHI certifies descriptions that cannot represent a finite geometric mip chain or portable multisample image, shifting deterministic validation failures into backend allocation/creation and exposing later code to unbounded level counts.
- Evidence: A strict probe shows IsValidRHITextureDesc and FMockDevice accept a 1x1 texture with two mip levels, a 64x64 texture with UINT32_MAX mip levels, and a two-sample texture with multiple mip levels; every request returns a usable texture.
- Resolution: Exact overflow-safe geometric mip limits now accept the boundary, reject over-limit and UINT32_MAX counts, and require one mip for multisampled textures; maintained and standalone tests pass.
- Verification: The same-source verifier preserves 3/3 valid mip boundaries on parent and current, while exact parent rejects 0/4 invalid chains and current rejects 4/4; Source/Tests match ca68ed4 and fresh maintained coverage passes RHI 186/0.
- Commit: `ca68ed4`

## CR001-B03-F008: Texture validity contradicts format and attachment usage compatibility

- Severity: S2
- Status: Verified
- Requirement: 008-FR-002; 008-FR-005; 008-FR-005a; texture resource contract format/usage compatibility
- Location: `Source/RHI/Public/RHI/FRHITextureDesc.h:25`
- Impact: Callers and backends cannot treat the public validity helper as authoritative; implementations that add ad hoc checks reject descriptions that other helper consumers may accept, causing backend divergence and false portable-validity claims.
- Evidence: The public IsValidRHITextureDesc returns true for an R8G8B8A8 color format declared only as DepthStencilAttachment and for D32_Float declared only as ColorAttachment. The same mock factory rejects both through a separate test-local helper, proving contradictory validity decisions for identical descriptions.
- Resolution: Portable depth/color attachment compatibility now lives in IsValidRHITextureDesc, while the mock device retains only format capability checks; helper/factory parity regressions and the standalone probe pass.
- Verification: The same-source verifier compiled against exact parent reports 0/2 shared format/usage rejections while current reports 2/2; the mock and Vulkan paths share public portable validity with separate capability checks, and fresh strict tests pass.
- Commit: `ca68ed4`

## CR001-B03-F009: Shader and layout validation omits constant-range and closed-domain checks

- Severity: S2
- Status: Verified
- Requirement: 012-FR-004, 012-FR-005, 012-FR-007, 012-FR-009, and the Feature 012 shader-interface contract
- Location: `Source/RHI/Public/RHI/FRHIPipelineLayoutDesc.h:34`
- Impact: Malformed interfaces can become usable layouts and pipelines, while mock and Vulkan implementations disagree about shader-layout compatibility.
- Evidence: The retained B03-S10 strict probe creates a layout with undefined descriptor/visibility values, accepts overlapping compute ranges, and creates a mock compute pipeline whose shader requires a constant range absent from its layout.
- Resolution: Closed descriptor, visibility, and constant-range domains; centralized shader-interface compatibility for mock and Vulkan pipeline layouts; added maintained and standalone regression coverage.
- Verification: Independent same-source verifier reproduced 0/7 interface rejections on exact parent f7aa909 and 7/7 on current source while preserving 2/2 valid paths; fresh full suite and shared mock/Vulkan call-site audit passed.
- Commit: `09d1a1b`

## CR001-B03-F010: Pipeline and render-pass validators accept undefined fixed-function state

- Severity: S2
- Status: Verified
- Requirement: 008-FR-013, 008-FR-014, 008-FR-019, 012-FR-007, and 012-SC-004
- Location: `Source/RHI/Public/RHI/FRHIGraphicsPipelineDesc.h:132`
- Impact: Undefined state can cross public validation and become usable objects, producing backend-dependent behavior and invalidating claimed negative-path coverage.
- Evidence: The retained B03-S10 strict probe creates a mock graphics pipeline with undefined raster/blend/depth and sample values and a render pass with undefined role/load/store/sample values.
- Resolution: Closed graphics fixed-function, format, sample, attachment-role, load, and store domains; shared render-pass validation across mock and Vulkan paths with negative regressions.
- Verification: Independent same-source verifier reproduced 0/12 fixed-function/render-pass rejections on exact parent and 12/12 on current source while preserving 2/2 valid state paths; sanitizer verifier and fresh full suite passed.
- Commit: `09d1a1b`

## CR001-B03-F011: Framebuffer validation ignores selected mip and array-layer semantics

- Severity: S2
- Status: Verified
- Requirement: 008-FR-015, 008-FR-016, and 008-FR-019
- Location: `Tests/RHICoreTests.cpp:1525`
- Impact: Callers can receive usable framebuffers for nonexistent subresources and cannot reliably target valid nonzero mips through the cross-backend contract.
- Evidence: The retained B03-S10 strict probe accepts mip and layer indices equal to their counts, then rejects valid mip one at its 32x32 extent because the factory compares only against the 64x64 base texture.
- Resolution: Validated framebuffer mip and array-layer bounds and exact selected-mip extents in mock and Vulkan helpers; added positive nonzero-subresource and negative boundary regressions.
- Verification: Independent same-source verifier reproduced 0/3 invalid-subresource rejections plus valid-mip rejection on exact parent, then 3/3 rejections and valid-mip acceptance on current source while preserving 2/2 baseline paths; mock/Vulkan regressions passed.
- Commit: `09d1a1b`

## CR001-B04-F001: Synthetic fallback is reported as an available Vulkan runtime

- Severity: S1
- Status: Verified
- Requirement: 009-FR-001, 009-FR-004, 009-FR-005, 009-SC-001, 009-SC-002, and the Backend Initialization Contract
- Location: `Source/Backend/Vulkan/Private/FVulkanInstance.cpp:42`
- Impact: Missing or broken Vulkan runtime support is indistinguishable from a usable backend through the primary factory, so callers can receive a false active device and CI can report contract success without proving runtime initialization.
- Evidence: The retained B04-S01 probe shows default initialization returns Success and Active, marks Availability=Available, and selects the invented Stoner adapter while bUsedRuntimeFallback=true. Focused source search finds no Vulkan loader, physical-device enumeration, or FVulkanNativeContext integration in the inspected RHI device path.
- Resolution: Introduced an explicit RealRuntime versus DeterministicFallback instance mode. RealRuntime is the production default and returns Unsupported because FVulkanDevice does not own native Vulkan resources; real execution remains owned and proven by FVulkanNativeContext. Deterministic fallback now requires explicit opt-in and reports distinct availability plus runtime-mode diagnostics, and all deterministic test callers were migrated.
- Verification: Exact-parent probe reproduced false Available/Active fallback; the independent ASan/UBSan verifier confirms the default factory now returns Unsupported/null while explicit fallback remains successful and observably distinct. Fresh fallback-strict and sanitizer maintained gates passed.
- Commit: `c603f8a`

## CR001-B04-F002: Adapter selection retains unsafe non-owned identity pointers

- Severity: S1
- Status: Verified
- Requirement: 009-FR-002, 009-FR-019, 009-SC-003, and the Adapter Selection Contract
- Location: `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanPhysicalDevice.h:32`
- Impact: Adapter identity and rejection diagnostics can mutate or dangle after selection, and a malformed public candidate can crash deterministic selection instead of producing an explicit rejection.
- Evidence: The retained probe shows Selected.Name aliases caller-owned mutable std::string storage. A minimal ASan/UBSan probe passing two equal-score candidates with one null Name aborts in std::string_view construction from SelectBestAdapter's deterministic tie-break.
- Resolution: Replaced borrowed candidate names, rejection reasons, and selection reasons with owned FString values. Empty/null source identities now fail the required gate with owned diagnostics before ordering, while deterministic tie-breaking uses the normalized owned name. Added mutable-storage and null-identity regressions.
- Verification: The exact-parent summary reproduced caller-storage aliasing and the parent ASan probe crashed on null identity. The independent current ASan/UBSan verifier preserves a temporary identity, safely rejects empty/null identity with an owned reason, and selects the same lexical winner across 20 tie runs; maintained gates passed.
- Commit: `c603f8a`

## CR001-B04-F003: Device format capabilities ignore the selected adapter

- Severity: S2
- Status: Verified
- Requirement: 009-FR-002, 009-FR-003, 009-FR-004, and the Device Contract
- Location: `Source/Backend/Vulkan/Private/FVulkanDevice.cpp:839`
- Impact: RHI capability queries and resource factories can claim formats rejected by the selected adapter summary, undermining capability-gated selection and deferring failure to a later backend path.
- Evidence: The retained B04-S01 probe initializes from a candidate whose Formats.bDepth is false, then observes D32_Float in RHI SupportedFormats and successfully creates a depth texture. MapCapabilities unconditionally installs GetDefaultVulkanSupportedFormats instead of deriving formats from the selected adapter.
- Resolution: Replaced the two-boolean adapter format summary with a normalized, encapsulated concrete ERHIFormat set. MapCapabilities copies the selected adapter's exact set and texture factory validation consumes that capability snapshot. Removed the unused backend-wide default-format helper and added a color-only adapter regression that rejects D32 publication and creation.
- Verification: The exact-parent probe reproduced depth capability overclaim. The independent current verifier normalizes an exact one-format candidate, publishes only that format, accepts its texture, and rejects unsupported color and depth textures; source review confirms MapCapabilities uses the selected set and maintained gates passed.
- Commit: `c603f8a`

## CR001-B04-F004: Vulkan presentation bypasses the backend-neutral RHI contract

- Severity: S2
- Status: Verified
- Requirement: 009-FR-011 through FR-016, 009-SC-006, 018-FR-003, and the Feature 018 RHI Presentation Contract
- Location: `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDevice.h:77`
- Impact: Renderer/Application code cannot use Vulkan presentation through the current backend-neutral RHI surface, descriptor, image, and synchronization contracts. Real visible presentation is therefore forced into a separate native facade, duplicating lifecycle ownership and blocking interchangeable backend execution.
- Evidence: FVulkanDevice overrides only the legacy frame-count swapchain factory and exposes FVulkanSurface/CreateSwapchainForSurface as backend-specific methods. Through an active IRHIDevice reference, CreatePresentationSurface and descriptor-based CreateSwapchain both return Unsupported, while the backend-specific deterministic path succeeds. FVulkanSwapchain also inherits null GetImage and Unsupported synchronized acquire/present defaults.
- Resolution: FVulkanDevice now overrides the backend-neutral presentation-surface and descriptor-based swapchain factories. Surface-backed deterministic swapchains expose imported image wrappers and implement semaphore-aware acquire/present with preflight-before-commit semantics; backend-specific helpers now adapt into the same path without claiming native execution.
- Verification: Exact-parent 7c9a3df reproduced unsupported backend-neutral presentation. The independent current ASan/UBSan verifier dispatches through IRHIDevice, validates imported images and generation replacement, and confirms synchronized acquire/present failure atomicity; fresh fallback-strict and sanitizer gates passed without claiming native execution.
- Commit: `6119322`

## CR001-B04-F005: Presentation surfaces have no device provenance or lifecycle ownership

- Severity: S2
- Status: Verified
- Requirement: 009-FR-013, FR-016, FR-017, the Surface and Swapchain Contracts, and 007-FR-002a
- Location: `Source/Backend/Vulkan/Private/FVulkanDevice.cpp:697`
- Impact: Surface/device compatibility is not enforceable, stale or cross-device presentation inputs are accepted, and successful recreation does not prove that a usable presentation target exists. Native ownership added behind this contract could dereference stale handles or publish a ready swapchain detached from its parent surface.
- Evidence: FVulkanSurface stores only a borrowed opaque pointer; FVulkanDevice tracks swapchains but no surfaces. The sanitizer probe creates a surface on Device A, shuts A down, observes the surface still valid, and successfully creates a swapchain from it on Device B. Invalidating that surface does not affect the swapchain, which can transition from Unavailable back to Ready through Recreate without restored presentation input.
- Resolution: Surfaces now share one validity record tied to a non-copyable device owner token. Surface-backed swapchains retain that surface, reject stale or foreign provenance, expose Unavailable after surface loss, block recreation against lost input, and are invalidated with surfaces and imported images during device shutdown.
- Verification: Exact-parent 7c9a3df reproduced owner-shutdown survival, cross-device stale-surface acceptance, and recovery after surface loss. The independent current verifier rejects foreign provenance, observes shared invalidation through a copied surface, blocks dependent work after loss, and confirms shutdown cascades through retained surface, swapchain, and image references under ASan/UBSan.
- Commit: `6119322`

## CR001-B04-F006: Surface and swapchain creation failures do not fail atomically

- Severity: S2
- Status: Verified
- Requirement: 009-FR-001, FR-010, FR-012, FR-013, FR-015, and the Backend Initialization and Surface Contracts
- Location: `Source/Backend/Vulkan/Private/FVulkanDevice.cpp:682`
- Impact: Callers can retain a usable output after a failed factory call and cannot distinguish malformed input from an unsupported capability. The public constructor also permits bypassing the device factory's validation, making negative-path behavior dependent on entry point.
- Evidence: CreateSurface returns early when the device is inactive without resetting OutSurface, so the probe receives InvalidState while a previously valid surface remains usable. CreateSwapchain groups zero FrameCount with unsupported device capability and returns Unsupported, while the public FVulkanSwapchain constructor silently normalizes zero to one; generic RHI coverage classifies a zero-frame request as InvalidState.
- Resolution: CreateSurface now resets the output before every device-state check. Zero frame count, zero extent, invalid format, and depth presentation format return InvalidState; valid requests beyond device capability return Unsupported. The concrete zero-frame constructor is terminal rather than silently normalizing to one.
- Verification: Exact-parent 7c9a3df reproduced preserved usable factory output and zero-frame Unsupported classification. The independent current verifier confirms output clearing, malformed/depth InvalidState, capability-limit Unsupported, unchanged output/state on synchronized failures, and passing fresh maintained gates.
- Commit: `6119322`

## CR001-B04-F007: Allocation accounting can overflow and bypass configured budgets

- Severity: S2
- Status: Verified
- Requirement: 010-FR-008, FR-008a, FR-019, SC-003, the allocation contract, and the oversized-resource edge case
- Location: `Source/Backend/Vulkan/Private/FVulkanMemoryAllocator.cpp:121`
- Impact: Configured budgets and allocation telemetry cease to bound or describe live resources after unsigned wraparound. A future real allocator or streaming/residency policy could overcommit memory while deterministic validation reports success, and shutdown accounting cannot recover the lost total.
- Evidence: The ASan/UBSan probe creates a UINT64_MAX-byte buffer and a 2-byte buffer through FVulkanDevice; AllocatedBytes wraps to 1 while two allocations remain live. After configuring a 2-byte budget, a third 1-byte buffer succeeds and the snapshot reports only 2 bytes for three live resources. Neither RHI capabilities nor the Vulkan factory publishes/enforces a maximum buffer size.
- Resolution: Checked byte/count arithmetic rejects overflow before mutation and preserves later budget enforcement; maintained and independent probes cover UINT64_MAX accounting.
- Verification: Exact parent reproduced unsigned wrap and budget bypass; current independent verifier proves overflow/count failures preserve counters and later budgets, with all fresh strict and sanitizer gates passing.
- Commit: `f1a3329`

## CR001-B04-F008: Copyable allocation records permit duplicate release accounting

- Severity: S2
- Status: Verified
- Requirement: 010-FR-007, FR-009, SC-003, T046, T050, and the allocation ownership/repeated-cleanup contract
- Location: `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanResourceAllocation.h:32`
- Impact: Allocation ownership is not unique and repeated cleanup is only idempotent for one value instance. Copied or fabricated records can decrement unrelated live accounting, bypass budget/count limits, and make resource lifecycle diagnostics contradict actual ownership.
- Evidence: FVulkanResourceAllocation is a public copyable value whose Released flag is local to each copy. The probe allocates two 32-byte records under a 64-byte budget, copies the first token, and releases both copies successfully. The allocator reaches zero bytes/count while the second allocation is still successful, then accepts a new 64-byte allocation.
- Resolution: Allocation records are no-allocation move-only ownership tickets bound to stable allocator identity and reset epoch; foreign, stale, moved-from, and repeated releases cannot mutate counters.
- Verification: Exact parent reproduced copied-ticket double release; current API rejects copying/fabrication, and independent move, foreign, stale-epoch, and same-address allocator-lifetime checks preserve one release authority.
- Commit: `f1a3329`

## CR001-B04-F009: Texture footprint estimation misaccounts formats samples and mip chains

- Severity: S2
- Status: Verified
- Requirement: 010-FR-003, FR-008, FR-008a, T053, SC-003, and the texture allocation/budget contract
- Location: `Source/Backend/Vulkan/Private/FVulkanMemoryAllocator.cpp:79`
- Impact: Deterministic texture budgets both admit over-budget resources and reject in-budget resources based on format, multisampling, and mip count. This undermines current allocation tests and would feed incorrect costs into planned texture assets, compression fallback, and residency management.
- Evidence: EstimateTextureBytes defaults R32G32B32_Float to four bytes, ignores SampleCount, and multiplies every mip by the base extent. Under a 64-byte budget, the probe accepts a 4x4 RGBA8 four-sample texture and a 4x4 RGB32F texture despite logical footprints of 256 and 192 bytes. It rejects an 8x8 RGBA8 four-mip chain under its exact 340-byte texel footprint because it estimates 1024 bytes.
- Resolution: Texture footprint estimation now checks every multiplication/addition and sums exact per-mip extents, format widths, layers, depth, and sample count.
- Verification: Exact parent reproduced format/sample undercount and mip overcount; current independent matrix verifies every current format, dimension, layer, mip, sample count, and both multiplication and mip-total overflow.
- Commit: `f1a3329`

## CR001-B04-F010: Resource wrappers expose failed allocations and unbounded host storage

- Severity: S2
- Status: Verified
- Requirement: 010-FR-002, FR-004, FR-007, FR-008, SC-002, T035, and the no-usable-partial-resource contract
- Location: `Source/Backend/Vulkan/Private/FVulkanBuffer.cpp:8`
- Impact: Backend callers can create usable partial resources detached from successful allocation ownership, and a valid RHI upload request can escape the result contract as an exception. The public constructors bypass the device factory's no-partial-object invariant and oversized input can terminate exception-free engine call paths.
- Evidence: FVulkanBuffer and FVulkanTexture have public constructors that unconditionally begin Valid even when given a Failed allocation. The probe constructs both from injected BudgetExceeded records; the buffer accepts Upload successfully. Separately, FVulkanDevice accepts a UINT64_MAX host-visible buffer and FVulkanBuffer::Upload calls vector::resize for that full size, throwing std::length_error instead of returning an explicit RHI failure.
- Resolution: Buffer/texture wrappers are device-factory-only with rollback-safe tracking; host buffer mirrors grow only to uploaded ranges and translate storage failures to Unavailable.
- Verification: Exact parent reproduced usable failed wrappers and throwing huge upload; current negative compile closes those constructors, allocation-failure injection proves all factory rollbacks, and host upload failure preserves prior bytes with explicit results.
- Commit: `f1a3329`

## CR001-B04-F011: Descriptor reservations are forgeable and allocation is not failure-atomic

- Severity: S2
- Status: Verified
- Requirement: 010-FR-011, 010-FR-011a, and the Descriptor Pool Contract require sets to allocate only from fixed available capacity with explicit failure
- Location: `Source/Backend/Vulkan/Private/FVulkanDescriptorSet.cpp:47`
- Impact: Callers can overcommit configured descriptor capacity, while allocation failure can leak a pool slot and escape the explicit-result API, eventually making valid descriptor allocation permanently unavailable.
- Evidence: The B04-S10 sanitizer probe constructs an unallocated public descriptor set whose destructor releases another set's reservation, then re-allocates the supposedly full slot; FVulkanDevice::CreateDescriptorSet also increments the pool before two throwing allocations without rollback.
- Resolution: Replaced public scalar Allocate/Release with a move-only pool-issued RAII reservation, made pool/set construction device-only, and added rollback for pool, wrapper, control-block, and tracking allocation failures.
- Verification: Exact parent reproduced forged release and quota overcommit; current API closure, descriptor state-machine and failure-rollback verifier, full local gates, and CI run 30207463089 at 5488528 all passed.
- Commit: `5830901`

## CR001-B04-F012: Sampler construction bypasses device validation

- Severity: S2
- Status: Verified
- Requirement: 010-FR-005 and the RHI device-owned creation contract require unsupported sampler descriptions to return explicit failure without a usable sampler
- Location: `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanSampler.h:11`
- Impact: Backend-facing callers can inject unsupported sampler state into descriptor records, making device capability checks optional and leaving later native translation to handle an object that should never exist.
- Evidence: The B04-S10 sanitizer probe passes the same invalid compare-plus-no-mip-filter description rejected by FVulkanDevice to the public FVulkanSampler constructor and receives a sampler reporting Valid lifecycle.
- Resolution: Made sampler construction device-only, preserved validation at the factory boundary, and mapped wrapper/control-block/tracking failures to Unavailable without exposing a usable object.
- Verification: Exact parent reproduced direct valid construction of an unsupported sampler; current private-construction assertions, allocation rollback coverage, full local gates, and CI run 30207463089 at 5488528 all passed.
- Commit: `5830901`

## CR001-B04-F013: Texture upload staging accepts invalid subresources and byte footprints

- Severity: S2
- Status: Verified
- Requirement: 010-FR-016, 010-FR-017, T099, and the Upload Staging Contract require mip-aware destination-region and format-compatibility validation when the request is created
- Location: `Source/Backend/Vulkan/Private/FVulkanUploadStaging.cpp:41`
- Impact: Invalid pending records escape the validation boundary; the mip case violates explicit rejection, while an underfilled record can be scheduled and gives future execution code insufficient source bytes, risking corrupt transfer or out-of-bounds reads.
- Evidence: The B04-S10 sanitizer probe accepts an 8x8 upload into mip 1 of an 8x8 two-mip texture, whose mip extent is 4x4, and accepts one staging byte for a 4x4 R8G8B8A8_UNorm region requiring 64 bytes.
- Resolution: Added shared exact RHI format widths, selected-mip region checks, checked exact byte footprints, copy-usage and single-sample validation, factory-only upload records, and explicit allocation-failure mapping with maintained regressions.
- Verification: Exact parent reproduced oversized nonzero-mip and underfilled RGBA uploads; all-format footprint, subresource, unsupported/overflow and allocation-failure matrices, full local gates, and CI run 30207463089 at 5488528 all passed.
- Commit: `5830901`

## CR001-B05-F001: Command buffers retain device-owned diagnostics beyond device lifetime

- Severity: S1
- Status: Verified
- Requirement: 011-FR-016, 011-FR-017, and 011-SC-002 require shutdown invalidation and post-shutdown rejection without crashes
- Location: `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanCommandBuffer.h:84`
- Impact: A retained command buffer can access diagnostics after its owning device object has been destroyed, turning required post-shutdown rejection into undefined behavior instead of a deterministic result.
- Evidence: FVulkanDevice has no destructor that calls Shutdown; CreateCommandBuffer returns shared ownership while FVulkanCommandBuffer stores a raw pointer to the device Diagnostics member. Invalidate leaves that pointer intact, and every failed or successful recording transition may dereference it through MarkRecordingDiagnostic.
- Resolution: Device destruction now performs shutdown, command pools solely own command-buffer invalidation, and invalidated command buffers detach device diagnostics; retained command buffers deterministically reject later recording.
- Verification: Parent/current comparison confirms the diagnostics lifetime assumption was removed; the maintained retained-command regression passes through the full Debug test gate, and strict Debug/Release builds pass.
- Commit: `7e92de1`

## CR001-B05-F002: Command and render objects bypass device ownership and state authority

- Severity: S2
- Status: Verified
- Requirement: 011-FR-001 through 011-FR-006, 011-FR-011, and 011-FR-013 require device-mediated allocation, valid render objects, and queue/completion-owned lifecycle transitions
- Location: `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanCommandBuffer.h:26`
- Impact: Backend callers can bypass capability and description validation, forge in-flight completion state, or receive exceptions and partial tracking instead of the required explicit failure with no usable object.
- Evidence: The B05 inspection probe directly constructs a Present command pool that allocates successfully, directly constructs empty render-pass and framebuffer descriptions that report Valid, and advances an ended command buffer through Submitted and Resettable without a queue or completion object. Device factories also leave wrapper and tracking allocations outside explicit-result failure handling.
- Resolution: Command, submission, render-pass, and framebuffer construction or lifecycle transitions are owner-only; device, pool, and queue factories map wrapper or tracking allocation failure to Unavailable without publishing partial objects.
- Verification: Current headers close direct construction and state-transition access, maintained compile-time assertions pass in strict Debug and Release, and factory control-flow maps wrapper/tracking failures without publishing partial objects.
- Commit: `7e92de1`

## CR001-B05-F003: Texture transfer validation accepts invalid subresources and compatibility

- Severity: S2
- Status: Verified
- Requirement: 011-FR-009, 011-FR-020, and 011-SC-003 require valid texture-copy regions, resource compatibility, and upload/readback bounds
- Location: `Source/Backend/Vulkan/Private/FVulkanCommandBuffer.cpp:32`
- Impact: Invalid transfer records enter executable command buffers; native execution can address outside the selected subresource, misinterpret formats, or understate destination byte requirements after arithmetic wraparound.
- Evidence: The B05 inspection probe accepts an 8x8 copy and readback from mip 1 of an 8x8 texture whose selected extent is 4x4, and accepts a texture copy from R8G8B8A8_UNorm to R8_UNorm. The readback byte calculation at line 371 uses unchecked multiplication and a private duplicated format-width table.
- Resolution: Texture transfers now validate selected mip extents, compatible dimensions/formats/sample counts, valid destination resources, and checked exact buffer footprints through shared RHI helpers.
- Verification: Parent/current comparison confirms base-mip and unchecked-size paths were replaced; maintained selected-mip, incompatible-format, padded-footprint, overflow, and deferred null-readback regressions pass in the full Debug gate.
- Commit: `7e92de1`

## CR001-B05-F004: Vulkan queue submission is not failure-atomic

- Severity: S2
- Status: Verified
- Requirement: 011-FR-014 and the Feature 011 Submission Batch contract require failed validation to preserve submission observability and accept only consumable/signalable synchronization sets
- Location: `Source/Backend/Vulkan/Private/FVulkanQueue.cpp:93`
- Impact: A rejected submission can consume a producer signal, strand a command as submitted, increment successful submission statistics, or partially signal dependents, making frame ordering and recovery nondeterministic.
- Evidence: Submit consumes wait semaphores sequentially before validating the full set, then marks the command submitted and increments the count before validating all signal semaphores and the fence. A later wait or signal failure therefore leaves earlier synchronization objects or the command/count mutated; maintained RHI tests at RHICoreTests.cpp:1769-1809 require preflight failure without partial transition.
- Resolution: FVulkanQueue now preflights the complete command, wait, signal, and fence set, including validity, device provenance, duplicates, overlap, readiness, and signalability, before allocating tracking or mutating any object; accepted submission commits only no-fail internal transitions and increments the count last.
- Verification: Independent B05-S06 review confirmed full submission preflight precedes allocation and mutation, commit transitions are no-fail, maintained rejection tests preserve all inputs and the success counter, and fresh strict-debug, fallback-strict, and strict-release gates pass.
- Commit: `3dbfed0`

## CR001-B05-F005: Submission completion can diverge from command lifecycle

- Severity: S2
- Status: Verified
- Requirement: 011-FR-015, 011-FR-015a, and the Feature 011 completion contract require completed work to make submitted command buffers resettable and wait-idle to complete fallback submissions
- Location: `Source/Backend/Vulkan/Private/FVulkanCommandSubmission.cpp:20`
- Impact: Callers can observe successful completion while reset remains invalid, or permanently strand fallback command buffers after a transient not-ready/timeout observation, preventing deterministic reuse.
- Evidence: ObserveCompletion with any nonzero timeout sets Completed and returns Success without calling MarkCompletedOrResettable. Forced NotReady or Timeout changes the submission state away from Pending, while FVulkanQueue::WaitIdle only completes Pending entries, so those commands remain Submitted with no recovery path through queue wait-idle.
- Resolution: Completion observation and queue wait-idle now share one transition that marks the command Resettable before publishing Completed; retryable outcomes remain recoverable, nonzero timeout success completes the command, and later observation of an already completed submission is idempotently successful.
- Verification: Independent B05-S06 review confirmed completion publishes command-buffer Resettable state before Completed, retryable NotReady and Timeout outcomes remain recoverable through WaitIdle, observation is idempotent, maintained tests cover each transition, and fresh strict-debug, fallback-strict, and strict-release gates pass.
- Commit: `3dbfed0`

## CR001-B05-F006: Queue and synchronization wrappers bypass device ownership

- Severity: S2
- Status: Verified
- Requirement: 009-FR-009, 009-FR-017, and the Feature 009 data model require queues, fences, and semaphores to be device-created, device-owned, and invalidated on shutdown
- Location: `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanQueue.h:14`
- Impact: Callers can bypass capability checks and shutdown invalidation or mix objects from different devices, allowing otherwise invalid work to appear accepted and remain usable after the owning device closes.
- Evidence: FVulkanQueue and FVulkanFence expose public constructors and FVulkanSemaphore is publicly default-constructible. None carries a device owner identity, and Submit accepts any concrete FVulkan command/synchronization wrapper, including objects created by another device; directly constructed objects are absent from device shutdown tracking.
- Resolution: Queue, command pool/buffer, fence, and semaphore wrappers now share an active device owner identity, close construction to FVulkanDevice, reject cross-device composition, invalidate through owner shutdown, and map queue/fence/semaphore wrapper or tracking allocation failures to explicit Unavailable results without publishing partial objects.
- Verification: Independent B05-S06 review confirmed device-owned construction and shared owner provenance for queue, pool, command buffer, fence, and semaphore; foreign/stale objects reject composition, shutdown invalidates the owner first, factory failures do not publish partial wrappers, maintained tests cover provenance and shutdown, and fresh strict-debug, fallback-strict, and strict-release gates pass.
- Commit: `3dbfed0`

## CR001-B05-F007: Shader validation accepts malformed and wrong-stage SPIR-V

- Severity: S2
- Status: Verified
- Requirement: 012-FR-001, 012-FR-002, 012-FR-003a, 012-SC-002, and the Shader Module Contract
- Location: `Source/RHI/Public/RHI/FRHIShaderModuleDesc.h:106`
- Impact: Deterministic fallback can publish Valid shader modules and pipelines from unusable or wrong-stage payloads, so failures move from the documented creation boundary into later native pipeline creation or asset consumption and SC-002's malformed/wrong-stage rejection claim is unsupported.
- Evidence: IsValidRHIShaderBytecode checks only size >= 4, format text, and the magic word; it neither validates the complete SPIR-V header/instruction stream nor finds the declared entry point and execution model. Maintained MakeShaderDesc/ShaderDesc fixtures contain only four words, no complete five-word header or OpEntryPoint, yet device and mock factories accept them as valid vertex, fragment, and compute modules.
- Resolution: Added bounded SPIR-V header and instruction validation plus exact execution-model and entry-point matching; maintained fixtures now use valid modules and cover truncated, overrun, wrong-stage, and missing-entry cases.
- Verification: Independent B05-S09 review confirmed complete bounded SPIR-V header/instruction traversal and exact stage/entry-point matching; strict fallback tests passed truncated-header, instruction-overrun, wrong-stage, and missing-entry regressions.
- Commit: `d5f1714`

## CR001-B05-F008: Shader and layout objects bypass device ownership and provenance

- Severity: S2
- Status: Verified
- Requirement: 008-FR-017, 012-FR-002, 012-FR-018, T034-T035, and the Shader Module Contract
- Location: `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanShaderModule.h:11`
- Impact: Callers can bypass the authoritative device factory and compose cross-device dependencies; a future native handle would then be consumed by the wrong device. Allocation failure can also escape the RHI result contract, while factory publication and shutdown ownership cannot prove one authoritative device.
- Evidence: FVulkanShaderModule and FVulkanPipelineLayout expose public constructors and carry no device owner identity. Graphics and compute pipeline validation checks only dynamic type, lifecycle, stage, and metadata compatibility, so valid wrappers created directly or by another active FVulkanDevice are accepted. FVulkanDevice::CreateShaderModule also performs throwing MakeShared and vector insertion without mapping allocation or tracking failure to an explicit result.
- Resolution: Made Vulkan shader modules and pipeline layouts device-owned, retained owner identity, rejected cross-device composition, and made native/wrapper/tracking allocation failures rollback atomically.
- Verification: Independent B05-S09 review found device factories as the only shader/layout construction sites, owner checks on graphics/compute/descriptor composition, rollback on native/wrapper/tracking publication failure, and passing direct-construction/provenance/lifecycle regressions.
- Commit: `d5f1714`

## CR001-B05-F009: The RHI real-runtime shader contract is unreachable

- Severity: S2
- Status: Verified
- Requirement: 012-FR-001, 012-FR-003a, 012-SC-001, the Feature 012 clarification on runtime objects, and the Shader Module Contract
- Location: `Source/Backend/Vulkan/Private/FVulkanInstance.cpp:62`
- Impact: Renderer/RHI consumers can never obtain the documented real-runtime shader module or pipeline path, forcing actual rendering through duplicated backend-private contexts and leaving Feature 012's runtime validation and abstraction promise unfulfilled despite native Vulkan availability elsewhere in the process.
- Evidence: FVulkanInstance rejects every RealRuntime request as Unsupported because FVulkanDevice owns no native Vulkan device; the only active FVulkanDevice mode is explicit deterministic fallback. FVulkanDevice::CreateShaderModule contains no vkCreateShaderModule path and therefore its Runtime validation branch is unreachable. Native contexts create VkShaderModule separately but do not return IRHIShaderModule objects or satisfy the RHI factory contract.
- Resolution: Added an opt-in owner-safe native shader context used by the RHI factory; only wrappers retaining a real VkShaderModule report RealRuntime, while default device resources remain explicit deterministic fallback.
- Verification: Independent B05-S09 review traced actual vkCreateShaderModule ownership through the RHI factory and destruction on invalidation/shutdown; native-enabled maintained tests passed all four shader assertions while unrelated deterministic objects retained fallback truthfulness.
- Commit: `d5f1714`

## CR001-B05-F010: Pipeline wrappers bypass device authority and capability checks

- Severity: S2
- Status: Verified
- Requirement: 008-FR-017; 012-FR-006, FR-007, FR-013, FR-018, SC-004, and SC-006
- Location: `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanGraphicsPipeline.h:11`
- Impact: A caller can directly construct an unvalidated pipeline or bind a pipeline from another device/backend, bypassing the authoritative factory and shutdown ownership. A valid-enum but unsupported render-target format can also produce a nominally compatible pipeline, moving failure beyond the documented creation boundary.
- Evidence: FVulkanGraphicsPipeline and FVulkanComputePipeline have public constructors and no creating-device identity. FVulkanCommandBuffer::BindGraphicsPipeline and BindComputePipeline accept any valid-lifecycle RHI pipeline without backend type or owner checks. FVulkanDevice validates shader/layout provenance but CreateGraphicsPipeline never checks each color/depth attachment format against selected device capabilities; the existing ColorOnly adapter test constrains texture creation only.
- Resolution: Fixed by device-authoritative pipeline wrappers, selected-device attachment format validation, foreign-device binding rejection, and maintained regressions.
- Verification: Verified by parent/current static audit, private device-owned wrappers, format capability rejection, foreign-device binding rejection, strict Debug/Release/fallback gates, and sanitizer-backed maintained regressions.
- Commit: `00751c7`

## CR001-B05-F011: Pipeline cache can reuse non-equivalent or stale pipelines

- Severity: S2
- Status: Verified
- Requirement: 012-FR-016, FR-017, SC-007, Pipeline Cache and Reuse Contract
- Location: `Source/Backend/Vulkan/Private/FVulkanPipelineCache.cpp:17`
- Impact: Semantically different shader requests can collide, and corrected requests after dependency invalidation can be reported as successful cache hits while returning a pipeline tied to stale dependencies. This violates deterministic equivalence and the prohibition on reusing invalidated state.
- Evidence: AppendShaderKey serializes only stage, unescaped payload identity, and entry point, omitting shader interface metadata required by the contract and allowing delimiter collisions. FindGraphics and FindCompute validate only the cached pipeline lifecycle, not the lifecycle of its retained shader/layout dependencies. A replacement valid request with the same textual key can therefore return an old valid wrapper whose dependencies were invalidated.
- Resolution: Fixed by collision-safe complete cache serialization, interface metadata coverage, delimiter-safe identity encoding, stale dependency rejection, and maintained regressions.
- Verification: Verified by parent/current cache audit, complete length-delimited cache keys including interface metadata, stale dependency rejection, strict/fallback/sanitizer gates, and maintained cache regressions.
- Commit: `00751c7`

## CR001-B05-F012: Pipeline factories neither create native objects nor publish atomically

- Severity: S2
- Status: Verified
- Requirement: 012-FR-006, FR-008, FR-010, FR-017, SC-001, SC-008, and the Graphics/Compute Pipeline Contracts
- Location: `Source/Backend/Vulkan/Private/FVulkanDevice.cpp:891`
- Impact: RHI consumers cannot obtain the documented real graphics or compute pipeline even when Vulkan is available. Allocation or cache-publication failure can escape the explicit ERHIResult contract and leave a retained valid pipeline without a returned success, corrupting creation-limit and cache authority.
- Evidence: The RHI pipeline wrappers retain only descriptions and contain no native context or VkPipeline ownership. FVulkanDevice RealRuntime initialization remains Unsupported and native contexts create VkPipeline through separate non-RHI paths, so real-runtime pipeline factory branches are unreachable. Both factories also build allocating text keys, copy descriptions, allocate wrappers, append tracking, and insert cache entries without catches or rollback; cache insertion occurs after device tracking publication.
- Resolution: Fixed by native graphics/compute pipeline ownership in FVulkanNativeContext plus failure-atomic device publication and maintained native integration regressions.
- Verification: Verified by parent/current native ownership audit, real RHI graphics and compute pipeline native tokens, cleanup on invalidation/shutdown, strict/fallback/sanitizer gates, and maintained native integration regressions.
- Commit: `00751c7`

## CR001-B05-F013: Visible native frame failure paths leave acquired image or unsignaled fence state

- Severity: S2
- Status: Verified
- Requirement: Feature 011 FR-013 through FR-015 and SC-002/SC-004 require failed submissions and invalid transitions to avoid usable partial state and preserve consistent completion observation.
- Location: `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp:1291`
- Impact: A suboptimal acquire can permanently block the visible frame state behind bFrameAcquired, and a submit failure after fence reset can make the next frame-slot acquire wait until timeout. The visible native runtime can therefore convert recoverable resize/submit failures into persistent invalid-state or timeout behavior.
- Evidence: AcquireVisibleFrame returns ResizeRequired for VK_SUBOPTIMAL_KHR after setting bFrameAcquired=true; DrawVisibleFrame returns immediately for non-success acquire results. SubmitAndPresentVisibleFrame resets the frame-slot fence before vkQueueSubmit and clears bFrameAcquired on submit failure without restoring a signaled fence.
- Resolution: Fixed by suboptimal-acquire handoff through submit/present, acquired-frame abandon cleanup on record/reset/submit failure, signaled fence recreation after post-reset submit failure, and maintained visible failure lifecycle regression.
- Verification: Verified parent/current visible native frame failure lifecycle: suboptimal acquire continues through cleanup, record failure uses shared abandon path, submit failure recreates a signaled frame-slot fence, lifecycle regression passes, and strict/fallback/release/sanitizer gates pass. Refreshed tests gate failure is limited to current Mac MoltenVK/Metal environment boundary.
- Commit: `d5b83ac`

## CR001-B06-F001: Render graph execution resolves resources from culled branches

- Severity: S2
- Status: Verified
- Requirement: Feature 013 FR-012, FR-014, FR-015 and SC-007/SC-008 require output-based pass culling, execution of the compiled scheduled pass sequence, and rejection only for missing or invalid required imported resources.
- Location: `Source/Renderer/Private/FRenderGraphExecutor.cpp:76`
- Impact: A graph can compile with an unused branch correctly culled, yet still fail execution because a resource that cannot affect requested outputs is missing or transient resolution is simulated to fail. This makes culling incomplete at execution time and weakens render graph composition for optional debug/post-processing branches.
- Evidence: FRenderGraphCompiler culls unused passes by replacing ScheduledPasses with the required-only CulledSchedule, but FRenderGraphExecutor::Execute iterates Graph.Resources instead of resources reachable from Graph.GetCompiledGraph().ScheduledPasses. Missing imported bindings or transient resolution failure from a culled branch can therefore fail execution before any scheduled pass callback runs.
- Resolution: Fixed by deriving the execution resource set from compiled ScheduledPasses, skipping culled-branch resources during imported validation/transient resolution, and adding regression coverage for missing imports and transient resolution failures used only by culled passes.
- Verification: Verified parent/current executor behavior: parent resolved all Graph.Resources before scheduled pass execution, current derives RequiredResources from compiled ScheduledPasses and leaves culled resources unresolved. Regression tests cover culled missing import and culled transient resolution failure; fallback-strict, strict-release, and sanitizer gates pass.
- Commit: `7292a65`
