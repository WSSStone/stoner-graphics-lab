# Feature 028 Baseline Evidence

This directory indexes reviewed production image baseline policy and hardware
evidence. Candidate captures remain ignored until semantic probes pass,
twenty-capture calibration rejects the mutation set, and a maintainer changes
the matching record to `accepted`.

All formal references and candidates are exactly 512-by-512. Semantic
classification uses workload-versioned 9-by-9 regions (four-pixel radius),
channel medians, minimum matching-sample fractions, and directional normal
coverage rather than single edge pixels. FLIP remains an exact same-size,
same-coordinate comparison with no registration or alignment; the calibration
set rejects a one-pixel whole-image translation.

Reference pixels are stored only as losslessly compressed PNG. The registry
hashes the PNG bytes, while admission separately proves that decoding produces
the exact reviewed RGB pixels. Raw PPM/RGBA captures, hardware packages, DDC,
and cooked generations remain ignored under `Build/Validation/` and are not
source-controlled evidence.

## Accepted macOS Apple8 baselines

The corrected native winding exposed that the former Lantern
`production-content-v1` image showed the back face and relied on a light placed
behind the intended surface. Those records are retained as `superseded` and
cannot be consumed. Revision `production-content-lantern-v2` preserves the
frozen identity camera, validates the intended approximately -X world normal,
and moves the key light to the camera-facing side. Its Metal and
MoltenVK/Vulkan records were explicitly accepted by the maintainer after their
twenty same-revision captures produced zero FLIP error and the calibration
suite rejected blank, stale-frame, origin, missing-geometry, material-swap,
color-space, one-pixel translation, and opposite-normal mutations.

| Backend | Device class | Baseline ID | Calibration evidence SHA-256 |
| --- | --- | --- | --- |
| Metal | `macos.apple8.metal.rgba8` | `production-content-lantern-v2.macos.apple8.metal.rgba8.v2` | `c50a9df0d32ab68d1b7042285b106c0f4b1135fb3d6d4c181badbde41c8f25d6` |
| Vulkan (MoltenVK) | `macos.apple8.moltenvk.rgba8` | `production-content-lantern-v2.macos.apple8.moltenvk.rgba8.v1` | `e53e0fbcc5a7797f4ed4d35c4cb4f59311e4965abfde1a1c75160dbd47468d73` |

The MoltenVK record uses reference image SHA-256
`a3a2f8d402f3ea4c7396006572c8936f675eac47db6e18b04f0e53130a32af6a`.
Revision `2d8bc582a7f87bcd0a5cd57f1f73fe19ea43f44b` corrected the formal Metal
authority window on Retina displays by converging the measured drawable to
exactly 512 by 512. Its presented pixels exactly matched the same-submission
FinalOutput, but correctly did not match the older blurred Metal reference.
Three independent processes then produced 60 identical captures in one mode,
with decoded-pixel SHA-256
`41ae81b11cff782330ee35aca87ca25df76593c314917a113b21fe7949c8bbb2`;
all required mutations were rejected. The maintainer explicitly accepted this
replacement on 2026-09-01. Its lossless PNG SHA-256 is
`0ac8a8f04d32ead184dd9f4b22dcf40d6932a50fdfe993f1bc39268ed773172f`.
No translation, crop, scale, resampling, or alignment was applied. The prior
Metal v1 reference is superseded by repository history and is not retained as
a consumable mode.

Both current records remain selected only by exact workload, backend,
registry-derived device class, and capability-signature equality. The
`StateFixtures/` records retain all four non-accepted lifecycle states against
a fixture-only workload so the production selector must continue to reject
them.

Feature 028 freezes `sampleCount=1` and performs no anti-aliasing or general
post-processing. That visibly aliased output is the reviewed v2 authority for
this phase; a future post-processing/anti-aliasing feature must advance the
workload revision and repeat calibration rather than silently replacing it.

## Accepted Sponza v2 image baseline

The explicitly accepted camera and corrected outward-facing surface convention
for `production-content-sponza-v2` observes the frozen diagnostic world normal
as approximately `(-0.0000304, +0.999512, -0.00919342)` on Apple8 Metal and
MoltenVK. The semantic gate requires at least 60 percent of the bounded
diagnostic region to align with +Y at dot 0.8 or greater, so a future common
backend winding regression fails before FLIP without depending on one edge
pixel. The original 2026-08-24 acceptance remains authoritative for MoltenVK;
the former Sponza v1 workload is `superseded` and cannot be selected.

