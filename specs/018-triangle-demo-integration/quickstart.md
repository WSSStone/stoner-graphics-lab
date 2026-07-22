# Quickstart: Triangle Demo Integration Milestone

## Prerequisites

- SCons 4.10.1 and a C++20 compiler.
- Existing `godot` conda environment may be used for local SCons execution.
- Real Windows/macOS presentation additionally needs GLFW 3.4-compatible headers/library and a Vulkan SDK/runtime.
- macOS needs MoltenVK available through the Vulkan SDK/runtime.
- Linux native CI needs Vulkan loader/development headers and Mesa Lavapipe; no display is required.

The build must still support deterministic compilation/tests when native graphics dependencies are absent. Native modes report dependency/runtime unavailability rather than falling back silently.

## Build and Existing Regression Suite

```bash
conda run -n godot scons
Build/Mac/Debug/Tests/StonerTest
```

Expected demo outputs follow the existing platform/config convention:

```text
Build/Mac/Debug/Demo/StonerDemo
Build/Linux/Debug/Demo/StonerDemo
Build/Win64/Debug/Demo/StonerDemo.exe
```

## Deterministic Headless Validation

Runs on Windows, macOS, and Linux without display/GPU requirements:

```bash
Build/Mac/Debug/Demo/StonerDemo \
  --mode headless \
  --frames 4096 \
  --warmup-frames 512 \
  --memory-sample-interval 128 \
  --max-memory-growth-mib 16 \
  --validation-output Build/Mac/Debug/Demo/headless-validation.log
```

Expected results:

- exit code 0
- exactly 4,096 completed frames
- deterministic runtime proof, never native-visible proof
- memory gate passes
- all final demo-owned live counts are zero

## Linux Software Vulkan Validation

Provision CI packages using the supported Ubuntu package manager, then identify the Lavapipe ICD. A typical hosted-runner setup uses packages equivalent to:

```bash
sudo apt-get update
sudo apt-get install -y libvulkan-dev mesa-vulkan-drivers vulkan-tools glslang-tools
```

Resolve the installed `lvp_icd*.json` path and set `VK_DRIVER_FILES` to that explicit file. Then build with native Vulkan enabled and run:

```bash
Build/Linux/Debug/Demo/StonerDemo \
  --mode headless-vulkan \
  --frames 4096 \
  --warmup-frames 512 \
  --memory-sample-interval 128 \
  --max-memory-growth-mib 64 \
  --validation-output Build/Linux/Debug/Demo/lavapipe-validation.log
```

The log must report native runtime mode and a software adapter. No window, surface, swapchain, screenshot, or visible-success marker is expected.

## Windows Real-Window Smoke

From a developer command prompt with Vulkan SDK and GLFW paths configured.
`GLFW_ROOT` must contain `Include/GLFW/glfw3.h` (or lowercase `include`) and
`Lib/glfw3.lib`, lowercase `lib/glfw3.lib`, or an official prebuilt directory such
as `lib-vc2022/glfw3.lib`:

```powershell
$env:VULKAN_SDK = "C:\VulkanSDK\<version>"
$env:GLFW_ROOT = "C:\Libraries\glfw-3.4"
```

```powershell
scons
Build\Win64\Debug\Demo\StonerDemo\StonerDemo.exe `
  --mode validate `
  --frames 10000 `
  --warmup-frames 1000 `
  --memory-sample-interval 120 `
  --max-memory-growth-mib 64 `
  --validation-output Validation\018\Windows\triangle.log
```

During the run:

1. Confirm one triangle is visible with distinguishable RGB vertex interpolation.
2. Resize, minimize, and restore the window; confirm presentation resumes.
3. Capture `Validation/018/Windows/triangle.png` from this run.
4. Let the bounded run exit automatically and confirm exit code 0.

## macOS Real-Window Smoke

With Vulkan SDK/MoltenVK and GLFW paths configured:

```bash
conda run -n godot scons
Build/Mac/Debug/Demo/StonerDemo/StonerDemo \
  --mode validate \
  --frames 10000 \
  --warmup-frames 1000 \
  --memory-sample-interval 120 \
  --max-memory-growth-mib 64 \
  --validation-output Validation/018/macOS/triangle.log
```

Confirm the log reports native Vulkan through the portability path, capture `Validation/018/macOS/triangle.png`, exercise resize/minimize/restore, and allow automatic successful exit.

## Interactive Mode

```bash
Build/Mac/Debug/Demo/StonerDemo/StonerDemo --mode interactive
```

- Runs until window close or Escape.
- Does not auto-pass a bounded validation profile.
- Zero drawable extent pauses presentation while event polling continues.
- Missing native presentation exits non-zero; deterministic fallback cannot produce visible success.

## Shader Verification

The source and checked-in payload pairs are:

