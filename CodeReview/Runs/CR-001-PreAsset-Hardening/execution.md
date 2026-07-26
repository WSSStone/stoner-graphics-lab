# Execution

Execute one `crctl next` packet per session.

## B00: Bootstrap
- B00-S01: Framework and Draft PR
- B00-S02: Baseline and CodeGraph

## B01: Build, CI, and Architecture
- B01-S01: Inspect
- B01-S02: Fix
- B01-S03: Verify

## B02: Core Features 003-006
- B02-S01: Inspect: value identity and containers
- B02-S02: Fix: value identity and containers
- B02-S03: Verify: value identity and containers
- B02-S04: Inspect: memory allocation and module lifecycle
- B02-S05: Fix: memory allocation and module lifecycle
- B02-S06: Verify: memory allocation and module lifecycle
- B02-S07: Inspect: scalar, vector, and color math
- B02-S08: Fix: scalar, vector, and color math
- B02-S09: Verify: scalar, vector, and color math
- B02-S10: Inspect: matrix, quaternion, transform, and geometry math
- B02-S11: Fix: matrix, quaternion, transform, and geometry math
- B02-S12: Verify: matrix, quaternion, transform, and geometry math
- B02-S13: Inspect: logging system
- B02-S14: Fix: logging system
- B02-S15: Verify: logging system
- B02-S16: Inspect: assertion and platform-break macros
- B02-S17: Fix: assertion and platform-break macros
- B02-S18: Verify: assertion and platform-break macros
- B02-S19: Inspect: platform selection, window, misc, and memory
- B02-S20: Fix: platform selection, window, misc, and memory
- B02-S21: Verify: platform selection, window, misc, and memory
- B02-S22: Inspect: platform time, filesystem, and process
- B02-S23: Fix: platform time, filesystem, and process
- B02-S24: Verify: platform time, filesystem, and process

## B03: RHI Features 007-008
- B03-S01: Inspect: device, capabilities, runtime, and results
- B03-S02: Fix: device, capabilities, runtime, and results
- B03-S03: Verify: device, capabilities, runtime, and results
- B03-S04: Inspect: commands, queues, synchronization, and swapchain
- B03-S05: Fix: commands, queues, synchronization, and swapchain
- B03-S06: Verify: commands, queues, synchronization, and swapchain
- B03-S07: Inspect: buffer, texture, and sampler resources
- B03-S08: Fix: buffer, texture, and sampler resources
- B03-S09: Verify: buffer, texture, and sampler resources
- B03-S10: Inspect: shaders, descriptors, pipelines, render passes, and framebuffers
- B03-S11: Fix: shaders, descriptors, pipelines, render passes, and framebuffers
- B03-S12: Verify: shaders, descriptors, pipelines, render passes, and framebuffers

## B04: Vulkan Foundation 009-010
- B04-S01: Inspect: instance, adapter, device, and capabilities
- B04-S02: Fix: instance, adapter, device, and capabilities
- B04-S03: Verify: instance, adapter, device, and capabilities
- B04-S04: Inspect: surface and swapchain lifecycle
- B04-S05: Fix: surface and swapchain lifecycle
- B04-S06: Verify: surface and swapchain lifecycle
- B04-S07: Inspect: memory allocation, buffers, and textures
- B04-S08: Fix: memory allocation, buffers, and textures
- B04-S09: Verify: memory allocation, buffers, and textures
- B04-S10: Inspect: descriptors, samplers, and upload staging
- B04-S11: Fix: descriptors, samplers, and upload staging
- B04-S12: Verify: descriptors, samplers, and upload staging

## B05: Vulkan Execution 011-012
- B05-S01: Inspect: command pools, buffers, barriers, and render scope
- B05-S02: Fix: command pools, buffers, barriers, and render scope
- B05-S03: Verify: command pools, buffers, barriers, and render scope
- B05-S04: Inspect: queues, submission, and synchronization
- B05-S05: Fix: queues, submission, and synchronization
- B05-S06: Verify: queues, submission, and synchronization
- B05-S07: Inspect: shader modules and interfaces
- B05-S08: Fix: shader modules and interfaces
- B05-S09: Verify: shader modules and interfaces
- B05-S10: Inspect: graphics and compute pipelines and cache
- B05-S11: Fix: graphics and compute pipelines and cache
- B05-S12: Verify: graphics and compute pipelines and cache
- B05-S13: Inspect: native context execution
- B05-S14: Fix: native context execution
- B05-S15: Verify: native context execution

