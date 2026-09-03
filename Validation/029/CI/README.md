# Feature 029 CI and Authority Evidence

This index records bounded evidence captured from the implementation working
tree before its first commit. It is not a completion record or an exact-commit
execution claim, even after this index is committed. It does not supply hosted
run IDs, same-revision M4/Windows SDR v3
authority, maintainer SDR acceptance, or maintainer-authored HDR visual
attestations. Raw logs, readbacks, cooked generations, and machine probes remain
under ignored `Build/Validation/029/`.

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

## SDR v3 Candidate State

The physical M4 produced preliminary exact 512-by-512, `sampleCount=1`,
lossless v3 Candidates. Neither Candidate is Accepted or valid same-revision
closeout authority. Preserve these files; generate new frozen-revision bundles
under a fresh subdirectory, including calibration and native linkage.

| Workload | Candidate JSON SHA-256 | PNG SHA-256 | State |
| --- | --- | --- | --- |
| Lantern v3 | `da2c0971143930a22dff75a2d13224afce66743550aed3e0543fa33ba4c55211` | `7a3c6aaac225471a408a27b1457df0848818ec03b587091e19279e8a25afb49b` | `candidate` |
| Sponza v3 | `7aee802ac71c934417a720037755ee045a2aa4449a7881ca4585f0e128bf3615` | `4f72c4871fe51280f7db47f026059ca31150b2130d9f53f0d65eb9281eb249e7` | `candidate` |

The independently synchronized physical Windows Vulkan authority must generate
its own Lantern and Sponza v3 Candidates. Feature 028 carry-forward is forbidden.
After both platforms have fresh Candidates, only an explicit maintainer edit to
`Config/Validation/OutputTransform/SDR/Baselines-v3.json` may admit or reject
each exact record. No alignment, crop, scale, warp, resize, or resampling is
permitted.

## macOS HDR Preflight and Human Authority

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

- T101: rerun M4 Metal SDR Candidates/calibration/native capture at the frozen
  software SHA; the preliminary working-tree files cannot be relabeled.
- T102: physical Windows Vulkan Lantern/Sponza SDR v3 Candidates.
- T103: explicit maintainer SDR v3 acceptance or rejection.
- T104: exact-commit four-profile HDR preflight and machine-authored request.
- T105: maintainer live PQ/EDR visual observations.
- T106: accepted-SDR and current four-pass HDR authority aggregation.
- T112: exact-revision hosted Windows/macOS/Linux strict Release and Linux
  sanitizer run IDs and artifact digests.
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
reopened to reflect the enforced exact-revision requirement accurately: 109/118
tasks are complete, with nine real external/same-revision gates remaining.

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
