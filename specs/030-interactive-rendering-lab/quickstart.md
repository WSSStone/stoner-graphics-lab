# Quickstart: Feature 030 Implementation and Validation

**Status**: Planning guide. Feature 030 code, new CLI flags, suites and validation scripts below are implementation deliverables and are not available merely because this document exists. This planning invocation did not build or run the lab.

## 1. Read the contract before coding

Start with [plan.md](plan.md), [data-model.md](data-model.md), [runtime limits](contracts/lab-runtime.md) and [UI color/ownership](contracts/ui-rendering.md). The existing feature spec's accepted choice is A: preset import adapts projection to the current drawable while preserving pose/vertical FOV. Formal captures still use their frozen extent/camera.

Follow the generated [tasks.md](tasks.md), and run `/speckit-analyze` before implementation to check cross-artifact consistency. Keep M0's real Vulkan deferred-submission work visible; deleting Demo readback calls alone is insufficient.

## 2. Build and deterministic checks

After the relevant implementation exists, run from repository root using the existing SCons 4.10.1 environment:

```bash
scons config=debug strict=1
scons config=release strict=1
```

Mac output is `Build/Mac/{Debug,Release}`; Windows uses `Build/Win64/{Debug,Release}` with `.exe`; Linux uses `Build/Linux/{Debug,Release}`. Native paths require the existing GLFW/Vulkan or Metal dependencies. Vulkan startup queries optional VK_EXT_swapchain_maintenance1: supported/enabled devices use presentation fences; otherwise the lab uses bounded acquire-history fallback. Logs/report identify the selected mode and terminal cleanup assurance. Extension absence does not block startup or implementation; actual target execution is still required. See contracts/lab-runtime.md for transition limits and the accepted terminal-idle assumption. Deterministic tests remain runnable without a display.

Proposed new suites after registration:

```bash
Build/Mac/Debug/Tests/StonerTest \
  --suite application-free-camera \
  --suite application-ui-input \
  --suite application-lab-preset \
  --suite renderer-ui-draw \
  --suite renderer-ui-color \
  --suite rhi-deferred-submission \
  --suite interactive-lab-lifecycle
```

Existing regression suite names to retain:

```bash
Build/Mac/Debug/Tests/StonerTest \
  --suite application-window \
  --suite production-camera-preview \
  --suite renderer-output-transform-math \
  --suite renderer-output-transform \
  --suite rhi-presentation-output \
  --suite output-presentation-lifecycle
```

Also run existing Asset JSON/material/cooker and Core filesystem tests after shared yyjson/no-replace changes. Sanitizer jobs use `scons config=debug strict=1 sanitizers=address,undefined` and a separate `sanitizers=thread` build on Linux, following the existing 029 workflow.

## 3. Prepare strict-cooked lab content

Reuse the checked-in Lantern corpus and staged hash-pinned Sponza package through the existing 028/029 producer instructions. Add new UI shader descriptor roots to the lab cook request, then publish a new immutable generation for the exact target profile. UI-on lab startup requires both scene and UI shader closure. The US1 UI-off MVP requires only the existing scene/output closure; a later enable attempt preflights UI dependencies in the same generation and remains UI-off with an explicit diagnostic if absent. Formal UI-off commands continue requiring only their historical scene/output closure. Never modify a previous generation in place.

For Metal, derive MSL deterministically through the existing Tools-only SPIRV-Cross path and finalize target-tagged metallib offline. Runtime shader-source compilation/fallback is rejected. The font is a fixed build-embedded asset from the pinned upstream commit, not a runtime source lookup. Store provenance/license/file digests during vendoring; this guide does not invent those digests.

## 4. Launch the proposed lab

The following command becomes valid after M0–M2. Replace `GENERATION_SHA256` with the actual producer result and use a published root that contains the new UI shader closure. Directory choices below are local examples, not evidence identities.

```bash
TASK_REPO="$PWD"
mkdir -p Build/Validation/030/leases Build/InteractiveLab/Exports

Build/Mac/Release/Demo/StonerDemo/StonerDemo \
  --mode interactive --interactive-lab --lab-ui on \
  --backend metal --workload production-content --render-path deferred-full \
  --width 1280 --height 720 --frames-in-flight 2 \
  --cooked-root "$TASK_REPO/Build/Validation/030/cook/publication" \
  --lease-root "$TASK_REPO/Build/Validation/030/leases" \
  --target-profile "$TASK_REPO/Config/AssetCooker/Profiles/Production/Mac-Metal-Arm64.json" \
  --production-root 'StaticModel:Lantern.glb#idx.scene.0' \
  --strict-generation GENERATION_SHA256 \
  --workload-revision production-content-lantern-v3 \
  --output-device-profile Sdr.sRGB.v1 \
  --output-transform-version Sdr.KhronosPbrNeutral.v1 \
  --output-exposure-stops 0 \
  --lab-export-root "$TASK_REPO/Build/InteractiveLab/Exports" \
  --lab-report "$TASK_REPO/Build/Validation/030/interactive-report.json"
```

