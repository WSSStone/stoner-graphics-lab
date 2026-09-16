# Capability-v2 validation handoff

Frozen software: `74e1c435ecd79056ef3d72c563ab4cd865a6ca19`.
Source branch: `codex/030-sdr-capability-validation`.
This exact commit is pushed to origin with maintainer approval. Hosted run [35078558137](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/35078558137) is in progress at this exact SHA. The maintainer will execute the M4 lane and return evidence. Do not fetch a different SHA or treat the old `9154e421` evidence as current.

## M4 operator

Create a clean detached checkout at the exact SHA above. Build strict Release with the existing M4 environment, acquire the pinned Sponza corpus, cook both recipes with `Production/Mac-Metal-Arm64.json`, and run `validate --strict-files` on both new publications. Follow [hardware-validation.md](hardware-validation.md) using Coverage-v2 and new output paths. Keep the Metal profile actions strict (`profile`), with the existing 20-cycle sequence; only Windows uses `profileByCapability`.

| Required case | Presented-frame budget |
| --- | ---: |
| `macos-metal-sponza-sdr-endurance` | 1120 |
| `macos-metal-lantern-pq2000-endurance` | 1120 |
| `macos-metal-lantern-edr2000-endurance` | 1120 |
| `macos-metal-lantern-sdr-smoke` | 120 |
| `macos-metal-lantern-pq1000-smoke` | 120 |
| `macos-metal-lantern-edr1000-smoke` | 120 |
| `macos-metal-sponza-pq1000-smoke` | 120 |
| `macos-metal-sponza-pq2000-smoke` | 120 |
| `macos-metal-sponza-edr1000-smoke` | 120 |
| `macos-metal-sponza-edr2000-smoke` | 120 |
| `macos-metal-sponza-sdr-lifecycle-stress` | 1120 |
| `macos-metal-lantern-mode-transition-stress` | 1120 |
| `macos-metal-lantern-lifecycle-integration` | 120 |
| `macos-metal-sponza-lifecycle-integration` | 120 |

Run every command through `interactive_lab_validation.py run --git-revision 74e1c435ecd79056ef3d72c563ab4cd865a6ca19`, followed by `verify` in a fresh process. Return original bounded native/wrapper/script/request JSON bytes and their SHA-256 identities; keep cooked packages, raw logs and absolute commands local. Preserve existing warmup accounting and metadata rules. Intel macOS remains explicitly skipped.

## Remaining order

1. Collect all 13 new-SHA hosted results and all five Windows plus fourteen M4 machine cases.
2. Only then close T118 and run inherited exact-dimension Lantern/Sponza formal SDR capture/calibration/probe/comparison on both physical lanes. No interactive/preset/UI-smoke arguments; no tolerance or Accepted changes.
3. Generate current human requests from successful new-SHA machine runs. Maintainer review covers WASD/QE/Shift/RMB navigation, Escape cursor release, typing/drag/scroll isolation, readability, exposure/output controls, preset restore after aspect change, UI toggle, real focus/minimize recovery and exit. M4 also requires all four HDR profiles for both scenes. Record the maintainer's own decision; automation supplies no acceptance.

If the new software or policy must change, freeze again and repeat affected validation. PassThrough capability is not a monitor Gamma2.2 calibration claim. Windows AcquireHistory retains its IdleAssumed limitation.
