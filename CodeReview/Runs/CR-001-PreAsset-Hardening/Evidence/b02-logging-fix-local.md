# B02-S14 Local Logging Fix Evidence

- Fix commit: `8303045d6b977ecc873033a2da3100756f347055`
- Host: macOS arm64
- GitHub Actions used: none

## Build And Maintained Tests

| Check | Result |
|---|---|
| Strict Debug fallback build (`graphics=disabled`) | Pass |
| Strict Debug full fallback suite | Pass |
| Strict Release fallback build (`graphics=disabled`) | Pass |
| Strict Release full fallback suite | Pass |
| Release logging suite | `45 passed, 0 failed` |
| ASan/UBSan strict build and full suite | Pass |

The maintained Fatal child checks passed in Debug and Release:

```text
[PASS] Fatal log child process starts and completes
[PASS] Fatal log routes the labeled message to stderr
[PASS] Fatal log terminates before returning from SG_LOG
```

The maintained global/concurrency regressions passed:

```text
[PASS] Global severity filter suppresses Verbose when min is Info
[PASS] Global severity early-out does not evaluate format arguments
[PASS] Runtime logging thresholds remain valid under concurrent access
```

## Independent Reproducers

The exact B02-S13 global-filter reproducer was rebuilt against the fix:

| Profile | Before | After | Exit |
|---|---:|---:|---:|
| Debug-like | `side_effect_count=1` | `side_effect_count=0` | 0 |
| Optimized Release | `side_effect_count=1` | `side_effect_count=0` | 0 |

The exact category-only and combined global/category ThreadSanitizer
reproducers were rebuilt with `-O0 -fsanitize=thread`. Both now exit `0` with
no ThreadSanitizer report; before the fix each identified its corresponding
threshold race.

## Authoritative Records

- `Evidence/gate-sanitizers.json`
- `Batches/B02-logging-inspection.md`
- `Evidence/b02-logging-probes.md`

`git diff --check` passes. Windows and Linux compilation/runtime evidence is
deferred to the single B02 batch-boundary CI matrix.
