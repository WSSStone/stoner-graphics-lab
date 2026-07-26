# Findings

## CR001-B01-F001: macOS builds emit 33 Vulkan aggregate-initialization warnings

- Severity: S2
- Status: Accepted
- Requirement: CR-001 completion criterion: no unexplained compiler warnings before enabling -Werror or /WX
- Location: `Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp:240`
- Impact: Warning noise prevents warning-as-error enforcement and can hide newly introduced initialization defects.
- Evidence: Both clean Debug and Release gates pass but end with 33 -Wmissing-field-initializers warnings; see Evidence/gate-debug.json and gate-release.json.
- Resolution: pending
- Verification: pending
- Commit: `pending`

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
