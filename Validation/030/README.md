# Feature 030 validation preflight

This file records the T001 baseline inventory for the Interactive Rendering Lab.
It was written on 2026-09-06 from the repository working tree at
`c2b2a614961e315e0c750b0838f5746611de6d74`. That initial inventory was a planning/setup record, not an implementation
result. Later local implementation checkpoints and capability observations are
recorded separately below; none claims formal acceptance or human approval.

## Existing regression commands

These are the existing build and regression entry points that remain relevant
while the lab is added. They are command inventory, not a claim that Feature
030 has passed them.

Build the existing configurations with the repository's SCons environment:

```bash
conda run -n godot scons config=debug strict=1
conda run -n godot scons config=release strict=1
```

The Feature 030 quickstart retains this camera, window, output, and
presentation regression group:

```bash
Build/Mac/Debug/Tests/StonerTest \
  --suite application-window \
  --suite production-camera-preview \
  --suite renderer-output-transform-math \
  --suite renderer-output-transform \
  --suite rhi-presentation-output \
  --suite output-presentation-lifecycle
```

The existing Asset JSON/material/model/cooker group is:

```bash
Build/Mac/Debug/Tests/StonerTest \
  --suite asset-material-shader \
  --suite asset-gltf-material \
  --suite asset-gltf-hardening \
  --suite asset-cooker-codec \
  --suite asset-cooker-target-profile \
  --suite asset-cooker-production-texture
```

The production content producer/consumer commands remain the Feature 028
entry points. The target profile, build root, output root, and workload must
be changed together for each lane:

```bash
python3 .github/scripts/run_production_content_validation.py \
  --profile regular \
  --target-profile Config/AssetCooker/Profiles/Production/Mac-Metal-Arm64.json \
  --build-root Build/Mac/Release \
  --output Build/Validation/028/regular-macos-metal \
  --timeout-seconds 600

python3 .github/scripts/run_production_content_validation.py \
  --verify-only Build/Validation/028/regular-macos-metal \
  --target-profile Config/AssetCooker/Profiles/Production/Mac-Metal-Arm64.json
```

The bounded medium and physical entry points are retained for later lane work:

```bash
python3 .github/scripts/run_production_content_validation.py \
  --profile medium \
  --target-profile Config/AssetCooker/Profiles/Production/Mac-Metal-Arm64.json \
  --build-root Build/Mac/Release \
  --output Build/Validation/028/medium-macos-metal \
  --acquire-missing --timeout-seconds 1800

STONER_PRODUCTION_VISIBLE=1 \
python3 .github/scripts/run_production_content_validation.py \
  --profile hardware \
  --local-metal-authority \
  --target-profile Config/AssetCooker/Profiles/Production/Mac-Metal-Arm64.json \
  --build-root Build/Mac/Release \
  --output Build/Validation/028/hardware-macos-metal \
  --acquire-missing --timeout-seconds 3600
```

The Windows physical command is documented in PowerShell form:

```powershell
python .github/scripts/run_production_content_validation.py `
  --profile hardware `
  --local-windows-vulkan-authority `
  --target-profile Config/AssetCooker/Profiles/Production/Windows-Vulkan.json `
  --build-root Build/Win64/Release `
  --output Build/Validation/028/hardware-windows-vulkan `
  --acquire-missing --timeout-seconds 3600
```

The output-transform Python regression names retained from Feature 029 are:

```bash
python3 .github/scripts/test_verify_output_transform_vectors.py
python3 .github/scripts/test_verify_output_transform_evidence.py
python3 .github/scripts/test_run_output_transform_validation.py
python3 .github/scripts/test_aggregate_output_transform_validation.py
python3 .github/scripts/test_output_transform_workflows.py
python3 .github/scripts/verify_output_transform_vectors.py \
  --profiles Config/Validation/OutputTransform/Profiles.json \
  --manifest Tests/Fixtures/OutputTransform/manifest-v1.json \
  --vectors Tests/Fixtures/OutputTransform/vectors-v1.json \
  --output Build/Validation/030/output-transform-vectors.json
python3 .github/scripts/verify_output_transform_architecture.py
```

## Frozen input digests

The following SHA256 values are the current bytes observed during this
preflight. They are a reproducibility inventory for future 030 commands. They
do not freeze the dirty workspace, promote an old Candidate to Accepted, or
create a current-SHA authority record.

