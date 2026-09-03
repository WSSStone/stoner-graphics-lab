# Contract: HDR Output Pipeline

## Public Boundary

Forward and Deferred MUST expose one `FHDRSceneColorHandoff`. Neither may select
a tone map, output encoding, swapchain format, or formal readback path.

Conceptual public entry points:

```cpp
FOutputTransformPrepareResult FHDRPostProcessPipeline::Prepare(
    const FHDRSceneColorHandoff& SceneColor,
    const FOutputTransformSettings& Settings) const;

FOutputTransformGraphDeclaration FHDRPostProcessPipeline::DeclareGraph(
    FRenderGraph& Graph,
    const FOutputTransformPlan& Plan) const;

FOutputTransformExecutionResult FOutputTransformExecutor::Execute(
    const FOutputTransformPlan& Plan,
    const FCompiledRenderGraph& Graph,
    const FOutputTransformExecutionBindings& Bindings) const;
```

All inputs/results are backend-neutral. Public Renderer headers expose no
Vulkan, Metal, Cocoa, native window, validation, or Asset handles.

## Stage Contract

The only legal canonical order is:

1. `SceneColorHandoff`
2. `ManualExposure`
3. zero or more `PreTonemap` operations
4. exactly one `SDRToneMap` or `HDRViewingTransform`
5. zero or more `PostTonemap` operations
6. exactly one `OutputDeviceTransform`
7. optional `FormalReadback`
8. optional `Presentation`

Pre-tonemap input is exposed linear Rec.709/D65 RGBA16F after exposure. Post-
tonemap input is display-referred linear color with explicit reference-white/
peak meaning before final gamut/transfer encoding. An operation cannot change
extent, sample count, domain, transform version, output profile, or identity.

## Strategy and Composite Rules

- Each transform Strategy has one version identity and exact input/output
  domains. Unknown identities fail; no nearest or default-native selection.
- An insertion Composite has at most 16 operations, unique stable identities,
  unique order keys, acyclic dependencies, and bounded declared resources.
- Empty Composites are valid and do not change the plan fingerprint except for
  their canonical empty representation.
- Duplicate writers, read-before-write, undeclared hazards, cycles, or a second
  formal output fail before native recording.

## Formal Output and Diagnostic Bypass

One plan publishes at most one formal output. A diagnostic bypass selects a
named stage and declares its source domain plus either HDR-preserving readback
or a bounded visualization transform. It is a separate non-authoritative output
and cannot alter, replace, or be accepted as the formal output.

## Failure Contract

Preparation, graph compile, binding, recording, submission, completion,
readback, or presentation failure publishes no formal output. Diagnostics retain
the first stable actionable failure, stage, operation/profile/version identity,
and recovery class without native pointers or sensitive machine data.
