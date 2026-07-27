# B05-S08 Local Evidence

## Implementation

- Commit: `d5f1714`
- Scope: bounded SPIR-V validation, device provenance, failure-atomic
  publication, optional native RHI shader ownership, maintained regressions,
  and Feature 012 contract migration.

## Strict Debug

Profile:

```text
crctl gate strict-debug
```

Result: exit 0 at `2026-07-27T03:35:07Z`. Project sources and maintained tests
compiled with warnings treated as errors.

## Strict Deterministic Tests

Profile:

```text
crctl gate fallback-strict
```

Result: exit 0 at `2026-07-27T03:33:34Z`. The graphics-disabled strict Debug
build and complete maintained test executable passed, including the new
structure, stage, entry-point, direct-construction, provenance, and lifecycle
regressions.

## Strict Release

Profile:

```text
crctl gate strict-release
```

Result: exit 0 at `2026-07-27T03:34:03Z`. Project sources and maintained tests
compiled with warnings treated as errors.

## Graphics-Enabled Native Observation

The maintained native test executable was run after the final strict Debug
build. It returned exit 1 only for:

- `Deferred native validation completes a real Vulkan submission`;
- `Mapped attachment probes are finite, unique, and within semantic tolerances`;
- `Deferred native validation passes semantic probes and releases frame-owned objects`.

Those assertions are already tracked by accepted finding
`CR001-B08-F001`. The following B05 native shader assertions all passed:

- owner-safe native shader runtime enablement;
- retained real native shader module through the RHI factory;
- execution-model mismatch rejection before runtime creation;
- native shader destruction on explicit invalidation and device shutdown.

## Repository Checks

- `git diff --check`: exit 0.
- Obsolete four-word payloads remain only in explicit truncated-header negative
  tests.
- Only the device factory constructs `FVulkanShaderModule` and
  `FVulkanPipelineLayout`.
- F007, F008, and F009 moved from Accepted to Fixed at `d5f1714`.
- Production/test commit contains no CR state or report files.
- No debugger, custom probe, remote CI, or network operation was used.
