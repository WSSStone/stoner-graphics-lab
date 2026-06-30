# Research: Vulkan Command Recording & Submission

**Feature**: 011-vulkan-commands-submission  
**Date**: 2026-06-30

## Decision: Add a Vulkan command pool and command buffer layer

**Rationale**: The RHI already exposes command buffer lifecycle and symbolic command recording. The Vulkan backend needs concrete command ownership so allocation, Begin/End/Reset, command summaries, submission compatibility, and shutdown invalidation can be validated without leaking backend details to Renderer or Application code.

**Alternatives considered**:
- Keep command buffers unsupported until pipeline work: rejected because command submission is the current roadmap phase and is needed before pipeline/shader integration can be tested.
- Fold command buffer logic into the device or queue: rejected because it would create hidden ownership and lifecycle coupling.

## Decision: Use real queue submission when available and deterministic fallback submission otherwise

**Rationale**: Supported runtime environments should exercise the real backend path. Unsupported runtime environments still need stable contract tests for command lifecycle, queue compatibility, diagnostics, and completion behavior. Deterministic fallback submission preserves this value while reporting that no real GPU execution occurred.

**Alternatives considered**:
- Reject all submissions without a runtime: rejected because it prevents command lifecycle and queue submission validation in common CI or developer environments.
- Pretend fallback submission is real execution: rejected because it would create misleading diagnostics and weaken future rendering validation.

## Decision: Treat fallback completion as immediate by default with injectable not-ready/timeout outcomes

**Rationale**: Immediate completion keeps tests deterministic and avoids implying asynchronous GPU work where none occurred. Test-controlled not-ready and timeout outcomes cover wait paths without relying on runtime scheduling.

**Alternatives considered**:
- Always return not-ready first: rejected because it adds artificial state progression to every fallback test.
- Disable completion observation in fallback mode: rejected because completion observation is part of this feature's acceptance criteria.

## Decision: Record draw and dispatch as placeholder commands before pipeline binding exists

**Rationale**: Draw and dispatch ordering, queue compatibility, render pass scope, and lifecycle validation belong in this phase. Real shader/pipeline execution does not. Placeholder commands with missing-pipeline diagnostics preserve the boundary and still prepare the command path for the next roadmap phase.

**Alternatives considered**:
- Reject draw and dispatch until pipelines exist: rejected because it would leave graphics/compute command validation mostly untested.
- Treat draw and dispatch as successful no-ops without diagnostics: rejected because it hides the missing pipeline requirement.

## Decision: Implement minimal single-subpass backend render pass and framebuffer validation

**Rationale**: The RHI resource/pipeline phase already defined single-subpass render pass and framebuffer contracts. Command recording needs a real backend scope object to validate Begin/EndRenderPass and graphics command scope. Keeping this minimal avoids pulling in full pipeline compatibility or multi-subpass behavior.

**Alternatives considered**:
- Record render pass scope without objects: rejected because it would not validate framebuffer compatibility or invalidated attachment behavior.
- Defer Begin/EndRenderPass entirely: rejected because draw validation requires render pass scope in the current spec.
- Implement full render pass/pipeline compatibility: rejected because pipeline and shader are a later roadmap phase.

## Decision: Limit barriers/layout transitions to declarative intent plus basic validation

**Rationale**: This phase should verify resource lifecycle, usage compatibility, range/region validity, and before/after state consistency for recorded barrier intent. Full per-resource state tracking across command buffers belongs with render graph or deeper synchronization work.

**Alternatives considered**:
- Implement full state tracking now: rejected because it expands the feature into render graph-level synchronization.
- Record barriers without validation: rejected because invalid resource or usage paths would not be caught.

## Decision: Schedule existing upload staging records into command buffers without claiming execution before submission

**Rationale**: The previous resource phase created pending upload records with CPU-visible staging data. This phase connects them to command recording and submission state while preserving the distinction between scheduled work and completed work.

**Alternatives considered**:
- Leave uploads untouched until renderer integration: rejected because upload scheduling is explicitly part of this command phase.
- Mark uploads completed when recorded: rejected because recording is not execution.

## Decision: Keep public RHI changes minimal and backend details inside VulkanRHI

**Rationale**: The constitution requires strict RHI abstraction and future multi-API support. Existing public RHI command contracts may need small extensions only if required for backend-neutral command categories, but Vulkan diagnostics, fallback modes, and test controls belong under the Vulkan backend.

**Alternatives considered**:
- Add Vulkan-specific fields to RHI contracts: rejected because it leaks backend implementation details.
- Avoid any RHI evolution: rejected if existing command interfaces cannot express render pass, copy, barrier, or upload scheduling intent needed by backend-neutral tests.
