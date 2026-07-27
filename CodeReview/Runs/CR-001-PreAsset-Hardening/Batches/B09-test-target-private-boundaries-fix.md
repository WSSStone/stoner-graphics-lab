# B09-S02 Fix: Test Target Architecture And Private Boundaries

## Findings Addressed

- `CR001-B09-F001`: Tests bypass Public/Private boundaries through global private include paths
- `CR001-B09-F002`: Test target exposes Private include paths to every test source

## Fix Commit

- `cefe65f build(tests): scope private include paths`

## Changes

`Tests/SConscript` no longer appends Private include directories to the shared test compile environment. It now builds explicit object nodes for each test source:

- ordinary tests use the public-only `test_env`;
- `ApplicationWindowInputTests.cpp` receives `Source/Application/Private`;
- `CorePlatformTests.cpp` receives `Source/Core/Private`;
- `TriangleDemoIntegrationTests.cpp` receives `Demo/StonerDemo/Private`.

The final `StonerTest` executable still links the same object set and libraries, keeping runtime behavior and CI invocation stable while restoring boundary proof for ordinary tests.

## Verification

```text
conda run -n stoner-cr scons config=debug --implicit-deps-changed
```

Result: passed; SCons parsed the object-based test target.

```text
conda run -n stoner-cr scons config=debug strict=1 graphics=disabled --implicit-deps-changed
```

Result: passed. The emitted compile commands showed public-only include paths for ordinary tests such as `ApplicationSceneEcsTests.cpp`, `CoreFoundationTests.cpp`, `RHICoreTests.cpp`, and renderer/vulkan public-contract tests. Only the three intended internal-test sources received their scoped Private include directory.

```text
Build/Mac/Debug/Tests/StonerTest
```

Result: exit code 0 under the graphics-disabled strict build output.

```text
rg -n "test_env\.Append\(CPPPATH=\[Dir\('#.*Private|private_include_paths_by_source|ApplicationWindowInputTests|CorePlatformTests|TriangleDemoIntegrationTests|Program\('StonerTest', objects\)" Tests/SConscript
```

Result: no global Private `test_env.Append(CPPPATH=...)` remains; the file contains only the per-source private mapping and links `StonerTest` from explicit objects.

## Remaining Debt

`CR001-B09-F003` remains Accepted S3. Suite selection and larger test-file decomposition are intentionally left out of this S2 boundary fix.
