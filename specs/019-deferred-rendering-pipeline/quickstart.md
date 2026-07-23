# Quickstart: Deferred Rendering Pipeline

## Prerequisites

- Repository root is the working directory.
- SCons 4.10.1 and a C++20 compiler are available.
- On this macOS workspace, use the `godot` conda environment.
- Native Linux validation additionally requires Vulkan headers/loader, Mesa Lavapipe, Vulkan tools, and GLSL/SPIR-V tools.

## Build and Run Deterministic Coverage

macOS local workflow:

```bash
conda run -n godot scons -Q
Build/Mac/Debug/Tests/StonerTest
```

Linux:

```bash
scons -Q
Build/Linux/Debug/Tests/StonerTest
```

Windows from an MSVC developer prompt:

```powershell
scons -Q
Build\Win64\Debug\Tests\StonerTest.exe
```

The complete test executable must include deferred planner, surface-layout, material, directional/point/spot light, graph declaration, executor binding/command, comparison, and failure/cleanup coverage. Equivalent deterministic frame preparation is repeated 20 times and must produce byte-identical normalized reports.

## Inspect Deterministic Deferred Output

After implementation, run the deferred validation helper in deterministic mode:

```bash
python .github/scripts/run_deferred_validation.py \
  --profile deterministic \
  --tests Build/Mac/Debug/Tests/StonerTest \
  --output Build/Mac/Debug/Tests/deferred-deterministic-report.txt \
  --timeout-seconds 1200
```

Confirm the report includes:

- one valid surface layout identity;
- canonical pass/resource order;
- accepted/rejected draw counts;
- accepted/culled/rejected directional, point, and spot counts;
- transparent handoff count;
- normalized diagnostics;
- zero final deferred frame-owned objects.

## Run Linux Lavapipe Native Readback

Install dependencies on Ubuntu/Debian:

```bash
sudo apt-get update
sudo apt-get install -y libvulkan-dev mesa-vulkan-drivers vulkan-tools glslang-tools
```

Select Lavapipe explicitly and verify the adapter:

```bash
export VK_DRIVER_FILES="$(find /usr/share/vulkan/icd.d -name 'lvp_icd*.json' -print -quit)"
test -n "$VK_DRIVER_FILES"
vulkaninfo --summary
```

Build and execute required validation:

```bash
scons -Q
python .github/scripts/run_deferred_validation.py \
  --profile native-lavapipe \
  --tests Build/Linux/Debug/Tests/StonerTest \
  --readback-output Validation/019/Linux/deferred-readback-report.txt \
  --comparison-output Validation/019/Linux/renderer-comparison-report.txt \
  --timeout-seconds 1200
```

The native profile fails unless it proves real Vulkan through a software adapter, executes the deferred graph through RHI offscreen bindings under both standard-Z and reversed-Z, validates at least 12 named probes per convention, completes all four comparison tiers, and reports zero final deferred frame-owned objects.

## Inspect Native Probe Results

```bash
rg "^(runtime-mode|adapter|software-device|surface-layout|depth-convention|depth-clear|depth-compare|probe-count|probe |validation-result|final-live-objects)=" \
  Validation/019/Linux/deferred-readback-report.txt
```

Expected gates:

- final LDR color error `<= 2/255` per asserted channel;
- normalized depth error `<= 1e-4`;
- decoded world-normal dot product `>= 0.999`;
- metallic and roughness error `<= 1e-3`;
- 8-bit UNorm ambient-occlusion error `<= 2e-3`;
- no non-finite value;
- standard-Z uses far clear `1.0` with `ERHICompareOp::LessEqual`;
- reversed-Z uses far clear `0.0` with `ERHICompareOp::GreaterEqual`;
- at least 12 passing probes per convention;
- `validation-result=pass`;
- `final-live-objects=0`.

## Inspect the Performance Baseline

```bash
rg "^(tier|scene-fingerprint|measured-frames|forward-median|forward-p95|deferred-median|deferred-p95|crossover|comparison-result)=" \
  Validation/019/Linux/renderer-comparison-report.txt
```

The report must contain `0`, `16`, `64`, and `256` local-light tiers with at least 100 measured frames per strategy after warm-up. Timing does not decide pass/fail; missing tiers, mismatched fingerprints/workloads, incomplete samples, or non-finite timings do.

## Verify Deferred Shader Assets

When `glslangValidator` and `spirv-val` are installed:

```bash
find Source/Renderer/Shaders/Deferred -name '*.spv' -print -exec spirv-val {} \;
```

SCons must use checked-in SPIR-V when compilers are unavailable and regenerate or verify it when tools are present. Missing, malformed, wrong-stage, or interface-incompatible payloads must fail native initialization.

## Run GitHub CI and Download Artifacts

Push the feature branch, inspect CI, and download the latest successful reports:

```bash
git push -u origin 019-deferred-rendering-pipeline
gh run list --branch 019-deferred-rendering-pipeline --limit 5
gh run watch <run-id>
gh run download <run-id> --dir /tmp/stoner-019-artifacts
```

Required CI outcomes:

- Windows deterministic build/tests pass.
- macOS deterministic build/tests pass.
- Linux deterministic build/tests pass.
- Linux Lavapipe native deferred readback passes.
- Linux comparison artifact is complete, without requiring deferred to outperform forward.
- Existing triangle demo validation remains passing.

## Failure Triage

- `InvalidSurfaceLayout`: compare ordered semantic formats, extent, sample count, normal space, and depth convention.
- `InvalidMaterial`: inspect unsupported blend/domain/extension or missing required surface semantic.
- `InvalidLight`: inspect finite color/intensity/range/direction and radian spot cone ordering `0 <= inner <= outer < pi/2`.
- `InvalidBinding`: inspect descriptor layout, attachment order, index/readback usages, and object lifecycle.
- `RecordFailed`: inspect the first failed graph pass and symbolic command record.
- `ReadbackFailed`: confirm copy-source transition, destination capacity/host visibility, fence completion, and decode format.
- `ValidationFailed`: inspect the first failed named probe and semantic threshold.
- `ComparisonInvalid`: compare scene/view/material/light fingerprints and measured frame counts.

Do not convert missing Vulkan/Lavapipe, shader, readback, or probe support into success for the required Linux native profile.