| Input | SHA256 |
| --- | --- |
| `Validation/028/Camera/production-content-sponza-v2.json` | `f833e379ee0e50a7a76c83eff12442288490d0f99d3989b01d06e14a3fa1eff6` |
| `Demo/StonerDemo/Private/FProductionCameraPreset.cpp` | `a339917d8724528f21920d1dc031a00548cf292ef6df5aaba845fa573ac0ea0a` |
| `Config/Validation/OutputTransform/Profiles.json` | `6e3373ab31b36aff9e91d1b6b854d75d1171226e57bc3c236f8a1bbce1e1f7d9` |
| `Config/Validation/OutputTransform/Workloads/Lantern-v3.json` | `d6c18185f08cdc1429c05c90c0b2c7ee4e8f53af50bf774c48548427b5715c37` |
| `Config/Validation/OutputTransform/Workloads/Sponza-v3.json` | `6f35158767e6f9beae50735b56db020d1639bd86b75b6c794230323aca6f658a` |
| `Config/Validation/OutputTransform/SDR/Baselines-v3.json` | `9e13dfc7d51658a8940f80a9109e27981d739fe6d5b378125b7fc03ac605d7ab` |
| `Tests/Fixtures/OutputTransform/manifest-v1.json` | `77aabc634d565862079cc6aa55625bccfc934632d7d4068d360654952aaa121d` |
| `Tests/Fixtures/OutputTransform/vectors-v1.json` | `16189846ba601040696fbb1db6120eed36253feffeae02362a691e250c24aa76` |
| `Content/Shaders/PostProcess/Fullscreen.vert` | `aeb7401391e688b8b91972973ef84138020733f7d40146027309be6b71921995` |
| `Content/Shaders/PostProcess/Fullscreen.vert.spv` | `347c12d94126663d233b9baa37d4a1e9a2bbcc015f79d8e587cf3992cfacce36` |
| `Content/Shaders/PostProcess/OutputTransform.frag` | `927dbc992806e7e69528e2df0a6a0a6b8d30809b73bd8face4c9ea8c80e264df` |
| `Content/Shaders/PostProcess/OutputTransform.frag.spv` | `3d4faf6d5ba0fdfb1f92e557ef3e1a107816bf0f2bbd8cea463f2fb2216df2fa` |
| `Content/Shaders/Deferred/Composition.frag` | `3cdd453a7e828148f707c1a9f96f1c1a49286bcfe85b1c7b35baed3ab4a390f6` |
| `Content/Shaders/Deferred/Composition.frag.spv` | `5403e87683733035ad2c5745444c3833f223f9deced4fe67eadc5de7cdf310e8` |
| `Content/ProductionAcceptance/Corpus/corpus-v1.json` | `7c337f69e3614bd31925a992a9067a3e31f34d7065b36f1d6bd796da0623526d` |
| `Content/ProductionAcceptance/Corpus/coverage-v1.json` | `f87bed92e799443283329127e3612c3cf54ae5009d1049ad7e565704597bc749` |
| `Content/ProductionAcceptance/Regular/Lantern/Lantern.glb` | `a79458c4b02d695187a952f23a63b8bf278e7bc3d316a3c2a314f2d6974181f1` |
| `Content/ProductionAcceptance/External/Sponza/Sponza.gltf` | `646c10cbc8fab990ca29f363e90e2d65155f3a3569506852eb1434a9465b9501` |
| `Content/ProductionAcceptance/External/Sponza/Sponza.bin` | `fdbdbfb6a76edeb6626f28a1401bc1536bb1c864131a64e90fbc3df2d2d191bd` |
| `Config/AssetCooker/Profiles/Production/Windows-Vulkan.json` | `c157918a460ea086d8089753ad6f04c46484bed2d653d07193310a2397064058` |
| `Config/AssetCooker/Profiles/Production/Linux-Vulkan.json` | `9b33ab4b64d6badafc484882bc46749ec4555e50bfaa0970b7de0ad424f8a18d` |
| `Config/AssetCooker/Profiles/Production/Mac-Metal-Arm64.json` | `82681655db59befc20366978759ae42ace9ed805e2d4e0adc7de7980854f4c8f` |
| `Config/AssetCooker/Profiles/Production/Mac-Metal-X86_64.json` | `4a6a2bedb1b15ca645c01235d5037d6beccbea526f98cc33ffd4bd1ee5f8322e` |
| `Config/AssetCooker/Profiles/Production/Mac-Vulkan.json` | `d4805fbce24ab907f2d8da45ef9583260c50b33f43ac9cbac4ffc225c0797fee` |

## `VK_EXT_swapchain_maintenance1` preflight

The revised Feature 030 Vulkan lifecycle queries device-level
`VK_EXT_swapchain_maintenance1` and prefers presentation fences when enabled;
otherwise it uses the bounded AcquireHistory fallback. The maintainer
authorized this compatibility policy on 2026-09-06; implementation is still
pending in T015/T021/T030. Existing results below do not test that new path. A source declaration, Vulkan
instance version, or instance extension string is insufficient evidence.
The optional extension inventory for both intended native lanes is currently
**unverified/unavailable**:

| Intended lane | Evidence inspected | Device-level result |
| --- | --- | --- |
| Windows physical discrete Vulkan | Archived `vulkaninfo` log at `Build/Validation/029/windows-formal-cook-import-20260903-YVpO4g/Build/Validation/029/windows-vulkan-0b9418c-20260903-02/vulkan-device-resolved.log` (SHA256 `bc948428604527150e0f5c8539ad6184b048b779ee2a3b6372c5672d52b7551c`) identifies the target NVIDIA GeForce RTX 3080 (`0x10de:0x2216`, driver `581.32.0.0`, console session). The log reports Vulkan instance version 1.4.350 and GPU0 `apiVersion` 1.4.312, and lists only `VK_EXT_surface_maintenance1` in the instance extension list; it has no per-device extension list, extension feature struct, or function-pointer result. | Unverified/unavailable; no 030 support claim. |
| Linux hosted Lavapipe software native Vulkan | Intended target is `Config/AssetCooker/Profiles/Production/Linux-Vulkan.json`; this macOS session has no Linux device or per-device extension/feature report. Hosted Lavapipe/build evidence does not establish the optional device-level maintenance1 capability for the software native lane. This is a required software lane, not physical-device authority. | Unverified/unavailable; no 030 support claim. |

Run the following on the actual target host and preserve the lane, adapter,
driver, session, and software-revision metadata with the output. The two
examples use concrete filenames so the angle-bracket placeholders cannot be
interpreted as shell redirection:

```bash
mkdir -p Build/Validation/030
vulkaninfo --show-all \
  --output Build/Validation/030/windows-vulkan-vulkaninfo.txt
vulkaninfo --json=0 \
  --output Build/Validation/030/windows-vulkan-vulkaninfo-gpu0.json
rg -n "VK_EXT_swapchain_maintenance1|swapchainMaintenance1|VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT" \
  Build/Validation/030/windows-vulkan-vulkaninfo.txt \
  Build/Validation/030/windows-vulkan-vulkaninfo-gpu0.json
```

