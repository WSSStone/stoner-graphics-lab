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
  --acquire-missing --timeout-seconds 5400
```

Hardware validation uses the same runner and additionally requires a physical
display-backed application surface, accepted image evidence, and the declared
1,000 lifecycle cycles. The command below is the sole Feature 028 physical
authority entry point:

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
- No Feature 028 self-hosted runner exists, so pushes do not queue a hardware
  workflow. Maintainers run the explicit local Metal command for closeout and
  accepted reference/render-path changes. Windows/macOS Vulkan physical
  qualification is deferred to a future hardware-lab phase. Hosted Windows/
  Linux Vulkan build, strict-runtime, lifecycle, and native readback are the
  documented non-equivalent fallback and MUST NOT be labeled physical proof.
- A missing host, device, display, backend, or tool is `Unsupported`, names its
  prerequisite and replacement hardware lane, and fails aggregate acceptance.

The runner derives `github-hosted`, `maintainer-local-metal`, or
`local-diagnostic` from repository policy and rejects generic caller promotion.
Hosted lanes own exact functional/lifecycle completion, including 1,000/20
cycles, 2,000 captures, seven readbacks, zero terminal owners, and stale-handle
rejection. Hosted RSS/task-VM/allocator/peak/elapsed metrics are observations:
they are preserved in evidence but do not decide acceptance. The
maintainer-local Metal lane additionally requires native arm64 macOS, exact
target/registered device class, non-Rosetta execution, exclusive process lock,
clean committed revision, default production allocator, declared sample
protocol, and window presentation plus GPU readback. Only that lane owns the
16 MiB RSS and accepted-image gates.
Local diagnostics never replace a required hosted or physical lane.

Regular lanes have a 10-minute workload budget, hosted medium package lanes
have a 90-minute complete budget with an independently capped 80-minute native
stage, and serialized visible hardware lanes have a 60-minute budget. Hosted
medium workflow setup, compilation, and execution are enclosed by a separate
120-minute job timeout. An operational timeout fails because required work is
incomplete; it is not a performance baseline. Evidence is written below
`Build/Validation/028/`, uploaded with failure-safe steps, and promoted into
this directory only after digest and privacy verification.
