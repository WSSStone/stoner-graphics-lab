# CR-001: Pre-Asset Hardening Final Report

## Outcome

CR-001 audited and hardened Features 003-019 before the Asset layer begins in
Feature 020. The review closed with every accepted S0-S2 finding verified, all
current requirements classified, all required local and remote gates passing,
and two S3 maintainability items explicitly deferred.

- Frozen baseline: `9092a97593fb29cffbffdbe534e3dda143f463a5`
- Audit closeout head: `3f59801`
- Review branch: `codex/review-001-pre-asset-hardening`
- Pull request: [#4](https://github.com/WSSStone/stoner-graphics-lab/pull/4)
- Scope: production C++, tests, SCons, CI, demos, validation, architecture
  documents, roadmap, constitution, and Features 003-019
- Baseline inventory: 24,078 production lines and 7,445 test lines

This report is the durable mainline record. The completed `CodeReview/Runs`
snapshot was removed from the final file tree; its detailed checkpoint history
remains available through the review commits and pull request.

## Findings

| Severity | Verified | Deferred | Total |
| --- | ---: | ---: | ---: |
| S0 Blocker | 0 | 0 | 0 |
| S1 Critical | 5 | 0 | 5 |
| S2 Major | 75 | 0 | 75 |
| S3 Minor | 3 | 2 | 5 |
| **Total** | **83** | **2** | **85** |

The fixes covered:

- Build and CI warning discipline, strict Debug/Release profiles, sanitizers,
  and duplicate-workflow control.
- Core identity, allocation overflow, finite-safe math, transform truthfulness,
  logging concurrency, assertions, platform detection, files, dynamic modules,
  and ownership.
- RHI validation, lifecycle, failure atomicity, resource compatibility, texture
  subresources, shader metadata, pipelines, framebuffer semantics, and native
  execution truthfulness.
- Vulkan device and resource provenance, swapchain and queue state, allocation
  accounting, descriptors, uploads, synchronization, native shader/pipeline
  creation, and visible-frame recovery.
- Renderer graph culling, shader/material invalidation, deterministic ordering,
  ambient fallback, and material resource edges.
- Application event ingestion, window failure cleanup, input vocabulary and
  reset behavior, component validation, and transform propagation.
- Triangle/deferred integration startup cleanup, presentation recovery, graph
  declarations, local-light readback coverage, validation artifacts, test
  include boundaries, traceability, and documentation drift.

## Deferred Debt

### CR001-B09-F003: Focused test suite selection

`StonerTest` still runs through one aggregate entry point without a first-class
suite selector. Full-suite gates are correct and passing, but focused review
steps rely on environment skips or output filtering.

Target: test-runner ergonomics work before broad Asset pipeline validation.

### CR001-B09-F005: Native deferred execution function size

`FVulkanNativeOffscreenSession::Execute` still concentrates resource setup,
pipeline construction, command recording, readback, probe decoding, and oracle
composition in a large native validation path. Existing behavior is covered,
but future backend and asset validation changes would benefit from a dedicated
decomposition.

Target: native validation refactor before or alongside Feature 020/021
validation work.

## Traceability

The initial charter estimated 463 requirements and success criteria. The final
specification set extracted 466 records across Features 003-019. All 466 were
classified as `FeatureMapped`, with:

- no missing required columns;
- no duplicate trace IDs;
- implementation, API, test, and CI evidence for every row;
- consistent coverage of Features 003 through 019.

CodeGraph was rebuilt for the review worktree. The project C/C++ inventory under
`Source`, `Tests`, and `Demo` was fully indexed: 308 of 308 files, with no
missing or unexpected project C/C++ paths.

## Validation

Final local gates:

| Gate | Result |
| --- | --- |
| CLI unit tests | PASS |
| Debug build | PASS |
| Release build | PASS |
| Strict Debug | PASS |
| Strict Release | PASS |
| Strict graphics-disabled fallback | PASS |
| ASan + UBSan | PASS |
| Full test suite | PASS |

Native deferred validation was rerun three times at closeout. Each run exited
zero with 917 passing checks, zero failures, valid attachment readback, and no
remaining live native objects.

Final GitHub checks passed for:

- Linux, macOS, and Windows headless Debug build/test;
- Linux, macOS, and Windows strict Release build;
- Linux ASan + UBSan;
- reusable CR tool tests.

## Decisions

### Batch-boundary CI

Inspection and fix commits may remain local until a batch boundary. A coherent
cross-platform matrix runs once per completed batch unless a platform-sensitive
change justifies an earlier run.

### Truthful TRS operations

Transform composition, inverse, and relative conversion use explicit Try-based
APIs. Affine results that cannot be represented by editable TRS, including
shear from rotated non-uniform scale, are rejected instead of returning a
plausible but incorrect transform. Preserve-world hierarchy mutations validate
representability before changing the scene.

### Atomic logging thresholds

Category and global logging thresholds are relaxed atomics. The logging macro
combines their enabled-severity masks before evaluating format arguments, while
the internal log function retains a defensive global check.

## Readiness

CR-001 found no remaining accepted S0-S2 issue that blocks Feature 020. Feature
020 may begin from the merged review head, while the two deferred S3 items remain
visible roadmap-adjacent engineering debt.
