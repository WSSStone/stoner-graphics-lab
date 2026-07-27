# Evidence: B08-S11 Deferred Native Local-Light Coverage Verified

## Finding

- `CR001-B08-F005`: Native deferred reference scene under-covers local light volume cases.

## Code Changes

- Added explicit transition into the lighting attachment's first color-attachment layout before the lighting pass, fixing the first-convention native lighting write failure observed on macOS.
- Expanded the native deferred reference scene from 3 to 7 light records:
  - directional
  - point visible
  - point outside-view
  - point camera-inside
  - spot visible
  - spot outside-cone
  - spot near-plane-intersecting
- Added named `LocalLightCase` readback probes for both `StandardZ` and `ReversedZ`.
- Updated native integration tests to require extended probe counts and the local-light case probe identities.

## Verification

- `conda run -n stoner-cr scons config=debug --implicit-deps-changed`: passed.
- `STONER_REQUIRE_DEFERRED_NATIVE=1 STONER_DEFERRED_READBACK_REPORT=/private/tmp/f005-final-native-report.txt Build/Mac/Debug/Tests/StonerTest`: passed.
- `conda run -n stoner-cr python CodeReview/Tools/crctl.py gate fallback-strict --id CR-001`: passed at `2026-07-27T10:13:54+00:00`.

## Native Report Summary

- report: `CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/b08-deferred-native-readback-f005-final-report.txt`
- runtime: `RealRuntime`
- adapter: `Apple M4 Pro`
- native submission: `true`
- draw count: `20`
- light count: `7`
- probes: `36`
- final live objects: `0`
- result: `PASS`

Required local-light probes are present and passed for both depth conventions:

- `point-visible`
- `point-outside-view`
- `point-camera-inside`
- `spot-visible`
- `spot-outside-cone`
- `spot-near-plane`
