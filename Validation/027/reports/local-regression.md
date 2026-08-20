# Feature 027 Local Regression

Date: 2026-08-20
Host: macOS arm64, Apple M4 Pro development machine
Environment: Conda `godot`, SCons 4.10.1, C++20, macOS deployment target 12.0

## Results

| Gate | Result | Notes |
|---|---|---|
| Strict Debug build | Pass | `scons platform=macos arch=arm64 config=debug strict=true -j8` |
| Strict Release build | Pass | `scons platform=macos arch=arm64 config=release strict=true -j8` |
| Release Feature 027 suites | Pass | Full `--suite metal`, including 10,000 lifecycle iterations and 90 RSS samples |
| Debug full regression | Pass | All registered suites; optional native driver execution explicitly skipped where unavailable |
| Feature 027 verifier | Pass | Contracts, frozen RHI overload inventory, architecture, vendor provenance, and build isolation |
| Generic architecture verifier | Pass | Metal/Objective-C++ ownership and Tools-only SPIRV-Cross boundaries |
| Validation runner tests | Pass | Nine standard-library unit tests |
| Hosted-like native probe | Unavailable | The sandbox exposes no Metal device; the report remained unavailable and contained no false native identity |

The Release lifecycle run sampled resident memory every 100 iterations from
1,100 through 10,000. It retained all 90 samples and reported zero median
growth against the `max(16 MiB, 5%)` allowance. Native Metal, visible
presentation, native metallib finalization, and Metal/Vulkan comparison remain
separate hardware/toolchain gates and are not claimed by this checkpoint.
