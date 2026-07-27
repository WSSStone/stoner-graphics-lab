# Evidence: B08-S12 Deferred Native Execution And Readback Verification

## Scope

- Step: `B08-S12 Verify: deferred native execution and readback`
- Batch: `B08 Integration 018-019`
- Head under verification: `f0b82a55fd5f475a12201fa7b701434f1f2a5f2d`

## Commands

- `conda run -n stoner-cr python CodeReview/Tools/crctl.py gate debug --id CR-001`
- `STONER_REQUIRE_DEFERRED_NATIVE=1 STONER_DEFERRED_READBACK_REPORT=CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/b08-s12-deferred-native-readback-report.txt Build/Mac/Debug/Tests/StonerTest`
- `rg -n "result=|native_submission|draw_count|light_count|point-|spot-|final_live_objects" CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/b08-s12-deferred-native-readback-report.txt`

## Results

- CR debug gate passed at `2026-07-27T10:18:43+00:00`.
- Required macOS native deferred validation exited with code `0`.
- Native report:
  - `native_submission=true`
  - `draw_count=20`
  - `light_count=7`
  - `final_live_objects=0`
  - `result=PASS`

## Local-Light Coverage

The report includes passing `LocalLightCase` probes for both `StandardZ` and `ReversedZ`:

- `point-visible`
- `point-outside-view`
- `point-camera-inside`
- `spot-visible`
- `spot-outside-cone`
- `spot-near-plane`

## Conclusion

B08 deferred native execution/readback verification passes locally on macOS native Vulkan and preserves the required expanded local-light coverage from `CR001-B08-F005`.
