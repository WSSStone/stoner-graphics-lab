# Feature 019 Completion Audit

Status: **complete**

## Delivered Identity

- Feature: `019-deferred-rendering-pipeline`
- Roadmap phase: `018 — Renderer: Deferred Rendering Pipeline`
- Implementation commit:
  `3012b4f500b8496bf69c2089d534a5b56adc6b77`
- GitHub Actions run: `30079550556`
- Run URL:
  `https://github.com/WSSStone/stoner-graphics-lab/actions/runs/30079550556`

The roadmap phase and SpecKit feature numbers differ because Feature 018
already delivered the roadmap's Phase 017 triangle integration milestone.

## Platform Gates

| Job | Job ID | Result | Required evidence |
|-----|--------|--------|-------------------|
| macOS headless | `89437773852` | Pass | Build, complete deterministic suite, endurance validation |
| Windows headless | `89437773871` | Pass | Build, complete deterministic suite, endurance validation |
| Linux headless | `89437773921` | Pass | Deterministic suite, Lavapipe native-headless, deferred native readback, injected failures, comparison |

All jobs used the same commit. Existing forward-renderer and triangle-demo
regressions ran inside the same `StonerTest`/CI workflow and passed.

## Retained Linux Evidence

`Validation/019/Linux/deferred-readback-report.txt`

- SHA-256:
  `f601576de6401fac6fe08e5d5f9b15e2ea0c288fce7e1e5b57d0796d9384ad74`
- Runtime: real Vulkan through `llvmpipe (LLVM 20.1.2, 256 bits)`
- Reference path: `NativeDeferredReadback`
- Native submission: complete
- Standard-Z probes: 12/12 passing
- Reversed-Z probes: 12/12 passing
- Non-finite or duplicate probes: none
- Peak/final deferred live objects: `88/0`

`Validation/019/Linux/renderer-comparison-report.txt`

- SHA-256:
  `5c26e000c24184b53c20fad333d78f010b5c0f54e10e515f6d52065ce42c0795`
- Executed tiers: `0`, `16`, `64`, `256`
- Samples: 100 measured frames after 20 warm-up frames per tier
- Fingerprints: equivalent at every tier
- Deferred surface work: constant at 100 draws
- Recorded crossover: `DeferredAt64`
- Result: pass; timing was evidence, not a speedup gate

## Failure And Cleanup Gates

The deferred native suite always runs the runtime-independent lifecycle model.
The required Linux native profile additionally injects
`PartialInitialization`, `Record`, `Submit`, `Fence`, `Copy`, `Map`, `Decode`,
and `Probe` failures into real Vulkan sessions. CI passed only after every
injection stopped before later success, used bounded fence completion, released
partial state in reverse order, and reached zero live frame-owned objects.

Local Apple M4 Pro/MoltenVK validation also passed the normal 24-probe readback
and all eight real injected failure points.

## Contract Conclusion

FR-001 through FR-025 and SC-001 through SC-010 are satisfied. Deferred is an
explicit sibling strategy; forward remains the default. The implementation
provides world-space GBuffer normals, convention-matched StandardZ/ReversedZ,
bounded and instanced local-light volumes, deterministic diagnostics,
failure-safe native readback, four-tier comparison evidence, and passing
Windows/macOS/Linux validation without a semantic oracle.