## B06: Renderer 013-015
- B06-S01: Inspect: render graph declaration, compilation, and lifetimes
- B06-S02: Fix: render graph declaration, compilation, and lifetimes
- B06-S03: Verify: render graph declaration, compilation, and lifetimes
- B06-S04: Inspect: render graph execution, resources, and diagnostics
- B06-S05: Fix: render graph execution, resources, and diagnostics
- B06-S06: Verify: render graph execution, resources, and diagnostics
- B06-S07: Inspect: shader library and permutations
- B06-S08: Fix: shader library and permutations
- B06-S09: Verify: shader library and permutations
- B06-S10: Inspect: material definitions, instances, and parameters
- B06-S11: Fix: material definitions, instances, and parameters
- B06-S12: Verify: material definitions, instances, and parameters
- B06-S13: Inspect: forward frame planning, lights, views, and sorting
- B06-S14: Fix: forward frame planning, lights, views, and sorting
- B06-S15: Verify: forward frame planning, lights, views, and sorting
- B06-S16: Inspect: forward execution, graph declaration, and diagnostics
- B06-S17: Fix: forward execution, graph declaration, and diagnostics
- B06-S18: Verify: forward execution, graph declaration, and diagnostics

## B07: Application 016-017
- B07-S01: Inspect: window lifecycle, drivers, and loop
- B07-S02: Fix: window lifecycle, drivers, and loop
- B07-S03: Verify: window lifecycle, drivers, and loop
- B07-S04: Inspect: input mapping, events, and snapshots
- B07-S05: Fix: input mapping, events, and snapshots
- B07-S06: Verify: input mapping, events, and snapshots
- B07-S07: Inspect: entity slots and components
- B07-S08: Fix: entity slots and components
- B07-S09: Verify: entity slots and components
- B07-S10: Inspect: hierarchy, reparenting, and destruction
- B07-S11: Fix: hierarchy, reparenting, and destruction
- B07-S12: Verify: hierarchy, reparenting, and destruction
- B07-S13: Inspect: transforms and render collection
- B07-S14: Fix: transforms and render collection
- B07-S15: Verify: transforms and render collection

## B08: Integration 018-019
- B08-S01: Inspect: triangle demo configuration and lifecycle
- B08-S02: Fix: triangle demo configuration and lifecycle
- B08-S03: Verify: triangle demo configuration and lifecycle
- B08-S04: Inspect: triangle native session, presentation, and synchronization
- B08-S05: Fix: triangle native session, presentation, and synchronization
- B08-S06: Verify: triangle native session, presentation, and synchronization
- B08-S07: Inspect: deferred frame plan, surfaces, and graph
- B08-S08: Fix: deferred frame plan, surfaces, and graph
- B08-S09: Verify: deferred frame plan, surfaces, and graph
- B08-S10: Inspect: deferred native execution and readback
- B08-S11: Fix: deferred native execution and readback
- B08-S12: Verify: deferred native execution and readback
- B08-S13: Inspect: validation, failure injection, and artifacts
- B08-S14: Fix: validation, failure injection, and artifacts
- B08-S15: Verify: validation, failure injection, and artifacts

## B09: Cross-Cutting
- B09-S01: Inspect: test target architecture and private boundaries
- B09-S02: Fix: test target architecture and private boundaries
- B09-S03: Verify: test target architecture and private boundaries
- B09-S04: Inspect: diagnostics, determinism, and failure semantics
- B09-S05: Fix: diagnostics, determinism, and failure semantics
- B09-S06: Verify: diagnostics, determinism, and failure semantics
- B09-S07: Inspect: concurrency, lifetime, and ownership duplication
- B09-S08: Fix: concurrency, lifetime, and ownership duplication
- B09-S09: Verify: concurrency, lifetime, and ownership duplication
- B09-S10: Inspect: performance hotspots and large functions
- B09-S11: Fix: performance hotspots and large functions
- B09-S12: Verify: performance hotspots and large functions
- B09-S13: Inspect: documentation, specification drift, and traceability
- B09-S14: Fix: documentation, specification drift, and traceability
- B09-S15: Verify: documentation, specification drift, and traceability

## B10: Closeout
- B10-S01: Traceability
- B10-S02: Final Gates
- B10-S03: Close

## Required Gates

- Debug and Release
- ASan/UBSan after B01 introduces the profiles
- Deterministic and required native validation
- Three-platform GitHub CI at batch boundaries
- Final CodeGraph rebuild and coverage report