For the hosted Linux Lavapipe software native lane, repeat with a selected
Lavapipe GPU and these concrete output paths:

```bash
mkdir -p Build/Validation/030
vulkaninfo --show-all \
  --output Build/Validation/030/linux-lavapipe-vulkaninfo.txt
vulkaninfo --json=0 \
  --output Build/Validation/030/linux-lavapipe-vulkaninfo-gpu0.json
rg -n "VK_EXT_swapchain_maintenance1|swapchainMaintenance1|VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT" \
  Build/Validation/030/linux-lavapipe-vulkaninfo.txt \
  Build/Validation/030/linux-lavapipe-vulkaninfo-gpu0.json
```

The target adapter must be selected explicitly when more than one GPU is
present; `--json=0` is only an example for the selected GPU index. For a supported result, the
`--show-all`/JSON output must show `VK_EXT_swapchain_maintenance1` in that
physical device's extension set and the
`VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT::swapchainMaintenance1`
feature as available. This `vulkaninfo` result proves advertised device-level
availability only. It does not prove that the lab device enables the feature
or that runtime entry points resolve. After implementation, startup tests must
query the selected device and verify the auto-selected path: enable the
extension/feature and resolve actually used entry points when available, or
start the AcquireHistory path when absent. Present fences use the present-info
chain, not a separate invented fence entry point; those tests provide
the runtime enabled/entry-point result and are later implementation evidence,
not part of this T001 preflight. If `vulkaninfo` does not expose the feature
struct or device-level details, use the existing native startup/probe path on that same host to call `vkEnumerateDeviceExtensionProperties` and query
`VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT` through
`vkGetPhysicalDeviceFeatures2`. Record availability as supported, unsupported,
or unavailable. Do not convert an instance-only extension string into device
support.

Missing inventory is **not an implementation gate**. Runtime capability
selection belongs to T021; missing extension/feature support selects fallback,
not Unsupported. A real passing fallback run may close its platform gate with
mode/limits/IdleAssumed cleanup recorded as required by
`specs/030-interactive-rendering-lab/contracts/lab-runtime.md`. Missing target
execution remains an evidence gap; it must not be confused with extension
absence or relabeled as a pass. No target report has been obtained here.

## No-claim rules

- No Feature 030 hardware, formal, visual, or human-approval result is claimed
  by this preflight.
- Existing Feature 028/029 test, CI, cooked-content, and accepted-reference
  records are regression context only. Their one-time exceptions and
  carry-forward evidence do not authorize a 030 result or a new current-SHA
  claim.
- The current workspace digest inventory does not establish a frozen software
  revision. Future formal captures must record the exact tested commit and
  preserve frozen camera/output/settings inputs.
- Formal scene captures remain UI-disabled with frozen settings. Interactive
  previews, UI smoke output, and preset exports cannot update Accepted
  baselines.
- Windows evidence that lacks a device-level extension/feature/function probe
  cannot establish maintenance1 support. Linux hosted Lavapipe evidence
  cannot establish the optional device-level maintenance1 capability for the
  software native lane.
- An earlier sandbox-restricted Vulkan enumeration attempt, including the
  macOS MoltenVK session without a usable Metal device in that environment, is
  recorded as unavailable and is not a target-lane failure or pass. Later
  unsandboxed Metal/native diagnostics are recorded separately below.

T001's repository inventory is complete. Unresolved Windows/Linux
device-level records are follow-up inventory for T021 and native validation,
not a blocker for Vulkan implementation. Target runtime tests must report
the actual selected mode and cleanup assurance.

## Implementation checkpoint (19/127)

The current local checkpoint is recorded in
[implementation-checkpoint.json](../../Build/Validation/030/implementation-checkpoint.json)
with status `partial-phase-local-validation-passed`. Both latest strict
Debug and Release builds exited 0; their logs are
[failure-order-build-debug.log](../../Build/Validation/030/failure-order-build-debug.log)
and
[failure-order-build-release.log](../../Build/Validation/030/failure-order-build-release.log).

The broader T023 local round recorded 14 suite invocations and 96 assertions,
plus four two-frame/one-cycle native Metal helper probes; all passed. The
latest callback-order round is recorded separately with six suite invocations,
56 assertions, and four helper probes; all passed. The raw outputs remain
bounded local diagnostics under `Build/Validation/030/`, including
[final-t023-results.json](../../Build/Validation/030/final-t023-results.json)
and
[failure-order-results.json](../../Build/Validation/030/failure-order-results.json).
These results make no current-SHA formal acceptance, human display or
physical-scanout authority, or Accepted-baseline claim.

The intended Windows physical discrete Vulkan and hosted Linux Lavapipe
execution evidence remains pending. The former optional-extension design
blocker is removed; complete runtime detection and the bounded fallback, then
run the actual target lanes. The 19/127 implementation checkpoint is unchanged
by this documentation/policy revision.

## Current Mac optional-capability inventory (partial)

On 2026-09-06, an unsandboxed `vulkaninfo --show-all` inspection of Apple M4
Pro / MoltenVK 1.4.1 listed `VK_EXT_swapchain_maintenance1` revision 1 and
`swapchainMaintenance1=true`. The utility subsequently aborted (exit -6) while
querying unsupported cooperative-matrix properties. This is a partial advertised
capability observation, not enabled-device proof, a successful full probe or a
030 native gate. It says nothing about Windows or hosted Linux support.

