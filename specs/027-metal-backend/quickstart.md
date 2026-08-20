# Quickstart: Native Metal Backend

Commands run from the repository root. Commands for code not yet implemented
become executable as their corresponding milestones land.

## 1. macOS Toolchain Doctor

```bash
Build/Mac/Debug/Tools/AssetCooker/StonerAssetCooker doctor \
  --target-profile Config/AssetCooker/Profiles/Mac-Metal-Arm64.json \
  --report Validation/027/reports/metal-toolchain-doctor.json
```

The current development Mac has Xcode but reports the optional Metal Toolchain
as missing. Install it explicitly before M2/native cook validation:

```bash
xcodebuild -downloadComponent MetalToolchain
```

The build and doctor must fail with an actionable message when the toolchain is
absent; implementation must never install it automatically.

## 2. Shared Build And Architecture Gates

```bash
conda run -n godot scons -j8 config=debug strict=1
conda run -n godot scons -j8 config=release strict=1
conda run -n godot python Tests/verify_architecture.py
conda run -n godot python Tests/verify_metal_backend.py --root . --mode architecture
conda run -n godot python -m unittest Tests/test_verify_architecture.py Tests/test_verify_metal_backend.py
```

Expected on Windows/Linux: no Apple framework/header lookup and no `.mm` compile;
API-free Metal selection and shared RHI contracts still compile. Expected on
macOS: Metal private units compile with ARC and architecture scans include them.
All macOS compile/link/finalization actions must record
`MACOSX_DEPLOYMENT_TARGET=12.0`; Objective-C++ availability diagnostics are
errors, and the verifier rejects a newer implicit target.

## 3. Deterministic Shader Derivation

```bash
conda run -n godot python .github/scripts/run_metal_validation.py \
  --root . \
  --tier deterministic \
  --repetitions 20 \
  --output Validation/027/reports/msl-derivation.json
```

Run on Windows, macOS, and Linux. All 20 repetitions and all three hosts must
produce identical normalized MSL and derivation-evidence digests for the same
SPIR-V fixtures. This tier does not count as native Metal execution.

## 4. Native Metal Shader Cook

Apple Silicon:

```bash
Build/Mac/Release/Tools/AssetCooker/StonerAssetCooker cook \
  --target-profile Config/AssetCooker/Profiles/Mac-Metal-Arm64.json \
  --source-root Content \
  --root ShaderProgram:Engine/Shaders/Triangle \
  --root ShaderProgram:Engine/Shaders/Deferred/Surface \
  --root ShaderProgram:Engine/Shaders/Deferred/Composition \
  --root ShaderProgram:Engine/Shaders/Deferred/DirectionalLight \
  --root ShaderProgram:Engine/Shaders/Deferred/PointLight \
  --root ShaderProgram:Engine/Shaders/Deferred/SpotLight \
  --root ShaderProgram:Engine/Shaders/Validation/NoOp \
  --output Build/Feature027Cook/arm64 \
  --ddc Build/Feature027Cook/DDC-arm64 \
  --report Validation/027/reports/cook-arm64.json
```

Intel uses `Mac-Metal-X86_64.json` and a distinct output root. Repeat each cook
20 times through the validation runner. Expected: identical payload identity,
dependency/version evidence, pipeline result, and normalized report. Exact
metallib bytes are compared only under identical recorded Apple toolchains.
Feature 027 intentionally cooks only its repository-owned shader roots; the
full production content closure, including material texture dependencies, is
owned by Feature 028.

The production runner performs the twenty isolated cooks, strict generation
loads, and native graphics/compute pipeline creations required by T089:

```bash
conda run -n godot python .github/scripts/run_metal_native_cook.py \
  --root . \
  --cooker Build/Mac/Release/Tools/AssetCooker/StonerAssetCooker \
  --tests Build/Mac/Release/Tests/StonerTest \
  --profile Config/AssetCooker/Profiles/Mac-Metal-Arm64.json \
  --work Build/Feature027Validation/native-cook-arm64 \
  --output Validation/027/reports/us3-native-cook-determinism.json \
  --repetitions 20 \
  --timeout-seconds 1800
```

Windows/Linux final-cook attempts must return `HostUnsupported`, publish no
generation, and leave existing `Current.json` untouched.

## 5. Focused Contract Suites

