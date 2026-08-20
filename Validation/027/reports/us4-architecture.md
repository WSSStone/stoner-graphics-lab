# Feature 027 US4 Architecture Checkpoint

Date: 2026-08-20
Host: macOS arm64
Status: PASS for architecture and strict-build isolation; native evidence is
still pending T101-T103.

## Commands

- `conda run -n godot python Tests/verify_metal_backend.py`: PASS.
- `conda run -n godot scons platform=macos arch=arm64 config=debug strict=true -j8`: PASS.
- `Build/Mac/Debug/Tests/StonerTest --suite deferred-native`: PASS with Metal
  and MoltenVK explicitly classified as unavailable in the current sandbox.
- `git diff --check`: PASS.

## Boundary Result

- `Source/Renderer` and `Source/Application` contain no Apple API imports,
  Objective-C++, or Metal-specific rendering algorithms.
- Backend selection and failure identity remain explicit in the Demo factory;
  an unavailable Metal request cannot substitute Vulkan.
- `FDeferredRenderer`, `BuildDeferredRenderGraphDeclaration`, and
  `FDeferredFrameExecutor` are shared unchanged by Vulkan and Metal.
- `Tests/MetalDeferredNativeProbe.cpp` owns only test composition: it derives
  repository SPIR-V, creates Metal RHI objects, submits the shared deferred
  graph, and reads six real GPU attachments. Its output cannot report success
  when the Metal device, compiler, submission, fence, or readback is absent.
- SPIRV-Cross and compiler orchestration remain Tools-only. Logical Shader Asset
  entry `main` maps deterministically to native `stoner_main`; the Backend loads
  that exact symbol and does not probe fallback names.

## Pending Native Evidence

The Codex execution sandbox exposes neither a Metal device nor Metal-backed
MoltenVK. Therefore this checkpoint does not satisfy T101, T102, or T103 and
does not claim triangle, deferred, or cross-backend native equivalence.