After T196 removed Retina drawable resampling from formal Metal authority,
revision `a21ae57fe4bfa207104c775f6844fe3c1130d694` completed the full Metal
1,000/20 lifecycle, 2,000 window captures, seven readbacks, zero terminal
owners, stale-handle rejection, 20 same-frame semantic probes, exact
FinalOutput/window equality, and zero RSS growth. Three independent processes
then produced 60 exact 512-by-512 captures in one zero-noise mode. Blank,
stale-frame, origin, one-pixel translation, missing-geometry, material-swap,
color-space, and opposite-normal mutations were all rejected. The maintainer
explicitly accepted this exact-drawable Metal replacement on 2026-09-01.

| Backend | Device class | Accepted baseline ID | Calibration evidence SHA-256 |
| --- | --- | --- | --- |
| Metal | `macos.apple8.metal.rgba8` | `production-content-sponza-v2.macos.apple8.metal.rgba8.v2` | `9f16d72122dad114e01785feb9de29a5a9a643a532f1d59250a36f7cae8d1ad2` |
| Vulkan (MoltenVK) | `macos.apple8.moltenvk.rgba8` | `production-content-sponza-v2.macos.apple8.moltenvk.rgba8.v1` | `7ff0ed70e15b50851fdfabdc6993dfb8398d5cfd0d0f05ba73071ffd9cce7963` |

The exact-drawable Metal PNG SHA-256 is
`b2c7f49b45fb3c695229c66a1f29e93a5b41bdb9b69cca0499e2b278c313cb93`
and its decoded-pixel SHA-256 is
`2e5805e31f005c4184ae759d156a3f24884781178eac218df12ce1f1f47b9bbb`.
The unchanged MoltenVK PNG SHA-256 is
`7e98feb07832b7d8dfdf87977a27ac1dbb955070575d2b092c985d1bb3ca8f4f`.
No translation, crop, scale, resampling, or alignment was applied to either
accepted reference.

## Accepted Windows Vulkan Lantern v2 baseline

The maintainer synchronized revision
`738d66f150c0cd554267939328d0f8366336a2ff` to the x86_64 Windows device and
ran strict Release plus the hardware profile. The registry derived
`windows.discrete-vulkan.rgba8`; 1,000 lifecycle cycles, 2,000 captures, seven
readbacks, zero terminal owners, and stale-handle rejection completed. The
twenty 512-by-512 application-window captures were byte-identical. The
maintainer explicitly accepted the reviewed Lantern image on 2026-08-28.

| Backend | Device class | Accepted baseline ID | Calibration evidence SHA-256 |
| --- | --- | --- | --- |
| Vulkan | `windows.discrete-vulkan.rgba8` | `production-content-lantern-v2.windows.discrete-vulkan.rgba8.v1` | `6b8465cadf8941395a22225bdd92b866176db1bb1dcb6f8e7c59bb6dbb4ab31c` |

The accepted source PPM had SHA-256
`4147e4c5b0a2380ebcd3177785e06d964b2851132cbf4050c831c7b5dc25c95b`.
Its lossless PNG has SHA-256
`47d0abd805f608148323259c1878fcaee0e6cd9c49ade727960b002320618a61`;
decoded RGB was verified pixel-identical before registration.

Revision `7d0cd7327ea26bf0778002df018b3b1c19d0b12d` subsequently reconfirmed the
accepted baseline across three independent processes and 60 byte-identical
captures. Decoded pixels had SHA-256
`5a6dbaefdf2762001367e14376051b067ff7c662f371afb58a0dbf83c16bcf85`;
all pairwise and accepted-baseline FLIP fields were zero, 20 semantic probes
passed, and every declared mutation was rejected. The bounded calibration JSON
under `Validation/028/hardware-windows-vulkan-7d0cd73-evidence/` has SHA-256
`5db81ec60fba169184039e9db8812a083d7d14a5bcf7d86a1830c41623362511`.

The same full authority run observed 169,361,408 bytes of non-monotonic Windows
working-set growth despite zero terminal owners. Decision 51/T179 classifies
that metric as observed while preserving every physical image and lifecycle
gate. Final closeout does not claim a new Windows hardware execution: the
maintainer explicitly accepted the prior complete Windows physical bundle,
the exact references admitted from it, and final-revision hosted Windows
producer/consumer evidence as a one-time attested carry-forward.

