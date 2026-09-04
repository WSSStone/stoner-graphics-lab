# Feature 029 CI and Authority Evidence

This index preserves the original implementation working-tree evidence and
separately records the frozen-revision Windows T102 and M4 T101/T104 runs below. The original
captures remain preliminary; committing this index does not change their
provenance. This index does not establish Feature completion, hosted run IDs,
complete maintainer SDR acceptance, or maintainer-authored
HDR visual attestations. Windows raw evidence is archived on the evidence share
under `Build/Validation/029/`; its current cooked packages remain on local NTFS.

## Source State

- Working-tree base:
  `e66f848d5537db2f5f98b79ae9892b8f1ee9da26`
  (`docs(roadmap): add HDR output and temporal phases`).
- These captures are intentionally recorded as working-tree evidence, not
  retrospectively attributed to the commit that stores them. Freeze the
  implementation SHA after commit, then rerun the formal producers on both
  platforms. Later evidence-only commits retain that tested software SHA.
- Feature 028 v2 references remain immutable historical evidence. Feature 029
  v3 Candidates do not reuse or reinterpret their authority.

## Local macOS Implementation Evidence

On the physical arm64 M4 host running macOS 26.6.2:

- strict Release built successfully with
  `/Users/wangshi/miniforge3/envs/godot/bin/scons config=release strict=1`;
- the Release test and demo SHA-256 digests are
  `98f80d6255e931ac71f7b4eee2335358e543e92cfae957246baa6cc862d66e54`
  and
  `a57913a403a41ed46807bc0ca2d10f7bfffa59bd48ff970d3c21b725bea33248`;
- all Feature 029 CPU, profile, RHI, Render Graph, insertion, lifecycle, Vulkan
  native, Metal native, and GPU-conformance suites passed;
- Vulkan and Metal each completed 288 frozen GPU samples over 20 repeats. The
  maximum encoded-code, decoded-RGB, and decoded-XYZ errors were
  `0.0000258371`, `0.0577269`, and `0.0488552` respectively;
- the physical deferred regression passed native Metal and Vulkan-through-
  MoltenVK submission, 36 depth-policy probes, cross-backend comparison, and
  zero-live teardown. The Metal readback and comparison SHA-256 digests are
  `f711688ed3445e8dfcc2015093dad0c6446bc7bb9d18e887be43fc6027bda632`
  and
  `8c7d1cb251eaa17dfebc626195a6ae2c8314471e39f8133604236b386ed96348`;
- the Feature 028 regular producer/consumer regression completed 20 clean
  cooks, warm cook, publication, semantic equivalence, strict runtime, and
  native Metal lifecycle with zero terminal owners. Its ignored local summary
  and manifest SHA-256 digests are
  `7f98925d20b02c5a22621c8c3a3054d368cd8f4bc1d1d60b2d168d39f62883e0`
  and
  `c1317a910c8f13fdcab4c06a4859eb6fd00b2e8ac3b20968d1015ff0633429c4`.

The normalized regression inventory is
`Validation/029/CI/regressions.json`. Vector, architecture, evidence, numbering,
dependency, anchor, task-reference, old-phase-reference, and whitespace scans
currently report zero findings.

## Preliminary SDR v3 Candidate History

The physical M4 produced preliminary exact 512-by-512, `sampleCount=1`,
lossless v3 Candidates. Neither Candidate is Accepted or valid same-revision
closeout authority. Preserve these files; generate new frozen-revision bundles
under a fresh subdirectory, including calibration and native linkage.

| Workload | Candidate JSON SHA-256 | PNG SHA-256 | State |
| --- | --- | --- | --- |
| Lantern v3 | `da2c0971143930a22dff75a2d13224afce66743550aed3e0543fa33ba4c55211` | `7a3c6aaac225471a408a27b1457df0848818ec03b587091e19279e8a25afb49b` | `candidate` |
| Sponza v3 | `7aee802ac71c934417a720037755ee045a2aa4449a7881ca4585f0e128bf3615` | `4f72c4871fe51280f7db47f026059ca31150b2130d9f53f0d65eb9281eb249e7` | `candidate` |

