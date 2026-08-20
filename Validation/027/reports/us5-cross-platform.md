# Feature 027 Cross-platform Isolation And Derivation

Status: **PASS** for the hosted matrix at
`de423cf0e28b90410184bb9ab7138987d40231a5`.

GitHub Actions run
[32355696832](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/32355696832)
passed all ten required jobs. Each Debug/strict Release job built with strict
warnings, ran the architecture and Feature 027 verifiers, executed the focused
shared/unsupported-host suites, and completed twenty deterministic shader
derivations. Linux additionally passed ASan/UBSan and TSan.

| Host | Architecture | Build isolation | Derivation | Native classification |
|---|---|---|---|---|
| Windows | x86_64 | no `.mm`, Apple header, or framework use | PASS | unavailable by platform |
| Linux | x86_64 | no `.mm`, Apple header, or framework use | PASS | unavailable by platform |
| macOS | arm64 | private Objective-C++ and Apple frameworks only | PASS | hosted native-offscreen PASS |
| macOS | x86_64 | private Objective-C++ and Apple frameworks only | PASS | hosted native-offscreen PASS |

All ten derivation reports contain the same normalized evidence digest:
`f3e1be112f0aef8143924671c44187168d1ade5566130c4a3868a0c285d5593e`.
Host-specific report bytes differ only because each normalized report records
its host/runner identity and own report digest; this is expected and is not an
MSL derivation mismatch.

The hosted macOS arm64 and x86_64 probes used Apple Paravirtual Metal devices
and produced real GPU readback. They are correctly classified as native
offscreen evidence. The lightweight probes do not by themselves replace T122
or T123: arm64 still requires the physical M4 Pro full workflow, while x86_64
must rerun the full workflow on `macos-26-intel`. Visible presentation remains
the separate physical M4 Pro acceptance in T124.

Runner identities, job conclusions, native capability digests, and artifact
archive digests are retained in `Validation/027/CI/README.md`.
