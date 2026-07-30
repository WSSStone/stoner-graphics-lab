# Quickstart: KTX2 Cooking & Compression

## Prerequisites

- Active branch: `022-ktx2-cooking-compression`.
- SCons 4.10.1 and a project-supported C++20 compiler.
- Repository-owned KTX-Software, WAMR, and encoder WebAssembly files present
  under `ThirdParty/`; do not install a system libktx for engine builds.
- Python 3 for architecture and external-validator adapters.
- Vulkan is optional for core CPU/mock tests. Native compressed-texture evidence
  runs only when the device advertises the required format usage.
- Pinned `ktx` 4.4.2 CLI is optional locally but mandatory in CI.

## Verify Third-Party Provenance

```bash
python3 Tests/verify_ktx2_provenance.py
```

Expected: KTX-Software, WAMR, and encoder module versions, licenses, and
SHA-256 values match their checked-in provenance records. This command does not
download or execute remote content.

## Build

Debug with strict project warnings:

```bash
scons config=debug strict=1
```

Release with strict project warnings:

```bash
scons config=release strict=1
```

Third-party translation units use scoped warning settings. Project wrappers,
public headers, and integration code remain under strict warnings.

## Focused Validation

Run the fast authoritative encoder digest gate first:

```bash
Build/Mac/Debug/Tests/StonerTest --suite asset-ktx2-encoder
```

It executes ETC1S and UASTC 20 times through the checked-in WebAssembly module,
requires exact profile digests, and reopens each result with private libktx.
Windows and Linux CI run the same gate before the broader Asset suite.

macOS:

```bash
Build/Mac/Debug/Tests/StonerTest --suite asset
Build/Mac/Debug/Tests/StonerTest --suite renderer-texture
Build/Mac/Debug/Tests/StonerTest --suite rhi
Build/Mac/Debug/Tests/StonerTest --suite vulkan
```

Linux:

```bash
Build/Linux/Debug/Tests/StonerTest --suite asset
Build/Linux/Debug/Tests/StonerTest --suite renderer-texture
Build/Linux/Debug/Tests/StonerTest --suite rhi
Build/Linux/Debug/Tests/StonerTest --suite vulkan
```

Windows:

```powershell
Build\Win64\Debug\Tests\StonerTest.exe --suite asset
Build\Win64\Debug\Tests\StonerTest.exe --suite renderer-texture
Build\Win64\Debug\Tests\StonerTest.exe --suite rhi
Build\Win64\Debug\Tests\StonerTest.exe --suite vulkan
```

The Asset suite includes Features 020-022. Feature 022 coverage includes policy,
canonical metadata, 18 or more valid artifacts, 40 or more malformed mutations,
reopen/load, request-scoped transcode, exact level footprints, quality/size,
leases, and eight-way concurrency. Renderer/RHI/Vulkan suites cover the 36-case
capability matrix, block-aware upload, terminal mips, rollback, mappings, and
usage queries.

## Verify Asset Boundaries

```bash
python3 Tests/verify_asset_layer.py
```

Expected: Asset production code depends only on Core. Public Asset headers
contain no RHI, Renderer, Backend, Vulkan, KTX, WAMR, or encoder ABI include.
Only approved private adapters may include vendored headers.

## Verify the Independent KTX2 Oracle

Confirm the pinned tool:

```bash
ktx --version
```

Emit artifacts from the current implementation:

```bash
Build/Mac/Debug/Tests/StonerTest \
  --suite asset \
  --emit-ktx2-dir Validation/022/generated
```

Use the platform-matching executable on Linux/Windows. Then validate generated
and golden artifacts:

```bash
python3 Tests/verify_ktx2_artifacts.py \
  --ktx ktx \
  --expected-version 4.4.2 \
  --input Validation/022/generated \
  --input Tests/Fixtures/KTX2/Golden \
  --report Validation/022/ktx-validate.json
```

