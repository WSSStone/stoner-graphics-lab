# Contract: Metal Shader Derivation And Cooking

## Authority And Layering

Repository GLSL and validated SPIR-V remain authoritative. Metal source and
native libraries are derived data with the same stable Shader Asset identity.
SPIRV-Cross, binding assignment, and Apple compiler orchestration live in
`Tools/AssetCooker`; Asset stores bytes plus canonical binding/evidence values
but does not depend on Tools, RHI, Renderer, Backend, or Metal. Renderer selects
a compatible payload and copies typed bytes plus binding evidence into RHI.

## Cross-Platform Derivation

Input consists of exact SPIR-V bytes/digest, stage, entry point, interface,
Shader Asset/version, target profile, binding policy, SPIRV-Cross commit, and
fixed transformation options. The derivation must:

1. validate every input and declared interface;
2. configure explicit MSL resource bindings from policy v1;
3. generate MSL 2.4-compatible source;
4. project each logical Shader Asset entry point `name` to the deterministic
   native symbol `stoner_name`, retaining the logical entry point in Asset and
   RHI contracts;
5. normalize UTF-8, LF endings, final newline, and volatile comments/paths;
6. emit canonical evidence conforming to `metal-shader-evidence.schema.json`;
7. fail closed for unsupported SPIR-V or target limits.

Windows, macOS, and Linux must produce identical normalized MSL and derivation
evidence for the same inputs.

The checked-in SPIRV-Cross translation units are exactly `spirv_cross.cpp`,
`spirv_cross_parsed_ir.cpp`, `spirv_parser.cpp`, `spirv_cfg.cpp`, and
`spirv_glsl.cpp` and `spirv_msl.cpp`, plus their required headers. The GLSL unit
is the upstream-required base implementation for `CompilerMSL`, not an
additional runtime/source payload path. Adding another translation unit is
a reviewed dependency change with updated provenance and source hashes.

## Native Finalization

Only macOS may finalize. AssetCooker invokes `/usr/bin/xcrun` through a bounded
Core process API with explicit argv and no shell:

```text
xcrun -sdk macosx metal [fixed target/options] input.metal -o output.air
xcrun -sdk macosx metallib output.air -o output.metallib
```

Temporary files are confined to the cook work directory. Success requires both
commands to exit 0, nonempty output, complete compiler/SDK/profile evidence, and
a validated native library load in a matching Metal device test. Windows/Linux
finalization returns explicit `HostUnsupported` and publishes nothing.

## Schema Compatibility

- `MSL` identifies normalized source evidence only.
- `MetalLibrary` identifies final runtime binary payload bytes.
- Target profile v2 adds `MetalLibrary` and Metal deployment/language/toolchain
  requirements while retaining a reader for v1 Vulkan profiles.
- Cooked shader-payload codec v2 stores exact binary bytes and evidence digest;
  v1 SPIR-V payloads remain readable.
- Existing generations and DDC entries are immutable and are not rewritten.

## Derived Key

The DDC key includes all authoritative source/SPIR-V versions, stage/entry/
interface, SPIRV-Cross commit and options, binding-policy version/digest, target
profile, MSL/deployment values, and for finalization the Apple compiler/SDK
identity. Changing any field invalidates the relevant derived result.

Twenty finalizations compare metallib bytes and DDC/evidence identities only
when architecture, deployment target, Xcode build, SDK, compiler, profile, and
all authoritative inputs are identical. Different tuples compare derivation
identity only; native byte identity is not portable across Apple toolchains or
architectures.

## Runtime Consumption

Strict cooked runtime selects a `MetalLibrary` payload matching backend, CPU
architecture, target profile, stage, entry, interface, Asset version, and
evidence. It does not read GLSL/SPIR-V/MSL source or invoke a compiler. Pipeline
creation retains RHI/native ownership only; releasing Asset handles cannot
invalidate an already-created pipeline.

The Metal backend resolves the retained logical entry point through the same
`stoner_` prefix projection used by the pinned cooker. It never probes alternate
symbols or silently falls back to SPIRV-Cross's implementation-defined cleaned
name.