```text
Demo/StonerDemo/Shaders/Triangle.vert
Demo/StonerDemo/Shaders/Triangle.vert.spv
Demo/StonerDemo/Shaders/Triangle.frag
Demo/StonerDemo/Shaders/Triangle.frag.spv
```

When an offline compiler/validator is available, the SCons shader target regenerates or validates SPIR-V before the demo target. Without those tools, the build validates basic payload structure and copies the checked-in artifacts. Runtime compilation is not used.

## Failure Expectations

Representative expected failures:

- `--mode validate --frames 0`: exit 2, invalid configuration.
- visible mode without GLFW/Vulkan/MoltenVK: exit 3, dependency/runtime unavailable.
- malformed SPIR-V: exit 4, shader initialization failure.
- acquire/submit/present fatal failure: exit 5.
- memory/resource endurance gate failure: exit 6.
- unwritable validation output: exit 7.

The first error diagnostic identifies the primary lifecycle/frame stage; cleanup diagnostics do not replace it.

## Final Verification

```bash
conda run -n godot scons
Build/Mac/Debug/Tests/StonerTest
rg -n "Vk[A-Za-z_]*|GLFW" Source/Application/Public Source/Renderer/Public Source/RHI/Public
rg -n "native.*address|0x[0-9A-Fa-f]+" Validation/018 Build/Mac/Debug/Demo/*.log
```

Expected boundary scan: no raw Vulkan/GLFW types in Application, Renderer, or RHI public headers. Expected evidence scan: no native pointer/handle addresses in normalized logs.

## Local Implementation Verification (2026-07-20)

Verified from repository root with the `godot` conda environment:

```bash
/usr/local/bin/glslangValidator -V Demo/StonerDemo/Shaders/Triangle.vert -o Demo/StonerDemo/Shaders/Triangle.vert.spv
/usr/local/bin/glslangValidator -V Demo/StonerDemo/Shaders/Triangle.frag -o Demo/StonerDemo/Shaders/Triangle.frag.spv
/usr/local/bin/spirv-val Demo/StonerDemo/Shaders/Triangle.vert.spv
/usr/local/bin/spirv-val Demo/StonerDemo/Shaders/Triangle.frag.spv
conda run -n godot scons -Q
Build/Mac/Debug/Tests/StonerTest
python .github/scripts/run_triangle_demo_validation.py \
  --profile deterministic \
  --tests Build/Mac/Debug/Tests/StonerTest \
  --demo Build/Mac/Debug/Demo/StonerDemo/StonerDemo \
  --report Build/Mac/Debug/Demo/StonerDemo/ci-deterministic-report.txt \
  --timeout-seconds 1200
```

Observed deterministic result: all test suites passed; 4,096 of 4,096 frames completed;
28 post-warm-up RSS samples were recorded; baseline and final medians were equal in
the retained local report; final live object count was zero; validation passed.

The macOS native-headless probe compiled and reached MoltenVK. Inside the automation
sandbox it returned the required explicit runtime-unavailable result because Metal
was unavailable, without falling back. Running the same built executable outside
the sandbox completed 4,096 of 4,096 native Vulkan frames on the Apple M4 Pro,
recorded 28 memory samples, stayed within the 64 MiB growth budget, and reported
zero final live objects. After installing GLFW 3.4 through Homebrew, the interactive
macOS path created a real window and Vulkan surface/swapchain and visibly presented
one non-degenerate triangle with red, green, and blue vertices and smooth interpolation.
The smoke screenshot included unrelated desktop content and the run did not execute
the formal 10,000-frame/20-recovery profile, so it was inspected but intentionally not
retained as the required `Validation/018/macOS/triangle.png` evidence pair.

## Local Windows Verification (2026-07-21)

Verified from an x64 MSVC environment using Visual Studio Community 2026
(MSVC 19.51.36248), Python 3.12.12, SCons 4.10.1, Vulkan SDK 1.4.350.0, and the
persistent official GLFW 3.4 package at `D:\Programs\glfw-3.4`:

```powershell
scons -Q
Build\Win64\Debug\Tests\StonerTest.exe
python .github\scripts\run_triangle_demo_validation.py `
  --profile deterministic `
  --tests Build\Win64\Debug\Tests\StonerTest.exe `
  --demo Build\Win64\Debug\Demo\StonerDemo\StonerDemo.exe `
  --report Build\Win64\Debug\Demo\StonerDemo\deterministic-report.txt `
  --timeout-seconds 1200
