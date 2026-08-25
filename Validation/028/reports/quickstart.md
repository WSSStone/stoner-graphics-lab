# Feature 028 Quickstart Reproduction

## Status

Final evidence head `b7c89d6a5bbf92775db3b9f05af4d57e9bd5dc34`, with direct code
authority `ffdc1a73994c8fb47971d8033628aba831af669d`, passed the build, corpus,
regular, and independent-consumer entries below in an independent detached
worktree. T115 remains open until final clean-checkout medium and physical
hardware entry points also pass under T114/T126/T129/T134/T135.

## Clean Checkout

An independent detached git worktree at the exact revision was used. The
hash-pinned external Sponza package was staged into its ignored corpus root and
verified by the repository corpus verifier. The worktree had no tracked changes
after all commands completed.

| Quickstart entry | Result |
|---|---|
| `conda run -n godot scons config=debug strict=1 -j8` | PASS from an empty `Build/` tree |
| `conda run -n godot scons config=release strict=1 -j8` | PASS from an empty Release tree |
| `Build/Mac/Debug/Tests/StonerTest --suite production-content` | PASS, 4 checks |
| Regular corpus verification | PASS |
| Medium corpus verification | PASS, staged Sponza matched the pinned manifest |
| Metal regular profile | PASS in 95.564 seconds at the final evidence head; 37/37 warm reuse, 20 cycles with cycle 2 as the RSS origin, 1,409,024-byte RSS growth, zero terminal owners, stale-handle rejection |
| Regular artifact consumer verification | PASS, 3,523 artifacts; manifest SHA-256 `c101063b47ae96a9a75719d2512daf6c7af2d345af08365622bb46509d2e0c35`; summary SHA-256 `76ada3f2cc9866a4b97d06106156f376eeac9b122e8dc9043466222df5192c56` |
| Clean-predecessor Metal visible hardware profile | PASS at `426d8617` in 1,679.546 seconds; both workloads completed 1,000 cycles, accepted-image probes/FLIP, zero terminal owners, stale-handle rejection, and independent 4,369-artifact verification |
| Final-revision visible image gates | PASS on Metal and Vulkan after native priming; each completed 20 frames with 40 captures, 7 readbacks, 20 semantic probes, FLIP 0, zero terminal owners, and stale-handle rejection |

The detached worktree retained no tracked changes after all commands. The
initial archive-only experiment was rejected by the architecture verifier
because an archive has no git history and therefore cannot resolve the declared
Feature 028 base revision. It is not counted as a clean-checkout result; the
detached worktree replaced it.

## Final-Revision Scale Evidence

The detached predecessor `f05a793780920b01dc7d711282f362cecb0ba803`
passed the local M4 Pro Metal medium profile in 1,453.049 seconds. Lantern and
Sponza v2 each completed 1,000 lifecycle cycles with cycle 20 as the RSS origin,
complete strict-no-source loading and semantic equivalence, 100-percent warm
reuse, zero terminal owners, and RSS growth below 16 MiB. The only final code
delta is visible native-presentation recovery, which the final clean hardware
profile exercised. Exact metrics and digests are recorded in
`Validation/028/CI/README.md`.

The final manual Linux medium workflow and required Windows Vulkan plus physical
macOS Vulkan/Metal hardware workflow remain separate phase-closeout evidence on
the same code authority.
