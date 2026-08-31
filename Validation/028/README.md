# Feature 028 Validation

Status: **in progress**

Feature 028 evidence is split into deterministic reports and bounded host
observations. A native pass requires strict-cooked loading, transactional
Renderer realization, requested-backend proof, GPU-produced readback, semantic
probes, and an accepted device-class image comparison.

Tracked `Validation/028/` content is deliberately small: canonical summaries
live in `reports/`, calibration/index metadata lives in `Baselines/`, and final
run/artifact digests live in `CI/`. Raw work, downloaded artifacts, external
content, DDC, cooked generations, logs, raw readbacks, and captures remain under
ignored `Build/Validation/` paths or external CI retention. Historical run
outputs are not build inputs; required test fixtures live under `Tests/Fixtures/`.
Accepted reference pixels live as losslessly compressed PNG under
`Content/ProductionAcceptance/Baselines/`, never as checked-in PPM/RGBA.

## Entry Points

Create Debug and Release binaries with the repository-pinned SCons version:

```bash
conda run -n godot scons config=debug strict=1
conda run -n godot scons config=release strict=1
```

Run the bounded regular profile with the target profile for the current host.
This example is the native Metal lane on an Apple Silicon macOS host:

```bash
python3 .github/scripts/run_production_content_validation.py \
  --profile regular \
  --target-profile Config/AssetCooker/Profiles/Production/Mac-Metal-Arm64.json \
  --build-root Build/Mac/Release \
  --output Build/Validation/028/regular-macos-metal \
  --timeout-seconds 600
```

Medium validation acquires the hash-pinned external package, exercises every
accepted package, and uses the 1,000-cycle/20-cycle-warm-up contract:

```bash
python3 .github/scripts/run_production_content_validation.py \
  --profile medium \
  --target-profile Config/AssetCooker/Profiles/Production/Mac-Metal-Arm64.json \
  --build-root Build/Mac/Release \
  --output Build/Validation/028/medium-macos-metal \
  --acquire-missing --timeout-seconds 5400
```

Hardware validation uses the same runner and additionally requires a physical
display-backed application surface, accepted image evidence, and the declared
1,000 lifecycle cycles. The commands below are the two Feature 028 physical
authority entry points and must run on one committed revision:

```bash
STONER_PRODUCTION_VISIBLE=1 \
python3 .github/scripts/run_production_content_validation.py \
  --profile hardware \
  --local-metal-authority \
  --target-profile Config/AssetCooker/Profiles/Production/Mac-Metal-Arm64.json \
  --build-root Build/Mac/Release \
  --output Build/Validation/028/hardware-macos-metal \
  --acquire-missing --timeout-seconds 3600
```

```powershell
python .github/scripts/run_production_content_validation.py `
  --profile hardware `
  --local-windows-vulkan-authority `
  --target-profile Config/AssetCooker/Profiles/Production/Windows-Vulkan.json `
  --build-root Build/Win64/Release `
  --output Build/Validation/028/hardware-windows-vulkan `
  --acquire-missing --timeout-seconds 3600
```

Artifact consumers must supply the original target profile. Verification
checks that profile digest, the current-generation pointer, generation manifest
and every indexed artifact before accepting the producer output:

```bash
python3 .github/scripts/run_production_content_validation.py \
  --verify-only Build/Validation/028/regular-macos-metal \
  --target-profile Config/AssetCooker/Profiles/Production/Mac-Metal-Arm64.json
```

## Ownership And Cadence

- Relevant pull requests and pushes run regular Windows Vulkan, Linux Vulkan,
  and macOS Metal jobs in
  `.github/workflows/feature-028-production-content.yml`.
- The default branch runs the medium Intel Metal profile every Monday; maintainers
  dispatch it at Feature 028 and release closeout. Arm64 Metal remains covered
  by regular validation and the required maintainer-local M4 Metal closeout. The Sponza cache key is the
  checked-in corpus-manifest digest, while acquisition still revalidates every
  declared file hash.
- No Feature 028 self-hosted runner is required, so pushes do not queue a hardware
  workflow. Maintainers manually synchronize and run the explicit local Metal
  and Windows Vulkan commands for closeout and accepted reference/render-path
  changes. macOS Vulkan qualification is deferred. Hosted Windows/
  Linux Vulkan build, strict-runtime, lifecycle, and native readback are the
  documented non-equivalent fallback and MUST NOT be labeled physical proof.
- A missing host, device, display, backend, or tool is `Unsupported`, names its
  prerequisite and replacement hardware lane, and fails aggregate acceptance.

The runner derives `github-hosted`, `maintainer-local-metal`,
`maintainer-local-windows-vulkan`, or `local-diagnostic` from repository policy
and rejects generic caller promotion.
Hosted lanes own exact functional/lifecycle completion. The schema-v4 medium
contract assigns Lantern 1,000/20 endurance with 2,000 captures and Sponza
100/10 scale lifecycle with 200 captures; both retain seven readbacks, zero
terminal owners, stale-handle rejection, and complete per-cycle work. Hosted
RSS/task-VM/allocator/peak/elapsed metrics are observations:
they are preserved in evidence but do not decide acceptance. The
maintainer-local physical lanes additionally require their exact native host,
target/registered device class, exclusive process lock, clean committed
revision, default production allocator, declared sample protocol, and window
presentation plus GPU readback. Both own accepted-image gates only for their
exact targets. Metal additionally owns the calibrated 16 MiB RSS gate; Windows
working-set RSS is recorded with observed disposition and cannot independently
decide acceptance.
Local diagnostics never replace a required hosted or physical lane.

Regular lanes have 900/1,200/600-second package/profile/native budgets. The
package covers clean/warm/strict/native work, native remains independently
capped, and profile-only headroom covers target-toolchain discovery and
orchestration without reducing any required work. Hosted
medium lanes have 5,400-second package and profile budgets with an independently
capped 4,800-second native stage. Serialized visible hardware lanes give each
package and native stage 3,600 seconds and the complete two-package profile
7,800 seconds. Normal package validation failures are collected; authority,
revision, device-loss, and evidence-integrity failures stop immediately. Hosted
medium workflow setup, compilation, and execution are enclosed by a separate
120-minute job timeout. An operational timeout fails because required work is
incomplete; it is not a performance baseline. Evidence is written below
`Build/Validation/028/`, uploaded with failure-safe steps, and promoted into
this directory only after digest and privacy verification.

## Baseline calibration

Accepted baseline changes require at least three independent native processes,
twenty real submission-derived captures per process, and two-process
reproduction of every observed mode. Use
`.github/scripts/run_production_image_calibration.py` with a small JSON command
file containing `nativeCommand` and `mutationCommand` arrays, plus explicit
`--backend`, `--workload-revision`, and `--output` arguments. The tool expands
to at most six processes, records decoded-pixel SHA-256 and pairwise-mode FLIP,
runs each mode's threshold calibration only across processes in that mode,
verifies the real first/last submission tokens through the stale-frame gate,
runs every mutation against every candidate reference mode, and emits only
`calibration.json` plus one lossless PNG per unique mode. It verifies PNG
decoded-pixel identity and removes temporary PPM/process roots. Its output is
Candidate-only; accepting any reference still requires an explicit maintainer
change to the baseline registry.
