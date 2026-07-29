# Quickstart: Image & Texture Asset Foundation

## Prerequisites

- Active branch: 021-image-texture-assets.
- SCons 4.10.1 and a project-supported C++20 compiler.
- No visible window or display server is required for Asset tests.
- Vulkan is optional for core decode/mip tests. Linux CI uses Lavapipe for
  supplementary native upload/readback only when that test is enabled.
- The decoder is repository-owned under ThirdParty/stb. Do not install a system
  image library for this feature.

## Build

Debug with strict project warnings:

~~~bash
scons config=debug strict=1
~~~

Release with strict project warnings:

~~~bash
scons config=release strict=1
~~~

## Run Focused Asset Validation

macOS:

~~~bash
Build/Mac/Debug/Tests/StonerTest --suite asset
~~~

Linux:

~~~bash
Build/Linux/Debug/Tests/StonerTest --suite asset
~~~

Windows:

~~~powershell
Build\Win64\Debug\Tests\StonerTest.exe --suite asset
~~~

The Asset suite includes Feature 020 identity/registry/dispatch coverage plus
Feature 021 source probes, a 12-or-more-file PNG/JPEG/HDR valid matrix, a
30-or-more-case bounded negative matrix, metadata/origin policy,
canonical-format validation, content digests, semantic mip chains, atomic
publication, mock-RHI realization, rollback, and failure diagnostics.

## Verify the Asset Boundary

~~~bash
python3 Tests/verify_asset_layer.py
~~~

Expected result: Asset depends only on Core; public Asset headers include no RHI,
Renderer, Backend, graphics API, or third-party decoder header; only the
approved private stb wrapper may include ThirdParty/stb/stb_image.h.

## Full Regression

~~~bash
Build/Mac/Debug/Tests/StonerTest
~~~

Use the matching Linux or Windows executable path. This retains existing Core,
RHI, Vulkan, Renderer, Application, and integration coverage.

## Sanitizer Validation

Linux:

~~~bash
scons config=debug strict=1 sanitizers=address,undefined
Build/Linux/Debug/Tests/StonerTest --suite asset
Build/Linux/Debug/Tests/StonerTest
~~~

ThreadSanitizer focused concurrency gate on Linux:

~~~bash
scons config=debug strict=1 sanitizers=thread
Build/Linux/Debug/Tests/StonerTest --suite asset
~~~

Fixture mutations and at-least-eight-request concurrent immutable-import cases
must finish without an ASan/UBSan/ThreadSanitizer report, hang, leak, or output
divergence.

## Expected CI Completion

1. Windows, macOS, and Linux Debug jobs build and run --suite asset.
2. All strict Release jobs build without unexplained warnings.
3. Linux ASan/UBSan passes the focused Asset suite and full regression; Linux
   ThreadSanitizer passes the focused Asset suite containing the concurrent
   import/mip test.
4. The Asset boundary verifier passes locally and in CI.
5. A 20-run deterministic fixture report agrees exactly on CPU metadata, payload
   bytes, mip bytes, content digests, and normalized diagnostics on all supported
   platforms.
6. Native Vulkan upload/readback, if enabled, accepts one-LSB UNorm, FP16
   `max(1e-3, abs(expected) * 1e-3)`, and FP32
   `max(1e-6, abs(expected) * 1e-6)` per-channel error; it is supplementary
   evidence and no visible screenshot is required.

## Useful Failure Checks

- Rename a valid fixture: content probing still chooses the correct importer.
- Truncate a fixture: expect TruncatedSource, no payload output, no registry mutation.
- Request sRGB for Normal or Data: expect validation failure before decode completes.
- Raise limits only through FImageImportLimits; zero/unbounded limits are rejected.
- Import one source as Color and Normal with different Texture IDs: source
  provenance may match, but Texture content evidence must differ.

## Validation Record

Local macOS validation at the current Feature 021 implementation HEAD:

- `scons config=debug strict=1`: passed.
- `Build/Mac/Debug/Tests/StonerTest --suite asset`: passed, including the
  Feature 020 regression aggregate and 24 Feature 021 image/texture assertions.
- `Build/Mac/Debug/Tests/StonerTest`: passed full regression.
- `scons config=release strict=1`: passed.
- macOS `sanitizers=address,undefined` focused Asset and full regression:
  passed without an ASan/UBSan report.
- `python3 Tests/verify_asset_layer.py`: passed.
- `--suite renderer-texture`, `--suite rhi`, and `--suite vulkan`: passed.
- `--suite vulkan-native`: the runtime reported unavailable explicitly because
  the local MoltenVK environment has no usable Metal device; no fallback result
  was accepted as native evidence.

GitHub Actions validation for PR #6 at
`3b506bf8523fcb8a99c508872de11b562f8ad393`:

- [CI run 30436673168](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/30436673168)
  completed successfully with all eight jobs passing.
- Windows, macOS, and Linux strict Debug jobs passed the focused Asset suite
  and the existing full regression suite.
- Windows, macOS, and Linux strict Release builds passed.
- Linux ASan/UBSan passed the focused Asset suite and full regression without a
  sanitizer report.
- Linux ThreadSanitizer passed the focused concurrent Asset gate without a
  sanitizer report, hang, or output divergence.
- Linux Lavapipe native-headless and deferred native-readback validation passed
  as supplementary graphics evidence. macOS native Vulkan remains explicitly
  unavailable on the local MoltenVK environment described above; no fallback
  result was counted as native evidence.

The final quickstart reconciliation found no conflict between the implemented
behavior, `spec.md`, `plan.md`, and `tasks.md`. All Feature 021 requirements and
success criteria have corresponding local or cross-platform CI evidence.
