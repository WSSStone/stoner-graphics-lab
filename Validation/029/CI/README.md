# Feature 029 CI and Authority Evidence

This index preserves the original implementation working-tree evidence and
separately records the frozen-revision Windows T102 and M4 T101/T104 runs below. The original
captures remain preliminary; committing this index does not change their
provenance. Feature 029 is now complete by the explicit maintainer exception
recorded below, not by passing the original strict same-SHA authority gate. Windows raw evidence is archived on the evidence share
under `Build/Validation/029/`; its current cooked packages remain on local NTFS.

## Final closeout — explicit maintainer exception, 2026-09-06

Software `2ee7116ffb382c021ed575aff223c7b760a2ce7d` is closed by the
maintainer's explicit **“agree”** to the fully scoped question in
[closeout.md](../../../specs/029-hdr-output-transform/closeout.md).
There are **118 closed task dispositions: 114 completed as scoped and four
exceptions (T102/T105/T106/T118)**, not 118 original gates passed.

- Current physical M4 Metal Lantern/Sponza SDR Candidates are explicitly
  accepted in the v3 registry, with a separate
  [acceptance record](../SDR/M4-Metal/acceptance-2ee7116-20260906.json).
- Current Windows physical SDR rerun is waived. Previously accepted physical
  `1f46352` Windows bundles retain their exact SHA and session/adapter limits;
  no `2ee7116` Windows hardware execution is claimed.
- Four exact-`2ee7116` +3 EV HDR hidden 1,000/20 runs passed. The maintainer
  expressly retained the `1f46352` +3 EV live acceptance and waived repeat
  viewing plus the separate attestation. No current-SHA live observation or
  synthetic HDR attestation is created; the request remains ready-for-live-review.
- Hosted run 34002580090 passed 14/14 at `2ee7116`, including machine
  producer/consumer and sanitizers. This is not physical/human aggregate authority.
- [closeout-2ee7116-20260906.json](closeout-2ee7116-20260906.json) pins the
  decision and evidence digests. [us5-authority.json](us5-authority.json)
  explicitly records `strictSameRevisionGatePassed=false`.
  Strict validators remain unchanged and are expected to reject this
  mixed-revision/no-attestation set; no general exception flag was added.

The dated capture summaries below remain immutable point-in-time facts,
including their then-pending review/Feature-completion fields. The expanded
Accepted registry is a later explicit human admission, not the registry consumed
by the earlier hosted run. Old failed foreground replays remain failed;
successful background reruns are separately identified. Feature 028 v2 is
unchanged, Windows HDR is not claimed, and this exception is not reusable.
Feature 030 is next; Meshlet 031 keeps 024/025/026/028 dependencies.

[closeout-audit-2ee7116-20260906.json](closeout-audit-2ee7116-20260906.json)
records zero disposition/provenance findings, four SDR bundles verified at
original SHAs, all four current HDR bundles, and unchanged strict rejection.
The five expected strict-gate rejections (two stale Windows reports, two missing
current Windows reports, one missing independent HDR attestation) are preserved
as a negative-test result, not hidden or converted into a strict pass.
Closeout decision SHA-256:
`627199fe3c88428ad5497f5e403a93c221b6294acf923fd73fae9508d59f327a`.
Current M4 SDR admission SHA-256:
`c4c589d70db28f513cb8646a4f4f872d5ab2be32a5b20ffc3ac730d9332eecc4`.
Accepted registry after admission SHA-256:
`9e13dfc7d51658a8940f80a9109e27981d739fe6d5b378125b7fc03ac605d7ab`.

## Current Hosted Gate — 2026-09-06

Storage retention update: the later user-authorized cleanup is recorded in
[cleanup-20260906.json](cleanup-20260906.json). It permanently removed 1,743
untracked files (2,005,580,792 logical bytes): both runs' regenerable DDC, the
older publication copies verified byte-identical to the retained current
publications, 80 raw PPMs, and five Finder caches. All 149 existing formal
evidence files, Accepted references, original commands/logs, and human-review
records were verified unchanged. Historical raw PPM/DDC/publication paths in
earlier receipts describe capture-time locations, not current retention.
Current `m4-formal-2ee7116-20260906-01/{Lantern,Sponza}/publication/` packages
are retained; replay of an older command requires restoring its publication
path from the verified identical current package or recooking at its original
SHA. Deleted PPM bytes have no retained raw copy; formal PNG/JSON remain.

Post-decision verification passed 52 focused Python tests, the unchanged
output-architecture scan, and the roadmap numbering/dependency/anchor/task/stale-
phase scan (zero findings). All 89 checked relative document links/HTML anchors
resolved; `git diff --check` passed. The nine-section delivered HTML was updated
through the mandatory `speckit.docs.implement` completion-document workflow.
The protected `.gitignore`, tutorial workflow, `Tools/Tutorial/`, and
`doc/tutorial/` content digest remained
`b0bb9e806a26359c4d0faf7cdcb0087506a6b34dc4828f29510c68afa0e935d7`.
This closeout changes documentation and explicit SDR admission only; no new
renderer build, Windows recapture, or human HDR review was performed for it.

T112 passed at frozen software revision
`2ee7116ffb382c021ed575aff223c7b760a2ce7d` in
[run 34002580090](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/34002580090),
attempt 1: fourteen jobs passed, zero failed, zero skipped. This includes all
three platforms' strict Debug/Release builds, Linux ASan/UBSan/TSan, Lavapipe,
Metal PQ/EDR non-visual checks, Windows Python, and machine producer/consumer
and aggregate jobs. The workflow completed at 2026-09-06T01:20:21Z.