The 106,497-byte partial output is retained at
`Build/Validation/030/current-mac-vulkaninfo.txt` (SHA256
`a1f5738e36e836dcfe97a53214b68143095125a6fc58a847acabbee1a268d303`);
`Build/Validation/030/current-mac-vulkaninfo-status.json` preserves the failed
exit and no-claim classification. Actual engine auto/forced-fallback execution
is still pending.

The later project-owned targeted capability helper passed a required native run
on the same Apple M4 Pro using the Vulkan 1.0 + enabled KHR properties2 query
path: extension advertised true, feature advertised true, seven assertions
passed (exit 0). Independent strict compilation and native output are in
`Build/Validation/030/capability-review/compile-native.log` and `native-m4.log`.
The minimal query fixture intentionally creates no logical device or surface;
it proves the selected-device query, not extension enablement, lab startup or
native presentation acceptance. The earlier failed full utility output remains
preserved above.

## Vulkan policy review checkpoint

The frozen private presentation-policy helper and its tests passed independent
`clang++ -std=c++20 -Wall -Wextra -Werror` compilation and all 48 assertions.
Logs are `Build/Validation/030/policy-review/compile-frozen.log` and
`Build/Validation/030/policy-review/tests-frozen.log`. This verifies deterministic
selection/retry, bounded image and generation ownership, and terminal-assurance
transitions. It does not verify native swapchain synchronization or the process
watchdog. An earlier compile against files still being edited failed; its
`compile-final.log` is preserved separately and is not a passing build.

Two later regressions reproduced lost query-failure diagnostics and stale
post-create entrypoint evidence (`tests-diagnostic-before.log`, two failures).
Their fixes passed strict compilation and all 50 assertions in
`compile-diagnostic-fixed.log` and `tests-diagnostic-fixed.log` in the same
directory. This supersedes the 48-assertion policy checkpoint only.

Metal's added backend-neutral NativeCallback capability and its integration-test
assertion also passed strict Debug object compilation, recorded in
`Build/Validation/030/retirement-metal-capability-compile.log`; that compile is
not a new native presentation run. No additional whole task is marked complete
at this intermediate checkpoint.

## Vulkan render submission checkpoint (partial)

Strict Debug integration passed after correcting the capability test's
source-scoped Vulkan macro registration. The first failed link and corrected
build remain in `Build/Validation/030/deferred-review/build-debug.log` and
`build-debug-registration-fixed.log`. Required M4 native execution passed 20
deferred-submission assertions and seven capability-query assertions in
`native-debug-first.log`. The delayed case holds observation of a real native
submission; it does not claim a physically stalled GPU. Separate GPU copies
verify data, upload revisions, in-flight protection and dropped-buffer cleanup.

Seven affected regression suites passed 367 assertions (`debug-regressions.json`).
The default output suite only opts out of its native presentation portion; a
separate explicitly required legacy native presentation run passed all eight
assertions (`debug-vulkan-output-presentation-required.log`). These are local
working-tree checks, not formal authority or a completed lab. Shared-texture
two-frame ownership, actual capability enablement/presentation fallback, Metal
review corrections and Release integration remain open at this checkpoint.
`deferred-review/checkpoint.json` records that scope; no additional whole task
is checked off.


## Subsequent review checks (working tree)

The logical-device startup fixture now enables the available instance dependencies
and checks actual device creation. On the M4 Pro it passed all 12 assertions,
including optional maintenance1 enablement and an explicitly forced ordinary
swapchain device. Logs are `Build/Validation/030/capability-review/compile-startup-fixed.log`
and `startup-m4-fixed.log`. The earlier `startup-m4.log` failed one stale test
expectation that assumed a fallback-only candidate; that failure is preserved.
This fixture creates no swapchain and does not close native lab presentation.

Metal's paused-target and mixed legacy/borrowed-acquire corrections passed the
Debug unit suites (`Build/Validation/030/metal-review/debug-unit-fixed.log`).
The native borrowed-preview helper then completed two frames and one lifecycle
with clean ownership and released presentation leases (`borrowed-preview-fixed.json`
and `.log` in the same directory). This is a backend lifecycle probe, not a
production scene or physical scanout claim.

The shared-texture strict Debug build passed
(`Build/Validation/030/deferred-review/build-debug-shared-textures.log`), but its
first required native run failed three new shared-texture assertions while the
existing 20 submission/buffer assertions passed (`native-shared-textures-debug.log`).
The shared-texture correction remains under review; no whole task is closed by
this intermediate check.


The subsequent native fixture correction omitted an invalid no-op
`CopySource -> CopySource` declaration. The required shared-texture run then
passed all 25 assertions (`deferred-review/build-debug-noop-transition-fixed.log`
and `native-noop-transition-fixed-debug.log`). The intermediate diagnostic run
is retained as `native-texture-diagnostic-debug.log`; its four failures preceded
that correction. These checks cover two pending texture uses, reverse completion
observation, native copied bytes and deferred invalidation retirement.

The capability fixture also corrected ordinary `KHR_surface` enablement to be
independent of optional surface-maintenance dependencies. Auto selection and
force-off startup were tested with those optional dependencies both enabled and
omitted, passing 21 assertions (`capability-review/compile-no-optional-instance.log`
and `startup-no-optional-instance.log`). Required platform/feature-query instance
extensions remain available; this is not an assertion that every instance
extension was disabled. Real borrowed Vulkan swapchain presentation is pending.

Review continues on retained native pipeline ownership and terminal synchronous
failure reporting before the Vulkan implementation tasks can be closed.


