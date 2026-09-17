# Feature 031 entry — Anti-Aliasing & Temporal Reconstruction

Status: phase entry authorized on 2026-09-17; specification/planning is next, implementation has not started. Feature 030 is accepted under its explicit closeout decision. This is a scoped entry handoff, not an approved implementation plan.

## Inherited scope

- M0: define velocity units/sign, jitter removal, current/previous ViewProjection and object-transform ownership, spawn/despawn and invalidation; implement deterministic post-tonemap FXAA fallback.
- M1: pre-tonemap TAA, deterministic jitter, history ping-pong/reprojection, depth/normal rejection, disocclusion and neighborhood clamp. Start with fixed input/output extent and Deferred sampleCount=1.
- M2: reusable signal-specific history adapters, exposure-ratio compensation or explicit reset, exposure-change regressions. Later consumers must reuse temporal lifetime/invalidation infrastructure.
- Reuse 030 lab controls/presets/debug views and 029 sole output transform; terminal display-linear UI must remain outside scene temporal history.
- Validate static convergence, thin geometry, object motion, disocclusion, camera cuts, resize/FOV changes, transparent/reactive boundaries and exposure steps on applicable Forward/Deferred Vulkan/Metal paths.
- Changed SDR output needs workload revision, exact-size Candidate, unchanged comparison policy and explicit acceptance. HDR appearance needs live human authority. The 030 exception does not carry forward.
- Exclude general MSAA, dynamic-resolution upscaling, DLSS/FSR/XeSS and unrelated effects. Full profiling remains Feature 041.

## Next executable work

Read roadmap Phase 031, repository constitution, 030 runtime/UI contracts and 029 output-transform contracts; create spec.md and acceptance checklist, then plan/contracts and dependency-ordered tasks before implementation. Resolve velocity conventions and history ownership in those contracts rather than silently choosing backend-specific semantics.

Dependencies: 004, 013, 015, 017, 019, 028, 029, 030. Source baseline: 6a9e5df4be780b31a8e3a9ec3ee18b35f0512838. Feature 030 documentation follow-up (T124/T125/T127) stays visible in its task list.