The physical Windows Vulkan authority has generated its own frozen-revision
Lantern and Sponza v3 Candidates as recorded below. Feature 028 carry-forward
is forbidden.
After both platforms have fresh Candidates, only an explicit maintainer edit to
`Config/Validation/OutputTransform/SDR/Baselines-v3.json` may admit or reject
each exact record. No alignment, crop, scale, warp, resize, or resampling is
permitted.

## Physical Windows T102 — 2026-09-04

T102 passed on physical x86_64 Windows, an NVIDIA RTX 3080 Vulkan device, and
an active local Console session. The tested software revision is exactly
`1f463520006d2ade3d1b4375a51ad947dd7f1847`. The later documentation commit that
records this result is not the tested software revision; consumers must check
out the full SHA above rather than the moving branch tip.

Strict Debug (serial) and Release builds, both configurations' seven output
contract suites, all eight Python checks, and the applicable cook, image,
Vulkan and Deferred regressions passed. Both workloads were freshly cooked and
captured on local NTFS at 512x512, `sampleCount=1`, without image alignment,
cropping, scaling or resampling. Calibration used three independent processes
with twenty captures each, measured zero cross-process pixel differences, and
rejected all eight existing mutations. Native readback, same-frame presentation,
Candidate linkage and both SDR reports passed with zero verifier findings.

| Workload | Fresh cook / warm reuse | Strict runtime | SDR report SHA-256 |
| --- | --- | --- | --- |
| Lantern v3 | 42 / 42 | 27/27, zero source fallback | `0aa99bd90f835bef85e8f2f641680a7498221c2d0dbb58093d6b0774410543e7` |
| Sponza v3 | 194 / 194 | 27/27, zero source fallback | `4e67bc1b9ae342ac921af60644934a96ff833fd0c2a98eb429e52267c0ff2ea5` |

The evidence share is `Y:\stoner` on this Windows host. Paths below are relative
to that share, not files required in a fresh repository checkout:

- Archive: `Build/Validation/029/windows-vulkan-repair-20260904-09/feature-029-windows-t102-evidence-only.zip`.
- Archive SHA-256: `ec7a041bce19aecd51349de6b8f96d721bc94ddb9b6b1111eb0fce30c1a47abe`.
- Archive size: 2,571,279 bytes; 33 entries; all 325 original evidence records
  independently reconstructed and verified after transfer.
- Exact commands, outcomes, input and binary identities: the same run's
  `exact-commands.json`, archive inventory and `local-final-check.json`.
- Bounded summary: `Validation/029/CI/windows-vulkan-1f46352-20260904-09/summary.json`.
- Candidate bundles: `Validation/029/SDR/Windows-Vulkan/{Lantern,Sponza}/1f46352-20260904-09/`.
- Source bundle, patch and exact-commit checkout instructions: the same run's
  `software-fix.bundle`, `software-fix.patch` and `SOURCE-HANDOFF.txt`.

The bounded CI JSON and both complete SDR bundles are now imported at their
original repository-relative paths. The archive manifest also indexes raw
files retained only on the share; it is an archive inventory, not a claim that
those raw files are checked in. Earlier failed runs remain historical records
and were not relabeled. Source Candidates retain `acceptance=null` unchanged.

On 2026-09-04 the maintainer viewed both exact Windows Candidate PNGs and
explicitly replied "acceptable". The two Accepted registry copies and
`Validation/029/SDR/Windows-Vulkan/acceptance-1f46352-20260904.json` record
that decision and immutable hashes. It covers Windows Lantern/Sponza only;
T103 remains open for macOS, and no HDR decision is implied. This run
establishes the Windows local portion of T112 only; the
other same-revision platform and sanitizer jobs remain outstanding. macOS must
rebuild and recapture against the tested software SHA. No Windows HDR or
Console/RDP equivalence validation is claimed.

## Physical M4 T101/T104 — 2026-09-04

Fresh captures used the exact checkout
`1f463520006d2ade3d1b4375a51ad947dd7f1847`, with software-input guards before
and after every command. Strict Release was rebuilt, including the changed
native writer/test translation units and linked executables. No earlier probe
was relabeled. `m4-metal-1f46352-20260904-01/summary.json` records binary,
command, log, cook, and bounded artifact digests.

- Fresh Lantern cook: 42 assets, followed by 42/42 warm reuse.
- Fresh Sponza cook: 194 assets, followed by 194/194 warm reuse.
- Both SDR workloads: three independent processes, twenty captures each,
  zero cross-process pixel noise, all eight mutations rejected, exact 512x512
  native readback/Candidate/calibration linkage, and zero verifier findings.
