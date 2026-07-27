# B08-S09 Evidence: Deferred Frame Plan, Surfaces, And Graph Verification

Step: `B08-S09`.

Finding verified:

- `CR001-B08-F004`: Deferred surface pass declares lighting accumulation writes.

Verification command:

- `conda run -n stoner-cr python CodeReview/Tools/crctl.py gate fallback-strict --id CR-001`

Result:

- Passed.
- Recorded at `2026-07-27T09:27:29+00:00`.

Gate details:

- Build command: `scons config=debug strict=1 graphics=disabled`
- Build return code: `0`
- Test command: `Build/Mac/Debug/Tests/StonerTest`
- Test return code: `0`

Focused regression output:

```text
[PASS] Deferred graph ownership keeps lighting accumulation out of surface writes
[PASS] Deferred empty frame graph keeps accumulation as composition input only
```

Finding state evidence:

- `CR001-B08-F004` moved to `Verified`.
- Verification note records the fallback-strict pass and focused lit/empty-frame
  graph ownership PASS lines.

Next step:

- Continue CR-001 with the next B08 packet from `crctl next`.
