# B02-S11 Local Fix Evidence

- Fix commit: `70cacb7`
- Host: macOS arm64
- CoreMath Debug: `91/0`
- CoreMath optimized Release: `91/0`
- Strict Debug fallback build/full tests: pass
- Strict Release build: pass
- ASan/UBSan build/full tests: pass
- Scene exact-TRS regression: pass
- GitHub Actions used: none

## Reproduction Transition

| Probe | Before | After |
|---|---|---|
| Large finite quaternion normalization | Identity/zero rotation | Unit finite rotation |
| Equivalent `q` and `-q` | Unequal | Nearly equal |
| Zero matrix with negative tolerance | Success with NaN | Failure with Identity |
| Non-representable TRS composition/inverse | False success | Explicit failure |
| PreserveWorld hierarchy requiring shear | Mutated with wrong transform | `InvalidHierarchyOperation`, no mutation |
| Scaled plane equation at its plane point | Signed distance `-1` | Signed distance `0` |
| Extreme finite box center | Non-finite | Finite endpoint |
| Infinite-radius sphere validity | Valid | Invalid |
| Large sphere far-point containment | Incorrectly contained | Rejected |

## Authoritative Gate Records

- `Evidence/gate-fallback-strict.json`
- `Evidence/gate-strict-release.json`
- `Evidence/gate-sanitizers.json`