The integrated startup strict Debug build passed on 2026-09-07. The required
native deferred and capability suites passed 50 assertions, including retained
pipeline invalidation and simulated post-submit observation failure. The first
startup test was incorrectly compiled without its native availability macro;
that failure is retained in `deferred-review/native-startup-integration-debug.log`.
After registration was corrected, the real visible startup and legacy output
presentation suites passed all 17 assertions in
`deferred-review/native-startup-macro-fixed-debug.log`: auto enabled maintenance1
on this device, while force-off created an ordinary device without enabling it.
Public retirement remains Unknown until borrowed swapchain integration exists.

The seven related Debug suites passed 354 assertions in
`deferred-review/debug-integration-regressions-fixed.log`; the initial invocation
used an unknown suite name and is retained separately. Architecture validation
reported zero findings. These are working-tree integration checks, not formal
030 acceptance. Release verification and startup failure-diagnostic propagation
remain pending at this checkpoint; no additional task is marked complete.


The corresponding strict Release build and four required Vulkan native suites
then passed (`deferred-review/build-release-startup-integration.log` and
`native-startup-integration-release.log`, 67 assertions). The seven related
Release suites passed 354 assertions in `release-integration-regressions.log`.
Metal's Release borrowed-preview helper also completed two frames/one cycle
with clean shutdown (`metal-review/borrowed-preview-release.log` and `.json`).
These results precede the startup-diagnostic propagation correction and do not
close the remaining native borrowed Vulkan presentation or scene-level lab work.


The retained Vulkan render-submission slice is now reviewed: T017, T019 and T020
are complete, bringing reviewed implementation to **22/127**. The native deferred
suite contributed 29 of the 67 Release assertions above; it covers actual GPU
submission and copied bytes, with explicit simulated host observation delays
and failure injection. Presentation semaphore wiring, generation-owned borrowed
images, independent presentation retirement and terminal lab cleanup remain open
under T015/T021/T022 and later Application tasks. No US1 or Feature030 native
acceptance is claimed.


The startup-diagnostic correction subsequently passed strict Debug and Release
builds and each configuration's 17 required startup/legacy-presentation assertions
(`deferred-review/build-{debug,release}-startup-diagnostics.log` and
`native-startup-diagnostics-{debug,release}.log`). Native query/create failures
now retain bounded owned detail, typed reason and a valid exact native error
where available after temporary Context cleanup. Local failures do not invent
native results. Failure propagation was code-reviewed; no artificial production
startup injection API was introduced, and these successful device runs do not
claim runtime coverage of every native allocation/query failure.


After checkpoint commit `681afac`, replacement policy was extended to finalize
against the actual driver image count. An independent strict optimized standalone
build passed 54 assertions (`policy-review/compile-actual-count-reviewed.log`
and `actual-count-reviewed.log`), including requested/actual count differences,
more than eight images and an actual-count aggregate budget overflow. Rejected
replacement publication keeps the predecessor retired and cannot resume it.
The new native runtime helper is still staged separately pending integration;
this policy correction does not add a completed task or native acceptance claim.

Terminal policy review subsequently passed 70 independent optimized strict
standalone assertions (`policy-review/compile-terminal-owner-reviewed.log` and
`terminal-owner-reviewed.log`). A canceled reacquisition can still prove release
of its predecessor without releasing its own canceled image. Explicit terminal
resolution requires caller-proven acquire/render completion, retains unretired
presentations and incomplete render uses, and preserves replacement failure,
forced-termination and device-loss distinctions. The staged native helper and
its real borrowed-window integration remain under review; these deterministic
policy results are not GPU presentation evidence or additional completed tasks.

The frozen staged swapchain helper passed 37 independently rerun assertions
against controlled Vulkan entrypoints (`swapchain-runtime-review/root-compile.log`
and `root-results.log`). They exercise preferred same-image waits, canceled
acquire-history proof, pending-slot reuse, canceled preferred terminal cleanup,
and first-failure preservation. These are simulated native calls, not GPU
evidence. The submitted semaphore adapter separately passed strict Debug build
and 29 existing required native deferred assertions
(`deferred-review/build-debug-semaphore-adapter.log` and
`native-semaphore-adapter-debug.log`); those regressions use the preserved
no-semaphore overload and do not validate the new window synchronization path.
The helper has now been copied into Source for backend integration. The new
`vulkan-lab-borrowed-native` suite is registered for explicit native window
verification; it has not yet run, and T021/T022 remain open.

Positive presentation notifications were then independently checked with 68
controlled-call assertions (`swapchain-runtime-review/root-callback-compile.log`
and `root-callback-results.log`). The first agent run needed a temporary name
qualification shim; the primary run instead compiled the corrected helper
directly from `callback-reviewed/`. Coverage includes callback timing on either
side of render completion, exactly-once old-token notification, canceled
reacquisition, generation retirement, and no promoted-token notification during
terminal compatibility cleanup. This remains simulated-call evidence while the
Context/queue/swapchain bridge and native tests are under implementation.

The first unified strict Debug bridge build passed, but both required native
cases failed the device factory's historical pre-acquire `GetImage(0)` check
(`deferred-review/build-debug-borrowed-integration.log` and
`native-borrowed-integration-debug.log`). After the factory checked actual image
count for lab creation, auto completed all twelve native clear/present frames
and cleanup; forced acquire-history failed presentation at frame six
(`build-debug-borrowed-factory-fixed.log` and
`native-borrowed-factory-fixed-debug.log`, 22 passing/2 failing assertions).
Repeated same-image cycles isolated an uncleared presentation-attempt substate.
Independent controlled-call runs reproduced 6 failures in 81 assertions before
the fix and passed 85/85 afterward (`swapchain-runtime-review/repeated-reviewed/`
compile/results logs), without a namespace shim. These failures remain recorded;
the complete revised native bridge has not yet passed both modes.

