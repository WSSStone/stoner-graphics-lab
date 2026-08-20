# Feature 027 Non-macOS Finalization Gate

Status: **PASS** for Windows and Linux at
`de423cf0e28b90410184bb9ab7138987d40231a5`.

GitHub Actions run
[32355696832](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/32355696832)
passed Debug and strict Release on Windows x86_64 and Linux x86_64. Every job
ran `metal-shader-compiler`, `metal-shader-cooker`,
`metal-shader-publication`, and `metal-shader-runtime`.

The exercised contracts prove that:

- toolchain inspection and native finalization return `HostUnsupported` without
  invoking an executable on non-macOS hosts;
- a final Metal cook returns `TargetUnavailable` with empty artifact bytes;
- source-only SPIR-V cannot form or replace a strict Metal generation;
- failed native finalization emits no publishable generation image; and
- an already published `Current.json` and immutable generation remain unchanged
  after a rejected replacement.

The same jobs still completed twenty deterministic SPIR-V-to-MSL derivations.
Derivation is therefore available on these hosts, while final MetalLibrary
publication remains fail-closed as required.

| Host | Configurations | Derivation | Finalization | Publication |
|---|---|---|---|---|
| Windows x86_64 | Debug, strict Release | PASS | `HostUnsupported` | rejected, no generation |
| Linux x86_64 | Debug, strict Release | PASS | `HostUnsupported` | rejected, no generation |

