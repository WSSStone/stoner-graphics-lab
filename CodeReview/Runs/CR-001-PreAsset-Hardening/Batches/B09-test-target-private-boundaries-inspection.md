# B09-S01 Inspection: Test Target Architecture And Private Boundaries

## Scope

Inspected the cross-cutting test executable build structure, test entry point, and public/private include boundary behavior.

Files inspected:

- `Tests/SConscript`
- `Tests/Main.cpp`
- `site_scons/LayerBuilder.py`
- `Demo/StonerDemo/SConscript`
- `Tests/TriangleDemoIntegrationTests.cpp`
- `Tests/ApplicationWindowInputTests.cpp`
- `Tests/CorePlatformTests.cpp`
- test file size/selector evidence across `Tests/*.cpp`

## Boundary Evidence

Production layers use `BuildLayer()` in `site_scons/LayerBuilder.py`, which scopes each layer compile environment to its own Public directory plus permitted dependency Public directories. That is the intended compile-time boundary enforcement model.

`Tests/SConscript` builds every `Tests/*.cpp` source into one `StonerTest` executable with one shared `test_env`. The shared include path first adds all public layer include directories, then globally adds:

- `Demo/StonerDemo/Private`
- `Source/Application/Private`
- `Source/Core/Private`

Actual private-header users are limited:

- `Tests/TriangleDemoIntegrationTests.cpp` includes `FDemoConfiguration.h`, `FStonerDemoApplication.h`, and `FDemoValidationMonitor.h`.
- `Tests/ApplicationWindowInputTests.cpp` includes `FWindowDriver.h`.
- `Tests/CorePlatformTests.cpp` includes `FPlatformFileSystemInternal.h` and `FPlatformProcessInternal.h`.

All other test translation units inherit these Private include paths even though they only need public headers.

## Test Target Evidence

`Tests/SConscript` auto-discovers all `Tests/*.cpp` files into one executable. `Tests/Main.cpp` always runs every suite, except for the special child-process logging modes. There is no first-class suite selector.

Large suites currently include:

- `Tests/RHICoreTests.cpp`: 2670 lines
- `Tests/VulkanBackendTests.cpp`: 1755 lines
- `Tests/LoggingAssertionTests.cpp`: 1180 lines

This makes focused local gates less explicit than the CR process wants, although it is a maintainability issue rather than an immediate correctness failure.

## Findings

- Existing `CR001-B09-F001` remains the primary S2 boundary finding for global private include paths.
- Added `CR001-B09-F002` as a detailed S2 refinement that includes Core Private exposure and identifies the exact limited private-header users. B09-S02 can fix F001 and F002 together.
- Added `CR001-B09-F003` as S3 test architecture debt for the single all-suite test entry point lacking focused suite selection.

## Recommended Fix Direction

B09-S02 should stop exposing Private include paths to every test source. A low-risk approach is to compile test sources into objects with a public-only environment by default, and use narrow cloned environments only for the three internal-test sources that currently need Private headers. This keeps one `StonerTest` binary while restoring boundary proof for ordinary tests.

`CR001-B09-F003` can be deferred if adding a suite selector would broaden B09-S02 beyond the private-boundary fix.
