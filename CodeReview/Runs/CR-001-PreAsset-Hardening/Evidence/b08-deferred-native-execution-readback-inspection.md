# Evidence: B08-S10 Deferred Native Execution And Readback Inspection

## Commands

- `git status --short --branch`
- `conda run -n stoner-cr python CodeReview/Tools/crctl.py status --id CR-001`
- `nl -ba Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp | sed -n '544,623p'`
- `nl -ba Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp | sed -n '624,760p'`
- `nl -ba Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp | sed -n '1125,1215p'`
- `nl -ba Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp | sed -n '1215,1325p'`
- `nl -ba Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp | sed -n '1325,1395p'`
- `nl -ba Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp | sed -n '1395,1465p'`
- `nl -ba Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp | sed -n '1460,1538p'`
- `nl -ba Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp | sed -n '1538,1629p'`
- `rg --files | rg "(Deferred|deferred|DirectionalLight|PointLight|SpotLight|Composition|SurfaceData|\\.glsl|\\.spv)$"`
- `nl -ba specs/019-deferred-rendering-pipeline/contracts/deferred-validation-contract.md | sed -n '1,80p'`
- `nl -ba specs/019-deferred-rendering-pipeline/spec.md | sed -n '141,171p'`
- `conda run -n stoner-cr python CodeReview/Tools/crctl.py finding --id CR-001 add ...`
- `conda run -n stoner-cr python CodeReview/Tools/crctl.py finding --id CR-001 triage CR001-B08-F005 ...`

## Evidence Points

- `specs/019-deferred-rendering-pipeline/spec.md:144` requires Linux native
  offscreen deferred execution and pixel validation through Lavapipe.
- `specs/019-deferred-rendering-pipeline/spec.md:145` requires finite semantic
  thresholds for final LDR, depth, world normal, metallic, roughness, and
  ambient occlusion.
- `specs/019-deferred-rendering-pipeline/spec.md:163` requires both StandardZ
  and ReversedZ native profiles to produce at least 12 readback/reference
  samples spanning the declared semantics.
- `specs/019-deferred-rendering-pipeline/contracts/deferred-validation-contract.md:35`
  requires the same native scene to execute once with standard-Z and once with
  reversed-Z.
- `specs/019-deferred-rendering-pipeline/contracts/deferred-validation-contract.md:41`
  requires native point lights covering visible, outside-view, and camera-inside
  cases.
- `specs/019-deferred-rendering-pipeline/contracts/deferred-validation-contract.md:42`
  requires native spot lights covering visible, outside-cone, and
  near-plane-intersecting cases.
- `Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp:1174`
  creates three native lights: one directional, one point, and one spot.
- `Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp:1329`
  begins the surface-data render pass and records the opaque and masked draws.
- `Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp:1351`
  begins the lighting render pass, draws directional light, then point and spot
  volumes.
- `Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp:1400`
  transitions/copies all six native images to readback buffers.
- `Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp:1465`
  starts probe creation from mapped readback buffers.
- `Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp:1489`
  records the 12 per-convention probes. The probes cover background, masked
  coverage, surface semantics, lighting accumulation, and final composition, but
  they do not identify the required local-volume edge cases.
- `Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp:1546`
  executes both StandardZ and ReversedZ conventions and fails unless exactly 24
  probes pass with zero final live objects.

## Finding Updates

- Added `CR001-B08-F005` as S2.
- Triaged `CR001-B08-F005` as Accepted.
