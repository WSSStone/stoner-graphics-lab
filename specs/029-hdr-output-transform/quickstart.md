# Quickstart: Feature 029 HDR Output Transform

This guide uses the implemented command surfaces. Feature 029 separates four
authorities: deterministic contracts, native non-visual execution, exact SDR
image review, and macOS Metal live HDR review. No command silently falls back to
SDR, admits a Candidate, or authors a human HDR decision.

Before formal capture, freeze one implementation SHA, check out that exact
revision, rebuild, and recook the platform-specific closure. M4 and Windows
must use the same software SHA. Keep preliminary working-tree evidence in place
and choose fresh output directories. Subsequent evidence-only commits record
the tested SHA, not their own storage commit. See
[Windows handoff](windows-handoff.md) for the physical Vulkan lane.

## 1. Build and Run the Deterministic Suites

```bash
python3 -m SCons config=debug strict=1
python3 -m SCons config=release strict=1

Build/Mac/Release/Tests/StonerTest \
  --suite renderer-output-transform-math \
  --suite output-device-profile \
  --suite renderer-output-transform \
  --suite renderer-post-process-graph \
  --suite renderer-post-process-insertion \
  --suite rhi-presentation-output \
  --suite output-presentation-lifecycle
```

Use `Build/Win64/...` or `Build/Linux/...` on Windows or Linux. These suites
cover the three SDR curves, the versioned ACES 2 HDR viewing transform, manual
exposure, insertion ordering, typed graph execution, output profiles,
capability resolution, resize, failure, and ownership. They do not prove HDR
appearance.

Verify the frozen CPU authority and checked-in vectors independently:

```bash
python3 .github/scripts/verify_output_transform_vectors.py \
  --profiles Config/Validation/OutputTransform/Profiles.json \
  --vectors Tests/Fixtures/OutputTransform/vectors-v1.json \
  --output Build/Validation/029/vectors.json
```

Expected values are read-only. The verifier rejects provenance, constants,
tolerance, quantization, repeatability, and expected-value drift; it never
regenerates the expected vector set.

## 2. Run Backend Native Suites

```bash
Build/Mac/Release/Tests/StonerTest --suite metal-output-transform-native
Build/Linux/Debug/Tests/StonerTest --suite vulkan-output-transform-native
```

The applicable backend suite checks exact format/color-space pairs,
Renderer-owned transfer, same-frame submission/readback/presentation identity,
mode generation, and terminal ownership. Linux uses Lavapipe when configured;
Windows and Linux retain SDR validation. Feature 029 makes no Windows HDR
validation claim.

## 3. Generate a Real Native Probe from StonerDemo

The native producer is `StonerDemo`; `native-capture` launches it between
exact-revision checks. The following argv describes the visible workload.
For formal authority, put this argv (executable and each argument as separate
JSON strings, without shell expansion) in a command file instead of running it
first. Create the lease/evidence directories, use absolute strict-cooked paths,
and bind the exact generation and v3 workload revision:

```bash
TASK_REPO="$PWD"
mkdir -p Build/Validation/029/native-probe/lease
mkdir -p Validation/029/SDR/M4-Metal/Lantern/frozen-1

Build/Mac/Release/Demo/StonerDemo/StonerDemo \
  --mode validate --backend metal \
  --workload production-content --render-path deferred-full \
  --frames 4096 --warmup-frames 512 --memory-sample-interval 128 \
  --width 512 --height 512 --frames-in-flight 2 \
  --cooked-root "$TASK_REPO/Build/Validation/029/cook/publication" \
  --lease-root "$TASK_REPO/Build/Validation/029/native-probe/lease" \
  --target-profile "$TASK_REPO/Config/AssetCooker/Profiles/Production/Mac-Metal-Arm64.json" \
  --production-root StaticModel:Lantern.glb#idx.scene.0 \
  --strict-generation GENERATION_SHA256 \
  --workload-revision production-content-lantern-v3 \
  --baseline-root "$TASK_REPO/Content/ProductionAcceptance/Baselines" \
  --device-class-registry "$TASK_REPO/Config/Validation/ProductionContent/DeviceClasses.json" \
  --output-device-profile Sdr.sRGB.v1 \
  --output-transform-version Sdr.KhronosPbrNeutral.v1 \
  --output-exposure-stops 0 \
  --output-native-profile native-sdr \
  --output-native-probe "$TASK_REPO/Validation/029/SDR/M4-Metal/Lantern/frozen-1/native-probe.json" \
  --production-cycles 20 --production-warmup-cycles 2 \
  --production-max-rss-growth-mib 16 \
  --validation-output "$TASK_REPO/Build/Validation/029/native-probe/run.json" \
  --visible-capture
```