The integrated boundary review then passed strict Debug compilation. The required
native suite first passed 32 assertions and failed two zero-extent assertions
(`deferred-review/build-debug-borrowed-boundaries.log` and
`native-borrowed-boundaries-debug.log`): the helper returned Unavailable for a
paused acquisition instead of retryable NotReady. Both modes already completed
twelve real clear/present frames, typed and semaphore leases, finite render
completion, positive presentation release, resume, cancellation and cleanup.
After correcting that return value, the unchanged required suite passed 34/34
(`build-debug-borrowed-pause-fixed.log` and
`native-borrowed-pause-fixed-debug.log`). Negative cases additionally reject a
wrong same-device acquire semaphore, a foreign-device render semaphore and a
duplicate typed render lease before allowing the valid frame to proceed.
Zero extent here is an explicit public Reconfigure request; it is not a claim
of OS-driven minimize/focus coverage.

The same strict Debug binary passed the existing required Vulkan deferred,
capability, startup and output-transform native suites (29+21+9+8 assertions),
and seven related RHI/Renderer/Metal/Vulkan suites (374 assertions). Logs use
`deferred-review/*-borrowed-review-debug.log`. The output-transform architecture
checker reported zero findings. These are current working-tree backend probes,
not Lantern/Sponza lab acceptance, formal same-SHA evidence or HDR visual
attestation. Release integration and final bridge review remain pending.

The final bridge review closed generic-submit and canceled-target bypasses,
immutable image/generation identity checks, ordinary Submit's logical consumption
of lab semaphores, and logical release after wrapper invalidation. Forty-seven
required borrowed native assertions now pass in both strict Debug and Release.
The current M4 auto run reports mode=1/reason=1 (PresentationFence/Preferred,
optional enabled); force-off reports mode=2/reason=2
(AcquireHistory/ForcedOff, optional disabled). Surface queries refresh the lab
selection after generation creation while preserving the display generation.
A preceding 45-pass/2-fail run exposed the initial cached Unknown mode; that
failure and the intermediate surface-access compile error remain in the ignored
logs rather than being relabeled as successful runs.

Final evidence is under `deferred-review/`: strict build logs
`build-debug-borrowed-surface-access-fixed.log` and
`build-release-borrowed-surface-refresh.log`; required borrowed logs
`native-borrowed-surface-refresh-debug.log` and
`vulkan-lab-borrowed-native-surface-refresh-release.log`. Existing required
native regressions passed 67/67 in both configurations (Debug
`*-published-mode-debug.log`, Release `*-surface-refresh-release.log`), and the
seven related suites passed 374/374 per configuration
(`*-surface-refresh-{debug,release}.log`). Nonnative guarded syntax passed in
`nonnative-borrowed-surface-refresh.log`; architecture review again reported
zero findings. The final controlled-call helper snapshot also passed 85/85 in
`swapchain-runtime-review/owner-guards-reviewed/`, including the cancellation
eligibility guard exercised by the native cancellation probes.

T021/T022 are now reviewed complete, bringing implementation to **24/127**.
These are working-tree backend integration checks. The full scene/UI lab,
application terminal worker/watchdog, remaining failure fixtures, cross-platform
closeout and current human HDR acceptance remain pending. IdleAssumed terminal
fallback cleanup is never counted as proven presentation release. Nothing here
updates Accepted images or claims a successful formal Feature 030 hardware gate.


## T026 Renderer asynchronous preview review (2026-09-07)

T026 is reviewed complete, bringing implementation to **25/127**. Renderer now
records a validated zero-readback preview graph into a shared opaque ticket and
exposes explicit submit, poll and retire operations. Native admission/completion
flags are independent of success/failure results. Pending admission/cancellation
and retirement remain retryable; a real first failure survives later completion
or successful cleanup. Render-ticket retirement releases its executor/target
references while retaining diagnostics for independently owned presentation
leases. Preview never publishes formal output or calls the legacy synchronous
wait/readback/presentation facade. Formal execution explicitly rejects a preview
plan.

Strict Debug and Release builds passed. Each configuration passed 46
renderer-output-transform assertions (including 19 preview assertions), 20
output-transform math, 22 RHI presentation-output and seven output-presentation
lifecycle assertions: **95/95 per configuration**. Architecture validation passed
with zero findings. Logs are under `Build/Validation/030/deferred-review/`:
`build-{debug,release}-preview-final.log` and
`{renderer-output-transform,renderer-output-transform-math,rhi-presentation-output,output-presentation-lifecycle}-preview-final-{debug,release}.log`.
An earlier test failure remains in `renderer-preview-debug.log`; its weak-owner
fixture mistakenly retained the caller's shared binding, which the final test
explicitly releases before checking retired-ticket ownership.

These are working-tree deterministic seam/regression checks, not native scene
preview acceptance. Non-successful acquisition produces no Renderer ticket;
the adapter independently retains pending acquisition and its persistent session
owner must retry or drain it. T027/T028/T029 still own the Demo resource,
submission and acquired-target integration. No scene/HDR/hardware closeout or
Accepted baseline update is claimed.


## T027 zero-readback production bindings review (2026-09-07)

T027 is reviewed complete, bringing implementation to **26/127**. Explicit
InteractivePreview/None resources use a validated borrowed single-sample 2D
ColorAttachment|Present target and allocate no readback buffers. Formal callers
retain six authoritative readbacks and the existing FinalOutput-only lifecycle
selection. Renderer records output-transform stages independently of the
validation-readback pass and transitions the actual terminal output once.
Releasing the builder's resources does not invalidate the borrowed target.

