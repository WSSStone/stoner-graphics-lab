# Feature 029 — Physical Windows SDR Handoff

The invoking prompt supplies the exact 40-hex **tested software SHA**. Do not
substitute the moving branch tip. This handoff addresses T102 and the applicable
Windows build/regression evidence for T112, not Feature 029 completion.

## Boundaries

- Read repository `AGENTS.md`, `tasks.md`, `quickstart.md`, and
  `contracts/validation-evidence.md` in this feature directory first.
- Use native x86_64 Windows, a physical discrete Vulkan device, and a visible
  application window. Hosted runners, software rendering, and remote desktop
  captures cannot substitute for physical image authority.
- Windows validates **SDR only**. Do not run or claim Windows HDR native,
  swapchain, image, physical, or visual acceptance. macOS PQ/EDR remains a
  separate maintainer live-view gate; never author those decisions.
- Preserve all unrelated changes. Inspect status before checkout; use a clean
  separate worktree when necessary, without reset/clean/stash of user work.
- Build and cook from the supplied SHA. No borrowing macOS binaries, metallib,
  cooked generations, old probes, Feature 028 v2 references, or its Windows
  carry-forward. Do not relabel previous evidence with this SHA.

## Execute

1. Fetch `origin`, resolve the supplied SHA, and check it out safely. Record
   `git rev-parse HEAD`, host/architecture, native Vulkan capability/device
   class, build commands and binary digests. Use the repository's MSVC/Vulkan
   setup and pinned SCons 4.10.1. Run strict Debug and Release builds; fresh
   formal capture uses `Build/Win64/Release/Tests/StonerTest.exe` and
   `Build/Win64/Release/Demo/StonerDemo/StonerDemo.exe`.
2. Run the deterministic/native suites and Python evidence tests listed in
   `quickstart.md` and `.github/workflows/feature-029-hdr-output.yml`. Add the
   existing production-content, strict-runtime, image-acceptance, and applicable
   native regression suites. Preserve the first failure; do not weaken tests,
   tolerances, resource checks, or reference admission to obtain a pass.
3. Stage the pinned `khronos-lantern-glb` and `khronos-sponza-gltf` sources using
   the existing corpus acquisition/hash checks; large Sponza source data stays
   ignored. License selection/approval belongs to the maintainer. Cook fresh
   strict Windows closures with
   `Config/AssetCooker/Profiles/Production/Windows-Vulkan.json`. Reuse the
   production runner's regular/medium source-staging and cook mechanisms, not
   its v2 hardware acceptance lane. Record each publication, generation digest,
   root identity, target-profile digest, and binary digest locally.
4. For each workload, configure the native test environment using
   `build_native_lifecycle_stage` in
   `.github/scripts/run_production_content_validation.py`: backend `vulkan`,
   exact publication/generation/lease/target profile/root, v3 workload,
   cycles=20, warmup=2, visible=true, image acceptance=false. In particular:
   `STONER_REQUIRE_VULKAN_PRODUCTION=1`,
   `STONER_PRODUCTION_VULKAN_PUBLICATION_ROOT`,
   `STONER_PRODUCTION_VULKAN_LEASE_ROOT`,
   `STONER_PRODUCTION_VULKAN_GENERATION`,
   `STONER_PRODUCTION_VULKAN_TARGET_PROFILE`,
   `STONER_PRODUCTION_ROOT`, and `STONER_PRODUCTION_VISIBLE=1` must identify
   the real run. Do not set flags that force Accepted state.
5. Run `.github/scripts/run_production_image_calibration.py` with
   `--backend vulkan`, the exact `--git-revision`, and respectively
   `production-content-lantern-v3` / `production-content-sponza-v3`.
   Its command JSON contains `nativeCommand` running
   `StonerTest --suite production-content-vulkan-native` and `mutationCommand`
   running `StonerTest --suite production-image-calibration`. It guards the
   clean software SHA before/after, captures 20 frames per process across 3–6
   processes, requires each mode in at least two processes, and tests the
   existing eight image/stale mutations. Use fresh ignored output directories.
6. Run fresh visible Deferred Demo captures through
   `run_output_transform_validation.py native-capture --command-file ...`
   as described in `quickstart.md`. Use the exact cooked generation, the v3
   workload, 512×512, sampleCount=1, `Sdr.sRGB.v1`,
   `Sdr.KhronosPbrNeutral.v1`, zero exposure, and no insertions. Add
   `--production-capture-root` for the exact SDR PPM. Write each small native
   probe directly under a fresh `Validation/029/SDR/Windows-Vulkan/<workload>/`
   subdirectory before generating its report; report paths must remain valid
   when the evidence is transferred back. The native-capture command requires
   a fresh output path and the supplied `--git-revision`.
7. Generate Candidate JSON/lossless PNG from the settled exact PPM with
   `run_output_transform_validation.py candidate`. Use
   `windows.discrete-vulkan.rgba8`, the actual capability digest and matching
   calibration mode's digest/policy. Retain the calibration JSON beside it.
   Generate `sdr-report` using `--probe`, `--native-report`, `--candidate`,
   `--calibration`, the exact `--git-revision`, `--root .`, and fresh `--output`.
   This verifies native bytes against PNG, calibration against pixels/settings,
   and software revision. Do not edit the immutable Candidate after reporting.
8. Run the bounded evidence verifier on both SDR reports and the registry, then
   the vector, architecture, roadmap consistency scans and `git diff --check`.
   Schema success is not acceptance. A full closeout aggregate is expected to
   remain blocked until the other platform and maintainer gates exist.

## Output and Failure Policy

Return the tested SHA, exact commands and outcomes, target/generation/binary
digests, two bounded Candidate bundles, and remaining findings. Retain only
bounded PNG/JSON under `Validation/029/`; raw PPM, readbacks, local paths, cooked
data, and large logs stay under ignored `Build/Validation/029/`. Respect JSON
≤1 MiB, ≤64 artifacts, ≤64 MiB each and ≤256 MiB total. No automatic alignment,
cropping, translation, scaling, warping, resizing, or resampling is permitted.

Keep Candidate `acceptance=null`; do not modify `Baselines-v3.json`, T103–T106,
roadmap completion, or Speckit completion hooks. Mark T102 only when both
physical workload bundles pass all machine/provenance checks. Hosted Windows
success does not replace the physical run. T112 also needs the other hosted
platform/sanitizer run IDs at the same SHA.

If code or validation inputs need changes, stop formal capture and report the
failure. A newly fixed software SHA requires coordinated fresh M4/Windows/HDR
evidence, not a mixture of revisions. Report before implementing a new fix in
this handoff. Do not commit or push evidence unless the maintainer separately
authorizes it; evidence-only commits must retain the tested software SHA.
