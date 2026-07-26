# B04-S03: Instance, Adapter, Device, And Capabilities Verification

## Verification Target

This packet independently verifies the repairs committed at `c603f8a`:

- `CR001-B04-F001`: truthful real-runtime and explicit fallback reporting;
- `CR001-B04-F002`: owned, normalized, and deterministic adapter identity;
- `CR001-B04-F003`: exact selected-adapter format capabilities.

No production or maintained test implementation changed during verification.

## Exact-Parent Reproduction

The exact fix parent `0ad2209` was exported with `git archive` into an isolated
temporary directory and built with the strict graphics-disabled Debug profile.
The retained B04-S01 summary probe linked against those parent libraries and
reproduced all three repaired defects:

```text
fallback_masquerades_as_available=1
selected_identity_aliases_caller_storage=1
depth_capability_overclaimed=1
uninitialized_shutdown_reports_success=1
classification=instance-device-contract-defects
```

The fourth line remains the previously documented lifecycle decision gap and
is not part of this finding set. A separately compiled parent null-identity
probe terminated with exit code 134 under ASan, with a zero-address read
through `_platform_strlen`, `std::string_view(char const*)`, and the
`SelectBestAdapter` comparator. These parent results establish that the
verification method detects the original behavior rather than merely passing
on both revisions.

## Independent Current-Head Verifier

The B04-S03 verifier was written independently from the fix verifier and
linked against the current ASan/UBSan strict Debug libraries. It adds checks
for factory-level default behavior, temporary identity lifetime, exact format
normalization, rejection of a second unsupported color format, and 20 repeated
equal-score tie breaks.

Observed output:

```text
default_factory_truthful=1
fallback_factory_explicit=1
identity_lifetime_stable=1
empty_identity_safe=1
format_set_normalized=1
format_factory_exact=1
tie_break_stable=1
classification=instance-device-contracts-verified
```

The verifier exits zero only when every assertion holds. ASan and UBSan
reported no error.

## Call-Site And Authority Review

- The removed `GetDefaultVulkanSupportedFormats` symbol has no remaining
  source, test, or evidence-probe call site.
- The remaining no-argument `CreateVulkanDevice()` and default
  `Initialize()` calls are the intentional negative regression and retained
  inspection/verification probes that require real-runtime rejection.
- Maintained deterministic fixtures assign
  `EVulkanInstanceRuntimeMode::DeterministicFallback` explicitly.
- `FVulkanDevice::MapCapabilities` copies
  `Adapter.Formats.GetSupportedFormats()`, and texture creation validates the
  resulting device capability snapshot.
- Native runtime proof remains in `FVulkanNativeContext`; deterministic
  fallback does not satisfy a native gate.

## Fresh Gate Evidence

The current source state passed:

- `fallback-strict` at `2026-07-26T12:41:21+00:00`: strict
  graphics-disabled Debug build and complete maintained test executable;
- `sanitizers` at `2026-07-26T12:42:42+00:00`: strict ASan/UBSan Debug build
  and complete maintained test executable with the separately tracked optional
  deferred-native case skipped.

The sanitizer run still executed and passed native Vulkan instance/device,
offscreen triangle, frame-local release, and zero-live-object tests. The
optional deferred-native skip is governed by accepted
`CR001-B08-F001`; this packet neither waives nor verifies that finding.

Detailed commands and outputs are retained in
`Evidence/b04-instance-device-verification-probes.md`, the verifier source,
and the authoritative gate JSON records.

## Finding Decisions

- `CR001-B04-F001`: Verified.
- `CR001-B04-F002`: Verified.
- `CR001-B04-F003`: Verified.

The next packet may inspect the next B04 responsibility domain. No push or
GitHub Actions run occurred in this packet.
