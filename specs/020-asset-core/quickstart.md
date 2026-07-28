# Quickstart: Asset Core, Identity & Registry

## Prerequisites

- Active branch: `020-asset-core`
- SCons 4.10.1
- Project-supported C++20 compiler
- No Vulkan SDK, display server, or graphics device is required for focused
  Feature 020 validation

The pinned Unicode source is repository-owned under `ThirdParty/utf8proc` and
is private to Core's `FUnicode` wrapper; a system installation is neither
required nor used.

## Build

Debug with strict project warnings:

```bash
scons config=debug strict=1
```

Release with strict project warnings:

```bash
scons config=release strict=1
```

## Discover Test Suites

macOS:

```bash
Build/Mac/Debug/Tests/StonerTest --list-suites
```

Linux:

```bash
Build/Linux/Debug/Tests/StonerTest --list-suites
```

Windows:

```powershell
Build\Win64\Debug\Tests\StonerTest.exe --list-suites
```

The output must contain `asset`.

## Run Focused Asset Validation

macOS:

```bash
Build/Mac/Debug/Tests/StonerTest --suite asset
```

Linux:

```bash
Build/Linux/Debug/Tests/StonerTest --suite asset
```

Windows:

```powershell
Build\Win64\Debug\Tests\StonerTest.exe --suite asset
```

The focused run covers canonical identity, NFC pairs, digest vectors,
collision-safe equality, typed soft references, atomic registry mutation,
unresolved/resolved dependency transitions, cycles, deterministic queries,
resolver/importer ambiguity, extension leases, diagnostics, and concurrent
registry access.

## Run Full Regression

Invoke `StonerTest` without arguments to preserve and verify all existing suites:

```bash
Build/Mac/Debug/Tests/StonerTest
```

Use the equivalent platform path above on Linux or Windows.

## Sanitizer Validation

Linux:

```bash
scons config=debug strict=1 sanitizers=address,undefined
Build/Linux/Debug/Tests/StonerTest --suite asset
Build/Linux/Debug/Tests/StonerTest
```

The focused stress cases must complete without sanitizer findings, hangs,
crashes, stale callbacks, or partial registry observations.

## Optional Registry Benchmark

The benchmark is a standalone, non-gating executable and is not run by
`StonerTest` or CI:

```bash
scons config=debug strict=1 asset_benchmark=1
Build/Mac/Debug/Tests/StonerAssetBenchmark
```

Use the equivalent platform output path on Linux or Windows. Timing output is
informational and is excluded from normalized deterministic test evidence.

## Expected Architecture Checks

- `Source/Core/Public/Core/FUnicode.h` and `Source/Asset/Public` expose no
  `utf8proc` header or type.
- `Source/Asset` depends only on Core; it does not compile or link `utf8proc`
  directly.
- `Source/Asset` includes no RHI, Renderer, Application, Backend, Tools, editor,
  or graphics API header.
- `Source/Asset/SConscript` declares Core as its only engine-layer dependency.
- Tests link Asset and can run without graphics runtime availability.

## CI Completion

Feature completion requires:

1. Windows, macOS, and Linux Debug jobs run `--suite asset`.
2. Existing no-argument deterministic/full regression paths remain passing.
3. Windows, macOS, and Linux Release strict builds pass.
4. Linux ASan/UBSan full suite passes.
5. No graphics artifact or visible screenshot is required.

## Implementation Outcome (2026-07-28)

Completed on macOS:

- strict Debug and Release builds;
- focused `asset` and `test-runner` suites;
- no-argument full regression;
- address/undefined sanitizer build plus focused and full regression;
- `python3 Tests/verify_asset_layer.py`;
- opt-in 10,000-record / 50,000-edge benchmark.

GitHub Actions run
[`30347149237`](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/30347149237)
passed Windows, macOS, and Linux Debug jobs, all three strict Release builds,
the focused Asset suite on each platform, the Linux ASan/UBSan full and focused
suites, and the Linux Lavapipe native validation paths.

## Next Workflow

Feature 020 is ready for final review and merge:

```text
gh pr checks 5
```