## Accepted Windows Vulkan Sponza v2 baseline

Revision `83979434abe47afd209d6f657370c0556a8eddef` produced a stable
512-by-512 Windows Vulkan Sponza Candidate after the exact 1,000/20 lifecycle,
2,000 captures, seven readbacks, zero terminal owners, and stale-handle
rejection. The PNG SHA-256 is
`cf94b5aba99cd3746090283f4115f99c9bf28c13c9e33b95c06c2a4cfcb586a2`;
its decoded-pixel SHA-256 is
`4e294eb54577dfb7d4b3d7373e782a6bc76bc1d1f9d30ccc1d892f8818a61db2`.
The run reported zero semantic probes because the old native acceptance order
stopped at `baseline-missing`; T180 corrected that ordering.

Revision `0cf018257902a5077cff27098d7117b842eddbe6` then bound all Deferred
attachments, FinalOutput, and the window capture to one real submission frame
token. The corrected Windows authority reported 20 semantic probes, exact
1,000/20 lifecycle work, 2,000 captures, seven readbacks, zero terminal owners,
stale-handle rejection, and the same decoded candidate pixels. Its canonical
filter-zero lossless PNG has SHA-256
`63243567ebe8c711d68c2ac463072964b6f2d0169e96b9f635c1d9c6efabea1b`.
Cross-process calibration retained one mode across three independent processes
and rejected blank, stale, origin, one-pixel translation, missing-geometry,
material-swap, color-space, and opposite-normal mutations.

The maintainer explicitly accepted the reviewed Windows Sponza v2 image on
2026-08-30. It is registered as
`production-content-sponza-v2.windows.discrete-vulkan.rgba8.v1` with one exact
reference; no translation, crop, scale, resampling, or alignment was applied.
The calibration evidence SHA-256 is
`b11ea1cc3abfe888985c8c0d2f89f754f99204bd465d63051f600a0379bf1d4c`.
The Accepted record contains the exact decoded pixels emitted by that physical
run. On 2026-09-01 the maintainer explicitly accepted this prior physical
evidence for Feature 028 closeout without asserting that the final revision ran
on Windows hardware. Future Windows reference-image or render-path changes do
not inherit this carry-forward decision.

## Final local consumption evidence

Final implementation revision
`c82e51790643fc6583a80240239f6f4123cc0df1` consumed the Lantern v2 Metal and
MoltenVK records without fallback in post-prime 20-frame visible gates. Each
reported 20 semantic probes, zero FLIP error, 40 captures, seven readbacks, zero
terminal owners, and stale-handle rejection. Its clean predecessor
`426d8617fe8558114110b09a60260de3895da82f` consumed all four accepted Apple8
records without fallback and rendered 1,000
visible lifecycle cycles per workload and reported 20 semantic probes plus zero
FLIP error for both Lantern v2 and Sponza v2. Independent consumer verification
accepted 4,369 artifacts with manifest SHA-256
`5cd36ce6a287e40884d77f538b838a5f06bce3edd23c76258a1b16cd3918bfdb`
and summary SHA-256
`6c456e04daf300a02fc5e7668034094d5760c859ca795923059e1a22e72c1c83`.

The final pre-prime binary also passed separate 20-frame visible MoltenVK gates
for both workloads with exact registry-derived baseline selection.

Final closeout revision
`588d24560dc6ba7b67aa4f14e60026d058b0ab31` then passed the maintainer-local
M4 Metal authority for both workloads. Each consumed its exact-drawable Metal
v2 reference with all four FLIP fields equal to zero, 20 semantic probes,
1,000/20 lifecycle cycles, 2,000 captures, seven readbacks, zero terminal
owners, stale-handle rejection, and zero RSS growth. The authority summary
SHA-256 is
`26a479e8b93cd1bbd74c2550be69c3b8a3624a90d7efb11dee3e408873d5f89c`,
artifact manifest SHA-256 is
`c6a01a2f772dd2cc747b97e12d2b840ec9af29cb3b3eec027fcd32bea8f33bb9`,
and preflight evidence digest is
`468c589ccd35caf49722f1ad617ccf3e86a15ebec2eeb6efa2c19772f744874b`.
Decision 57 records the separately attested Windows carry-forward and its
residual provenance limitation.
