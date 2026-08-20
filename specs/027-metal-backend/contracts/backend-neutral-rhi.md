# Contract: Backend-Neutral RHI Corrections

## Purpose

Feature 027 may correct RHI vocabulary only where the current SPIR-V/Vulkan
shape cannot express an existing backend-neutral semantic. Every change in this
contract migrates Vulkan, mocks, callers, tests, and documentation in the same
milestone.

## Shader Payload

- Replace word-only shader storage with immutable typed bytes.
- Required Feature 027 formats are `SPIRV` and `MetalLibrary`; unknown formats
  fail before backend object creation.
- Stage, entry point, interface metadata, payload identity, target profile, and
  digest remain explicit and are validated independently of bytes.
- SPIR-V-specific structural helpers accept a checked byte view and may convert
  to aligned words privately. No generic RHI API exposes SPIR-V word storage.
- Vulkan accepts only `SPIRV`; Metal accepts only `MetalLibrary`. Neither backend
  silently transforms or compiles payloads.

## Capability Vocabulary

`FRHIDeviceCapabilities` adds explicit, backend-neutral limits needed to validate
existing public operations:

- maximum buffer/texture dimensions and resource bytes;
- per-stage buffer, texture, and sampler bindings;
- constant-data ranges and bytes;
- compute threadgroup dimensions and total threads;
- supported sample counts and existing per-format usage flags;
- synchronization and presentation support already represented by RHI.

Zero means unsupported, not unknown. Capability snapshots must pass invariant
validation before a device reaches `Ready`.

## Native Binding Metadata

`FRHINativeBindingMap` is an immutable API-neutral value containing a policy
identifier, canonical ordered `(stage, set, binding, descriptor type, array
element) -> (native resource class, native index)` entries, reserved ranges,
target-limit snapshot, and digest. Asset stores equivalent cooked evidence;
Renderer copies it into RHI metadata after validating the Asset record. RHI and
Backend do not depend on Asset or Tools. Metal validates and consumes this map
without assigning resource indices independently.

## Operation Baseline

`rhi-operation-matrix.md` freezes every public Feature 007/008 interface method
before implementation. A verifier extracts the current headers and fails for an
unlisted or duplicated operation. All operational methods are applicable to
Metal; `Unsupported` is accepted only for an invocation rejected by a published
device capability, never for an unfinished method.

## Device And Object Rules

- All child objects carry an opaque owner identity and generation-safe lifecycle.
- Public operations reject null, stale, foreign-owner, destroyed, and descriptor-
  incompatible objects before implementation-specific use.
- Recorded or in-flight references retain implementation ownership until terminal
  submission completion.
- `Unsupported` is allowed only for a capability the selected backend/device
  truthfully lacks; it is not an implementation-progress result.

## Presentation Bridge

`FRHIPresentationSurfaceDesc` continues to contain the borrowed Core platform
window and backend-neutral presentation requirements. It does not expose
`NSView`, `CAMetalLayer`, or Vulkan surface handles. Backends own their API
presentation object and release it before the borrowed window expires.

## Backend Selection

Demo composition chooses an explicit backend enum/config before creating RHI.
An explicit Metal request returns Metal success or Metal failure. It cannot
substitute Vulkan/MoltenVK while preserving a success result.

## Compatibility

- Existing serialized Feature 025/026 Vulkan payloads remain readable.
- Existing Vulkan tests must pass after payload migration.
- Public headers compile on Windows, macOS, and Linux without graphics SDK types.
- Architecture checks include `.mm` files and reject Metal imports above Backend.
- macOS compile/link/finalization actions use deployment target 12.0 and reject
  unguarded use of newer APIs.
