# Feature 028 Quickstart Reproduction

## Status

Final local candidate `7f0dab41be21722827987d77cc8e22ff0c651697`
passed every directly executable quickstart entry from a clean tracked workspace
on 2026-08-28. The accepted camera-preview example remains a calibration-only
template with generation/lease placeholders and was not replayed because this
revision does not change the frozen camera. Hosted and controlled-physical
closeout remain separate T112-T114 authority and cannot be replaced by these
local results.

## Final-Candidate Clean Workspace

| Quickstart entry | Result |
|---|---|
| `conda run -n godot scons config=debug strict=1 -j8` | PASS after rebuilding one Debug archive corrupted by an invalid concurrent Debug/Release invocation; the counted quickstart run was serialized |
| `conda run -n godot scons config=release strict=1 -j8` | PASS |
| `Build/Mac/Debug/Tests/StonerTest --suite production-content` | PASS, 4 checks |
| Regular corpus verification | PASS without network access |
| Pinned medium acquisition | PASS by verified reuse, 71 files and 52,686,624 bytes |
| Medium corpus verification | PASS |
| Metal regular profile | PASS in 71.961 seconds; 20 clean runs, 37/37 warm reuse, 20/2 lifecycle cycles, 40 captures, seven readbacks, zero terminal owners, and stale-handle rejection; 5,980,160-byte RSS growth remained `observed` |
| Regular artifact consumer verification | PASS, 3,523 artifacts; manifest SHA-256 `42189484555090aaa134581d01b914d98e3c0c9bf611f08b2ff80f6b46d201d1`; summary SHA-256 `60b22e1af27056dbb5bad1cb89a62fd6849b693f5d0e710acfc8e7485f5638f6` |
| Metal medium profile | PASS in 610.498 seconds; Lantern and Sponza v2 each completed 1,000/20 cycles, 100-percent warm reuse, 2,000 captures, seven readbacks, zero terminal owners, and stale-handle rejection; RSS growth observations were 0 and 622,592 bytes |
| Medium artifact consumer verification | PASS, 926 artifacts; manifest SHA-256 `5b22cde52a93af2813778453d40faef0ca3ccb8bfda7ca9f6c67e2a537b133ea`; summary SHA-256 `e12dbfbb92cbcf585615c3db7a2a7d807f8d16a9a16659e9a78b0d74fa3f0f68` |
| Visible Metal hardware local diagnostic | PASS in 633.338 seconds; both workloads completed 1,000/20 cycles, 2,000 captures, seven readbacks, zero terminal owners, and stale-handle rejection; the report correctly retained `local-diagnostic`, image `not-required`, and RSS `observed` rather than claiming controlled-physical authority |
| Hardware local artifact consumer verification | PASS, 1,006 artifacts; manifest SHA-256 `9489f7bdac6e83cab0ba90f53d1aace78a9c1919d82a46d284af3b0078ec4a4c`; summary SHA-256 `5aa1798b420d0ae8ae89ebdb7fdca9f0f9d1f1461d9acff2d80b108dddb915de` |

The original medium example selected the hosted Intel x86_64 target and was
therefore correctly `Unsupported` on this M4 arm64 host before any native work.
The quickstart now uses `Mac-Metal-Arm64.json` for Apple Silicon and explicitly
reserves `Mac-Metal-X86_64.json` for the hosted Intel lane. An initial visible
local diagnostic also exposed that presentation and formal image authority were
coupled; `7f0dab4` separates them, and the final clean replay proves that local
window/native validation does not enable the controlled-physical image gate.

## Final-Candidate Clean Clone

| Quickstart entry | Result |
|---|---|
| `conda run -n godot scons config=debug strict=1 -j8` | PASS from an empty `Build/` tree |
| `conda run -n godot scons config=release strict=1 -j8` | PASS from an empty Release tree |
| `Build/Mac/Debug/Tests/StonerTest --suite production-content` | PASS, 4 checks |
| Regular corpus verification | PASS without network access |
| Pinned medium acquisition | PASS, 71 files and 52,686,624 bytes |
| Medium corpus verification | PASS |
| Metal regular profile | PASS in 75.026 seconds; 20 clean runs, 37/37 warm reuse, 20 cycles with cycle 2 as the RSS origin, 6,438,912-byte RSS growth, 40 captures, seven readbacks, zero terminal owners, stale-handle rejection |
| Regular artifact consumer verification | PASS, 3,523 artifacts; manifest SHA-256 `328d93964fd3998c53f5f0c4ea4f611c3ca3125e79a46fc80ca9b2b84f5a1827`; summary SHA-256 `59b23b030bb0bde4d1ac0ecf17e2dd37b0bff6ec4ece0d1bbda61305842e9b59` |
| Metal medium profile | PASS in 702.581 seconds; Lantern and Sponza v2 each completed 1,000 cycles, 100-percent warm reuse, 2,000 captures, seven readbacks, zero terminal owners, and stale-handle rejection; RSS growth was 671,744 and 442,368 bytes respectively |
| Medium artifact consumer verification | PASS, 926 artifacts; manifest SHA-256 `069c064bd47338ec933a77d9d3723d5ed3bb1bfd8cfe36437ab04eaa3769b2bd`; summary SHA-256 `162841fa3a351d5d2e4ac4dfea750d4a0823173ccc6731186585b6817debd5cf` |
| Allocator-boundary Lantern medium replay | PASS in 143.225 seconds; 1,000/20 cycles, 2,000 captures, seven readbacks, zero terminal owners, stale-handle rejection, and 475,136-byte RSS growth |
| Bounded-readback Lantern medium replay | PASS in 161.890 seconds; 1,000/20 cycles, 2,000 captures, seven readbacks, zero terminal owners, stale-handle rejection, and 524,288-byte RSS growth |

The native runners were executed with normal host Metal access. A preliminary
restricted-process attempt intentionally remained fail-closed at native device
initialization with zero cycles; it was not counted as acceptance and was not
used as the output root for the passing rerun.

## Earlier Clean Checkout

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
