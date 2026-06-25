# Research: RHI Core Interfaces

**Feature**: 007-rhi-core-interfaces  
**Date**: 2026-06-25

## Decision: Use explicit result/status values for recoverable RHI outcomes

**Rationale**: The spec clarifies that recoverable failures and invalid lifecycle states must be deterministic and testable. Explicit result/status values let tests assert invalid state, unsupported capability, timeout, resize-required, and success outcomes without depending on exceptions, logs, or debug-only assertions.

**Alternatives considered**:
- Boolean-only results: rejected because negative-path tests would lose reason information.
- Exceptions: rejected because the existing engine style is simple C++ contracts and deterministic tests, not exception-driven control flow.
- Assertions only: rejected because invalid runtime states must remain recoverable and testable.

## Decision: Device is the authoritative factory/owner for RHI core objects

**Rationale**: A device-owned lifecycle matches the roadmap's `IRHIDevice` role and keeps queue, command buffer, synchronization, and swapchain creation consistent across mock and future backend implementations.

**Alternatives considered**:
- Independent object creation: rejected because ownership and shutdown behavior would become ambiguous.
- Queue-owned command buffer/synchronization creation: rejected because queues are execution lanes, not the global capability/lifetime authority.
- Deferring ownership to implementation: rejected because tasks and tests need clear lifecycle expectations.

## Decision: Keep command recording symbolic in this phase

**Rationale**: The next roadmap feature owns resources and pipeline interfaces. Symbolic draw, dispatch, and barrier commands allow lifecycle and ordering behavior to be validated now without inventing placeholder resource or pipeline validation that would be replaced later.

**Alternatives considered**:
- Full resource/pipeline validation now: rejected as scope creep into the next RHI feature.
- No command payloads, lifecycle only: rejected because renderer-facing smoke flow would be too thin.
- Placeholder handles: rejected because they can harden accidental API shapes before resource contracts exist.

## Decision: Include only a headless/mockable swapchain contract

**Rationale**: RHI core must define acquire, present, and resize-required behavior for future renderer/application integration, but real native windows and platform/backend surfaces belong to later phases.

**Alternatives considered**:
- Exclude swapchain entirely: rejected because the roadmap lists `IRHISwapchain` in RHI core and Phase 008 depends on it.
- Include native window binding: rejected because that would couple this feature to Application/Platform presentation concerns.
- Include full surface ownership: rejected because surface creation belongs to backend and windowing phases.

## Decision: Require lifecycle-state matrix and negative-path tests

**Rationale**: This feature's value is a stable abstraction boundary. Matrix tests catch invalid transitions early and make future backend conformance easier.

**Alternatives considered**:
- Smoke tests only: rejected because they would miss core lifecycle ambiguity.
- Exhaustive randomized state-machine tests: rejected for this phase because deterministic matrices are sufficient and easier to maintain.
- Defer negative-path detail: rejected because invalid lifecycle reporting is already a clarified requirement.

## Decision: Use focused headers per RHI contract plus `RHIMinimal.h` aggregate

**Rationale**: Focused headers preserve separable ownership and reduce accidental include coupling, while `RHIMinimal.h` gives downstream code a stable aggregate include consistent with existing project style.

**Alternatives considered**:
- Single monolithic RHI header: rejected because it trends toward a god-header and hides responsibility boundaries.
- Header-only mock/implementation in production code: rejected because mocks belong in tests and production contracts should remain backend-agnostic.

## Decision: Define contracts as C++ public API documentation

**Rationale**: This is a library layer, not a service. The relevant contract artifact is a public RHI API contract documenting types, states, and expected outcomes for implementers and consumers.

**Alternatives considered**:
- OpenAPI or endpoint schema: rejected because there is no network API.
- No contract artifact: rejected because Speckit planning should capture public interfaces for future tasks and tests.
