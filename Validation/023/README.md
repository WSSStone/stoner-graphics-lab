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

## CI Status

Windows, macOS, Linux, ASan/UBSan, ThreadSanitizer, and applicable native gates
are configured in `.github/workflows/ci.yml` but have not yet run for the
current uncommitted implementation. Feature 023 remains In Progress until that
remote evidence passes and is recorded here.
