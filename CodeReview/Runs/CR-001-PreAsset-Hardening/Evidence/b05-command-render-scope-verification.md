# B05-S03 Verification Evidence

## Revisions

- Defective implementation parent: `bd7b8c8`
- Fix commit: `7e92de1`
- Verification source state: `84d4fb5`

Historical evidence was gathered by static Git comparison. Current behavior
was verified with maintained tests and predefined ordinary local gates.

## Requirement Matrix

| Finding | Parent signal | Current evidence | Result |
|---|---|---|---|
| CR001-B05-F001 | Device lacks destructor shutdown; command invalidation retains diagnostics observer | Destructor shutdown, pool-owned invalidation, observer detach, retained-command maintained test | Verified |
| CR001-B05-F002 | Constructors and submitted/completed transitions are public | Private owner API, compile-time closure assertions, explicit factory return paths | Verified |
| CR001-B05-F003 | Base-level region bounds, no copy compatibility, unchecked readback multiplication | Selected-mip helper, format/dimension/sample checks, checked exact footprint helper and maintained boundaries | Verified |

## Maintained Signals

The full Debug suite returned exit 0 and includes:

- retained command buffer rejects recording after device destruction;
- zero command-buffer capacity returns `Unavailable`;
- direct wrapper construction and lifecycle transitions remain inaccessible at
  compile time;
- oversized selected-mip texture copy and readback are rejected;
- valid selected-mip texture copy succeeds;
- incompatible texture formats are rejected;
- padded readback footprint equals 72 bytes;
- unrepresentable footprint returns false and zero output;
- deferred readback with a missing target returns the expected failure and
  resets partial command recording.

## Gate Files

- `gate-strict-debug.json`
  - `scons config=debug strict=1`
  - exit 0
- `gate-tests.json`
  - `scons config=debug`
  - `Build/Mac/Debug/Tests/StonerTest`
  - both exit 0
- `gate-strict-release.json`
  - `scons config=release strict=1`
  - exit 0

All files are under
`CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/`.

## Boundaries

- No production or maintained test source changed in B05-S03.
