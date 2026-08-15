# Feature 026 Validation

Status: **in progress**

This directory owns compact, normalized evidence for Runtime Asset Manager
contracts, development/cooked equivalence, request lifecycle, cache ownership,
generation reader leases, completion delivery, stress, and cross-platform
gates.

Tracked evidence must record the command, source revision, fixture digest,
stable result counts, and deterministic output digests. Absolute host paths,
timestamps, process/thread IDs, native handles, and scheduler addresses are
excluded from normalized comparisons. Timing and RSS belong in a clearly
separated telemetry section.

Generated publication roots, lease coordination roots, payload caches, and raw
logs belong under `Validation/026/work/`, `Saved/Feature026*`, or CI artifacts;
they are not committed.

## Required Evidence

- contract and architecture verification
- development/cooked equivalence and mutation rejection
- coalescing, cancellation, retention, shutdown, and determinism matrices
- cross-process generation reader/exclusive maintenance ownership
- 1,000-asset/5,000-edge scale and Apple M4 Pro reference benchmarks
- Debug, strict Release, sanitizer, and eight-job cross-platform CI summaries