```

The build and complete regression suite passed. Deterministic validation completed
4,096 of 4,096 frames with 28 memory samples, RSS medians of 9,293,824 and
9,297,920 bytes, zero final live objects, and `validation-result=pass`.

Formal visible run `windows-018-682e236-20260721T112047` completed naturally with
exit code 0 on an NVIDIA GeForce RTX 3080 using driver 581.32. It completed 10,000
of 10,000 frames, presented first at 1,162.517 milliseconds, recorded exactly 20
minimize/restore recoveries with a maximum of 58.457 milliseconds, recorded 75 RSS
samples with baseline/final medians of 142,389,248 and 189,550,592 bytes, reported
zero final live objects, and passed. The normalized log contains no native address.
The same-run local screenshot was manually inspected and showed one non-degenerate
triangle with red, green, and blue vertices and smooth interpolation; it is temporary
local validation evidence and does not add a long-term screenshot archival requirement.

## Renderer/RHI Visible-Path Integration Verification (2026-07-22)

The visible frame coordinator now builds a valid Renderer forward plan, acquires
backend-neutral native RHI bindings, records the frame through `FForwardFrameExecutor`,
and submits/presents through two rotating Vulkan frame slots. Logical window size and
framebuffer pixel size are represented by separate ordered events.

```bash
conda run -n godot scons -Q
Build/Mac/Debug/Tests/StonerTest
python .github/scripts/run_triangle_demo_validation.py \
  --profile deterministic \
  --tests Build/Mac/Debug/Tests/StonerTest \
  --demo Build/Mac/Debug/Demo/StonerDemo/StonerDemo \
  --report Build/Mac/Debug/Demo/StonerDemo/ci-deterministic-report.txt \
  --timeout-seconds 1200
```

The build and complete regression suite passed. The deterministic profile completed
4,096 of 4,096 frames with 28 post-warm-up RSS samples, equal 9,060,352-byte baseline
and final medians, two configured frame slots, zero final live objects, and a passing
validation result. Because this change replaces the native visible recording path,
the earlier Windows screenshot/log pair is historical evidence; the subsequent formal
macOS and Windows runs below refresh both platforms on the Renderer/RHI path.

## Formal macOS Visible Validation (2026-07-22)

Run `macos-018-75f1e38-20260722T232702` completed naturally with exit code 0 on an
Apple M4 Pro. It completed 10,000 of 10,000 frames through the Renderer/RHI visible
path, first presented at 335.915 milliseconds, and recorded 22 minimize/restore
recoveries with a 2.649-millisecond maximum. The 75 post-warm-up RSS samples produced
baseline/final medians of 99,516,416 and 122,945,536 bytes, remaining within the
64 MiB/10% configured growth gate. Final live objects were zero and validation passed.
The matching `Validation/018/macOS/triangle.png` was manually inspected and shows one
non-degenerate triangle with distinguishable red, green, and blue vertices and smooth
interpolation, with no unrelated desktop content.

## Formal Windows Visible Validation (2026-07-22)

The current `69c404b` Windows build and complete regression suite passed. Its
deterministic profile completed 4,096 of 4,096 frames with two configured frame slots,
28 post-warm-up RSS samples, equal 8,519,680-byte baseline/final medians, zero final
live objects, and a passing validation result.

Run `windows-018-69c404b-20260722T234116` completed naturally with exit code 0 on an
NVIDIA GeForce RTX 3080 using driver 581.32. It completed 10,000 of 10,000 frames
through the Renderer/RHI visible path with two frame slots, first presented at
3,772.743 milliseconds, and recorded 21 valid recoveries after exactly 20
minimize/restore operations with a 47.526-millisecond maximum. The 75 post-warm-up
RSS samples produced baseline/final medians of 138,719,232 and 190,373,888 bytes,
remaining within the 64 MiB/10% configured growth gate. Final live objects were zero,
validation passed, and the normalized log contains no native address.

The matching `Validation/018/Windows/triangle.png` was captured from the DWM visible
window bounds and manually inspected. It contains only the StonerDemo window and shows
one non-degenerate triangle with red, green, and blue vertices, smooth interpolation,
and no rendering corruption. Temporary same-run client captures confirmed that the
triangle reappeared correctly after each of the 20 restore operations.

## Formal Cross-Platform CI Validation (2026-07-23)

GitHub Actions run
[`29935956348`](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/29935956348)
completed successfully for commit `fa2ffff6b22baad9e4c8f5137285b8b08517555f`.
The Windows, macOS, and Linux jobs all built the demo and passed the complete regression
suite plus deterministic 4,096-frame validation.

The Linux job additionally selected Lavapipe through `VK_DRIVER_FILES` and completed
4,096 of 4,096 native-headless Vulkan frames on `llvmpipe (LLVM 20.1.2, 256 bits)`.
The run recorded 28 RSS samples with equal 84,000,768-byte baseline/final medians,
zero final live objects, and `validation-result=pass`. The downloaded normalized
artifact is retained at `Validation/018/Linux/triangle-report.txt`.