The adapter invokes `ktx validate --format json --warnings-as-errors` without a
shell. The pinned validator reports legal custom KTX2 keys as diagnostic 7010;
the adapter accepts only the complete, duplicate-free set of nine declared
`stoner.*` keys and rejects every other warning/error. A missing local `ktx`
may report a supplementary skip; CI must not skip it.

## Determinism and Quality Report

```bash
Build/Mac/Debug/Tests/StonerTest \
  --suite asset \
  --ktx2-determinism-runs 20 \
  --report Validation/022/determinism.json
```

Use the platform-matching executable on Linux/Windows. Expected:

- exact artifact bytes, digest, metadata, reopened level descriptions, and
  diagnostics across all 20 runs;
- report hashes equal checked-in golden hashes shared by Windows, macOS, and
  Linux;
- ETC1S/UASTC size thresholds pass on base extents at least 64x64;
- color PSNR and normal angular-error thresholds pass.

A native Basis encoder comparison may be emitted as research evidence but
cannot replace or satisfy the authoritative WebAssembly-encoder digest gate.

## Full Regression

```bash
Build/Mac/Debug/Tests/StonerTest
```

Use the matching Linux or Windows executable. All existing Core, Asset, RHI,
Vulkan, Renderer, Application, and integration tests remain required.

## Sanitizer Validation

Linux ASan/UBSan:

```bash
scons config=debug strict=1 sanitizers=address,undefined
Build/Linux/Debug/Tests/StonerTest --suite asset
Build/Linux/Debug/Tests/StonerTest --suite renderer-texture
Build/Linux/Debug/Tests/StonerTest --suite rhi
Build/Linux/Debug/Tests/StonerTest --suite vulkan
Build/Linux/Debug/Tests/StonerTest
```

Focused ThreadSanitizer:

```bash
scons config=debug strict=1 sanitizers=thread
Build/Linux/Debug/Tests/StonerTest --suite asset
```

Expected: malformed inputs, WAMR module traps, concurrent libktx operations, and
failure injection complete without sanitizer report, hang, leak, partial
payload, or output divergence.

## Native Vulkan Evidence

Linux CI with Lavapipe or a compatible physical Vulkan device:

```bash
Build/Linux/Debug/Tests/StonerTest --suite vulkan-native
```

The suite compares the advertised compressed-format usage with native image
creation, exact logical mip/block footprint upload, and raw-block readback. A
capability/behavior disagreement fails. If a local device exposes none of the
required compressed formats, the suite records that support as unavailable and
passes only that explicit branch. Linux CI provisions Lavapipe and therefore
expects the native path to execute. Deterministic fallback is not native
evidence.

## Expected CI Completion

1. Windows, macOS, and Linux strict Debug jobs run focused Asset,
   Renderer-texture, RHI, Vulkan, architecture, and full regression gates.
2. All three strict Release builds pass without unexplained project warnings.
3. Golden deterministic hashes match across all hosts for 20 repeated cooks.
4. A pinned independent `ktx validate` 4.4.2 job accepts every valid artifact.
5. Linux ASan/UBSan passes focused and full suites; focused ThreadSanitizer
   passes at least eight concurrent immutable requests.
6. Available Linux Vulkan evidence agrees with reported sampled-image and
   copy-destination capabilities.

## Useful Failure Checks

- Mutate one level offset into metadata: expect `MalformedContainer` before
  libktx image-data allocation.
- Set `KTXorientation=ru`: expect a container/semantic contradiction.
- Request ETC1S for Data: expect validation failure before encoder execution.
- Request UASTC Data without lossy opt-in: expect validation failure.
- Advertise BC7 without CopyDestination: selection must continue to the next
  profile candidate.
- Upload a 1x1 BC7 mip with fewer than 16 bytes: RHI must reject it.
- Inject failure at mip N transcode/upload: expect no target payload or RHI
  texture and exactly one release of request-owned state.
- Reorder capability records: selected format and diagnostics must not change.
