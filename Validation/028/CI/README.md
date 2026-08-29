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
- The required 16 MiB RSS gate is owned only by explicit maintainer-local arm64
  Metal after fail-closed physical preflight. Both Metal and x86_64 Windows
  Vulkan own accepted 512-by-512 semantic/FLIP image gates; Windows working-set
  RSS is a complete bounded observation, not result authority.
- The two physical lanes must pass on one committed revision. macOS Vulkan is
  deferred and no self-hosted runner is required.

## Current Windows Vulkan Evidence

The maintainer synchronized and ran revision
`7d0cd7327ea26bf0778002df018b3b1c19d0b12d` on the x86_64 Windows Vulkan
device. Strict Release, runner 53/53, report 20/20, RHI 256/256, Vulkan native,
production image/demo, Core lease, and regular-runner regressions passed. The
hardware profile derived device class `windows.discrete-vulkan.rgba8` and
completed:

| Check | Result |
| --- | --- |
| Workload | `production-content-lantern-v2` |
| Lifecycle / warm-up | 1,000 / 20 cycles |
| Captures / readbacks | 2,000 / 7 |
| Terminal owners | 0 |
| Stale handle | rejected |
| Accepted Lantern image | exact decoded-pixel match; FLIP mean/p95/max/bad fraction all zero |
| Cross-process calibration | 3 processes, 60 byte-identical captures, pairwise FLIP all zero |
| Decoded-pixel SHA-256 | `5a6dbaefdf2762001367e14376051b067ff7c662f371afb58a0dbf83c16bcf85` |
| Observed RSS growth | 169,361,408 bytes |
| RSS disposition | `observed` under Decision 51/T179 |

The accepted Lantern baseline was consumed exactly: all 20 semantic probes and
all FLIP limits passed, and blank/stale/origin/missing-geometry/material/color-
space/one-pixel-translation/opposite-normal mutations were rejected. Bounded
cross-process evidence is retained under
`Validation/028/hardware-windows-vulkan-7d0cd73-evidence/`; its calibration JSON
SHA-256 is `5db81ec60fba169184039e9db8812a083d7d14a5bcf7d86a1830c41623362511`.
Equivalent same-path runs observed non-monotonic working-set growth from
11,161,600 to 169,361,408 bytes while ownership and images remained exact.
Decision 51/T179 therefore makes Windows working-set RSS observed without
altering functional or image authority. This pre-policy run failed fast before
Sponza, so Windows Sponza image acceptance remains pending.

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
3. passing maintainer-local Metal and Windows Vulkan hardware profiles on the
   same revision, with Metal within 16 MiB RSS growth, Windows retaining complete
   working-set observations, and both consuming accepted image baselines for
   every required workload; and
4. bounded report, manifest, baseline, and preflight digests recorded here.

Large run directories are never promoted into this path. Only the minimal
digests and conclusions needed to audit the final decision are retained.
