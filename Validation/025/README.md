# Feature 025 Validation

Status: **complete**

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

## Final Result

Feature 025 passed local macOS strict Debug and Release builds, complete engine
regressions, contract/fixture/architecture verification, deterministic and
concurrency gates, the Apple M4 Pro benchmark, and every cross-platform gate in
GitHub Actions run 31827665459. The run completed 8/8 jobs across Windows,
macOS, and Linux, including Linux ASan/UBSan and TSan. Downloaded artifacts
confirmed 15/15 DDC corruption cases, 30/30 published corruption cases, and
the 1,000-asset/5,000-edge CI benchmark on all three Release hosts.

The authoritative remote identities and artifact digests are recorded in
`reports/cross-platform-ci.txt`. Generated DDC entries, staging directories,
cooked generations, and raw logs remain excluded from version control.
