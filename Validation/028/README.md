# Feature 028 Validation

Status: **in progress**

Feature 028 evidence is split into deterministic reports and bounded host
observations. A native pass requires strict-cooked loading, transactional
Renderer realization, requested-backend proof, GPU-produced readback, semantic
probes, and an accepted device-class image comparison.

Tracked reports live in `reports/`; accepted reference metadata lives in
`Baselines/`; final GitHub run and artifact digests live in `CI/`. Raw work,
downloaded artifacts, external content, DDC, generations, and local captures
remain under ignored paths.

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
  --acquire-missing --timeout-seconds 1800
```

Hardware validation uses the same runner and additionally requires a physical
display-backed application surface, accepted image evidence, and the declared
1,000 lifecycle cycles:

```bash
STONER_PRODUCTION_VISIBLE=1 \
python3 .github/scripts/run_production_content_validation.py \
  --profile hardware \
  --target-profile Config/AssetCooker/Profiles/Production/Mac-Metal-Arm64.json \
  --build-root Build/Mac/Release \
  --output Build/Validation/028/hardware-macos-metal \
  --acquire-missing --timeout-seconds 1800
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
  macOS Vulkan, and macOS Metal jobs in
  `.github/workflows/feature-028-production-content.yml`.
- The default branch runs the medium Linux profile every Monday; maintainers
  dispatch it at Feature 028 and release closeout. The Sponza cache key is the
  checked-in corpus-manifest digest, while acquisition still revalidates every
  declared file hash.
- Maintainers explicitly dispatch
  `.github/workflows/feature-028-production-hardware.yml` on labeled Windows
  Vulkan and macOS Vulkan/Metal runners for feature closeout and any accepted
  reference or production render-path change.
- A missing host, device, display, backend, or tool is `Unsupported`, names its
  prerequisite and replacement hardware lane, and fails aggregate acceptance.

Regular lanes have a 10-minute workload budget; medium and hardware lanes have
a 30-minute workload budget. Workflow setup and compilation have separate job
timeouts. Evidence is written below `Build/Validation/028/`, uploaded with
failure-safe steps, and promoted into this directory only after digest and
privacy verification.