Save the complete argv in `Build/Validation/029/native-probe/command.json`, then
launch it through the guarded producer. Existing probe/report files are rejected
to prevent assigning old captures to a new SHA:

```bash
python3 .github/scripts/run_output_transform_validation.py native-capture \
  --command-file Build/Validation/029/native-probe/command.json \
  --profile native-sdr \
  --probe Validation/029/SDR/M4-Metal/Lantern/frozen-1/native-probe.json \
  --git-revision EXACT_40_HEX_COMMIT \
  --renderer-strategy deferred \
  --root . \
  --output Validation/029/SDR/M4-Metal/Lantern/frozen-1/native-report.json
```

A status other than `passed`, unequal readback/presentation/frame tokens, a
nonzero terminal-owner count, or a stale generation fails closed. Preserve the
first reported stage; do not replace it with a later cleanup symptom.

## 4. Generate Formal SDR v3 Candidates

Add `--production-capture-root` to the visible command above. It is accepted
only for SDR v3 authority and writes exact P6 512x512 captures; HDR use is
rejected. V3 cross-process calibration uses
`run_production_image_calibration.py --git-revision EXACT_40_HEX_COMMIT`
with `--backend`, `--workload-revision`, `--command-file`, and a fresh
`--output` directory. It records the guarded SHA and requires 20 captures per
process across 3–6 independent processes, with each observed mode repeated in
at least two processes. Set up the strict-cooked native-test environment as in
the production-content validation runner; a JSON command file contains
`nativeCommand` and `mutationCommand` argv arrays. The latter runs
`StonerTest --suite production-image-calibration`.

After independent process captures pass calibration, generate a
Candidate directly from the settled PPM without conversion:

```bash
python3 .github/scripts/run_output_transform_validation.py candidate \
  --workload Config/Validation/OutputTransform/Workloads/Lantern-v3.json \
  --backend metal \
  --device-class macos.apple8.metal.rgba8 \
  --capability-digest CAPABILITY_SHA256 \
  --calibration-digest CALIBRATION_SHA256 \
  --ppm-input Build/Validation/029/M4-Lantern-v3/captures/metal/capture-19.ppm \
  --output-dir Validation/029/SDR/M4-Metal/Lantern/frozen-1
```

Repeat with `Sponza-v3.json` and the Sponza capture. A formal image is exactly
512x512 with `sampleCount=1`. The PPM parser accepts only the exact header and
byte count; image dimension mismatch fails before FLIP. Alignment, crop, scale,
warp, resize, and resampling are forbidden. Output is always `state=candidate`
with `acceptance=null`.

Use the calibration mode matching the Candidate pixels and copy its actual
`flipPolicy` without widening it. Retain the small `calibration.json` from that
run beside the Candidate (raw PPM/logs/process captures remain ignored). Link
the guarded native capture and the same-SHA calibration:

