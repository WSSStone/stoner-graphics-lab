# Feature 025 Validation

Status: **in progress**

This directory owns normalized, reproducible evidence for Asset Cooker,
manifest, derived-data, publication, and standalone-validation gates. Reports
must name the command, source revision, target profile digest, fixture-manifest
digest, result category, and deterministic artifact digests. Host timing and
RSS belong in a separate telemetry section.

Large DDC entries, request staging, cooked generations, and raw CI logs are not
tracked. They remain under ignored `Saved/` roots or uploaded CI artifacts.

## Required Reports

- `foundation-contracts.txt` and `foundation-architecture.txt`
- `us1-clean-deterministic-cook.txt`
- `us2-derived-data.txt`
- `us3-atomic-publication.txt`
- `us4-target-profiles.txt`
- `us5-cli-reproduction.txt`
- `determinism-20-runs.txt`, `concurrency.txt`, and `performance-m4-pro.txt`
- `regressions.txt`, `architecture.txt`, and `cross-platform-ci.txt`

The feature is not complete until every report is backed by a passing command
at the final revision and `fixture-manifest.json` covers every checked-in
Feature 025 fixture.
