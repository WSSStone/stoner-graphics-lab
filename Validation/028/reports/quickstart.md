# Feature 028 Quickstart Reproduction

## Status

Revision `0a5ad11f8d511a9b54da33da086b15cf530ca68a` has passed the
clean-checkout local build, focused suite, corpus, regular profile, and artifact
consumer steps below. The manual medium and physical hardware CI entry points
remain pending, so T115 is not yet marked complete.

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
| Metal regular profile | PASS in 90.188 seconds; 37/37 warm reuse; 20 cycles with cycle 2 as the RSS origin; 2,867,200-byte RSS growth; zero terminal owners; stale-handle rejection |
| Regular artifact consumer verification | PASS, 3,523 artifacts; manifest SHA-256 `e803adcf81a63b596a940916535913a654cbcb57871457c2154ba5fcc603581e` |

The initial archive-only experiment was rejected by the architecture verifier
because an archive has no git history and therefore cannot resolve the declared
Feature 028 base revision. It is not counted as a clean-checkout result; the
detached worktree replaced it.

## Final-Revision Scale Evidence

The same revision passed the local M4 Pro Metal medium profile in 1,483.692
seconds. Lantern and Sponza v2 each completed 1,000 lifecycle cycles with cycle
20 as the RSS origin, complete strict-no-source loading and semantic
equivalence, 100-percent warm reuse, zero terminal owners, and RSS growth below
16 MiB. Exact metrics and digests are recorded in `Validation/028/CI/README.md`.

The final manual Linux medium workflow and required Windows Vulkan plus physical
macOS Vulkan/Metal hardware workflow will complete the remaining quickstart CI
entry points.
