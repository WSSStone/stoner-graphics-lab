# B01-S01: Build, CI, and Architecture Inspection

## Inspection Budget

The inspection covered one responsibility domain and exactly eight build/CI
files, totaling 786 lines:

1. `SConstruct`
2. `site_scons/BuildConfig.py`
3. `site_scons/PlatformDetect.py`
4. `site_scons/LayerBuilder.py`
5. `site_scons/GraphicsDependencyDetect.py`
6. `Tests/SConscript`
7. `Source/Backend/SConscript`
8. `.github/workflows/ci.yml`

Supporting `rg`, Git-history, constitution, and existing baseline evidence were
used to validate references without expanding the production-file inspection.

## Confirmed Strengths

- Platform detection explicitly rejects unsupported hosts and requires an
  available compiler.
- Build variants use separate platform/configuration output trees.
- Layer libraries receive only their own public include path plus declared
  dependency public paths.
- Backend discovery ignores placeholder directories without an `SConscript`;
  Vulkan is currently the only compiled backend.
- CI pins SCons 4.10.1 and executes deterministic validation on all three
  platforms plus real Lavapipe native validation on Linux.
- The bootstrap branch passed all current remote jobs.

## Findings

### CR001-B01-F001 — Accepted S2

Clean Debug and Release builds both emit 33
`-Wmissing-field-initializers` warnings from Vulkan native offscreen setup.
Warning noise prevents safe warnings-as-errors enforcement.

### CR001-B01-F002 — Accepted S2

`BuildConfig.py` offers only Debug/Release optimization choices. There is no
opt-in ASan/UBSan profile and no strict-warning switch. CI builds only the
default Debug configuration, so Release-only and sanitizer-detectable failures
are not hosted gates.

### CR001-B01-F003 — Accepted S3

The main workflow subscribes unconditionally to both `push` and
`pull_request`. A push to the branch of Draft PR #4 produced two complete
Windows/macOS/Linux matrices. The tool-only workflow was already narrowed and
now runs once; the project workflow still duplicates.

### CR001-B09-F001 — Accepted S2

The monolithic test target globally adds `Demo/StonerDemo/Private` and
`Source/Application/Private` to its include path. Active tests include
`FWindowDriver.h`, `FDemoConfiguration.h`, `FStonerDemoApplication.h`, and
`FDemoValidationMonitor.h`. This does not create a runtime layer dependency,
but it defeats Public/Private compile-time discipline for every test source.
Designing a proper internal TestSupport boundary belongs to B09.

## Non-Findings And Limits

- The future Asset layer is intentionally absent from SCons before Feature 020;
  this is not roadmap drift.
- Tests and Demo currently link successfully on all three platforms. Static
  library ordering was not classified as defective without a failing symbol
  dependency or reproducible linker evidence.
- The architecture scanner's zero-violation result covers layer-direction
  includes only. It is not evidence that test/private-header boundaries are
  clean.
- Optional analyzer availability remains unchanged: clang-tidy, cppcheck, and
  IWYU are not installed and are not mandatory gates yet.

## B01-S02 Fix Packet

The next fix step may handle at most these three related B01 findings:

1. Eliminate project-owned Vulkan initialization warnings without suppressing
   diagnostics globally.
2. Add opt-in strict-warning and ASan/UBSan build controls, with platform-valid
   behavior and Linux sanitizer CI.
3. Remove duplicate project CI execution while retaining PR validation and
   deliberate default-branch push validation.

The private-header test boundary remains assigned to B09.
