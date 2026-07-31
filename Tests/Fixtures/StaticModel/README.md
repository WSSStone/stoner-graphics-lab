# Feature 024 Static Model Fixtures

This directory contains the checked-in, offline fixture corpus for static
model import. `Valid/` holds Khronos-valid in-scope models, `Invalid/` holds
repository-owned malformed or excluded-input mutations, and `Golden/` holds
expected canonical import facts. Fixture metadata lives in
`Validation/024/fixture-manifest.json`; every fixture added here must have a
corresponding manifest entry before it becomes a required test input.

No validation command downloads fixtures. Source licenses and attributions are
recorded under `Validation/024/licenses/`.