- Formal SDR bundles: `Validation/029/SDR/M4-Metal/{Lantern,Sponza}/1f46352-20260904-01/`.
  Both remain Candidates; the Windows acceptance does not admit macOS images.
- PQ1000, PQ2000, EDR1000, and EDR2000 completed fresh same-frame native
  preflights, with `EDRMetadata=nil` and their declared platform adaptation.
  Their probes/reports are under `Validation/029/HDR/{Probes,Reports}/1f46352-20260904-01/`.
- `Validation/029/HDR/hdr-live-review-request.json` is
  `ready-for-live-review`, with no human visual decision. The accompanying
  display-capabilities JSON records the ordered profile-capability digest
  derivation; it makes no photometric or achieved-peak claim.
- 71 Python tests passed. The seven output contract suites, Metal policy and
  opt-in native presentation checks passed. Metal and Vulkan each passed
  288 shader samples over 20 repeats. Architecture, frozen-vector, evidence,
  numbering, dependency, anchor, task-reference, and stale-phase scans passed.

These evidence-storage commits do not change the tested software identity.
T101/T104 are complete; T103 remains open for macOS review, and T105 requires
the maintainer's live four-profile observations. This is 112/118 tasks, not
Feature completion. Raw logs, commands, PPMs, DDC and cooked packages remain in
ignored `Build/Validation/029/m4-formal-1f46352-20260904-01/`.

## +3 EV Live Feedback and Background Endurance — 2026-09-04

`Validation/029/HDR/README.md` indexes the unchanged +3 EV live-review request,
settings, preflights, explicit conversation feedback, and incomplete foreground
replay history. The maintainer's "可以接受。关闭它们" applies to the +3 EV live
presentation only. It is not a separate manually authored T105 attestation and
does not accept macOS SDR Candidates or the earlier zero-EV HDR request.

All four subsequent hidden-background native runs passed 1,000 lifecycle
cycles after 20 warmup cycles at the frozen `1f46352` software revision.
Their probe/report pairs and summary are in
`Validation/029/HDR/Endurance/1f46352-ev3-background-20260904-02/`: nine JSON
files, 18,019 bytes, no image artifacts. Each reached same-frame token 2001,
completed command/readback/presentation, and left zero terminal owners.
These are non-visual endurance results, not automated HDR acceptance.

## Hosted CI Portability Repair — 2026-09-04

[Hosted run 33847099909](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/33847099909)
tested `efbf3611bf6904ca66e097d92937217e0319e7f0`, not the physical evidence's
`1f46352`. Windows strict Debug/Release, Windows Python, and machine
producer/consumer passed. Linux strict/sanitizer/Lavapipe and macOS strict/native
jobs failed during compilation; the aggregate was skipped. This is not a
passing T112 run. No hosted Feature 029 run at the exact `1f46352` SHA was found
when inspected on 2026-09-04.

Two platform-only helpers were compiled without their callers:

- Linux: `HostArchitecture` in `Tests/MetalShaderDerivationTests.cpp` was
  unused outside macOS, failing `-Werror=unused-function`.
- macOS without GLFW: `ApplyMetalPresentationLayerPolicy` in
  `FMetalPresentationContext.mm` was unused, failing the same strict gate.

The repair guards each helper with its caller's platform/availability condition.
It does not suppress warnings or alter color equations, shader bytes, expected
vectors, or tolerances. The native Metal job also now installs and checks GLFW,
exports its detected prefix before building, and retains mandatory native
presentation checks. A new workflow regression test first failed without that
provisioning and passes with it. The no-GLFW compilation error was reproduced
locally before the fix; strict syntax checks now pass with GLFW both disabled
and enabled.

Local working-tree verification passed full strict Release, all 72 focused
Python tests, the seven output contract suites, Metal presentation policy and
shader derivation/offline finalization, hidden native Metal output presentation,
and Metal/Vulkan GPU conformance (288 samples x 20 repeats each). Frozen vectors,
shader assets, output architecture, bounded endurance reports, roadmap numbering,
dependencies, anchors, task references, stale-phase references, and whitespace
checks passed. These checks do not replace a new hosted Linux/sanitizer run.