```bash
python3 .github/scripts/run_output_transform_validation.py sdr-report \
  --probe Validation/029/SDR/M4-Metal/Lantern/frozen-1/native-probe.json \
  --native-report Validation/029/SDR/M4-Metal/Lantern/frozen-1/native-report.json \
  --candidate Validation/029/SDR/M4-Metal/Lantern/frozen-1/candidate.json \
  --calibration Validation/029/SDR/M4-Metal/Lantern/frozen-1/calibration.json \
  --git-revision EXACT_40_HEX_COMMIT --root . \
  --output Validation/029/SDR/M4-Metal/Lantern/frozen-1/sdr-report.json
```

This fails unless the PNG matches the native readback and the calibration's
pixels, policy, mutation results, independent processes, and revision. It does
not create Accepted state. The source Candidate is immutable after reporting;
the Accepted registry copy uses a repository-relative PNG path and explicit
maintainer acceptance. All artifact paths are relative to repository root.

Run the same workflow on the independently synchronized physical Windows Vulkan
authority using `Build/Win64/Release`. Feature 028 v2 images and its one-time
Windows carry-forward cannot satisfy Feature 029. A maintainer admits or rejects
each exact v3 tuple only by an explicit repository edit to
`Config/Validation/OutputTransform/SDR/Baselines-v3.json`.

Windows T102 permits an active, unlocked Console or RDP session when the visible
application executes on a physical discrete Vulkan GPU. Record the actual
session and adapter before/after capture. Use the application's exact GPU
readback for Candidate pixels; retain all native same-frame presentation and
calibration checks. An RDP run claims only that session's GPU output and window
presentation, without physical-monitor scanout or Console-equivalence claims.
See `windows-handoff.md`; RDP-client screenshots and video are not input evidence.
Documentation-only policy changes are recorded by diff/digest separately from
the frozen software SHA. Validation/TEMP and cooked packages remain on local
NTFS; move verified bounded evidence to network storage after the run.

## 5. Prepare macOS Metal HDR Live Review

Run the StonerDemo probe command four times in this exact order, replacing the
SDR profile/version with:

1. `Hdr.PQ.Rec2020.1000.v1`
2. `Hdr.PQ.Rec2020.2000.v1`
3. `Hdr.Linear.1000.v1`
4. `Hdr.Linear.2000.v1`

All four use
`Hdr.ACES2.0.0_2025-04-04.Rec2020D65.v1` and
`--output-native-profile native-hdr-nonvisual`. Do not supply
`--production-capture-root`. Each visible run proves only machine-checkable
native preparation and completion. Use `native-capture` for each command with
a fresh probe under `Validation/029/HDR/Probes/` and its native report under
`Validation/029/HDR/Reports/`, all at the same software SHA. Do not normalize
pre-commit probes or mix sessions:

- PQ resolves to `BGR10A2Unorm`, ITU-R 2100 PQ,
  `wantsExtendedDynamicRangeContent=true`, permitted Core Animation color
  management, and `EDRMetadata=nil`.
- EDR resolves to RGBA16Float, extended-linear sRGB,
  `wantsExtendedDynamicRangeContent=true`, and `EDRMetadata=nil`.
- Neither path enables `CAEDRMetadata` system tone mapping.

After the implementation is committed and the probes are reproduced by that
exact revision, prepare an identity JSON with these fields:

```json
{
  "request_id": "hdr-review-029-001",
  "git_revision": "EXACT_40_HEX_COMMIT",
  "workload_revision": "production-content-lantern-v3",
  "device_class": "macos.apple8.metal.hdr",
  "display_class": "maintainer-hdr-edr-display",
  "display_capability_digest": "DISPLAY_CAPABILITY_SHA256",
  "review_session_id": "hdr-review-session-001"
}
```

Then create the machine-authored request:

