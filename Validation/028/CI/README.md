# Feature 028 CI Evidence

This index intentionally records only current closeout authority and bounded
digests. Raw captures, readbacks, DDC entries, cooked generations, downloaded
artifacts, and full logs belong under ignored `Build/Validation/028/` or the CI
provider's retention and are safe to regenerate. Superseded run narratives are
not source-controlled evidence.

## Environment Authority

- GitHub-hosted regular and medium lanes own build, deterministic cook,
  strict-cooked runtime, native functional/lifecycle completion, captures,
  readbacks, zero terminal owners, stale-handle rejection, sanitizers, and
  operational timeout completion.
- Hosted RSS, task-VM, allocator, peak, and elapsed values are observations;
  they do not decide hosted success.
- The required 16 MiB RSS and accepted 512-by-512 semantic/FLIP image gates are
  owned only by explicit maintainer-local arm64 Metal and x86_64 Windows Vulkan
  runs after fail-closed physical preflight.
- The two physical lanes must pass on one committed revision. macOS Vulkan is
  deferred and no self-hosted runner is required.

## Current Windows Vulkan Evidence

The maintainer synchronized and ran revision
`738d66f150c0cd554267939328d0f8366336a2ff` on the x86_64 Windows Vulkan
device. Strict Release and the focused regression suites passed. The hardware
profile derived device class `windows.discrete-vulkan.rgba8` and completed:

| Check | Result |
| --- | --- |
| Workload | `production-content-lantern-v2` |
| Lifecycle / warm-up | 1,000 / 20 cycles |
| Captures / readbacks | 2,000 / 7 |
| Terminal owners | 0 |
| Stale handle | rejected |
| Candidate source SHA-256 | `4147e4c5b0a2380ebcd3177785e06d964b2851132cbf4050c831c7b5dc25c95b` |
| Summary SHA-256 | `7c5c21b8428b8d1eeba24e0075c7c693f9922b09911a82ff69d3fb61c60d0941` |
| RSS growth | 62,119,936 bytes |
| Required RSS limit | 16,777,216 bytes |

The maintainer explicitly accepted the Lantern candidate on 2026-08-28. Its
pixel-identical lossless PNG and calibration record are now the consumable
source-controlled evidence. The run remains failed because RSS exceeded the
physical limit. Fail-fast correctly prevented Sponza from running, so Windows
Sponza image acceptance is still pending.

## Prior Hosted Result

Hosted run
[`33135020377`](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/33135020377)
at revision `ff754f40ece2330c97880a189912b29a71c71637` passed the regular
producers/consumers, ASan/UBSan, TSan, and Lantern medium shard. Its Sponza
medium native stage timed out after 3,499 seconds, so the run is diagnostic
failure evidence rather than final closeout authority. Hosted telemetry from
that run remains observation-only.

## Remaining Closeout

T112-T114 remain open. The final shared revision must provide:

1. a passing hosted Windows/macOS/Linux regular matrix;
2. a passing hosted medium aggregate with the exact 1,000/20 lifecycle facts;
3. passing maintainer-local Metal and Windows Vulkan hardware profiles, each
   within 16 MiB RSS growth and consuming accepted image baselines for every
   required workload; and
4. bounded report, manifest, baseline, and preflight digests recorded here.

Large run directories are never promoted into this path. Only the minimal
digests and conclusions needed to audit the final decision are retained.