`hosted-34002580090/summary.json` records each job ID, result, step result, URL,
and SHA-256 of its downloaded log stream. Its SHA-256 is
`2291a04397dccc9c275c44b3a777a7336b2c9eb47c3fe353bd3b3061f77a7f0d`.
The 1,993-byte machine artifact (ID 9979923166) was downloaded and its ZIP
SHA-256 matched GitHub's advertised digest:
`877f5239347fde35ee94f7b46138170d48c4d66a2aefc51880cf702ffdaae603`.
Its two original JSON members are archived byte-for-byte beside the summary
and passed local consumer revalidation. Job logs were streamed through hashing;
no large log copies were retained. The CI artifact's advertised expiry is
2026-12-05T00:56:15Z.

The archived registry is the CI input snapshot, not new physical SDR authority.
Current-SHA physical M4/Windows bundles and explicit SDR/HDR review are still
required. Windows rerun instructions are in
`specs/029-hdr-output-transform/windows-prompt-2ee7116.md`.

## Current M4 T101/T104 — 2026-09-06

Fresh physical M4 captures used exact software revision
`2ee7116ffb382c021ed575aff223c7b760a2ce7d`, with clean software-input checks
before and after every command. Strict Release rebuilt successfully, the
output contracts/Metal native presentation/offline shader finalization passed,
and Metal/Vulkan each passed 288 GPU samples over twenty repeats.

- Lantern: 42 freshly cooked assets and 42/42 warm reuse; Sponza: 194 freshly
  cooked assets and 194/194 warm reuse. Sources were checked against corpus
  digests, and fresh publications/strict native captures used these generations.
- Each SDR workload passed three independent processes of twenty captures,
  zero measured cross-process noise, and all eight mutation checks. Exact
  512x512 Candidate/native/calibration bundles are under
  `Validation/029/SDR/M4-Metal/{Lantern,Sponza}/2ee7116-20260906-01/`.
  Both immutable source Candidates retain `acceptance=null`; the later explicit
  admission is recorded separately in the Accepted registry and closeout above.
- At +3 EV, PQ1000/PQ2000/EDR1000/EDR2000 each passed 1,000 lifecycle cycles
  after twenty warmup cycles, sequentially in hidden background applications.
  Elapsed times were 146.153/157.306/143.044/152.491 seconds. All four completed
  submission, readback, and presentation for token 2001 with zero outstanding
  terminal owners and no native first failure. Applications and the temporary
  sleep-prevention process exited at completion.
- HDR probe/report pairs are in
  `Validation/029/HDR/Endurance/2ee7116-ev3-background-20260906-02/`.
  The matching request is
  `Validation/029/HDR/Requests/2ee7116-ev3-background-20260906-02/hdr-live-review-request.json`,
  SHA-256 `008e9c05e788cd54f4f93f85ca15870488d87f835ce3c14189726cef19dfe6fc`.
  It is `ready-for-live-review`, with no human decision.

`m4-metal-2ee7116-20260906-01/summary.json` indexes 25 bounded PNG/JSON artifacts
totaling 1,208,400 bytes, plus binary, cook, command, and log digests. Its SHA-256
is `03b4b8c9b0938c583aec1f4953e07e8e120fdbf25e63de086284b64ca7ff5f50`.
The evidence verifier passed with zero findings. No HDR image artifacts were
created. Raw PPMs, command receipts, logs, DDC, and publications remain under
ignored `Build/Validation/029/m4-formal-2ee7116-20260906-01/`.

The first local HDR launcher attempt lacked its temporary app's Info.plist and
failed before StonerDemo executed. Its receipt remains a failed setup attempt.
The corrected temporary bundles launched fresh captures into the `-02` directory;
no engine or executable validation inputs changed. No incomplete capture was
promoted. Historical `1f46352` evidence and live feedback retain their identity.

## Historical implementation and repair snapshots

The following sections preserve what was known at each capture/repair date.
Their then-open tasks and pending-review wording are historical, superseded by
the final closeout disposition above; they are not current action requests.

## Source State

The sections following this heading retain implementation history. The current
frozen software revision and current physical results are recorded above
in the explicitly dated current-gate sections.

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

## Vulkan-disabled macOS Build Repair — 2026-09-06

[Hosted run 33853340448](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/33853340448)
tested `ac0cdf455003498421cf0e95a353e1d95433c9a5` and completed with ten passing
jobs, three failed macOS jobs, and a skipped aggregate. Windows and Linux
strict Debug/Release, Linux ASan/UBSan/TSan, Lavapipe, Windows Python, and the
machine producer/consumer passed. All three macOS jobs stopped at the same
`BuildVulkanShader` unused-function error in
`Tests/OutputTransformGPUConformanceTests.cpp`; native HDR execution was not
reached.

The helper now uses the same `STONER_TEST_VULKAN_RUNTIME_AVAILABLE` guard as
its callers. The failure was reproduced locally with Vulkan disabled. After
the repair, strict syntax checks passed all eight Debug/Release x Vulkan
enabled/disabled x GLFW enabled/disabled combinations for that translation
unit. The strict Release test target rebuilt successfully; Metal and Vulkan
each passed 288 GPU conformance samples over twenty repeats. The roadmap
consistency scan reported zero findings and the whitespace check passed.
These are local working-tree checks; the next hosted run must validate the new committed
revision before T112 can close. Existing physical evidence remains attributed
to its original software SHA.

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

## Original Closeout Gates — resolved on 2026-09-06

T103 was completed by current M4 SDR admission, retaining prior Windows
acceptance. T117 updates the roadmap and project memory. T102 and T105 are
explicitly waived as described above; T106 and T118 are resolved by a separate
maintainer exception record, **not successful strict same-revision aggregation**.
There are no remaining Feature 029 obligations under this approved disposition.
The original strict contract still applies to future work.

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