The production submission harness accepts at most two deferred submissions,
rejects duplicate pending commands, retains owners before native admission,
polls fences only with Wait(0), and retires only after observed completion.
First failures survive cleanup; reset NotReady remains retryable. Release and
reinitialization return NotReady while pending owners exist. The harness invokes
neither ordinary Submit nor WaitIdle on this path. Its session owner must outlive
pending work; destructor execution is not completion evidence.

Strict Debug and Release builds passed. Each configuration passed 51
production-content-demo assertions (nine new preview/submission checks), 39
deferred-renderer, 46 renderer-output-transform and 91 rhi-deferred-submission
assertions: **227/227**. Required real Vulkan and Metal deferred regression also
passed **13/13 per configuration**, including native GBuffer readback, matrix
packing and frozen cross-backend semantic tolerances. Architecture validation
reported zero findings. Logs under `Build/Validation/030/deferred-review/` are
`build-{debug,release}-t027.log`, the four named suite
`*-t027-{debug,release}.log` files, and
`deferred-native-t027-{debug,release}.log`.

These are working-tree implementation/regression checks. The native regression
exercises existing deferred rendering; zero-readback preview command inspection
and asynchronous harness lifecycle are deterministic tests. They do not claim
integrated Lantern/Sponza preview or Feature 030 hardware acceptance. T028 must
replace preview's shared-snapshot uniform updates with slot-local resources
before concurrent scene frames are supported; T029 supplies the native lab
adapter. The RHI currently returns a result without an explicit acceptance flag;
the harness retains successful submissions or commands observed Submitted after
the call, after rejecting any already-pending command before admission.

## T028 two-slot production frame context review (2026-09-07)

T028 is reviewed complete, bringing implementation to **27/127**. Each slot
owns private mutable frame/draw buffers and cloned mutable descriptor sets;
immutable scene geometry, material textures/samplers and pipelines retain their
exact snapshot bindings. Per-frame updates preserve buffer/descriptor identity
and do not allocate replacement GPU resources. Invalid input is rejected before
upload; native upload failure invalidates only the affected slot. Partial
initialization and explicit release invalidate each private allocation once.

The Demo context retains a real shared scene lease and at most two render slots.
It preflights drawable axis/pixel limits and checked aggregate attachment bytes
before allocation, then checks the realized descriptors. Either zero axis pauses
admission. Unchanged extents reuse scene attachments and rebind only the borrowed
terminal output framebuffer. The current UI-off attachment footprint is 56
bytes/pixel/slot; the 1 GiB aggregate limit remains unchanged. Borrowed outputs
are excluded from this budget and are never invalidated by the context.

Submission, polling and submission retirement reuse T027's deferred harness.
A typed render lease becomes available only after observed completion; its fence
is not reset until presentation or backend-confirmed logical cancellation.
Up to 16 independent presentation records cover active and retiring generations.
Render-complete slots can be reused while those presentation leases remain
pending. Recorded cancellation resets commands; retryable reset NotReady and
failed-then-completed native work retain their owners and first error. Successful
Shutdown requires drained owners and releases duplicate scene/device references.

Strict Debug and Release builds passed. Each configuration passed **269/269**
assertions: production-content-demo 65 (14 new slot/context checks),
renderer-static-model 28, deferred-renderer 39, renderer-output-transform 46,
and rhi-deferred-submission 91. Required native Vulkan/Metal deferred regressions
passed **13/13 per configuration**. Architecture validation reports zero findings.
Logs are under `Build/Validation/030/deferred-review/`:
`build-{debug,release}-t028.log`, the five named suite
`*-t028-{debug,release}.log` files, `deferred-native-t028-{debug,release}.log`,
and `architecture-t028.log`.

These are working-tree implementation and regression checks. Slot isolation and
failure sequencing use deterministic RHI fixtures; native regression covers the
existing deferred renderer. T029/T032 must connect the backend presentation
facade and T026 upper preview-ticket adapter to this frame context. T030 retains
responsibility for event service, terminal draining and watchdog ownership;
T033 supplies full native-operation counters. Integrated UI-off Lantern/Sponza
execution, formal Feature 030 hardware acceptance and human HDR authority remain
open. Context destruction is not a native completion proof; the session must keep
it alive until explicit drain or qualified terminal teardown.


## T029 borrowed-target facade review (2026-09-07)

T029 is reviewed complete, bringing implementation to **28/127**. The Demo
backend facade now initializes and prepares the lab independently, borrows
native output targets, accepts exact typed render proof, and tracks separate
presentation leases. Old implementations return Unsupported without calling
formal presentation methods. The facade bounds acquisition records to two and
presentation records to sixteen. Its owner counts describe facade records only;
they do not imply native retirement. Logical cancellation is not presentation
proof, and terminal cleanup preserves failure results and pending owners.

Vulkan creates its first lab swapchain at the actual drawable size. Review found
that a 1x1 bootstrap generation consumed the retirement budget and stalled the
first pause/resume; removing that generation passed the bounded resume check.
Metal device Shutdown now retains its native owner and registries while surface
cleanup or submissions remain pending; ordinary pending submissions return
NotReady without poisoning the terminal result. A native owner test verifies
retention on the first call and successful cleanup after completion.

