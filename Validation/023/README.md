# Feature 023 Validation

Feature 023 implements versioned Material and Shader Assets, deterministic
source definitions, repository shader migration, Renderer snapshots, and
bytecode-only native shader inputs.

## Local macOS Evidence

- Host: Apple silicon arm64, macOS, Apple Clang.
- Strict Debug build: passed.
- Strict Release build: passed.
- Full Debug regression: passed.
- Focused `asset-material-shader` and `renderer-material-asset`: passed.
- Existing material, forward, deferred, triangle, and Vulkan suites: passed.
- yyjson provenance, Asset architecture, repository inventory, and 19 Python
  verifier unit tests: passed.
- Release determinism: 40 valid and 40 invalid definitions, 20 repetitions,
  eight immutable readers, passed.
- Repository inventory: six Shader Programs, 11 GLSL sources, and 11 SPIR-V
  payloads, passed with exact digests.

Canonical corpus SHA-256:
`d9b2dbc96de3986c59752f8c3f99af452d48157805e5660c220cd212503cd64b`.

Failure corpus SHA-256:
`1ae4d6d742d5b0a52f62a16fe974dd2afdd751a99d83d49ccac6f12c4e7ad606`.

The normalized Release report is
`Validation/023/macOS/release-determinism.txt`.
The normalized repository inventory report is
`Validation/023/macOS/repository.txt`.

## Native Policy

The local sandbox reports that MoltenVK cannot access Metal, so macOS native
success is unavailable here. Runtime-independent lifecycle, failure, cleanup,
and explicit-unavailable contracts pass. Linux Lavapipe and supported
Windows/macOS runners remain the native-success authorities.

## Cross-Platform CI Evidence

GitHub Actions run
[30553736883](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/30553736883)
passed on 2026-07-30 for branch head
`f0e60cb3ec7a5a5cc7b07b1befcf8a48616fa870`.

All eight required jobs passed:

- Windows, macOS, and Linux headless Debug builds, architecture/provenance
  checks, focused Feature 023 suites, full regression, and applicable native
  availability/lifecycle gates;
- Windows, macOS, and Linux strict Release builds and 20-run determinism gates;
- Linux ASan/UBSan malformed-input, dependency, conversion, Renderer, and
  native lifecycle coverage;
- Linux ThreadSanitizer eight-reader immutable Asset and Renderer conversion
  coverage;
- Linux Lavapipe Vulkan, visible headless, and deferred native readback gates.

Debug and Release reports on all three platforms agree on 40 valid and 40
invalid definitions, 20 repetitions, eight immutable readers, and these exact
digests:

- canonical corpus:
  `d9b2dbc96de3986c59752f8c3f99af452d48157805e5660c220cd212503cd64b`;
- failure corpus:
  `1ae4d6d742d5b0a52f62a16fe974dd2afdd751a99d83d49ccac6f12c4e7ad606`.

The repository reports agree on six Shader Programs, 11 GLSL sources, and 11
SPIR-V payloads with exact-byte digest validation. The local focused
Asset/Renderer compatibility run contains 42 passed assertions and zero
failures; the same suites passed in every hosted Debug and Release job.

The normalized closeout summary is
`Validation/023/ci-run-30553736883.txt`. Hosted per-platform reports remain
available as run artifacts. All required local and hosted gates are complete.
Feature 023 is Done.
