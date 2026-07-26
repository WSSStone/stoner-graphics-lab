# B02-S15 Local Logging Verification Evidence

- Verification host: macOS arm64
- Fix under verification:
  `8303045d6b977ecc873033a2da3100756f347055`
- Repository changes from probes: none
- GitHub Actions used: none

## Independent Probe Results

| Probe | Debug-like | Optimized Release |
|---|---|---|
| 5x5x4 threshold matrix | 100/0 | 100/0 |
| Expected enabled side effects | 30 | 30 |
| Unexpected filtered side effects | 0 | 0 |

The macro concurrency probe used four readers, one writer, and 400,000 filtered
macro calls:

```text
macro_tsan side_effects=0
ThreadSanitizer reports=0
exit=0
```

## Optimized Early-Out Shape

The `-O2 -DNDEBUG` arm64 body before format argument evaluation is:

```asm
ldrb    w8, [x0, #8]
mov     w9, #31
lsl     w8, w9, w8
ldr     x9, [global-threshold-address]
ldrb    w9, [x9]
and     w8, w8, #0x2
lsr     w8, w8, w9
cbz     w8, filtered-return
```

The first counter load/increment occurs after `cbz`; this is one conditional
branch for the filtered path.

## Direct Fatal Child Results

```json
{
  "debug": {
    "returncode": -5,
    "stdout_bytes": 0,
    "stderr": "[HH:MM:SS.mmm] LogCore: Fatal: isolated fatal logging probe"
  },
  "release": {
    "returncode": -6,
    "stdout_bytes": 0,
    "stderr": "[HH:MM:SS.mmm] LogCore: Fatal: isolated fatal logging probe"
  }
}
```

## Maintained Gates

- `Evidence/gate-fallback-strict.json`: pass, build and full Debug suite.
- `Evidence/gate-strict-release.json`: pass, optimized strict build.
- `Evidence/gate-sanitizers.json`: pass at the fix commit.
- Optimized Release full suite with optional native deferred test skipped:
  process exit 0.
- Release logging suite: `45 passed, 0 failed`.

Windows and Linux evidence remains intentionally pending until the single B02
batch-boundary CI run.
