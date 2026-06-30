# Research: Vulkan Resource Management

**Feature**: 010-vulkan-resource-management  
**Date**: 2026-06-30

## Decision: Support real allocation when available and deterministic fallback allocation otherwise

**Rationale**: The previous Vulkan device phase intentionally supports SDK/runtime absence with explicit diagnostics. Resource management should preserve that development ergonomics while still allowing real backend allocation paths when the runtime is available. Deterministic fallback allocation lets the RHI resource contract be tested on machines without Vulkan support.

**Alternatives considered**:
- Require real Vulkan runtime for every success path: rejected because it would make resource contract validation depend on local SDK/runtime availability.
- Fallback only: rejected because the roadmap milestone should start exercising the real backend path where available.
- Return unsupported for all resources when runtime is unavailable: rejected because it would leave descriptor and upload staging behavior untestable in common development environments.

## Decision: Keep allocation ownership as a backend entity with test-controlled failure limits

**Rationale**: Allocation failure must be deterministic. Real memory exhaustion is non-portable, slow, and unsafe as a test strategy. Configurable budget and allocation-count limits let tests cover out-of-memory, partial creation cleanup, and shutdown release behavior without depending on machine memory state.

**Alternatives considered**:
- Depend on actual memory exhaustion: rejected because it is flaky and harmful to developer machines.
- Skip allocation failure coverage: rejected because the spec requires partial creation and cleanup validation.
- Bake fixed tiny limits into production behavior: rejected because tests need control without constraining normal use.

## Decision: Use fixed-capacity descriptor pools for this phase

**Rationale**: Fixed capacity is observable, easy to validate, and maps naturally to explicit descriptor pool exhaustion behavior. This is enough for early renderer integration and avoids hiding pool boundaries behind automatic growth.

**Alternatives considered**:
- Auto-grow descriptor pools: rejected for this phase because it obscures exhaustion tests and introduces policy decisions better handled after real renderer workloads exist.
- Ignore descriptor pool capacity: rejected because descriptor allocation failure is a key backend resource management behavior.
- Full descriptor heap virtualization: rejected as premature for the current RHI and roadmap stage.

## Decision: Descriptor sets retain binding records after resource invalidation

**Rationale**: Existing RHI lifecycle behavior allows invalidated objects to remain queryable. Retaining descriptor binding records preserves deterministic diagnostics while preventing later use of invalid resources. It also avoids hidden mutation of unrelated descriptor state when a resource is invalidated.

**Alternatives considered**:
- Prevent resource invalidation while descriptor-bound: rejected because it creates release-order coupling and hidden ownership rules.
- Automatically clear descriptor bindings: rejected because it makes invalidation side effects harder to inspect and could hide errors.
- Keep descriptors usable with invalid resources: rejected because it would violate lifecycle safety.

## Decision: Upload staging records CPU-visible data and destination ranges only

**Rationale**: Command recording and queue execution belong to a later roadmap phase. This feature should validate upload intent, preserve data and target metadata, and expose pending state without pretending GPU transfer work happened.

**Alternatives considered**:
- Create executable transfer batches now: rejected because it reaches into command recording and submission scope.
- Only validate parameters without storing data: rejected because later command work needs realistic staging records.
- Execute uploads immediately: rejected because command submission is explicitly out of scope.

## Decision: Keep shader, pipeline, render pass, framebuffer, and command factories outside scope

**Rationale**: Resource management should replace the previous unsupported buffer/texture/sampler/descriptor placeholders, but command recording, shader compilation, pipelines, render passes, framebuffers, and render graph scheduling are separate roadmap phases. Explicit unsupported or unchanged behavior keeps phase boundaries clean.

**Alternatives considered**:
- Implement all RHI resource and pipeline factories together: rejected because it merges multiple roadmap phases.
- Expand public RHI just for Vulkan resource details: rejected because it would leak backend-specific concepts into multi-API contracts.
