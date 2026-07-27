# B06-S08 Evidence: Shader Library Variant Metadata Fix

Finding: `CR001-B06-F002`.

Fix commit: `0daa334`.

Code evidence:

- `Source/Renderer/Private/FShaderLibrary.cpp` validates duplicate allowed flags,
  empty allowed flags, empty variant ids, undeclared variant flags, duplicate
  variant ids, and duplicate canonical variant keys before record insertion.

Test evidence:

- `Tests/RendererMaterialShaderTests.cpp` covers duplicate allowed flags,
  undeclared variant flags, duplicate variant ids, and duplicate variant keys at
  registration time.

Gate evidence:

- `gate-fallback-strict.json`: passed at `2026-07-27T06:24:18+00:00`.
- `gate-strict-release.json`: passed at `2026-07-27T06:24:35+00:00`.
- `gate-sanitizers.json`: passed at `2026-07-27T06:25:57+00:00`.

Decision: `CR001-B06-F002` is Fixed and ready for a later B06 verification step.
