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

Use `Mac-Metal-X86_64.json` only on the hosted Intel Metal lane. The offline
metallib finalizer is architecture-specific, so an Apple Silicon local replay
must use `Mac-Metal-Arm64.json`; target/host mismatch is structured
`Unsupported`, not a portable success.

Medium executes every admitted package through clean/warm 100-percent reuse,
strict no-source loading, and semantic equivalence. Its schema-v4 package
lifecycle assigns Lantern 1,000/20 endurance with 2,000 captures and Sponza
100/10 scale lifecycle with 200 captures. Every Sponza cycle still loads,
typed-decodes, realizes, renders, and releases the complete closure. Hosted
execution retains hard requirements for each package's exact cycle/capture
count, seven readbacks, native proof, zero terminal owners, and stale-handle
rejection. Package/profile/native operational timeouts are
2,400/2,700/1,800 seconds inside the 90-minute workflow job.

Hosted RSS, task-VM, allocator, peak-memory, and elapsed-time fields are reported
as observations and do not independently fail a completed correctness run. Do
not interpret this command as RSS or performance authority. The 16 MiB RSS hard
gate belongs only to preflighted maintainer-local physical runs.

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
navigation image, while every formal calibration capture, accepted reference,
and hardware-gate comparison uses the canonical 512-by-512 acceptance extent.
Both use the same square aspect ratio, near/far planes, coordinate convention,
and frozen projection matrix. Resizing the window changes only the aspect-
preserving letterboxed presentation; it cannot change the formal render target.
Formal authority additionally verifies an exact 512-by-512 native drawable.
On Retina/high-density displays only that authority window may adjust its
logical client size until the measured drawable is exact; the 1024 preview and
ordinary application windows keep their normal high-density behavior. No
capture is resized to make it match.

```bash
STONER_PRODUCTION_VISIBLE=1 \
python3 .github/scripts/run_production_content_validation.py \
  --profile hardware \
  --local-metal-authority \
  --target-profile Config/AssetCooker/Profiles/Production/Mac-Metal-Arm64.json \
  --build-root Build/Mac/Release \
  --output Build/Validation/028/hardware-metal \
  --acquire-missing \
  --timeout-seconds 3600
```

On the manually synchronized x86_64 Windows device, use PowerShell from the
same clean committed revision:

```powershell
python .github/scripts/run_production_content_validation.py `
  --profile hardware `
  --local-windows-vulkan-authority `
  --target-profile Config/AssetCooker/Profiles/Production/Windows-Vulkan.json `
  --build-root Build/Win64/Release `
  --output Build/Validation/028/hardware-windows-vulkan `
  --acquire-missing `
  --timeout-seconds 3600
```

These two local commands are the Feature 028 physical authorities. They must
pass on the same committed revision and each captures only the application
window. No self-hosted runner is required. macOS Vulkan remains deferred.
For hardware, `--timeout-seconds 3600` remains the per-package/native cap; the
two packages run serially under the profile-owned 7,800-second deadline.
Semantic/readback probes run before FLIP. A missing exact workload/backend/
device-class baseline is failure, not an automatically created reference. The
runner derives the class by exact canonical capability-signature match; it does
not accept an arbitrary caller-provided class token.

Formal comparison never translates, scales, crops, warps, resamples, or searches
for a best image alignment. Semantic material/normal/depth/lighting checks use
versioned bounded regions with minimum sample coverage rather than one exact
pixel. Calibration must reject an intentional one-pixel whole-image translation.
Each local physical lane must also prove its native OS/architecture/backend,
exclusive authority-lock ownership, clean committed HEAD, exact target/device
class, default production allocator behavior, and declared RSS/presentation
protocol before it can emit authoritative image evidence. Metal additionally
emits required 16 MiB RSS authority. Windows records the same working-set
endpoints and diagnostics as `observed`; RSS alone cannot fail an otherwise
complete Windows authority run.

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
  exact-package functional/lifecycle aggregate gate. Hosted RSS/timing remains
  observation/operational evidence. Arm64 Metal remains in the regular matrix;
  M4 Metal and Windows Vulkan are required maintainer-local closeout targets.
- No Feature 028 self-hosted workflow is required or registered: pushes do not
  remotely execute either maintainer device. The two local commands above are
  the physical image gates, the Metal RSS gate, and the Windows RSS observation.
- Local commands and CI call the same Python runner and profile files.
- Regular producer artifacts retain immutable attempt-suffixed names. During a
  failed-job rerun, each consumer resolves the newest non-expired artifact for
  its producer from the same workflow run at or before the current attempt,
  then performs the same target-profile and manifest/SHA-256 verification.
  Missing or ambiguous evidence remains a hard failure.

Before Feature 028 closes, retain passing final-revision evidence for Debug,
strict Release, sanitizer, deterministic 20-repeat reports, regular, medium,
and both same-revision maintainer-local physical hardware profiles.
