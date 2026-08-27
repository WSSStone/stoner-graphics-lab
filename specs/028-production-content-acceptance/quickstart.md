# Quickstart: Feature 028 Production Content Acceptance

This quickstart defines the intended entry points. Commands are implemented and
made authoritative during Feature 028; no command may silently downgrade an
unsupported native gate to success.

## 1. Build and Run Focused Tests

```bash
conda run -n godot scons config=debug strict=1 -j8
conda run -n godot scons config=release strict=1 -j8
Build/Mac/Debug/Tests/StonerTest --suite production-content
```

On Windows and Linux, use the existing platform output path and the same suite
name. Debug, strict Release, and Linux sanitizer profiles remain required.

## 2. Verify the Corpus

```bash
python3 .github/scripts/verify_production_corpus.py \
  --manifest Content/ProductionAcceptance/Corpus/corpus-v1.json \
  --content-root Content/ProductionAcceptance \
  --tier regular \
  --output Build/Validation/028/corpus-regular.json
```

The regular command performs no network access and no license checks. It fails
before import when a file is missing, extra, altered, path-escaping, or
normalization-colliding.

Acquire and verify the medium package into an ignored cache:

```bash
python3 .github/scripts/acquire_production_corpus.py \
  --manifest Content/ProductionAcceptance/Corpus/corpus-v1.json \
  --package khronos-sponza-gltf \
  --content-root Content/ProductionAcceptance

python3 .github/scripts/verify_production_corpus.py \
  --manifest Content/ProductionAcceptance/Corpus/corpus-v1.json \
  --content-root Content/ProductionAcceptance \
  --tier medium \
  --output Build/Validation/028/corpus-medium.json
```

Acquisition uses only the manifest's pinned source revision and accepted file
hashes. Hash mismatch or unavailable required content fails the medium profile.

## 3. Run Regular Source-to-Cooked Acceptance

```bash
python3 .github/scripts/run_production_content_validation.py \
  --profile regular \
  --target-profile Config/AssetCooker/Profiles/Production/Mac-Metal-Arm64.json \
  --build-root Build/Mac/Release \
  --output Build/Validation/028/regular-metal \
  --timeout-seconds 600
```

The runner verifies corpus integrity, imports the explicit root, performs clean
and warm cooks, validates KTX2 producer/output evidence, publishes a generation,
makes source unavailable, loads the complete closure in strict-cooked mode,
checks semantic equivalence, realizes the model transactionally, runs 20 full
lifecycle cycles with cycles 1-2 as warm-up, and emits a schema-validated
report. Warm-up counts toward the 20-cycle total.

Use `Mac-Vulkan.json`, the applicable Windows Vulkan profile, or the Linux
profile for other lanes. A target/profile mismatch fails closed.

## 4. Run Medium Acceptance

```bash
python3 .github/scripts/run_production_content_validation.py \
  --profile medium \
  --target-profile Config/AssetCooker/Profiles/Production/Mac-Metal-Arm64.json \
  --build-root Build/Mac/Release \
  --output Build/Validation/028/medium-metal \
  --acquire-missing \
  --timeout-seconds 2400
```

Medium executes every admitted package through clean/warm 100-percent reuse,
strict no-source loading, semantic equivalence, and 1,000 lifecycle cycles with
cycles 1-20 as warm-up. It must finish within the declared 40-minute environment
budget and enforce RSS growth from the post-warm-up sample to the terminal
sample at most 16 MiB.

## 5. Run Hardware Image Acceptance

Before accepting a new workload camera, launch the calibration-only preview
against an already published strict generation:

```bash
Build/Mac/Release/Demo/StonerDemo/StonerDemo \
  --mode interactive \
  --backend metal \
  --workload production-content \
  --render-path deferred-full \
  --production-camera-preview \
  --camera-preset-output Build/Validation/028/camera/sponza-candidate.json \
  --cooked-root <publication-root> \
  --lease-root <lease-root> \
  --target-profile Config/AssetCooker/Profiles/Production/Mac-Metal-Arm64.json \
  --device-class-registry Config/Validation/ProductionContent/DeviceClasses.json \
  --production-root StaticModel:Sponza.gltf#idx.scene.0 \
  --strict-generation <generation> \
  --workload-revision production-content-sponza-v2
```

Use right-drag look, W/S/A/D/Q/E movement, Shift acceleration, wheel FOV,
`R` reset, Enter snapshot, and Escape exit. The emitted candidate is not an
accepted preset or image baseline. Copying a reviewed candidate into a new
workload revision is an explicit implementation/review action and requires the
complete image recalibration below. The preview renders a 1024-by-1024
calibration image and the hardware gate renders its 256-by-256 acceptance
image. Both use the same square aspect ratio, near/far planes, coordinate
convention, and frozen projection matrix. Resizing the window changes only the
aspect-preserving letterboxed presentation.

```bash
STONER_PRODUCTION_VISIBLE=1 \
python3 .github/scripts/run_production_content_validation.py \
  --profile hardware \
  --target-profile Config/AssetCooker/Profiles/Production/Mac-Metal-Arm64.json \
  --build-root Build/Mac/Release \
  --output Build/Validation/028/hardware-metal \
  --acquire-missing \
  --timeout-seconds 3600
```

Run the equivalent command for Vulkan on Windows and for Vulkan plus Metal on
the physical macOS lane. The command captures only the application window.
Semantic/readback probes run before FLIP. A missing exact workload/backend/
device-class baseline is failure, not an automatically created reference. The
runner derives the class by exact canonical capability-signature match; it does
not accept an arbitrary caller-provided class token.

## 6. Validate Reports and Privacy

```bash
python3 .github/scripts/run_production_content_validation.py \
  --verify-only Build/Validation/028/regular-metal \
  --target-profile Config/AssetCooker/Profiles/Production/Mac-Metal-Arm64.json
```

Verification checks schema, artifact digests, bounded ordering, deterministic
report separation, path/credential/native-identifier redaction, and window-only
capture metadata.

## 7. CI Entry Points

- `feature-028-production-content.yml`: relevant PR/push regular matrix and
  weekly/manual isolated per-package Intel Metal medium lanes with an
  exact-package aggregate gate. Arm64 Metal remains in the regular matrix and
  required physical M4 hardware closeout.
- `feature-028-production-hardware.yml`: explicit Windows Vulkan and macOS
  Vulkan/Metal hardware closeout/reference-change workflow.
- Local commands and CI call the same Python runner and profile files.

Before Feature 028 closes, retain passing final-revision evidence for Debug,
strict Release, sanitizer, deterministic 20-repeat reports, regular, medium,
Windows Vulkan hardware, and macOS Vulkan/Metal hardware.