The repair changes software inputs and is not evidence-only. It must be committed
and frozen before a new formal validation round; old physical captures and human
feedback retain their original SHA. They cannot be relabeled or automatically
carried forward. `portability-repair-20260904.json` records this working-tree
diagnostic scope. T112 and final same-revision closeout remain open.

## Preliminary macOS HDR Preflight History

The physical M4 completed non-visual preflights for all four required modes:

| Profile | Ignored probe SHA-256 | Machine result |
| --- | --- | --- |
| PQ 1000 nit | `17bdd0ae3205fbe208a8562e4cae3d0597b0ce8215c55f9ede602a22031b3356` | passed |
| PQ 2000 nit | `73ec3186b7e74cca9d6aba25a8f96985a00e1c39782653651e63e4716b37dfa2` | passed |
| EDR 1000 nit | `4e08e09faabd837d90e2d1259222dbefeed3ebd8cbea285afbf67eb5985654ab` | passed |
| EDR 2000 nit | `b65039408c6bb62a654ddbe02b45621fe97ca9424717f6a14b3facc2cd35bcc0` | passed |

Metal PQ resolves to `BGR10A2Unorm`, ITU-R 2100 PQ,
`wantsExtendedDynamicRangeContent=true`, Core Animation color management, and
`EDRMetadata=nil`. Metal EDR uses extended-linear RGBA16Float with
`EDRMetadata=nil`. Neither path enables `CAEDRMetadata` system tone mapping.

These checks prove only machine preparation and native state. The formal
`hdr-live-review-request.json` must be reproduced from the exact committed
implementation revision before review. HDR appearance cannot be scored,
inferred, or accepted by automation; the maintainer must personally view every
settled mode and manually author the linked immutable decisions.

## Remaining Closeout Gates

- T103: Windows Lantern/Sponza accepted; macOS SDR review remains pending.
- T105: separate maintainer-authored HDR attestation; +3 EV conversation
  acceptance is preserved but is not an automatically authored substitute.
- T106: accepted-SDR and current four-pass HDR authority aggregation.
- T112: freeze the portability repair, then obtain passing exact-revision
  Windows/macOS/Linux strict Release and Linux sanitizer run IDs and digests.
- T117: roadmap/AGENTS completion status, only after every gate passes.
- T118: final same-revision producer/consumer closeout aggregation.

Until those gates are complete, Feature 029 remains active and no Windows HDR
validation, automated HDR visual decision, or Feature 028 v2 reinterpretation
is claimed.

## Evidence-Gate Repair Before Push

The aggregate previously allowed empty machine inputs and a self-consistent HDR
request/attestation pair for a different target revision. It now requires four
physical SDR bundles and four Metal HDR reports, verifies Candidate/PNG/native
readback/calibration linkage, target SHA, ordered HDR probe/report digests, and
all terminal results. The generic verifier closeout flag delegates to the same
gate. Formal native producers launch fresh captures between clean-SHA checks;
old probe relabeling is no longer a supported operation. JSON duplicate keys,
non-finite numbers, oversized data, and duplicate artifacts fail closed.

The eight focused Python test files pass 66 tests, including synthetic
positive/negative authority fixtures stored only in temporary test directories.
They do not create real maintainer acceptance or HDR attestations. T101 was
reopened to reflect the enforced exact-revision requirement accurately. At that
pre-push point, 109/118 tasks were complete, with nine real external/same-revision
gates remaining; the later Windows T102 result is recorded separately above.

The full staged-file whitespace scan also removed trailing whitespace from the
new GLSL/CPU ACES implementation and specification prose. Repository source
pins and the dependent manifest fingerprint were updated explicitly; equations,
constants, expected vectors, tolerances, and SPIR-V remain unchanged. Earlier
working-tree reports retain their historical pre-format source/manifest/binary
digests and are not represented as captures of the final committed bytes.

The post-format strict Release rebuild, all 66 Python tests, focused C++ suites,
offline Metal derivation/finalization, and a fresh 20-clean-cook Feature 028
producer/consumer/native Metal regression passed. The two environment-dependent
suites were executed through the producer with a fresh publication, not skipped.
`prepush-repair.json` records current hashes and explicitly remains pre-commit
diagnostic evidence. The Vulkan 1.3 shader recompile is byte-identical to the
checked-in SPIR-V; the final staged whitespace and consistency scans pass.
