# Findings

## CR001-B01-F001: macOS builds emit 33 Vulkan aggregate-initialization warnings

- Severity: S2
- Status: Fixed
- Requirement: CR-001 completion criterion: no unexplained compiler warnings before enabling -Werror or /WX
- Location: `Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp:240`
- Impact: Warning noise prevents warning-as-error enforcement and can hide newly introduced initialization defects.
- Evidence: Both clean Debug and Release gates pass but end with 33 -Wmissing-field-initializers warnings; see Evidence/gate-debug.json and gate-release.json.
- Resolution: Replaced partial Vulkan aggregate initialization with zero-initializing typed helpers and cleared project-owned cross-platform warnings; strict local Debug and Release builds pass.
- Verification: pending
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
- Status: Fixed
- Requirement: CR-001 completion requires Debug, Release, ASan/UBSan, clean warnings, and three-platform CI evidence
- Location: `site_scons/BuildConfig.py:12`
- Impact: Undefined behavior and Release-only defects can merge undetected, and the planned clean-warning policy is not mechanically enforced.
- Evidence: BuildConfig exposes only debug/release flags without sanitizer or strict-warning options; CI line 51 runs only default Debug.
- Resolution: Added validated strict and ASan/UBSan build controls, allow-listed CR gates, three-platform strict Release CI, and Linux sanitizer CI; all local profiles pass.
- Verification: pending
- Commit: `54e2599`

## CR001-B01-F003: CI runs duplicate three-platform matrices for pull-request branch pushes

- Severity: S3
- Status: Fixed
- Requirement: CR-001 Git and validation protocol requires meaningful batch-boundary evidence without redundant jobs
- Location: `.github/workflows/ci.yml:3`
- Impact: Every review push doubles hosted CI time, delays feedback, and makes check evidence harder to associate with the current head.
- Evidence: The workflow subscribes unconditionally to both push and pull_request; PR #4 produced two Linux, two macOS, and two Windows jobs for the same update.
- Resolution: Restricted branch push validation to master, retained pull-request validation, and ignored pure CodeReview/Runs state changes while preserving tool and engine CI.
- Verification: pending
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