```bash
Build/Mac/Debug/Tests/StonerTest \
  --suite metal-device \
  --suite metal-resource \
  --suite metal-pipeline \
  --suite metal-command \
  --suite metal-shader-cooker \
  --suite metal-native
```

Descriptor coverage is part of `metal-pipeline`; synchronization coverage is
part of `metal-command` and `metal-native`. Expected: the extracted public API matches
`contracts/rhi-operation-matrix.md`; every frozen operation passes native
conformance, or returns `Unsupported` only when the selected device's published
capabilities prove a genuine hardware limitation.

## 6. Native Offscreen Validation

After the production cook in section 4, run the shared triangle/deferred
acceptance entrypoint. It requires independent GPU readback from Metal and
Vulkan/MoltenVK and writes the four normalized User Story 4 reports:

```bash
conda run -n godot python .github/scripts/run_metal_render_acceptance.py \
  --root . \
  --tests Build/Mac/Release/Tests/StonerTest \
  --demo Build/Mac/Release/Demo/StonerDemo/StonerDemo \
  --profile Config/AssetCooker/Profiles/Mac-Metal-Arm64.json \
  --publication Build/Feature027Validation/native-cook-arm64/run-00/Cooked \
  --lease Build/Feature027Validation/render-acceptance-lease \
  --output-dir Validation/027/reports \
  --work Build/Feature027Validation/render-acceptance \
  --timeout-seconds 1800
```

The Metal triangle must consume the strict-cooked generation. The comparison
report is accepted only when the C++ probe emits independent Metal and Vulkan
native evidence under `metal-vulkan-tolerance-v1`.

```bash
conda run -n godot python .github/scripts/run_metal_validation.py \
  --root . \
  --tests Build/Mac/Release/Tests/StonerTest \
  --profile Config/AssetCooker/Profiles/Mac-Metal-Arm64.json \
  --tier native-offscreen \
  --work Build/Feature027Validation/native-offscreen \
  --output Validation/027/reports/native-offscreen-arm64.json \
  --timeout-seconds 1800
```

Expected: graphics, compute, transfer, barriers, synchronization, triangle, and
deferred probes contain real GPU readback, device/capability evidence, and
strict-cooked Metal-library evidence. No semantic oracle is recorded as output.

## 7. Visible Presentation Acceptance

Before the long visible acceptance, run the bounded layer/lifecycle smoke. This
clear-only probe intentionally has no shader payload; it proves real device,
drawable presentation, resize/minimize/restore recovery, layer detach, and zero
terminal ownership:

```bash
conda run -n godot python .github/scripts/run_metal_validation.py \
  --root . \
  --tests Build/Mac/Release/Tests/StonerTest \
  --presentation-probe Build/Mac/Release/Tests/MetalPresentationProbe \
  --tier visible-manual \
  --workload presentation-smoke \
  --smoke-frames 120 \
  --smoke-cycles 4 \
  --work Build/Feature027Validation/presentation-smoke \
  --output Validation/027/reports/us2-presentation-smoke.json \
  --timeout-seconds 600
```

The probe must record exactly one native Metal device, at least 120 presented
frames and four completed lifecycle cycles, clean layer/device/window shutdown,
and zero live presentation/in-flight ownership.

```bash
conda run -n godot python .github/scripts/run_metal_validation.py \
  --root . \
  --tests Build/Mac/Release/Tests/StonerTest \
  --demo Build/Mac/Release/Demo/StonerDemo/StonerDemo \
  --profile Config/AssetCooker/Profiles/Mac-Metal-Arm64.json \
  --publication Build/Feature027Validation/native-cook-arm64/run-00/Cooked \
  --lease Build/Feature027Validation/visible-acceptance-lease \
  --capture Validation/027/captures/visible-metal-arm64.png \
  --visible-frames 30000 \
  --visible-cycles 20 \
  --tier visible-manual \
  --work Validation/027/captures/local-mac \
  --output Validation/027/captures/visible-acceptance.json \
  --timeout-seconds 7200
```

The runner asks the Application window driver to execute 20 real resize,
minimize, and restore cycles while the native demo is presenting. The
30,000-frame budget leaves enough time to inspect and capture the visible output
on a non-throttled presentation loop; the acceptance threshold remains 3,000
frames. Exercise focus and scale/display movement manually where available, then
allow clean close. Accept only with at least 3,000 frames, 20 completed cycles, one
correctly oriented capture, zero unrecovered presentation error, clean layer
detach, and exit code 0. The runner drives the bounded demo and validates its
report; save and inspect the oriented capture separately in the same directory.

