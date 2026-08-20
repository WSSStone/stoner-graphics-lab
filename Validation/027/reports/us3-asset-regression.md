# Feature 027 US3 Asset Regression

Date: 2026-08-20
Host: macOS arm64
Configuration: strict Debug (`target=debug strict=1`)

## Result

PASS. Feature 027's Metal shader payload, publication, and strict runtime work
preserves the Feature 023, 025, and 026 contracts. The local Apple Metal
toolchain is not eligible for native hardware evidence, so this report does not
claim T089 native final-cook completion.

## Focused Feature 027 Gates

- `metal-shader-cooker`: PASS
- `metal-shader-publication`: PASS
- `metal-shader-runtime`: PASS
- `renderer-material-asset`: PASS

The publication gate accepted complete v2 MetalLibrary envelopes and rejected
source-only, CPU-incompatible, corrupted, and missing-finalization images
without replacing `Current.json`. The runtime gate loaded two stages through
`FAssetManager` strict cooked mode with no resolver/loader registration, rejected
backend/CPU/profile/stage/entry/version/interface/missing-payload mismatches, and
retained owned RHI bytes and binding maps after Asset release.

## Feature 023 Regression

- `asset-material-shader`: PASS
- `renderer-material-asset`: PASS

The existing repository shader selection and canonical Material/Shader Asset
corpus remain green. Metal selection now additionally requires the Asset shader
interface to match the logical entries in native binding evidence.

## Feature 025 Regression

- `asset-cooker-codec`: PASS
- `asset-cooker-derived-key`: PASS
- `asset-cooker-publication`: PASS
- `asset-cooker-published-validation`: PASS
- `asset-cooker-workflow`: PASS

Canonical DDC metadata now directly proves that all seven v2 named evidence
records survive encode/decode. Atomic generation publication and the 30-case
published-corruption corpus remain green.

## Feature 026 Regression

- `asset-manager-contract`: PASS
- `asset-manager-cooked`: PASS
- `asset-manager-equivalence`: PASS

Strict cooked mode continues to preserve source/cooked semantics and performs
zero resolver or source-fallback calls.
