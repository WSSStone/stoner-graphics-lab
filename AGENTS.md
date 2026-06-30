<!-- SPECKIT START -->
For additional context about technologies to be used, project structure,
shell commands, and other important information, read the current plan:
`specs/011-vulkan-commands-submission/plan.md`
<!-- SPECKIT END -->

## Active Technologies
- C++20 (traditional header/source separation; no C++20 Modules) + C++ standard library where portable (`<chrono>`, `<filesystem>`, `<fstream>`, `<system_error>`, `<thread>`); platform system libraries guarded behind Core implementation boundaries; SCons 4.10.1 build system (006-core-platform-abstraction)
- Local filesystem only for basic read/write/existence/directory operations; no persistent database or asset catalog (006-core-platform-abstraction)
- C++20 with traditional header/source separation + Existing Core layer types/math/logging/platform abstractions; SCons 4.10.1; C++ standard library for non-graphics utilities (007-rhi-core-interfaces)
- C++20 with traditional header/source separation + Existing Core layer types and containers; existing RHI core contracts (`ERHIResult`, `ERHIFormat`, `ERHIQueueType`, `IRHIDevice`, lifecycle/result conventions); SCons 4.10.1; C++ standard library for non-graphics utilities (008-rhi-resource-pipeline)
- C++20 with traditional header/source separation + Existing Core and RHI public contracts; Vulkan SDK or platform Vulkan loader/headers where available; platform presentation bridge guarded by backend implementation boundaries; SCons 4.10.1 (009-vulkan-backend-device)
- C++20 with traditional header/source separation + Existing Core types and RHI resource/descriptor contracts; existing Vulkan backend device, diagnostics, lifecycle, and SCons SDK detection; deterministic fallback allocation and process-local upload staging records; SCons 4.10.1 (010-vulkan-resource-management)
- C++20 with traditional header/source separation; no C++20 Modules + Existing Core types and containers; existing RHI command/queue/sync/resource/render pass/framebuffer contracts; existing Vulkan backend device, queue, sync, resource, diagnostics, and SCons SDK detection; Vulkan SDK or platform Vulkan loader/headers where available (011-vulkan-commands-submission)
- Process-local backend state only; command records, submission records, diagnostics, render pass/framebuffer objects, and upload scheduling records live in memory (011-vulkan-commands-submission)

## Recent Changes
- 006-core-platform-abstraction: Added C++20 (traditional header/source separation; no C++20 Modules) + C++ standard library where portable (`<chrono>`, `<filesystem>`, `<fstream>`, `<system_error>`, `<thread>`); platform system libraries guarded behind Core implementation boundaries; SCons 4.10.1 build system
- 007-rhi-core-interfaces: Planned RHI device, capabilities, command buffer, queue, synchronization, headless swapchain, result/status, and mock-test contracts
- 008-rhi-resource-pipeline: Planned RHI buffer, texture, sampler, shader module, descriptor, pipeline, render pass, framebuffer, lifecycle invalidation, and mock-test contracts
- 009-vulkan-backend-device: Implemented Vulkan backend runtime initialization, deterministic adapter selection, device/queue/sync objects, Core platform-window-backed surface validation, swapchain lifecycle, diagnostics, unsupported-runtime validation, and SCons SDK detection fallback
- 010-vulkan-resource-management: Implemented Vulkan backend buffers, textures, samplers, pipeline layouts, allocation ownership, deterministic fallback allocation, fixed-capacity descriptor pools, descriptor sets, upload staging records, diagnostics, lifecycle invalidation, and deterministic resource tests
- 011-vulkan-commands-submission: Implemented Vulkan command pools, command buffers, queue submission, deterministic fallback completion, minimal render pass/framebuffer scope, declarative barriers, upload scheduling, diagnostics, and regression tests

## Git Commit Style
- Commit messages must start with a conventional type prefix such as `feat`, `docs`, `fix`, `chore`, `refactor`, `test`, or `build`.
- Prefer `type(scope): summary` when a clear scope exists, for example `docs(spec-006): align platform abstraction numbering`.