## 8. Metal/Vulkan Comparison

```bash
conda run -n godot python .github/scripts/run_metal_validation.py \
  --root . \
  --tests Build/Mac/Release/Tests/StonerTest \
  --tier cross-backend \
  --work Build/Feature027Validation/comparison \
  --output Validation/027/reports/metal-vulkan-comparison.json \
  --timeout-seconds 1800
```

Expected: both backends are explicitly selected and independently native;
triangle and deferred outputs meet declared orientation, color, depth,
world-space-normal, and image tolerances. A failed Metal run cannot be replaced
by MoltenVK.

## 9. Failure And Lifecycle Stress

```bash
conda run -n godot python .github/scripts/run_metal_validation.py \
  --root . \
  --tests Build/Mac/Release/Tests/StonerTest \
  --tier deterministic \
  --workload failure \
  --repetitions 20 \
  --output Validation/027/reports/failure-determinism.json

conda run -n godot python .github/scripts/run_metal_validation.py \
  --root . \
  --tests Build/Mac/Release/Tests/StonerTest \
  --tier deterministic \
  --workload lifecycle \
  --lifecycle-iterations 10000 \
  --output Validation/027/reports/lifecycle-stress.json
```

Expected: every injection reaches its declared terminal state, no partial object
is published, and all native/in-flight counters return to zero. Iterations
1-1,000 warm up; RSS is sampled every 100 iterations from 1,100 through 10,000.
The final-ten median may exceed the first-ten post-warm-up median by at most
`max(16 MiB, 5%)`; the report retains all samples and computed values.

## 10. GitHub Actions

```bash
gh workflow run feature-027-metal-backend.yml --ref 027-metal-backend
gh run list --workflow feature-027-metal-backend.yml --branch 027-metal-backend --limit 1
gh run watch RUN_ID --exit-status
gh run download RUN_ID --dir Validation/027/CI/downloaded
gh run view RUN_ID --json databaseId,headSha,status,conclusion,jobs

gh workflow run feature-027-metal-hardware.yml --ref 027-metal-backend
gh run list --workflow feature-027-metal-hardware.yml --branch 027-metal-backend --limit 1
gh run watch HARDWARE_RUN_ID --exit-status
gh run download HARDWARE_RUN_ID --dir Validation/027/CI/hardware
```

GitHub only permits `workflow_dispatch` for workflows already registered on the
default branch. Before Feature 027 is merged, the first hardware run is created
by pushing a change to `feature-027-metal-hardware.yml` on `027-metal-backend`;
use the `gh run list`, `watch`, and `download` commands above for that run. After
the workflow exists on the default branch, use the explicit `gh workflow run`
command for subsequent branch or closeout runs.

Expected: the six Windows/macOS-arm64/Linux Debug/strict Release jobs, Linux
ASan/UBSan and TSan, and two `macos-26-intel` build/cook jobs pass. Each macOS
job records its Metal-device probe. A runner without a device reports native
execution as unavailable. The separate full-workload workflow must pass the
physical M4 Pro self-hosted arm64 job and GitHub-hosted `macos-26-intel`
x86_64 job; each fails on an unavailable probe and records the native device,
target profile, compiler, GPU readback, and artifact digests. A physical Intel
Mac run is optional. The physical M4 job additionally owns the required
Metal/Vulkan comparison. The hosted Intel job runs the full Metal-only workload
because its paravirtual Metal device does not provide a usable MoltenVK device.

## 11. Final Planning/Implementation Gate

```bash
git diff --check
rg -n "NEEDS CLARIFICATIO[N]|TOD[O]|TB[D]|\[FEATUR[E]\]|\[##[#]" \
  specs/027-metal-backend/spec.md \
  specs/027-metal-backend/plan.md \
  specs/027-metal-backend/research.md \
  specs/027-metal-backend/data-model.md \
  specs/027-metal-backend/quickstart.md \
  specs/027-metal-backend/tasks.md \
  specs/027-metal-backend/contracts
python -m json.tool specs/027-metal-backend/contracts/metal-shader-evidence.schema.json >/dev/null
python -m json.tool specs/027-metal-backend/contracts/metal-validation-report.schema.json >/dev/null
git status --short
```

Expected: no unresolved planning marker, invalid JSON, malformed whitespace,
unexplained output, or untracked validation artifact.
