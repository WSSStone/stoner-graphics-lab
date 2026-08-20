# Feature 027 Validation

Status: **in progress**

This directory owns normalized evidence for the native Metal backend. Accepted
reports distinguish deterministic, native-offscreen, visible-manual, and
cross-backend tiers. A native pass requires a real Metal device, committed GPU
work, completion, and readback or presentation evidence; deterministic output
and semantic expectations cannot substitute for native execution.

Tracked evidence records the source revision, host architecture, target profile,
device/capability identity where applicable, shader and binding evidence,
commands, stable results, and artifact digests. Raw logs, temporary compiler
files, local publications, and downloaded CI artifacts remain under ignored
`Validation/027/work/`, `Validation/027/CI/downloaded/`, or build roots.
