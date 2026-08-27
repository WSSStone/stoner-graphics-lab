# Feature 028 Local macOS Builds

## Host And Revision

- Host: Apple M4 Pro, macOS arm64
- Evidence head: `b7c89d6a5bbf92775db3b9f05af4d57e9bd5dc34`
- Direct code authority: `ffdc1a73994c8fb47971d8033628aba831af669d`
- Scope: final v2 cameras/baselines, winding and lighting correction, bounded
  long-lifecycle bookkeeping, Vulkan submission retirement, and transient
  native-presentation recovery, pre-measurement native lifecycle priming, and
  Linux/glibc trimming at only the authoritative RSS comparison cycles
- Environment: repository bundled Python/SCons 4.10.1 toolchain

## Results

| Gate | Command | Result |
|---|---|---|
| Strict Debug | `conda run -n godot scons config=debug strict=1 -j8` | PASS |
| Strict Release | `conda run -n godot scons config=release strict=1 -j8` | PASS |
| Production demo regression | `Build/Mac/Debug/Tests/StonerTest --suite production-content-demo` | PASS |
| Production image acceptance | `Build/Mac/Release/Tests/StonerTest --suite production-image-acceptance` | PASS, 27 checks |
| Camera preview regression | `Build/Mac/Debug/Tests/StonerTest --suite production-camera-preview` | PASS, 15 checks |
| Metal 20-frame visible image gate | `Build/Mac/Release/Tests/StonerTest --suite production-content-metal-native` with strict publication/image environment | PASS for Lantern v2; 40 captures, 7 readbacks, 20 semantic probes, FLIP 0, terminal owners 0 |
| Vulkan 20-frame visible image gate | `Build/Mac/Release/Tests/StonerTest --suite production-content-vulkan-native` with strict publication/image environment | PASS for Lantern v2 and Sponza v2; each produced 40 captures, 7 readbacks, 20 semantic probes, FLIP 0, terminal owners 0 |
| Architecture unit tests | `conda run -n godot python -m unittest Tests/test_verify_architecture.py` | PASS, 14 tests |
| Repository architecture scan | `conda run -n godot python Tests/verify_architecture.py` | PASS |

Both compiler configurations used the repository strict mode, which promotes
project warnings to errors. The linker emitted only the inherited macOS
deployment-target warning associated with the configured GLFW binary; no new
Feature 028 compiler warning or error was present.

The detached clean predecessor `426d8617fe8558114110b09a60260de3895da82f`
passed the complete M4 Pro Metal hardware
profile for Lantern v2 and Sponza v2: 1,000 lifecycle cycles per workload,
accepted-image selection, 20 semantic probes, zero FLIP error, zero terminal
owners, stale-handle rejection, and bounded post-warm-up RSS. Exact metrics and
consumer digests are recorded in `Validation/028/CI/README.md`. The final
revision then passed both strict builds, full backend regressions, runner tests,
and 20-frame Metal/Vulkan visible gates after adding one unmeasured,
fully-released native prime. Final CI owns its 1,000-cycle reconfirmation.

The Debug and Release builds completed without leaving an active build process.
This report is local build evidence only; sanitizer and cross-platform closeout
evidence remain owned by their dedicated CI tasks.
