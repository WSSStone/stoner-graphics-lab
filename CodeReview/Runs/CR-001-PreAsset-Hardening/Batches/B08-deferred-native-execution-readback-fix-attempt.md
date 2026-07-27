# Evidence: B08-S11 Deferred Native Fix Attempt

## Commands

- `conda run -n stoner-cr python CodeReview/Tools/crctl.py start --id CR-001`
- `rg -n "24|12|GetProbeCount|ProbeCount|Probes\\.size|light_count|draw_count|NativeDeferredReadback|DirectionalLight|PointLight|SpotLight" Tests Source/Backend/Vulkan Source/Renderer specs/019-deferred-rendering-pipeline`
- `nl -ba Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp | sed -n '1,115p'`
- `nl -ba Tests/DeferredNativeIntegrationTests.cpp | sed -n '1,220p'`
- `nl -ba Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp | sed -n '1040,1130p'`
- `nl -ba Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp | sed -n '760,940p'`
- `nl -ba Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp | sed -n '300,543p'`
- `conda run -n stoner-cr python CodeReview/Tools/crctl.py gate --id CR-001 fallback-strict`
- `conda run -n stoner-cr scons config=debug`
- `env STONER_REQUIRE_DEFERRED_NATIVE=1 STONER_DEFERRED_READBACK_REPORT=CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/b08-deferred-native-readback-f005-report.txt Build/Mac/Debug/Tests/StonerTest`
- `sed -n '1,120p' CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/b01-macos-native-deferred-report.txt`

## Observations

- `fallback-strict` passed at `2026-07-27T09:43:04+00:00`.
- Graphics-enabled `Debug` build passed after each attempted edit.
- The attempted seven-light shader-visible native scene failed on macOS native
  validation.
- A narrower attempt that exposed only three rendered lights to descriptors but
  retained seven light records still failed on macOS native validation.
- After code/test edits were reverted, there is no remaining diff in
  `Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp` or
  `Tests/DeferredNativeIntegrationTests.cpp`.

## Failed Native Report Summary

The failed report shows:

- runtime: `RealRuntime`
- adapter: `Apple M4 Pro`
- native submission: `true`
- final live objects: `0`
- `StandardZ/lighting-accumulation`: expected `0.42236,0.42236,0.25,0`, observed `0,0,0,0`
- `StandardZ/composed-final`: expected `0.637888,0.134472,0.025,1`, observed `0.298039,0.0509804,0,1`
- `ReversedZ/lighting-accumulation`: passed
- `ReversedZ/composed-final`: passed

The prior `b01-macos-native-deferred-report.txt` shows the original three-light
native path passed both StandardZ and ReversedZ on the same machine.
