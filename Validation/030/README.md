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
