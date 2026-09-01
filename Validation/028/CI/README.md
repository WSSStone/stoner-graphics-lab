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
Sponza; later evidence and acceptance are recorded below.

## Prior Hosted Result

Hosted run
[`33135020377`](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/33135020377)
at revision `ff754f40ece2330c97880a189912b29a71c71637` passed the regular
producers/consumers, ASan/UBSan, TSan, and Lantern medium shard. Its Sponza
medium native stage timed out after 3,499 seconds, so the run is diagnostic
failure evidence rather than final closeout authority. Hosted telemetry from
that run remains observation-only.

Hosted prerequisite run
[`33308514205`](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/33308514205)
at revision `2f33cb9b6efbf9cef9b001d1f3d097310a242e86` passed Regular Windows
Vulkan, Regular Linux Vulkan, arm64 and Intel macOS Metal, ASan/UBSan, TSan,
and all four immutable-artifact revalidation consumers. It validated the
900/1,200/600-second regular package/profile/native operational envelopes before
baseline admission. Because the Accepted Windows Sponza record is committed
after that revision, it is prerequisite evidence rather than the final shared-
revision closeout run.

Hosted diagnostic run
[`33362940103`](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/33362940103)
at revision `63db4128c6a9f155524f587cb01be942fb3a0ee2` passed both schema-v4
medium shards, their aggregate, all regular producers, both sanitizers, and all
producer validation. Its first attempt failed only when the macOS Intel artifact
upload encountered GitHub DNS `ENOTFOUND`. A failed-job rerun uploaded and
revalidated that Intel artifact, but exposed that the other regular consumers
incorrectly requested attempt-2 names for unchanged attempt-1 producer
artifacts. T195 records the rerun-safe immutable-artifact resolution correction;
this diagnostic run is not final closeout authority.

Hosted push run
[`33404255291`](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/33404255291)
at revision `2d8bc582a7f87bcd0a5cd57f1f73fe19ea43f44b` passed Regular Windows
Vulkan, Regular Linux Vulkan, arm64 and Intel macOS Metal, Linux ASan/UBSan,
Linux TSan, and all four immutable-artifact revalidation consumers. Medium
shards were intentionally skipped by push policy. This run validates the
exact-drawable correction and is prerequisite evidence for the subsequently
accepted Metal Lantern v2 reference; final closeout still requires one shared
post-admission revision.

Hosted workflow-dispatch run
[`33462511699`](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/33462511699)
at revision `a21ae57fe4bfa207104c775f6844fe3c1130d694` passed Regular Windows
Vulkan, Regular Linux Vulkan, arm64 and Intel macOS Metal, Linux ASan/UBSan,
Linux TSan, both schema-v4 medium shards, Aggregate medium Metal shards, and
all four immutable-artifact revalidation consumers. This is the passing hosted
prerequisite immediately before exact-drawable Metal Sponza admission; the
post-admission revision must still repeat final shared-revision closeout.

## Metal Sponza exact-drawable acceptance

The maintainer-local Metal hardware profile at revision
`a21ae57fe4bfa207104c775f6844fe3c1130d694` passed preflight. Lantern
consumed its exact-drawable v2 baseline with zero FLIP error. Sponza completed
the exact 1,000/20 lifecycle, 2,000 captures, seven readbacks, zero terminal
owners, stale-handle rejection, 20 same-frame semantic probes, exact
FinalOutput/window equality, and zero RSS growth; it failed closed only against
the superseded Metal reference. Three independent calibration processes then
produced 60 exact 512-by-512 captures in one zero-noise mode and rejected the
complete mutation set. The maintainer explicitly accepted the reviewed
replacement on 2026-09-01. Its PNG SHA-256 is
`b2c7f49b45fb3c695229c66a1f29e93a5b41bdb9b69cca0499e2b278c313cb93`,
decoded pixels are
`2e5805e31f005c4184ae759d156a3f24884781178eac218df12ce1f1f47b9bbb`,
and canonical calibration evidence SHA-256 is
`9f16d72122dad114e01785feb9de29a5a9a643a532f1d59250a36f7cae8d1ad2`.
The Accepted registry ID is
`production-content-sponza-v2.macos.apple8.metal.rgba8.v2`.

## Windows Sponza acceptance

The maintainer-local Windows Vulkan hardware profile at revision
`83979434abe47afd209d6f657370c0556a8eddef` passed preflight and completed both
1,000/20 workloads inside the 3,600-second operational budget. Lantern consumed
its accepted baseline with zero FLIP error. Sponza completed 2,000 captures,
seven readbacks, zero terminal owners, stale-handle rejection, and observed
176,783,360 bytes of working-set growth, then failed closed only because the
exact Windows Sponza baseline was missing. Its 20 candidate frames were
pixel-identical. After acceptance, their redundant lossless PNG was removed;
the exact decoded mode is retained once in the canonical baseline registry.

That run also exposed an ordering defect: accepted-baseline selection happened
before attachment/workload semantics, so expected Candidate evidence reported
zero semantic probes. T180 moved semantic probes before baseline lookup.
Revision `0cf018257902a5077cff27098d7117b842eddbe6` then reported 20 semantic
probes from a same-submission authoritative frame bundle and reproduced the
same decoded pixels while completing the full Windows physical workload. The
three-process calibration retained one exact mode and rejected the complete
mutation set. The maintainer explicitly accepted the reviewed candidate on
2026-08-30; the canonical PNG SHA-256 is
`63243567ebe8c711d68c2ac463072964b6f2d0169e96b9f635c1d9c6efabea1b`,
decoded pixels are
`4e294eb54577dfb7d4b3d7373e782a6bc76bc1d1f9d30ccc1d892f8818a61db2`,
and calibration evidence SHA-256 is
`b11ea1cc3abfe888985c8c0d2f89f754f99204bd465d63051f600a0379bf1d4c`.
The Accepted registry ID is
`production-content-sponza-v2.windows.discrete-vulkan.rgba8.v1`.

## Remaining Closeout

T112-T114 remain open. The final shared revision after baseline admission must
provide:

1. a passing hosted Windows/macOS/Linux regular matrix;
2. a passing hosted medium aggregate with exact schema-v4 package lifecycle
   facts: Lantern endurance 1,000/20 and Sponza scale lifecycle 100/10;
3. passing maintainer-local Metal and Windows Vulkan hardware profiles on the
   same revision, with Metal within 16 MiB RSS growth, Windows retaining complete
   working-set observations, and both consuming accepted image baselines for
   every required workload; and
4. bounded report, manifest, baseline, and preflight digests recorded here.

Large run directories are never promoted into this path. Only the minimal
digests and conclusions needed to audit the final decision are retained.
