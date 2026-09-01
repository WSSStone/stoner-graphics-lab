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

## Final Closeout

Final workflow-dispatch run
[`33467298777`](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/33467298777)
completed successfully at revision
`588d24560dc6ba7b67aa4f14e60026d058b0ab31`. Linux ASan/UBSan, Linux TSan,
Regular Windows Vulkan, Regular Linux Vulkan, Regular arm64 macOS Metal,
Regular Intel macOS Metal, all four immutable-artifact consumers, both medium
Metal shards, and Aggregate medium Metal shards passed.

| Hosted artifact | GitHub archive SHA-256 | Summary SHA-256 | Manifest SHA-256 |
| --- | --- | --- | --- |
| `production-regular-windows-vulkan-1` | `94fc0e6127551aeb284fbf44d0a13cda4f9d35120790ab0f2a1fd6be918c6d26` | `e9ad7ab143c1ed35a2f498d8a836652f55af07f9f3935c8c42f9008aab991020` | `9c3c2e15829205eb863167e2cb7d915d193d9f52fccd7a8b12127e09670e1863` |
| `production-regular-linux-vulkan-1` | `0e0c1bfb14d0d5a95682f59be7c4ea2dd8ccd5a75b9d7a31686d159437e8c27e` | `207cbe72f68a172af05971f1a3acb146a77327ad7d27269e26202247de1a5d1c` | `df1439d958dff989a599d5685f675c9e84145718c609b1b3b9a4d241958d4564` |
| `production-regular-macos-metal-1` | `7a464d40bdcbe87fc8f39a166ab411d8dc8682c7f5a78be2252bfcf8fd9daa23` | `ee2fdd6462834f4c423b5198d6f63b90d98933c43170281e38fcf13ab92cb38e` | `a7ffaeb32fd40b793e824a3b751c4983a7fc53c664badb4ec5be2f0cac1c2c8e` |
| `production-regular-macos-intel-metal-1` | `5ecf798a5ac73776ff043e154714064406d2b9a7eca52f6fcb9e19ef1a09c6b8` | `297f6df18e807d1a7c852d613c66b78a473e6ce5797945efd2a6ea1d7e321af3` | `6ea499153f0cf87f6773edf7ea50849f35262ea7a4299c28703e46f2c30932b2` |

The four consumer archives independently revalidated those manifests: Windows
`ccbef1078ba8696f94a8e8f19e65d6e73173e8872190b7ef6ab4c1f35f5cb837`,
Linux `8f489c4deeac5e03ad8168e8b4824cffd6528b02a96695eb6d08b08851c75d7d`,
arm64 Metal `92ca159110800640be3c511100d9ad03085292a453e2d24e01e2077acfe0c021`,
and Intel Metal
`e2b61bb45ed6eec89fb62253da483254df93b0e01d0127acd76856563183bb77`.
ASan/UBSan and TSan archives have SHA-256
`57b33107516f233740e036209db4b46ad19956c2eaf9902a1370bed07ba77b97`
and `089e34ab756125070f039da1899808c016802f37da35ecc31988e22605e9a7e2`.

Hosted medium retained the exact schema-v4 responsibilities. Lantern completed
1,000/20 endurance cycles, 2,000 captures, seven readbacks, zero owners, and
stale rejection; summary/manifest SHA-256 values are
`c13db5c877673f1be4e43f67db9339c51a6e88854c8723bab7cacf3dc87727a5`
and `4540244f5ce4e5f40d871684c7e1bafbaead1f5aaa34c6fda039d07845b4ccca`.
Sponza completed 100/10 scale-lifecycle cycles, 200 captures, seven readbacks,
zero owners, and stale rejection; summary/manifest SHA-256 values are
`41f867d737064277aebabed350557db76ba9dea35e7e5297b3767753a2ee9d1f`
and `7d5b33a7520119f02d550edfdd886b6e852b28bad766e35d56457affdb5bedbe`.
The passing aggregate summary SHA-256 is
`b0cf9be2cefdb08247738c18cae5509eee5930663f45ffe2c10e845c88bc33e3`;
hosted RSS and timing remained observations/operational measurements.

The same final revision passed the maintainer-local M4 Metal hardware profile
in 630.700 seconds. Both packages consumed their Accepted exact-drawable v2
references with FLIP mean/p95/maximum/bad fraction all zero, 20 semantic
probes, 1,000/20 cycles, 2,000 captures, seven readbacks, zero terminal owners,
stale rejection, and zero RSS growth. Summary SHA-256 is
`26a479e8b93cd1bbd74c2550be69c3b8a3624a90d7efb11dee3e408873d5f89c`,
manifest SHA-256 is
`c6a01a2f772dd2cc747b97e12d2b840ec9af29cb3b3eec027fcd32bea8f33bb9`,
and preflight evidence digest is
`468c589ccd35caf49722f1ad617ccf3e86a15ebec2eeb6efa2c19772f744874b`.

No Windows hardware run occurred at `588d245`. On 2026-09-01 the maintainer
explicitly accepted a one-time carry-forward of the prior complete Windows
physical evidence at `0cf018257902a5077cff27098d7117b842eddbe6`, the exact
Windows references later admitted at
`700005bc25972bdd3baf6da33493d737598ce575`, and the final hosted Windows
producer/consumer above. The prior physical summary and manifest SHA-256 values
are `7c676b88423c7fd8af2be291fb27e15903ab07e9dd29d1da32dc4588ce4c666b`
and `74ea6c7450c6af607e3d5662b6804c5412db2ff26ce249e9702beea76e7d7f29`.
This attestation closes Feature 028 while preserving the provenance gap; it is
not a fabricated pass and does not automatically authorize carry-forward for
future baseline or render-path changes.

Large run directories are not promoted here. The audit record retains only
these bounded conclusions and digests; provider artifacts remain externally
retained and local generated evidence remains ignored.
