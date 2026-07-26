# B02-S12 Local Spatial Math Evidence

- Fix commit under review: `70cacb7`
- Verification host: macOS arm64
- Production/spec changes during verification: none
- Independent Debug property probe: `1597/0`
- Independent optimized Release property probe: `1597/0`
- CoreMath Debug suite: `91/0`
- CoreMath Release suite: `91/0`
- Strict Debug build: pass
- Strict Release build: pass
- Strict fallback full suite: pass
- ASan/UBSan strict full suite: pass
- Stale infallible transform composition search: no matches
- Patch whitespace/error check: pass
- GitHub Actions used: none

## Requirement Coverage

| Finding | Direct local evidence | Remaining evidence |
|---|---|---|
| `CR001-B02-F007` | 56 `q/-q` and matrix-rotation checks plus huge finite quaternion normalization/inverse pass in Debug and Release | Windows/Linux CI |
| `CR001-B02-F008` | 1,491 exact composition/inverse/reflection/shear checks, Scene regressions, strict suites, and sanitizers pass | Windows/Linux CI |
| `CR001-B02-F009` | Extreme finite box, sphere, and plane checks pass in Debug and Release | Windows/Linux CI |

## Authoritative Gate Records

- `Evidence/gate-strict-debug.json`
- `Evidence/gate-strict-release.json`
- `Evidence/gate-fallback-strict.json`
- `Evidence/gate-sanitizers.json`

The temporary probes and instrumented diagnostics remain outside the repository
under `/tmp`.