```bash
python3 .github/scripts/run_output_transform_validation.py hdr-request \
  --identity Build/Validation/029/HDR/identity.json \
  --profile-preflight Validation/029/HDR/Probes/pq1000.json \
  --profile-preflight Validation/029/HDR/Probes/pq2000.json \
  --profile-preflight Validation/029/HDR/Probes/edr1000.json \
  --profile-preflight Validation/029/HDR/Probes/edr2000.json \
  --native-report Validation/029/HDR/Reports/pq1000.json \
  --native-report Validation/029/HDR/Reports/pq2000.json \
  --native-report Validation/029/HDR/Reports/edr1000.json \
  --native-report Validation/029/HDR/Reports/edr2000.json \
  --root . \
  --output Validation/029/HDR/hdr-live-review-request.json
```

The producer computes distinct canonical ordered raw-probe and native-report
bundle digests. Its strongest state is `ready-for-live-review`. The maintainer must personally
view all four settled modes on the physical M4 HDR/EDR display and manually add
an immutable record under `Validation/029/HDR/Attestations/`. No test, PNG,
screenshot, FLIP score, hosted run, or metadata result can write or infer
`pass`. A correction is a new attestation linked by
`supersedesAttestationId`; history is never rewritten.

Validate only structure, digest linkage, and bounds:

```bash
python3 .github/scripts/verify_output_transform_evidence.py \
  --hdr-request Validation/029/HDR/hdr-live-review-request.json \
  --attestation-dir Validation/029/HDR/Attestations \
  --root .
```

Schema success means only that the human-authored claim is well formed.

## 6. Diagnose Unsupported and Lifecycle Failures

Use the focused suites rather than obsolete runner profiles:

```bash
Build/Mac/Release/Tests/StonerTest \
  --suite rhi-presentation-output \
  --suite output-presentation-lifecycle \
  --suite metal-output-transform-native
```

Inspect the normalized first-failure stage, requested and resolved pair,
capability generation, mode generation, frame token, readback token,
presentation token, and terminal-owner count. `Unsupported` is evidence, not a
pass. Zero drawable is `Paused`, never a successful zero-size image. A profile
or display change invalidates incompatible extent- and mode-dependent objects
before the next formal frame.

## 7. Evidence and Closeout

```bash
python3 .github/scripts/aggregate_output_transform_validation.py \
  --git-revision EXACT_40_HEX_SOFTWARE_COMMIT \
  --report Validation/029/SDR/M4-Metal/Lantern/frozen-1/sdr-report.json \
  --report Validation/029/SDR/M4-Metal/Sponza/frozen-1/sdr-report.json \
  --report Validation/029/SDR/Windows-Vulkan/Lantern/frozen-1/sdr-report.json \
  --report Validation/029/SDR/Windows-Vulkan/Sponza/frozen-1/sdr-report.json \
  --report Validation/029/HDR/Reports/pq1000.json \
  --report Validation/029/HDR/Reports/pq2000.json \
  --report Validation/029/HDR/Reports/edr1000.json \
  --report Validation/029/HDR/Reports/edr2000.json \
  --sdr-registry Config/Validation/OutputTransform/SDR/Baselines-v3.json \
  --hdr-request Validation/029/HDR/hdr-live-review-request.json \
  --attestation-dir Validation/029/HDR/Attestations \
  --root . \
  --output Validation/029/CI/authority-aggregate.json
```

The verifier enforces canonical JSON <=1 MiB, <=64 artifacts, each <=64 MiB,
aggregate <=256 MiB, exact digests, safe paths, bounded lossless PNG/JSON-only
SDR evidence, and JSON/digest-only HDR authority. Closeout additionally requires
fresh Accepted M4 Metal and Windows Vulkan SDR v3 records plus four current,
non-superseded human HDR `pass` observations tied to the same committed
revision. Feature 029 must remain in progress until all of those gates pass.
The verifier's `--require-closeout` invokes the same full gate and requires
the same inputs plus `--git-revision`; schema-only validation is insufficient.
T112 separately requires the hosted matrix and sanitizer run IDs/digests on
that software revision. A passed hosted machine aggregate alone is not
Feature 029 closeout.