Strict Debug and Release builds passed. The final borrowed-target native suite
passed **27/27 in each configuration**, covering Vulkan automatic selection,
forced acquire history, and Metal: actual-size preparation, zero-extent pause,
bounded resume, repeated/conflicting acquisition identity, rejected fabricated
presentation proof, ten direct clear-frame presentations, logical cancellation,
and terminal facade cleanup. Existing required native deferred tests passed
**13/13 per configuration**. Native Metal device/command/presentation/native
suites passed **25/25 per configuration**. Release deterministic regressions
passed **256/256** across triangle-demo, production-content-demo,
renderer-output-transform, rhi-deferred-submission, metal-device,
metal-failure-injection and metal-presentation. Debug passed the same seven
suites before the final Vulkan bootstrap fix; final Debug triangle-demo and
production-content-demo regressions passed **102/102**. Architecture validation
reports zero findings.

Logs remain under `Build/Validation/030/deferred-review/`: `build-release-t029.log`,
`build-debug-t029-bootstrap-fix.log`, `demo-lab-presentation-native-t029-debug-bootstrap-fix.log`,
`*-t029-final-native-{debug,release}.log`, `*-t029-native-debug.log`,
`*-t029-final-{debug,release}.log`, and `architecture-t029.log`. Earlier failing
pause/resume diagnostic logs are preserved; they are not passing evidence.

These are working-tree implementation regressions, not formal Feature 030
hardware acceptance, integrated Lantern/Sponza lab execution, or HDR human
viewing authority. T030 must connect pending-acquire cancellation and terminal
worker/watchdog ownership; an empty public target is not proof that a native
acquisition has finished. T032 still owns scene-loop integration and T033 actual
native counters and qualified terminal diagnostics. The test process deadline
does not implement or validate T030's in-process shutdown watchdog.


## T030 Application session and terminal ownership review (2026-09-07)

T030 is reviewed complete, bringing implementation to **29/127**. Application
now coordinates the UI-off camera session, one active/latest-pending transition,
zero-extent pause, fresh-input resume and explicit exit. Unsupported extents
pause and permit a later valid resize; minimized time does not exhaust the
resume transition deadline. Camera actions use raw window releases to rearm
quarantined keys. Escape releases capture without closing. The session collects
window/input diagnostics into one 256-entry ring with bounded UTF-8 detail and
monotonic aggregate count.

Terminal entry transfers the service callback and its captured native owners
exclusively to a worker. The main thread continues window/input service and
never calls the same backend after handoff. An independent steady-clock
watchdog latches drain timeout at five seconds and terminates a failed process
at ten seconds if cleanup cannot finish, without unwinding live native owners.
Core's existing process facade supplies the final no-destructor failure exit.
First failure and IdleAssumed/Proven/Forced/DeviceLost remain distinct. Successful
cleanup requires an explicit completed response with zero retained owners and
an appropriate terminal assurance; no render/presentation fence is fabricated.

The additive pending-acquire cancellation seam defaults to Unsupported. Native
swapchains retain exact attempted token/slot identities. Metal acknowledges only
after its unpublished nextDrawable job has finished; Vulkan cancels pending or
unpublished acquisitions without a new acquire call and retains acquired image
ownership for later retirement/terminal teardown. The Demo facade no longer
tries to acquire a public target merely to cancel a pending request.

Strict Debug and Release builds passed. Each configuration passed **337/337**
assertions: interactive-lab-lifecycle 21, interactive-lab-watchdog 1,
core-platform-termination 2, core-platform-process 7, application-free-camera 31,
application-window 60, production-camera-preview 22, rhi-deferred-submission 91,
triangle-demo 37, production-content-demo 65. Each configuration also passed
**96/96 native assertions**: demo-lab-presentation-native 29,
vulkan-lab-borrowed-native 47, metal-presentation 7, deferred-native 13.
The two new native Demo checks cover private asynchronous Metal acquisition,
foreign-token rejection and cancellation acknowledgment without publishing a
target. Architecture validation reports zero findings.

Logs are under `Build/Validation/030/deferred-review/`:
`build-{debug,release}-t030-bounds.log`, the ten named suite
`*-t030-bounds-debug.log` / `*-t030-final-release.log` files,
`*-t030-native-{debug,release}.log`, and `architecture-t030-final.log`.
Earlier failing lifecycle fixture logs are retained. The final fixtures feed
raw driver release events and explicit focus restoration, matching session input
ownership. A separate temporary Core test driver also passed the process-exit
checks before they were integrated into StonerTest; no new shipped executable
was added.

These are working-tree implementation/regression checks. Session worker and
watchdog fixtures use deterministic callback ownership, shortened timeout bounds
and a deliberately blocked child; they do not claim a real GPU idle timeout.
T032 still composes the strict-cooked scene loop and T026 preview adapter; T033
records actual native-operation/terminal counters and assurance. Integrated
Lantern/Sponza acceptance, formal Feature 030 physical/hosted closeout and current
human HDR authority remain open. No prior authority exception is carried forward.

## T031 lab configuration review (2026-09-07)

T031 is reviewed complete, bringing implementation to **30/127**. The lab flags
select native visible strict-cooked Deferred preview, default UI on and automatic
retirement selection. Forced acquire history is limited to bounded Vulkan lab
validation. Calibration, formal capture and native-probe options are rejected.
Bounded lab runs require an explicit positive frame budget and do not inherit
the formal endurance warmup/RSS sample matrix or Accepted registry requirement.
Live Metal HDR remains distinct from formal visible capture.

Strict Debug and Release builds passed. Each configuration passed 75
production-content-demo assertions and 37 triangle-demo regressions; the output
transform architecture check passed. Logs are under
`Build/Validation/030/deferred-review/*-t031-*.log`.

T032 startup is still pending: the new flag currently fails explicitly before
the legacy application loop. These configuration checks do not claim an
operational scene lab, native scene smoke or hardware acceptance.
