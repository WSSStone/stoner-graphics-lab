# Feature 028 Quickstart Reproduction

## Status

Final implementation revision
`c82e51790643fc6583a80240239f6f4123cc0df1` has passed local strict builds,
focused suites, runner contracts, and Metal/Vulkan visible image gates. Its
clean-checkout, hosted, physical, and manual medium runs remain pending, so T115
is not yet marked complete. The complete clean-checkout/hardware rows below are
predecessor evidence and are not promoted as final-revision authority.

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
| Metal regular profile | PASS in 89.749 seconds on the clean predecessor revision whose final-only delta is bounded presentation recovery; 37/37 warm reuse, 20 cycles, 5,685,248-byte RSS growth, zero terminal owners, stale-handle rejection |
| Regular artifact consumer verification | PASS, 3,523 artifacts; manifest SHA-256 `b8c0ee96df3805f1b492a2caa2229900124247cb36611672d80e6c29e454cc2e` |
| Clean-predecessor Metal visible hardware profile | PASS at `426d8617` in 1,679.546 seconds; both workloads completed 1,000 cycles, accepted-image probes/FLIP, zero terminal owners, stale-handle rejection, and independent 4,369-artifact verification |
| Final-revision visible image gates | PASS on Metal and Vulkan after native priming; each completed 20 frames with 40 captures, 7 readbacks, 20 semantic probes, FLIP 0, zero terminal owners, and stale-handle rejection |

The initial archive-only experiment was rejected by the architecture verifier
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
macOS Vulkan/Metal hardware workflow will complete the remaining quickstart CI
entry points on the final implementation revision.
