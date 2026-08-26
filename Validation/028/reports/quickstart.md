# Feature 028 Quickstart Reproduction

## Status

Candidate `ad98e2b17f7b24cb5faabdcf1d51826f364cc966` passed the build,
corpus, focused-test, regular, medium, and independent-consumer entries below in
an independent detached worktree on 2026-08-26. The subsequent dependency-first
throughput correction changes strict runtime scheduling, so T115 remains open
until those commands and the physical hardware entry point tracked by
T114/T126/T129 are repeated on the eventual final revision. The results below
remain the clean predecessor evidence rather than final closeout authority.

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
| Metal regular profile | PASS in 87.791 seconds at `ad98e2b`; 37/37 warm reuse, 20 cycles with cycle 2 as the RSS origin, 2,506,752-byte RSS growth, 40 captures, seven readbacks, zero terminal owners, stale-handle rejection |
| Regular artifact consumer verification | PASS, 3,523 artifacts; manifest SHA-256 `5c88539532cb0cad54ca1a4c89102536746d71c98c16f7482452f6ce2dc512ea`; summary SHA-256 `1b5328928f2106a7c171bb2460e07681d057c4c1b724efa66472efaa85ba7a28` |
| Metal medium profile | PASS in 1,024.822 seconds at `ad98e2b`; Lantern and Sponza v2 each completed 1,000 cycles, 100-percent warm reuse, 2,000 captures, seven readbacks, zero terminal owners, and stale-handle rejection; RSS growth was 0 and 3,178,496 bytes respectively |
| Medium artifact consumer verification | PASS, 926 artifacts; manifest SHA-256 `1efd2dbe2cf8d8da34056b88684ed847bb4c2c5f0a884b11ba584ef7d6e4d86f`; summary SHA-256 `07e9546735f166240618b80db4eb776065d7ae243ee63296be6f4cfff19902b1` |
| Clean-predecessor Metal visible hardware profile | PASS at `426d8617` in 1,679.546 seconds; both workloads completed 1,000 cycles, accepted-image probes/FLIP, zero terminal owners, stale-handle rejection, and independent 4,369-artifact verification |
| Final-revision visible image gates | PASS on Metal and Vulkan after native priming; each completed 20 frames with 40 captures, 7 readbacks, 20 semantic probes, FLIP 0, zero terminal owners, and stale-handle rejection |

The detached worktree retained no tracked changes after all commands. The
initial archive-only experiment was rejected by the architecture verifier
because an archive has no git history and therefore cannot resolve the declared
Feature 028 base revision. It is not counted as a clean-checkout result; the
detached worktree replaced it.

## Earlier Scale Evidence

The detached predecessor `f05a793780920b01dc7d711282f362cecb0ba803`
passed an earlier local M4 Pro Metal medium profile in 1,453.049 seconds. It is
retained as history; the `ad98e2b` clean-checkout result above supersedes it for
medium acceptance. Exact historical metrics and digests remain recorded in
`Validation/028/CI/README.md`.

The final manual Linux medium workflow and required Windows Vulkan plus physical
macOS Vulkan/Metal hardware workflow remain separate phase-closeout evidence on
the same code authority.
