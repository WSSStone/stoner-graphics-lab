# Contract: Triangle Demo Validation

## Required Validation Matrix

| Platform | Build `StonerDemo` | Deterministic headless | Native no-window Vulkan | Real-window triangle | Required evidence |
|----------|--------------------|------------------------|-------------------------|----------------------|-------------------|
| Windows | Required in CI | Required in CI | Optional local | Required before completion | Screenshot + matching normalized log |
| macOS | Required in CI | Required in CI | Optional local | Required before completion through MoltenVK | Screenshot + matching normalized log |
| Linux | Required in CI | Required in CI | Required in CI through Lavapipe | Not required | CI log/report artifact |

Android and other mobile targets do not participate.

## Default Endurance Profiles

All values may be overridden by CLI, but automated tasks use these defaults unless research evidence justifies a documented revision.

| Profile | Frames | Warm-up | Sample every | Absolute growth floor | Relative growth floor |
|---------|-------:|--------:|-------------:|----------------------:|----------------------:|
| Deterministic CI | 4,096 | 512 | 128 frames | 16 MiB | 5% of baseline |
| Linux Lavapipe CI | 4,096 | 512 | 128 frames | 64 MiB | 10% of baseline |
| Windows real smoke | 10,000 | 1,000 | 120 frames | 64 MiB | 10% of baseline |
| macOS real smoke | 10,000 | 1,000 | 120 frames | 64 MiB | 10% of baseline |

Allowed growth is the greater of absolute and relative floors. Longer scheduled/manual runs may increase frame budget without changing warm-up/sample semantics.

## Memory Calculation

1. Ignore resident-memory samples before warm-up completes.
2. Record resident bytes every configured sample interval.
3. Require at least ten post-warm-up samples.
4. Baseline = median of first five post-warm-up samples.
5. Final steady value = median of final five samples.
6. Growth = `max(0, final - baseline)`.
7. Fail when growth exceeds the configured allowed growth.

Missing/unsupported process resident-memory measurement is a validation failure for required endurance profiles, not an implicit pass.

## Resource-Lifecycle Gates

- Runtime live-count snapshot is captured after initialization, during every memory sample, before shutdown, and after shutdown.
- Counts may remain stable after initialization but must not increase monotonically with completed frames for frame-local categories.
- Frame context count equals the clamped frames-in-flight count.
- Swapchain image/framebuffer/render-finished-signal counts equal current image count and may change only during presentation generation replacement.
- Final demo-owned instance, device, surface, swapchain, resource, pipeline, command, and synchronization counts are all zero.
- A native-required run fails if runtime proof reports deterministic mode or zero native instance/device creation.

## Deterministic Test Requirements

Tests cover:

- CLI/default configuration and invalid arguments
- interactive versus bounded termination
- ordered frame stages and completed-frame accounting
- close/Escape before acquire
- zero-extent pause and restore
- resize/out-of-date recreation generations
- injected failures at window, runtime, shader, upload, pipeline, acquire, record, submit, present, memory, report, and shutdown stages
- first-failure ownership and stable normalized diagnostics
- native-required rejection of deterministic fallback
- resource-count stability and final-zero assertion
- memory median and threshold boundary behavior using synthetic samples
- byte-stable validation reports across repeated deterministic runs

## Linux Lavapipe CI Requirements

The job must:

1. Install Vulkan development headers/loader, Mesa Vulkan drivers, and Vulkan tools.
2. Resolve the Lavapipe ICD JSON and set `VK_DRIVER_FILES` explicitly.
3. Prove selected adapter/runtime is a software Vulkan device in the log.
4. Run `StonerDemo --mode headless-vulkan` with the Linux endurance profile.
5. Execute real native buffer allocation/upload, shader modules, pipeline, offscreen render target, command recording, queue submit/fence wait, and shutdown.
6. Upload the normalized validation log/report as a CI artifact.

The job must not create a fake surface, swapchain, or visible-presentation success marker.

## Windows/macOS Real Evidence

Required files:

```text
Validation/018/Windows/triangle.png
Validation/018/Windows/triangle.log
Validation/018/macOS/triangle.png
Validation/018/macOS/triangle.log
```

Each screenshot must visibly show:

- one non-degenerate triangle
- distinguishable red, green, and blue vertex-color interpolation
- the demo window identity

Each matching log must include:

- platform and build configuration
- native runtime proof
- normalized adapter name
- selected surface format, extent, image count, and frames in flight
- process-start-to-first-successful-present duration in milliseconds, no greater than 5,000
- requested/completed frame counts
- resize/recovery count
- ordered per-recovery durations in milliseconds for all 20 required cycles, each no greater than 2,000
- memory baseline, final median, allowed growth, and pass result
- peak and final resource counts
- zero error/fatal diagnostics
- successful shutdown marker

The screenshot/log pair must come from the same run, identified by one stable run ID. Native addresses and opaque handles are forbidden in logs.

## CI Pass/Fail Rules

- All three platform build jobs and deterministic tests pass.
- Linux Lavapipe native integration passes.
- No required test is converted to success because a dependency/runtime is unavailable.
- Optional real-window jobs may be explicitly skipped in hosted CI, but retained Windows/macOS evidence remains required before Feature 018 is marked complete.
- Existing regression tests remain passing.
- Failed validation reports and logs are uploaded when possible for diagnosis.

## Completion Record

Feature completion requires one summary record referencing:

- CI run/commit identity
- Windows screenshot/log pair
- macOS screenshot/log pair
- Linux Lavapipe report
- deterministic matrix result
- any explicitly accepted temporary gap and its follow-up task (none planned)
