# B02-S15: Logging System Verification

## Verification Object

- Fix commit:
  `8303045d6b977ecc873033a2da3100756f347055`
- Review-state commit:
  `b8e32c5a3cda17eb07cfd3c614ca278c7c98fbad`
- Findings: `CR001-B02-F010`, `CR001-B02-F011`, `CR001-B02-F012`
- Production/spec changes during this packet: none

The packet used independent probes rather than relying only on the tests added
by the fix.

## F010: Effective Early-Out

An external probe exhausted all 100 combinations formed by five category
thresholds, five global thresholds, and the four non-terminating message
severities. Debug-like and optimized Release builds both reported:

```text
threshold_matrix checks=100 failures=0 side_effects=30
```

The 30 side effects are exactly the combinations where the message severity is
at least both thresholds. Every other format argument remained unevaluated.

An independent arm64 optimized assembly probe confirms the filtered path loads
both one-byte atomics, combines their masks, and executes one `cbz` before the
counter load/increment. There is no earlier conditional comparison or branch.

## F011: Threshold Concurrency

A new ThreadSanitizer probe ran one writer changing both thresholds between
Warning and Fatal while four readers issued 400,000 Verbose `SG_LOG` calls.
It reported:

```text
macro_tsan side_effects=0
```

The process exited `0` with no ThreadSanitizer diagnostic. This covers the
actual macro path rather than only direct getters.

## F012: Fatal Routing And Termination

The maintained child mode was launched directly with stdout and stderr captured
separately:

| Profile | Return | stdout | stderr |
|---|---:|---:|---|
| Debug | `-5` (`SIGTRAP`) | 0 bytes | one labeled Fatal line |
| Release | `-6` (`SIGABRT`) | 0 bytes | one labeled Fatal line |

Both profiles therefore write through the required stderr route and terminate
before fallback return code 42. The maintained Release logging suite also
reports `45 passed, 0 failed`.

## Gates

- Strict Debug fallback build and full suite: pass.
- Strict optimized Release graphics-enabled build: pass.
- Strict optimized Release full suite with optional deferred native coverage
  skipped: pass.
- Existing ASan/UBSan record at the exact fix commit: pass.
- `git diff --check`: pass.
- GitHub Actions used by this packet: none.

## Disposition

The three findings have strong independent macOS evidence but remain `Fixed`.
Their final transition to `Verified` requires the B02 batch-boundary
Windows/macOS/Linux CI matrix, particularly for the Windows `CreateProcess`
branch and Linux `fork`/`exec` branch. No new finding was discovered.