Use the actually published workload revision if content/output changes required a bump; never label revised content as v3 just to match this example. For Sponza, select its exact production root/revision/generation from the producer. For Windows/Linux, use their matching cooked target profile/backend and executable paths. A missing profile or UI shader returns an explicit startup failure; a label alone is not native success.

Navigation: WASD/QE, Shift, RMB look; Escape releases capture/active interaction; F1 toggles UI outside text editing; wheel changes vertical FOV when the viewport owns scroll. Panel controls expose camera speed/FOV/reset, manual exposure, SDR/HDR selections, debug bypass, UI white and effective capability state. Window close or Exit ends the lab.

Import through the panel or add `--lab-preset-input` with a valid exported file. A bare filename such as `us5-ui-roundtrip.json` resolves in the configured export directory; a relative path containing folders resolves from the working directory, and absolute paths are supported. Scroll inside the panel to reach Local presets and its controls. Panel size and position persist for the session after dragging. For precise numeric edits, type in the corresponding Exact field and press Enter; existing finite/range validation still applies. At changed aspect the window stays unchanged. An unsupported preset profile rejects the entire import and preserves current valid settings; only independent loss of the active output invokes SDR fallback or pause. Zero drawable holds an otherwise valid full import pending. Export/overwrite and capture are explicit actions and never write Accepted baselines.

## 5. Run bounded machine validation

After new suites exist, use `--suite metal-ui-native` or `--suite vulkan-ui-native` for bounded native draw/color fixtures. Production lab tests use the command above with `--mode validate`, a positive `--frames` budget including warmup, and `--lab-input-script` pointing to a checked-in bounded sequence. Use 1120 presented frames (120 warmup + 1000 measured) for each of the four fixed endurance cases, and 120 presented frames without excluded warmup for each remaining profile-smoke case. The runner distinguishes smoke from endurance and verifies the case table in contracts/validation-evidence.md; do not run the long budget for every profile/workload by default.

The planned runner must expose these documented commands before closeout:

```bash
python3 .github/scripts/interactive_lab_validation.py run \
  --command-file Build/Validation/030/native-command.json \
  --git-revision EXACT_40_HEX_SOFTWARE_COMMIT \
  --output Build/Validation/030/native-report.json

python3 .github/scripts/interactive_lab_validation.py verify \
  --report Build/Validation/030/native-report.json --root .
```

`native-command.json` contains a `nativeCommand` argv array, with each argument a separate string and no shell expansion, plus expected coverage case ID, gate kind (smoke/endurance/lifecycle) and workload/backend/profile identity from Coverage-v1.json. The runner performs before/after software guards, invokes bounded cases and rejects stale output paths. Its schema and argument parser are implementation tasks. Run/verify/closeout share one thin entrypoint and reuse existing provenance/artifact/image helpers; independent consumption still runs in a fresh process/job.

Use [validation-evidence.md](contracts/validation-evidence.md) for exact lanes/cycles, readback/resource counters and bounded artifacts. Run the full macOS SDR/PQ/EDR sequence and Windows SDR physical lane independently of hosted fixtures. Linux virtual-display results never claim physical-display authority.

## 6. Preserve formal authority and finish review

For UI-off formal SDR, reuse [Feature 029 validation procedures](../029-hdr-output-transform/quickstart.md) with current frozen software and the applicable workload policy. Its historical closeout text/exception is not a permission for 030; the [030 evidence contract](contracts/validation-evidence.md) governs new closeout. No `--interactive-lab`, live preset or UI-inclusive capture is passed into formal acceptance. If pixels change, follow revisioned Candidate/calibration/explicit acceptance; never crop/resize to pass.

UI smoke PNG/JSON is separate from formal references. HDR exports are nonvisual diagnostics; current physical HDR appearance/readability requires the maintainer's live four-profile observation. The machine runner only prepares a linked request. Missing human review remains pending; neither a successful shader test nor earlier 029 feedback closes it.

After all gates exist and run, `interactive_lab_validation.py closeout --manifest PATH --git-revision EXACT_40_HEX_SOFTWARE_COMMIT --root .` accepts an explicit bounded bundle manifest and exact software revision, validates artifact/human linkage and reports incomplete until every required gate passes. Keep raw logs/buffers ignored under Build, check in only bounded artifacts and record run IDs/digests. Do not declare implementation complete from this planning guide.

## Interactive capture controls (T100 implementation)

Expand **Capture**, enter a name using ASCII letters, digits, underscores or hyphens, and click **Capture output**. **Include UI** defaults off. SDR writes PNG and JSON into the configured export directory; HDR writes a numeric report there and raw bytes under `Build/InteractiveLab/Raw`, never an HDR appearance PNG. To export a selected diagnostic stage, first select its numeric mode, then click **Capture selected numeric stage**. At most two requests are retained; additional requests report Busy. Existing output files are not replaced. Watch the panel status for completion or a controlled rejection. Captures use actual drawable pixels (which can differ from logical window size on Retina), without resizing. These exports never update Accepted references. Large explicit export event-service latency remains T101 work.
