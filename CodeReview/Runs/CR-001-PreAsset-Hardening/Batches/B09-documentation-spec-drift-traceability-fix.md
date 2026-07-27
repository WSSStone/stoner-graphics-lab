# B09-S14 Fix: Documentation, Specification Drift, And Traceability

## Scope

Fixed findings accepted in B09-S13:

- `CR001-B09-F006`: Traceability matrix remains unclassified with no evidence coverage
- `CR001-B09-F007`: Feature 019 HTML summary still claims Roadmap Phase 018

## Traceability Fix

Updated the reusable traceability seed tool so future CR runs do not generate an empty evidence matrix:

- `CodeReview/Tools/spec_trace.py`
  - Added feature-level evidence mappings for Features 003-019.
  - Added `enrich_records()` to populate API, implementation, tests, CI/validation evidence, notes, and `FeatureMapped` classification.
  - Changed `write_seed()` to emit enriched records by default.
- `CodeReview/Tools/tests/test_spec_trace.py`
  - Added coverage proving `write_seed()` emits `FeatureMapped` rows with known feature evidence.

Regenerated `CodeReview/Runs/CR-001-PreAsset-Hardening/traceability.csv` from current specs via `crctl trace`.

Result:

- Current rows: 466 FR/SC records for Features 003-019.
- Classification: 466 `FeatureMapped`.
- API evidence populated: 466 rows.
- Implementation evidence populated: 466 rows.
- Test evidence populated: 466 rows.
- CI/validation evidence populated: 466 rows.
- Notes populated: 466 rows.

The original inspection saw 463 rows because the existing CSV was stale. Regeneration from current specs found 466 records, preserving current source-of-truth requirements instead of retaining an outdated row count.

## Documentation Drift Fix

Updated `doc/019-deferred-rendering-pipeline.html`:

- metadata now says `Roadmap Phase 019`;
- feature table now says Roadmap 2.0 uses Phase 019 and aligns with Speckit feature numbering.

## Verification

- `conda run -n stoner-cr python -m unittest CodeReview.Tools.tests.test_spec_trace CodeReview.Tools.tests.test_crctl`
  - Result: passed, 8 tests.
- Traceability quantification script:
  - rows: 466;
  - classification: `FeatureMapped=466`;
  - API/implementation/tests/CI/notes populated for all 466 rows.
- `rg -n "Roadmap Phase 018|Roadmap 使用 Phase 018|offset Phase 019" doc/019-deferred-rendering-pipeline.html doc/roadmap.md specs/002-engine-development-roadmap/spec.md`
  - Result: only the intentional roadmap warning `Do not ... create an offset Phase 019 entry` remains; no Feature 019 HTML Phase 018 drift remains.
- `python CodeReview/Tools/crctl.py lint --id CR-001`
  - Result: passed.

## Result

`CR001-B09-F006` and `CR001-B09-F007` are Fixed. B09-S15 should independently verify the regenerated matrix and the 019 HTML correction from current HEAD.
