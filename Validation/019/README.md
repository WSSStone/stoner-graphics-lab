# Feature 019 Validation Evidence

`Validation/019/` retains only normalized, reviewable evidence required by the
deferred-rendering validation contract. Build products, generated shaders,
temporary logs, screenshots, and machine-local paths remain under `Build/` or
CI temporary storage and are not committed here.

Required Linux CI artifacts:

- `Linux/deferred-readback-report.txt`
- `Linux/renderer-comparison-report.txt`
- `completion.md`

The readback report proves native Lavapipe execution for both standard-Z and
reversed-Z, at least 18 named semantic probes per convention, the six
required point/spot local-light edge probes per convention, and zero final
deferred frame-owned resources. The comparison report contains the normalized
0/16/64/256 local-light tiers with at least 100 measured frames per strategy.

No screenshot is a Feature 019 completion gate. Failed CI reports should still
be uploaded when available, but only artifacts tied to a passing commit/run are
retained as completion evidence.

## CR-001 Evidence Note

The retained Linux artifacts in this directory were captured for the original
Feature 019 closeout. CR-001 later strengthened the native deferred reference
scene to require 18 probes per depth convention plus explicit point/spot
local-light edge probes. Fresh post-CR Linux artifacts must therefore be tied
to a newer CI run and must satisfy the stricter wrapper validation before they
can be used as current closeout evidence.
